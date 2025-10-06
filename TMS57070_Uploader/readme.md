## Arduino project
Basic interface for sending and receiving data to/from the TMS57070.

For ATmega328P based Arduinos (uses the SPI hardware).

Uses this Arduino library to accelerate the GPIO: https://github.com/pololu/fastgpio-arduino

## Qt project
PC app for controlling the TMS57070. Written for Qt 5.15 which is quite old so will be surely broken in the latest Qt.

The PMEM and CMEM tabs accept a comma-delimited list of values. Line-comments starting with // are allowed.

The CMEM live view tab can be configured to continuously read a certain region of CMEM, adjustable at the bottom of the screen.
