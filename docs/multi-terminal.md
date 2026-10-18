# Independent terminal implementation

Task 89 replaces the COM1 singleton state with two bounded terminal objects.
Each owns its queues, editing state, process group, carrier/settings, output delay,
flow control, exclusive-open flag and mpx ownership. Driver callbacks bind output,
formatting and modem operations to the appropriate transport. Unbound devices
return ENODEV; attaching over open descriptions returns EBUSY.

The primary device family uses major 1, minor 0/1. Console and historical profile
aliases continue using terminal zero; /dev/tty follows a valid controlling-terminal
pointer. Per-line receive/error and modem entry points prevent cross-terminal
routing. Existing COM1 entry points remain compatible with private fixtures.

Host acceptance uses two independently captured transports, distinct users/groups,
raw and canonical queues, settings, output, exclusive opens and close lifetime.
COM1 controller, modem, mpx, packet and real UART regressions remain required.
Hardware COM2 binding and full controlling-terminal acceptance follow in 90–93.
