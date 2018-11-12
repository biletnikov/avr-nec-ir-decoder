/*
 * IR Receiver for the NEC protocol.
 * http://www.sbprojects.com/knowledge/ir/nec.php
 *
 * Created: 18.04.2017 1:29:12
 * Author : Sergei Biletnikov
 */ 

// Set here the frequency of the CPU
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

// Timer compare value to make the timer calculate milliseconds very well
#define TIMER_PRESCALER 64 // check the datasheet, we use TCCR1B to set it
// The timer compare value which corresponds to the 1 ms (approx)
#define TIMER_COMPARE_VALUE_ONE_MS (uint16_t)( F_CPU / ((uint32_t) 1000 * TIMER_PRESCALER)) 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "IR_Receiver.h"

// We have to set the maximum time for the data transmission, after this time, the packet must be rejected
// it helps to avoid program stopping when data transmission was corrupted or interrupted
#define MAX_DELAY_FOR_REPEAT_COMMAND 100 // in ms
#define MAX_BIT_TRANSMISSION_DELAY 16 // in ms

// packet reading state
#define PACKET_STATE_NO_PACKET 0
#define PACKET_STATE_READING 1
#define PACKET_STATE_READY 2
#define PACKET_STATE_READ 3

static uint8_t packet_reading_state = PACKET_STATE_NO_PACKET;

// read bits counter
// 0 - the packet receiving was not started
// after the packet received, this counter is set to 0
static uint8_t read_bit_counter = 0;

// receiving packet of data
static struct IR_Packet packet;
// pulse and delay length
static uint16_t pulse_time_counter = 0;
static uint16_t pause_time_counter = 0;

// reset packet data
void reset_packet()
{
	packet.addr = 0;
	packet.addr_inv = 0;
	packet.command = 0;
	packet.command_inv = 0;
	packet.repeat = 0;
	
	read_bit_counter = 0;	
	packet_reading_state = PACKET_STATE_NO_PACKET;
}

// Start timer in ms, if you set start_IR_timer(100);  the timer will set for 100 ms and is ended with the interruption
void start_IR_timer(uint8_t time_ms)
{
	TCCR1A=0x0;
	TCNT1=0x0;	
	OCR1A= TIMER_COMPARE_VALUE_ONE_MS * time_ms; 
	TCCR1B|=(1<<WGM12); // CTC mode -> TCNT1 = OCR1A    
	TCCR1B|=(1<<CS10)|(1<<CS11); // prescaler 64
	TIMSK1|=(1<<OCIE1A); // allow interrupts
}

void stop_IR_timer()
{
	TCCR1B&=~(1<<CS10);
	TCCR1B&=~(1<<CS11);		
	TCCR1B&=~(1<<CS12);
}

void reset_IR_receiver()
{	
	stop_IR_timer();
			
	reset_packet();
	
	pulse_time_counter = 0;
	pause_time_counter = 0;
	
	#ifdef IR_STATUS_LED
		IR_STATUS_LED_OFF;
	#endif
}

void on_start_bit()
{
	// new packet will be received
	reset_packet();	
	packet_reading_state = PACKET_STATE_READING;
}

// the packet is received successfully and ready for further processing
void on_new_packet_received()
{
	packet_reading_state = PACKET_STATE_READY;
	start_IR_timer(MAX_DELAY_FOR_REPEAT_COMMAND); // start timer and wait repeat command	
	
	#ifdef IR_STATUS_LED
	IR_STATUS_LED_ON;
	#endif
}

void on_data_bit(uint8_t bit)
{	
	if (read_bit_counter < NEC_MAX_PACKET_BIT_NUMBER && packet_reading_state == PACKET_STATE_READING)
	{
		if (bit)
		{			
			if (read_bit_counter < 8) 
			{
				// address reading
				packet.addr |= (1<<read_bit_counter);
			} else if (read_bit_counter >=8 && read_bit_counter < 16) 
			{
				// inverting address reading
				packet.addr_inv |= (1<<(read_bit_counter - 8));
			} else if (read_bit_counter >= 16 && read_bit_counter < 24) 
			{
				// command reading
				packet.command |= (1<<(read_bit_counter - 16));
			} else if (read_bit_counter >= 24) 
			{
				// inverting command reading
				packet.command_inv |= (1<<(read_bit_counter - 24));
			}
		}				
		
		read_bit_counter++;
		
		if (read_bit_counter == NEC_MAX_PACKET_BIT_NUMBER) 
		{					
			// the packet is read, validate
			if (((packet.addr + packet.addr_inv) == 0xFF) && ((packet.command + packet.command_inv) == 0xFF))
			{
				// new valid packet is received
				on_new_packet_received();
			}
		}
	}		
}

// process repeat command, it happens when a button is pressed for a longer period of time
void on_repeat_command()
{
	if (packet_reading_state == PACKET_STATE_READY || packet_reading_state == PACKET_STATE_READ)
	{
		if (packet.repeat < 255)
		{			
			packet.repeat++; // repeat counter up, repeat command maybe send it sequence
		}
		packet_reading_state = PACKET_STATE_READY;
		
		start_IR_timer(MAX_DELAY_FOR_REPEAT_COMMAND); // wait next repeat command
	} 
	else 
	{
		//  problem, invalid protocol, reset receiver
		reset_IR_receiver();
		reset_packet();
	}
}

void read_chunk()
{
	if (pulse_time_counter > 0 && pause_time_counter > 0)
	{
		// pulse 7 ms (1750) - 11 ms (2750) 		
		if (pulse_time_counter > (7 * TIMER_COMPARE_VALUE_ONE_MS) && pulse_time_counter < (11 * TIMER_COMPARE_VALUE_ONE_MS)) 
		{			
			// pause 3.2 ms (800) - 6 ms (1500) 
			if (pause_time_counter > (3.2 * TIMER_COMPARE_VALUE_ONE_MS) && pause_time_counter < (6 * TIMER_COMPARE_VALUE_ONE_MS))
			{
				// start bit
				on_start_bit();
			} 
			// pause 1.6 ms (400) - 3.2 ms (800)
			else if (pause_time_counter > (1.6 * TIMER_COMPARE_VALUE_ONE_MS) && pause_time_counter <= (3.2 * TIMER_COMPARE_VALUE_ONE_MS))
			{
				// command repeat
				on_repeat_command();
			}						
		}
		// pulse 360 microseconds (90) - 760 microseconds (190)		
		else if (pulse_time_counter > 0.36 * TIMER_COMPARE_VALUE_ONE_MS && pulse_time_counter < (0.76 * TIMER_COMPARE_VALUE_ONE_MS))
		{
			// pause 1.5 ms (375) - 1.9 ms (475)
			if (pause_time_counter > (1.5 * TIMER_COMPARE_VALUE_ONE_MS) && pause_time_counter < (1.9 * TIMER_COMPARE_VALUE_ONE_MS)) 
			{
				// data bit = 1							
				on_data_bit(1);
			} 
			// pause 360 microseconds (90) - 760 microseconds (190)
			else if (pause_time_counter > (0.36 * TIMER_COMPARE_VALUE_ONE_MS) && pause_time_counter < (0.76 * TIMER_COMPARE_VALUE_ONE_MS)) 
			{
				// data bit = 0
				on_data_bit(0);
			}
		}		
	}
}

// check if new data packet received
// 0 - no, otherwise yes
// received_packet is a pointer to the IR_Packet structure to receive the data
// the packet updated only if the function result is not 0
extern uint8_t check_new_packet(struct IR_Packet * received_packet)
{
	if (packet_reading_state == PACKET_STATE_READY)
	{
		packet_reading_state = PACKET_STATE_READ;
		
		received_packet->addr = packet.addr;
		received_packet->addr_inv = packet.addr_inv;
		received_packet->command = packet.command;
		received_packet->command_inv = packet.command_inv;
		received_packet->repeat = packet.repeat;
		
		return 1;
	}
	return 0;
}

// external interrupt
ISR(INT0_vect)
{
	uint8_t rising_edge = (IRR_PIN_PORT & (1<<IRR_PIN));
	if (rising_edge) 
	{
		// rising edge interrupt, read the counter value -> duration of pulse
		pulse_time_counter = TCNT1;
		// reset counter
		TCNT1 = 0;
	} 
	else 
	{
		// falling edge interrupt
		if (pulse_time_counter == 0)
		{
			// new data chunk receiving, start timer to handle problem packets
			start_IR_timer(MAX_BIT_TRANSMISSION_DELAY);			
		} 
		else 
		{
			// keep pause duration
			pause_time_counter = TCNT1;
			// reset counter
			TCNT1 = 0;
			// read the piece of data received
			read_chunk();
			//
			pulse_time_counter = 0;
			pause_time_counter = 0;
		}
	}
}

ISR(TIMER1_COMPA_vect)
{
	// reset IR receiver on the timer interrupt
	reset_IR_receiver(); 
}

// init receiver
extern void init_receiver()
{
	cli();
			
	#ifdef IR_STATUS_LED
	IR_STATUS_LED_DDR|=(1<<IR_STATUS_LED_PIN);
	#endif
	
	// IR port init
	init_IRR_PIN();
	//
	
	reset_IR_receiver();
	reset_packet();
	
	sei();
}


