# Modified IECBasicSerial Code

This is modified code, to allow the printer application to received the secondary address, when
opening a serial channel. This is based of the last version of the IECDevice code at the following
repository:

https://github.com/dhansel/IECDevice

## Virtual Functions

Two virtual functions where added to the IECDevice class:

* primmary_address - The paramenter contains the primary address for the data. The low nibble holds the device,
and the high nibble holds the source (0x20 for output data, and 0xE0 closed channel)

* secondary_address - The parameter contains the secondary address for the data. The low nibble holds the
secondary address, and the high nibble holds the source (0x60 for output data, 0xE0 closed channel)

## Escape characters

The code sends an esape character before the primary and secondary address, so that the printer program
know that it is an out of band character. Because of that, the escape character is escaped.

If an escape is detected, and the character following the escape, is also an escape character, it will
be treated as a single character in the printer output.

## LED Indicator

Additional code has been added to light an LED when data is flowing from the serial port. This uses the
built-in LED, when available. You could connect an LED to the LED pin, with a resistor, to have a
external LED. A different PIN can be used as well.
