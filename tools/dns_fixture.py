"""Isolated DNS wire fixture. All sockets bind 127.0.0.1; no forwarding DNS."""
import contextlib
import socket
import socketserver
import struct
import threading

def name(text):
    return b''.join(bytes([len(label)])+label.encode('ascii') for label in text.rstrip('.').split('.') if label)+b'\0'

def record(owner, kind, data):
    return name(owner)+struct.pack('!HHIH',kind,1,60,len(data))+data

class DNSFixture:
    def __init__(self, primary=False):
        self.primary=primary
        self.queries=[]
        fixture=self
        class UDP(socketserver.BaseRequestHandler):
            def handle(self):
                query,peer=self.request[0],self.client_address
                response=fixture.answer(query,False)
                if response is None:return
                if fixture.last_name(query)=='badid.test':
                    wrong=bytes([response[0]^1])+response[1:]
                    self.request[1].sendto(wrong,peer)
                if fixture.last_name(query)=='badsource.test':
                    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
                        sock.bind(('127.0.0.1',0));sock.sendto(response,peer)
                if fixture.last_name(query)=='badquestion.test':
                    wrong=bytearray(response);wrong[13]^=1;self.request[1].sendto(wrong,peer)
                self.request[1].sendto(response,peer)
        class TCP(socketserver.BaseRequestHandler):
            def handle(self):
                self.request.settimeout(5)
                try:
                    while True:
                        header=fixture.read(self.request,2)
                        if not header:return
                        query=fixture.read(self.request,struct.unpack('!H',header)[0])
                        response=fixture.answer(query,True)
                        if response is None:return
                        for message in response if isinstance(response,list) else [response]:
                            self.request.sendall(struct.pack('!H',len(message))+message)
                except (OSError,ValueError):pass
        class Datagram(socketserver.ThreadingUDPServer):
            daemon_threads=True
        class Stream(socketserver.ThreadingTCPServer):
            daemon_threads=True
            allow_reuse_address=True
        self.udp=Datagram(('127.0.0.1',0),UDP)
        self.port=self.udp.server_address[1]
        self.tcp=Stream(('127.0.0.1',self.port),TCP)
        for server in (self.udp,self.tcp):threading.Thread(target=server.serve_forever,daemon=True).start()
    @staticmethod
    def read(sock,count):
        out=b''
        while len(out)<count:
            part=sock.recv(count-len(out))
            if not part:return None
            out+=part
        return out
    @staticmethod
    def question(query):
        at=12;labels=[]
        while at<len(query):
            length=query[at];at+=1
            if not length:break
            if length>63 or at+length>len(query):raise ValueError('bad question')
            labels.append(query[at:at+length].decode('ascii').lower());at+=length
        if at+4>len(query):raise ValueError('short question')
        return '.'.join(labels),struct.unpack_from('!H',query,at)[0],query[12:at+4]
    @classmethod
    def last_name(cls,query):return cls.question(query)[0]
    def answer(self,query,tcp):
        if not query:return None
        host,kind,question=self.question(query);self.queries.append((host,kind,tcp))
        flags=0x8180;answers=[]
        known={'silent.test','missing.test','servfail.test','failover.test','refused.test','empty.test','truncated.test','zone.test','alias.test','canonical.test','middle.test','multi.test','txt.test','badid.test','badsource.test','badquestion.test','host.search.test','host.second.test'}
        if host not in known and kind!=12:
            return query[:2]+struct.pack('!HHHHH',0x8183,1,0,0,0)+question
        if host=='silent.test':return None
        if host=='missing.test':flags|=3
        elif host=='servfail.test' or (host=='failover.test' and self.primary):flags|=2
        elif host=='refused.test':flags|=5
        elif host=='empty.test':pass
        elif host=='truncated.test' and not tcp:flags|=0x0200
        elif kind==252:
            soa=record(host,6,name('ns.test')+name('hostmaster.test')+struct.pack('!IIIII',1,60,60,60,60))
            first=query[:2]+struct.pack('!HHHHH',flags,1,1,0,0)+question+soa
            last=query[:2]+struct.pack('!HHHHH',flags,0,2,0,0)+record('www.'+host,1,b'\xc0\x00\x02\x2a')+soa
            return [first,last]
        elif kind==12:answers=[record(host,12,name('reverse.test'))]
        elif host=='alias.test':
            answers=[record('canonical.test',1,b'\xc0\x00\x02\x2a'),record('middle.test',5,name('canonical.test')),record(host,5,name('middle.test'))]
        elif kind==1:
            answers=[record(host,1,b'\xc0\x00\x02\x0a')]
            if host=='multi.test':answers.append(record(host,1,b'\xc0\x00\x02\x0b'))
        elif kind==16:answers=[record(host,16,b'\x07fixture')]
        elif kind==2:answers=[record(host,2,name('ns.test'))]
        response=query[:2]+struct.pack('!HHHHH',flags,1,len(answers),0,0)+question+b''.join(answers)
        return response
    def close(self):
        for server in (self.udp,self.tcp):server.shutdown();server.server_close()
    def __enter__(self):return self
    def __exit__(self,*args):self.close()
