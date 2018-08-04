## Synopsis

It is a simple library to decode the commands from a remote control using NEC protocol for Atmel AVR microcontrollers.
The explanation of NEC protocol can be found here : http://www.sbprojects.com/knowledge/ir/nec.php
The library can work on Atmel AVR Atmega48/88/168 microcontrollers (without any additional adjustments).

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

## How to setup timer

### Timer setup for 16 Mhz
```
// Start timer in ms
void start_IR_timer(uint8_t time_ms)
{
    TCCR1A=0x0;
    TCNT1=0x0;
    // max resolution is 4 microseconds
    OCR1A=((uint32_t) time_ms * 1000)/4; // it makes delay = 16 ms when Fcpu = 16 Mhz, it makes the delay enough for reading each bit according to NEC
    TCCR1B|=(1<<WGM12); // CTC mode -> TCNT1 = OCR1A
    TCCR1B|=(1<<CS10)|(1<<CS11); // prescaler 64
    TIMSK1|=(1<<OCIE1A); // allow interrupts
}
```

### Timer setup for 8 Mhz
```
// Start timer in ms
void start_IR_timer(uint8_t time_ms)
{
    TCCR1A=0x0;
    TCNT1=0x0;
    // max resolution is 8 microseconds
    OCR1A=((uint32_t) time_ms * 1000)/8;
    TCCR1B|=(1<<WGM12); // CTC mode -> TCNT1 = OCR1A
    TCCR1B|=(1<<CS10)|(1<<CS11); // prescaler 64
    TIMSK1|=(1<<OCIE1A); // allow interrupts
}
```

### Timer setup for 4 Mhz
```
// Start timer in ms
void start_IR_timer(uint8_t time_ms)
{
    TCCR1A=0x0;
    TCNT1=0x0;
    // max resolution is 16 microseconds
    OCR1A=((uint32_t) time_ms * 1000)/16; 
    TCCR1B|=(1<<WGM12); // CTC mode -> TCNT1 = OCR1A
    TCCR1B|=(1<<CS10)|(1<<CS11); // prescaler 64
    TIMSK1|=(1<<OCIE1A); // allow interrupts
}
```

## Schematic
You can find the schema in the "Schematic" folder.
The pull-up resistor R1 is optional, but recommended.
If you avoid capacitors, it may cause lots of noise on the TSOP output pin - the receiver pulls down the pin.
I had a noise about 1 kHz frequency without capacitors.

## License
Apache License 2.0
