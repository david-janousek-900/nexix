"""Independent V7 packet peer using QEMU COM1, with damaged/lost frames."""
import os
from pathlib import Path
import select
import subprocess
import sys
import time

CHECK = 0o125252

def checksum(body):
    total = 65535
    extra = 0
    for remaining, byte in zip(range(len(body), 0, -1), body):
        total = ((total << 1) | (total >> 15)) & 65535
        previous = total
        total = (total + byte) & 65535
        extra = (extra + (total ^ remaining)) & 65535
        if total <= previous:
            total ^= extra
    return total

def frame(control, body=None):
    code = 9 if body is None else len(body).bit_length() - 5
    check = (CHECK - (control if body is None else checksum(body) ^ control)) & 65535
    header = bytes((16, code, check & 255, check >> 8, control))
    return header + bytes((code ^ header[2] ^ header[3] ^ control,)) + (body or b'')

def data(payload, size, seq, ack=0):
    if len(payload) == size:
        return frame(128 | (seq << 3) | ack, payload)
    missing = size - len(payload)
    prefix = bytes((missing,)) if missing < 128 else bytes(((missing & 127) | 128, missing >> 7))
    return frame(192 | (seq << 3) | ack, (prefix + payload).ljust(size, b'\0'))


def main():
    command = [sys.argv[1], '-machine', 'pc', '-accel', 'tcg', '-cpu', 'qemu64,+rdrand',
               '-smp', '1', '-m', sys.argv[3], '-cdrom', sys.argv[2], '-display', 'none',
               '-serial', 'stdio', '-monitor', 'none', '-no-reboot']
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = bytearray()
    pending = bytearray()
    session = 0
    binary = False
    started = False
    dropped = False
    received = bytearray()
    ack = 0
    rx_started = False
    eof_sent = False
    saw_eof = False
    rejected = False
    retransmitted = False
    last_payload = b''
    deadline = time.monotonic() + 45
    def send(b):
        process.stdin.write(b)
        process.stdin.flush()
    try:
        while process.poll() is None and time.monotonic() < deadline:
            ready, _, _ = select.select([process.stdout], [], [], .1)
            if not ready:
                continue
            chunk = os.read(process.stdout.fileno(), 4096)
            if not chunk:
                break
            output.extend(chunk)
            pending.extend(chunk)
            while pending:
                if not binary:
                    marker = f'NEXIX_PACKET_READY{session+1}\r\n'.encode()
                    pos = pending.find(marker)
                    if session >= 3 or pos < 0:
                        if len(pending) > 4096:
                            del pending[:-128]
                        break
                    del pending[:pos+len(marker)]
                    session += 1
                    binary = True
                    started = False
                    ack = 0
                    received.clear()
                if len(pending) < 6:
                    break
                assert pending[0] == 16, ('wire synchronization', bytes(pending[:32]))
                _, k, lo, hi, ctl, xor = pending[:6]
                assert xor == k ^ lo ^ hi ^ ctl
                size = 0 if k == 9 else 32 << (k-1)
                assert k == 9 or k == 2, ('negotiated output size', k)
                if len(pending) < 6+size:
                    break
                body = bytes(pending[6:6+size])
                del pending[:6+size]
                assert ((lo | hi << 8) + (checksum(body) ^ ctl if size else ctl)) & 65535 == CHECK
                if not size:
                    kind, value = ctl >> 3, ctl & 7
                    if kind == 7 and not started:
                        assert value == (1 if session == 3 else 2)
                        started = True
                        send(frame(7 << 3 | 2) + frame(6 << 3 | 1)) # Receive 64-byte packets.
                    elif kind == 6:
                        assert value == (2 if session == 1 else 4 if session == 2 else 0)
                        send(frame(5 << 3 | 2))
                    elif kind == 2:
                        rejected = True
                    elif kind == 4 and session == 1 and value == 2 and not eof_sent:
                        eof_sent = True
                        send(data(b'', 128, 3, ack))
                    elif kind == 4 and session == 3 and value == 1:
                        send(data(b'', 32, 2, ack))
                    elif kind == 1:
                        binary = False
                    continue
                assert ctl & 128
                seq = ctl >> 3 & 7
                if ctl & 64:
                    missing = body[0] & 127
                    prefix = 1
                    if body[0] & 128:
                        missing |= body[1] << 7
                        prefix = 2
                    assert prefix <= missing <= size
                    payload = body[prefix:prefix+size-missing]
                else:
                    payload = body
                if session == 1 and seq == 1 and not dropped:
                    dropped = True
                    continue
                if seq != ((ack+1) & 7):
                    send(frame(2 << 3 | ack))
                    continue
                if session == 1 and seq == 1:
                    retransmitted = True
                ack = seq
                received.extend(payload)
                send(frame(4 << 3 | ack))
                if session == 1 and len(received) == 150 and not rx_started:
                    assert received == bytes(i*37 & 255 for i in range(150))
                    rx_started = True
                    payload = bytes((i*19+7) & 255 for i in range(137))
                    one = data(payload[:128], 128, 1, ack)
                    corrupt = bytearray(one)
                    corrupt[23] ^= 1
                    send(bytes(corrupt) + one + one + data(payload[128:], 128, 2, ack))
                if session == 1 and not payload:
                    saw_eof = True
                    send(frame(1 << 3))
                if session == 2:
                    assert payload == b'B'
                    last_payload = payload
                    send(data(b'second', 512, 1, ack))
                if session == 3:
                    if payload == b'C':
                        send(data(b'port', 32, 1, ack))
                    else:
                        assert payload == b'DRAIN'
            if b'BOOT OK' in output:
                break
        required = [b'NEXIX_PACKET_OK', b'PACKET CLEANUP OK', b'BOOT OK',
                    b'NEXIX_MPX_TTY_OK', b'TTY CLEANUP OK']
        assert all(m in output for m in required), ('missing markers', [m for m in required if m not in output])
        assert session == 3 and dropped and retransmitted and rejected and saw_eof and last_payload == b'B'
        assert b'PANIC' not in output and b'FAIL' not in output
    except BaseException:
        Path('/tmp/nexix-packet-failed.log').write_bytes(output)
        print(output.decode('ascii', errors='backslashreplace'))
        raise
    finally:
        process.terminate()
        try:
            process.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate()
    print(f'PASS: packet UART peer, checksum rejection, retransmission, two windows, EOF, mpx port, last close/reopen, cleanup ({sys.argv[3]})')

if __name__ == '__main__':
    main()
