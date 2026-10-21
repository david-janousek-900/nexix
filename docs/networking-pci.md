# PCI discovery and the QEMU e1000 (task 416)

`kernel/pci.c` enumerates PCI configuration space and decodes what it finds.
`kernel/pci_port.c` binds that core to x86 port I/O and maps BARs, and
`kernel/net/e1000.c` discovers QEMU's default network adapter. These are fresh
native implementations under Nexix's root BSD-3-Clause license. `pci_init`
runs on every boot after paging is set up and logs one line per function. It
leaves device state unchanged. Only e1000 discovery enables a device.

## Enumeration

Configuration mechanism #1 (`0xcf8`/`0xcfc`) is used with IRQs disabled across
each address/data pair. All 256 buses and 32 devices are scanned. Functions 1–7
are probed only when function 0 sets the multifunction header bit, and gaps
between functions are allowed. At most 32 functions are recorded; the bus
records any truncation. The core reads and writes configuration space only
through `struct pci_config_ops`, which lets host tests supply a simulated bus.

For each function the core records vendor/device IDs, class/subclass/prog-IF,
revision and header type.

- **Interrupt routing.** Legacy INTx is recorded as pin 1–4 and the 8259 IRQ
  that firmware programmed into the interrupt-line register. A pin outside 1–4,
  or a line of 16 or above (such as `0xff`), records `PCI_IRQ_NONE`.
- **BARs.** Memory and I/O decoding are disabled while a BAR is sized by writing
  all ones, and the original values are then restored. Upper bits of an I/O BAR
  that read back as zero are accepted, since x86 ports are 16-bit.
- **BAR types and malformed BARs.** BARs decode as I/O, 32-bit memory or 64-bit
  memory, the last with its upper half consumed. Prefetchability is recorded.
  These read as absent: a size that is not a power of two, a non-contiguous
  mask, and a 64-bit BAR in slot 5. Only type-0 headers have BARs decoded.
- **Command writes.** `pci_enable` sets the requested bits, clears INTx-disable
  and reads the register back to confirm. It writes zeros to the status half so
  that no write-1-to-clear bits are cleared.

## MMIO window

`paging_map_mmio` maps device memory at PML4 entry 511
(`0xffffff8000000000`, 1 GiB). Mappings are supervisor-only, writable, NX and
uncached (PCD|PWT).

- **Sharing.** Every address space shares one window table installed at
  creation, so a mapping made later is visible in every process. Address-space
  teardown skips the shared table, and user walks never reach it.
- **Refused ranges.** Physical frames below 1 GiB are refused: the kernel
  already identity-maps that range as write-back RAM, and an uncached alias
  there would mix memory types. Empty, overflowing and window-exhausting ranges
  are refused too.
- **Allocation.** The window hands out addresses in increasing order, never
  reuses them and never removes a mapping. That fits fixed boot-time devices.
- **BAR checks.** `pci_map_bar` maps only memory BARs that have a base and whose
  base is aligned to their size.

## e1000 discovery

`e1000_discover` looks for vendor `8086` device `100e` (Intel 82540EM, QEMU's
`-nic ...,model=e1000` and the default NIC for `-machine pc`).

- **BAR0.** It requires a memory BAR0 of at least 128 KiB and maps it before
  enabling decode, so if the mapping fails the device stays disabled.
- **Command bits.** It then enables memory decoding and bus mastering (needed
  for task 417's DMA) and clears INTx-disable.
- **Result.** It keeps the PCI function, the register mapping and the INTx IRQ
  (11 on QEMU), and returns the same device on every later call. With no
  supported adapter it returns NULL and nothing is enabled.

The [task 417 driver](networking-e1000.md) handles reset, link and DMA;
interface attachment belongs to task 418.

## Validation

`make test-pci` runs a simulated bus under ASan/UBSan. It covers:

- multifunction devices, gaps, an ignored function 1 behind a single-function
  device, and a device on bus 1;
- I/O BARs, including set upper bits;
- 32-bit and 64-bit prefetchable memory BARs;
- malformed BARs (non-power-of-two size, non-contiguous mask, 64-bit BAR in slot
  5) and bridge headers;
- pin and line validation;
- sizing only while decoding is disabled, with the BAR and command values then
  restored;
- status-preserving command writes and refused command bits;
- table truncation and an empty bus.

`make test-pci-guest` boots the self-test image at 16 MiB and 128 MiB, once with
`-nic user,model=e1000` and once with `-nic none`. With the adapter present it
must print `E1000 FOUND`. The self-test then checks:

- class 02/00 is reported, and memory decoding and bus mastering are enabled
  with INTx left enabled;
- the mapping lies inside the MMIO window;
- STATUS and CTRL read back through the uncached mapping (QEMU reports link up
  and full duplex);
- the window refuses a RAM page, an empty range, a frame below 1 GiB and an
  overflowing range, and leaks no pages.

With no adapter the boot must print `E1000 ABSENT` and complete normally. Every
full boot also requires `PCI SCAN OK` and `PCI OK`.
