Minix i2c Driver
================

TODO: this probably belongs on the wiki

Overview
--------

This is the driver for the i2c bus. It provides the same /dev interface as
NetBSD and OpenBSD (see dev/i2c/i2c_io.h). It also provides an interface for
other drivers to access the I2C bus using Minix IPC.

Organization and Layout
-----------------------

i2c.c					generic i2c bus driver
i2c.conf				configuration for the i2c driver	
arch/					arch specific code
	earm/				earm specific code
		omap/
			omap_i2c.c				AM335X/DM37XX i2c bus driver	
			omap_i2c.h				AM335X/DM37XX function prototypes
			omap_i2c_registers.h 	AM335X/DM37XX register offsets, etc.
		rpi/
			rpi_i2c.c				BCM283X i2c bus driver	
			rpi_i2c.h				BCM283X function prototypes
			rpi_i2c_registers.h 	BCM283X register offsets, etc.

Testing the Code
----------------

Below are the steps needed to start up the i2c driver instances. Though,
now they get started at boot in /usr/etc/rc, it's still useful to know if
you take down the service and need to start it again.

Creating the device files (this is already done automatically, but if not):

cd /dev && MAKEDEV i2c-1 && MAKEDEV i2c-2 && MAKEDEV i2c-3

Starting up the instances:

/bin/service up /service/i2c -dev /dev/i2c-1 -label i2c.1 -args instance=1
/bin/service up /service/i2c -dev /dev/i2c-2 -label i2c.2 -args instance=2
/bin/service up /service/i2c -dev /dev/i2c-3 -label i2c.3 -args instance=3

There is an i2cscan program from NetBSD which can detect devices on the bus:

i2cscan -r /dev/i2c-1
i2cscan -r /dev/i2c-2
i2cscan -r /dev/i2c-3

Limitations
-----------

The i2c controllers used in the am335x and the dm37xx do not support zero
byte transfers. Writing 0 bytes is a common method used to probe the bus
for devices. Most of the address ranges i2cscan scans are done by this
method. Therefore, only a subset of devices on the bus will be detected by
i2cscan (i.e. the devices it detects using the 1 byte read method). See
the register description for I2C_CNT in the technical reference manuals
for details about why 0 byte transfers are not allowed.

Developing I2C Device Drivers
-----------------------------

The driver for the EEPROM (a.k.a. drivers/cat24c256) is the hello world of
Minix i2c device drivers. It shows how to use the i2cdriver library and
how to use the bus for reads and writes. commands/eepromread is another
place to look if you're interested in accessing devices through the /dev
interface.

Developing I2C Drivers For The New Arm Platform
-------------------------------------------

1) Create the directory "your_platform" in the arch/earm
2) Your driver has to implement "int your_platform_interface_setup
(int (**process)(minix_i2c_ioctl_exec_t *ioctl_exec), int i2c_bus_id);", 
which is called by the general i2c driver for control platform dependent
functions. See previous realizations for more examples.
3) Your files have to named your_platform_i2c.c your_platform_i2c.h
your_platform_i2c_registers.h
4) Add your platform name in the Makefile directories list.
5) Include "your_platform_i2c.h" in the i2c.c.
6) Add else/if section in the i2c.c in the sef_cb_init function.
