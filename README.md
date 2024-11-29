# Commodore Printer Emulator

This a printer emulator, that allows you to use a Commodore computer, which has an IEC port, to produce
printer output. This program, along with a piece of hardware, will add printer capability for your computer.

The retro world, has many different devices to make retro computers my useful again. One of the things, that
I found lacking, is printing device replacement. Most of the actual printers, are non-functional, or would
take a lot of effort to be useful. Printer ribbons are no longer available, which make the printer rather
useless.

This program is written in Python 3, using Tkinter for the user interface, and will run on Windows, MacOS, and Linux.
It currently only supports a MPS801 printer, but I have plans to expand it to include other printers that work on
the IEC port. That includes dot matrix, plotters, and thermal printer.

# Prerequisites

The following are required to run the program:

* Python 3 - I have tested with version 3.9 and higher. It may work with a lower version
* pyserial - This is required to talk to the serial port, to get data form the computer to the printer. Install it with the following command:
```
   pip3 install pyserial
```

* tkinter - This usually come with an installation, but you may have to install it

* Pillow library - This is used for manipulating images, for output to PDF. You can install this library will the following command:
```
   pip3 install pillow
```
* pynput - This library allows you to simulate key presses. It is used for the mouse scroll wheel, for the Linux platform. You can install it with the
following command:"
```
   pip3 install pynput
```
* Printer interface hardware - This is the hardware that you would connect to the Commodore computer. Information about the interface is
described below.

# Printer Interface Hardware

I am using the IECDevice library from the following repository:

https://github.com/dhansel/IECDevice

This hardware and firmware, allows you to connect micro-controller to the the IEC port on a Commodore computer. The code installed on
the micro-controller, is the IECBasicSerial example. This takes the data, sent out the IEC port, for device 4, and allows you to read
it on the serial port on the destination computer.

Refer to this repository, for more details about the hardware and the software

I have included a modified version of the code, to work correctly with the printer software

