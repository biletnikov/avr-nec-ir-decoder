## Synopsis

It is a simple library to decode the commands from a remote control using NEC protocol for Atmel AVR microcontrollers.
The explanation of NEC protocol can be found here : http://www.sbprojects.com/knowledge/ir/nec.php
The library can work on Atmel AVR Atmega48/88/168 microcontrollers (without any additional adjustements).

The NEC decoder is based on 16-bit timer and external interrupts (INT0 or INT1), however it can be rework for using PCI interrupts.
The base frequency is 16 Mhz (Crystal resonator). However, it can be decreased, but the time durations must be recalculated.

The library has support of status led, which is optional. It works like an indicator when a command received.

The library is fully emulated in Proteus. There is EasyDHL script, which models the IR transmitter.


## Motivation

I needed the IR decoder for one project, but did not find the appropriate library and I decided to create new one.

## Installation

1. Add header to your project : #include "IR_Receiver.h"
2. Setup ports as you needed : in IR_Receiver.h

## How to use
```
while (1)
	{
		cli();
		uint8_t check_result = check_new_packet(&received_packet);
		sei();
		if (check_result)
		{
			// process new command here
			
		}
	}
```

## Schematic
You can find the schema in the "Schematic" folder.
The pull-up resistor R1 is optional, but recommended.
If you avoid capacitors, it may cause lots of noise on the TSOP output pin - the receiver pulls down the pin.
I had a noise about 1 kHz frequency without capacitors.

## License
Apache License 2.0