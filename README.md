## Synopsis

It is a simple library to decode the commands from a remote control using NEC protocol for Atmel AVR microcontrollers.
The explanation of NEC protocol can be found here : http://www.sbprojects.com/knowledge/ir/nec.php
The library can work on Atmel AVR Atmega48/88/168 microcontrollers (without any additional adjustments).

The main idea of the library to work in the background, using an interrupt and timer to read data from an IR transmitter device.
You should check the receiving status periodically and when the new NEC packet is arrived, you can grab it for the further processing.

The NEC decoder is based on 16-bit timer and external interrupts (INT0 or INT1), however it can be rework for using PCI interrupts.
The library was tested on Atmega88p and must work on Atmega168 and Atmega328

Timeouts are calcualted dufring the complilation and based on the CPU frequency.
F_CPU must be set.  The example has 16 Mhz. The library was tested with 8 Mhz, but it shuld work normally on any frequency 1-20 Mhz, 
if you set F_CPU correctly, otherwise the timings will be wrong and packets will be lost.


```
// Set here the frequency of the CPU
#define F_CPU 16000000UL
// Timer compare value to make the timer calculate milliseconds very well
#define TIMER_PRESCALER 64 // check the datasheet, we use TCCR1B to set it
#define TIMER_COMPARE_VALUE (uint16_t)( F_CPU / ((uint32_t) 1000 * TIMER_PRESCALER))
```


The library has support for a status led, which is optional. It blinks when a command received.

The library is fully emulated in Proteus. There is EasyDHL script, which models the IR transmitter.

## News

I have updated the library on 28 Feb 2019.
The new version has support for Extended NEC protocol. It means more remote control devices can work with the library.
Thanks to Chris Trottier who sent me a RC with such protocol for testing.

The new version has improved error handling with using timeouts in any phase of the data transimission. It means, in case of 
failed transimssion the receiver library will reset itself as soon as possible with timeouts.

## Motivation

I needed the IR decoder for one project, but did not find the appropriate library and I decided to create new one.

## Installation

1. Add header to your project : #include "IR_Receiver.h"
2. Setup ports as you needed : in "IR_Receiver.h"
3. You can enable a led which is blinks when new packet is comming.

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

If you have unknown IR device then you need to know its commands. For that, you can run a test application : IR_Receiver_Test.cpp,
whic will connect MCU via UART to the PC (use USB-to-TTL converter can be used).
Run serial monitor (like in Arduino) or any other tool to read the messages from the MCU.
Press buttons on your Remote control and see the actual commands on the PC.
When you know all commands and the device address, you can programm them.
I would suggest to create "IR_XXX_Control.h" file and put all commands there.

#### How IR_Receiver_Test.cpp  output looks like:

```
A: 6186  C: 13  R: 0

A: 6186  C: f  R: 0

A: 6186  C: 10  R: 0 R: 1 R: 2 R: 3 R: 4 R: 5

A: 6186  C: 13  R: 0 R: 1

A: 6186  C: 12  R: 0

A: 6186  C: f  R: 0

A: 6186  C: 15  R: 0

A: 6186  C: 19  R: 0

A: 6186  C: 10  R: 0

A: 6186  C: 12  R: 0
```

A: 6186 - the device address
C: 13 - is a command code
R: 0   no repeats
A: 6186  C: 10  R: 0 R: 1 R: 2 R: 3 R: 4 R: 5     (Command 10  from device 6186  and 5 repeats)

### Repeated commands
According to the NEC standard, commands maybe repeated, usually it happens when button on the IR transiver is pressed for some time.
It is handy when you controll the light dimmer slowly changing PWM duty.
Actually the command is sent once and after that special repeat signal will be sent periodically.

To process this case, the "repeat" counter per each command is used.
if (received_packet.repeat == 0)   the command is sent without repeat
if (received_packet.repeat > 0)    it is repeat of the command

We will receive the same command but with the incremented received_packet.repeat field.

If you need to avoid repititions, just filter out commands with received_packet.repeat > 0.

Example:
```
if (check_result)
{
   // new packet received
   if (received_packet.command == RELAY_TURN_ON_COMMAND)
   {
       // we do not need repetitions for this command
       if (received_packet.repeat == 0)
       {
          //ok, it is not repetition
       } 
   
   } else if (received_packet.command == LIGHT_BRIGHTER)
    {
        // but here we need repetitions
		if (received_packet.repeat > 0)
		{
			// do something 
		}
    }
}
```

## Schematic
You can find the schema in the "Schematic" folder.
The pull-up resistor R1 is optional, but recommended.
If you avoid capacitors, it may cause lots of noise on the TSOP output pin - the receiver pulls down the pin.
I had a noise about 1 kHz frequency without capacitors.
A noise adds overhead to your device, because MCU will process the noise signals in attempt to find any packet from it, 
instead of working on something more important.

## Plans for the future
Support both timers : 8 bit and 16 bit, and use either one by demand. It is essential, because some devices may really needs 16-bit timer 
for their main tasks.

ATTiny support :  maybe attiny2313, attiny85 and even attiny13.

## License
Apache License 2.0
