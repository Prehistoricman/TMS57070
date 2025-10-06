## Sniffer

Sniffs the communication from host processor to TMS57070, and dumps it over UART. Send the UART command 's' to start sniffing, and then send any data to stop.

This is an Arduino project originally designed for the Arduino Due. The Due is much faster than the ATmega328P found in legacy Arduinos, allowing us to sniff the TMS57070 host interface in more appliances. To use a 328P in the same situation, it would be best to have the Arduino directly control the clock to the TMS57070 so that it can take as long as it needs to read the pins and print off bytes.

Uses this Arduino library to accelerate GPIO on the Due: https://github.com/mikaelpatel/Arduino-GPIO
