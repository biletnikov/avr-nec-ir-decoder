## Synopsis

It is a simple library to decode the commands from a remote control using NEC protocol for Atmel AVR microcontrollers.
The explanation of NEC protocol can be found here : http://www.sbprojects.com/knowledge/ir/nec.php
The library can work on Atmel AVR Atmega48/88/168 microcontrollers (without any additional adjustments).

The main idea of the library to work in the background, using an interrupt and timer to read data from an IR transmitter device.
You should check the receiving status periodically and when the new NEC packet is arrived, you can grab it for the further processing.

The NEC decoder is based on 16-bit timer and external interrupts (INT0 or INT1), however it can be rework for using PCI interrupts.
The base frequency is 16 Mhz (Crystal resonator).
The frequency can be changed and the timer setup should be adjusted automatically  (TIMER_COMPARE_VALUE) :

```
// Set here the frequency of the CPU
#define F_CPU 16000000UL
// Timer compare value to make the timer calculate milliseconds very well
#define TIMER_PRESCALER 64 // check the datasheet, we use TCCR1B to set it
#define TIMER_COMPARE_VALUE (uint16_t)( F_CPU / ((uint32_t) 1000 * TIMER_PRESCALER))
```


The library has support for a status led, which is optional. It blinks when a command received.

The library is fully emulated in Proteus. There is EasyDHL script, which models the IR transmitter.


## Motivation

I needed the IR decoder for one project, but did not find the appropriate library and I decided to create new one.

## Installation

1. Add header to your project : #include "IR_Receiver.h"
2. Setup ports as you needed : in IR_Receiver.h

## How to use
When the receiver is setup properly, it is ready for working. It is continiously waiting for the signals from IR sender and decodes it properly.

You have to call 'check_new_packet' routine and check the resulted flag.
If it is not 0, it means new packet is received and you read it passing the pointer to the packet structure : &received_packet

```
// where to store the received packet
struct IR_Packet received_packet;

while (1)
	{
		cli();
		// we have to call this function periodically, when CPU is not busy for other routines, 
		// the 'check_result' is a flag, which is not equal to 0 when new packet is arrived
		uint8_t check_result = check_new_packet(&received_packet);
		sei();
		if (check_result)
		{
			// now we are ready to read the packet : 
			
			// received_packet.addr       - address of the IR sender
			// received_packet.command    - command code
			// received_packet.repeat     - repeates

		}
	}
```

If you have unknown IR device then you do not know about its command codes.
It is recommended to use USB-to-TTL converter and IR_Receiver_Test.cpp to check and read all commands that your IR device can send to the receiver.

## Schematic
You can find the schema in the "Schematic" folder.
The pull-up resistor R1 is optional, but recommended.
If you avoid capacitors, it may cause lots of noise on the TSOP output pin - the receiver pulls down the pin.
I had a noise about 1 kHz frequency without capacitors.

## License
Apache License 2.0
