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
#define MAX_DELAY_FOR_NEXT_REPEAT_COMMAND 120 // in ms
#define MAX_INITIAL_PULSE_TRANSMISSION_DELAY 16 // in ms (actually 9 + 4.5 or 9 + 2.25)
#define MAX_BIT_TRANSMISSION_DELAY 16 // in ms (actually 2.25, but we take 4)

// receiver reading state
#define RECEIVER_STATE_START_REPEAT 0  // when receiver is waiting for start or repeat command
#define RECEIVER_STATE_READING 1       // reading packet state
#define RECEIVER_STATE_READY 2         // packet ready

static uint8_t receiver_state = RECEIVER_STATE_START_REPEAT; // initial state

// read bits counter
// 0 - the packet receiving was not started
// after the packet received, this counter is set to 0
static uint8_t read_bit_counter = 0;

// receiving data
static uint8_t rec_addr; // address
static uint8_t rec_addr_inv; // inverted address
static uint8_t rec_command; // command
static uint8_t rec_command_inv; // inverted command
static uint8_t rec_repeat; // 0 if the packet is not repeat
static uint16_t rec_ext_addr; // device extended address, 0 if it is not used
// control repeat command state cycle
#define REPEAT_COMMAND_MAX_QTY 255 // how many repeat commands can be sent in sequence, for the one packet
#define REPEAT_COMMAND_ALLOWED 1
#define REPEAT_COMMAND_NOT_ALLOWED 0
static uint8_t receiver_allow_repeat = REPEAT_COMMAND_NOT_ALLOWED;

// pulse and delay length
volatile static uint16_t pulse_time_counter = 0;
volatile static uint16_t pause_time_counter = 0;

// reset current packet data
void reset_packet()
{
	rec_addr = 0;
	rec_addr_inv = 0;
	rec_ext_addr = 0;
	rec_command = 0;
	rec_command_inv = 0;
	rec_repeat = 0;
		
	read_bit_counter = 0;		
}

// Start timer in ms, if you set start_ir_timer(100);  the timer will set for 100 ms and is ended with the interruption
void start_ir_timer(uint8_t time_ms)
{
	TCCR1A=0x0;
	TCNT1=0x0;	
	OCR1A= TIMER_COMPARE_VALUE_ONE_MS * time_ms; 
	TCCR1B|=(1<<WGM12); // CTC mode -> TCNT1 = OCR1A    
	TCCR1B|=(1<<CS10)|(1<<CS11); // prescaler 64
	TIMSK1|=(1<<OCIE1A); // allow interrupts
}

void stop_ir_timer()
{
	TCCR1B&=~(1<<CS10);
	TCCR1B&=~(1<<CS11);		
	TCCR1B&=~(1<<CS12);
}
// reset receiver to the initial state
void reset_ir_receiver()
{	
	stop_ir_timer();			
	reset_packet();
	
	receiver_state = RECEIVER_STATE_START_REPEAT;
	receiver_allow_repeat = REPEAT_COMMAND_NOT_ALLOWED;
	
	pulse_time_counter = 0;
	pause_time_counter = 0;
	
	#ifdef IR_STATUS_LED
		IR_STATUS_LED_OFF;
	#endif
}

void on_start_bit()
{
	// new packet is going to be received :)
	reset_packet();	
	receiver_state = RECEIVER_STATE_READING;
}

// the packet is received successfully and ready for further processing
void on_new_packet_received()
{	
	receiver_state = RECEIVER_STATE_READY;
	read_bit_counter = 0;
	receiver_allow_repeat = REPEAT_COMMAND_ALLOWED;
	
	start_ir_timer(MAX_DELAY_FOR_NEXT_REPEAT_COMMAND);
	
	#ifdef IR_STATUS_LED
	IR_STATUS_LED_ON;
	#endif
}
// proccess next data bit
void on_data_bit(uint8_t bit)
{	
	if ((read_bit_counter < NEC_MAX_PACKET_BIT_NUMBER) && (receiver_state == RECEIVER_STATE_READING))
	{
		if (bit)
		{			
			if (read_bit_counter < 8) 
			{
				// address reading
				rec_addr |= (1<<read_bit_counter);
			} else if (read_bit_counter >=8 && read_bit_counter < 16) 
			{
				// inverting address reading
				rec_addr_inv |= (1<<(read_bit_counter - 8));
			} else if (read_bit_counter >= 16 && read_bit_counter < 24) 
			{
				// command reading
				rec_command |= (1<<(read_bit_counter - 16));
			} else if (read_bit_counter >= 24) 
			{
				// inverting command reading
				rec_command_inv |= (1<<(read_bit_counter - 24));
			}
		}				
		
		read_bit_counter++;
		
		if (read_bit_counter == NEC_MAX_PACKET_BIT_NUMBER) 
		{				
			// all bits are read, now check the packet on errors
			// // command consist of 2 bytes, the second one is inverted value of first byte
			if ((rec_command + rec_command_inv) == 0xFF)
			{
				// ok , command is fine, now check address
				int8_t invereted_address_pair = ((rec_addr + rec_addr_inv) == 0xFF);							
				// extended nec protocal is allowed, so address will may 2 bytes as the address, but not allowed addressed from not extended protocol : non-inverted + inverted
				if (!invereted_address_pair)
				{
					// the protocol is NEC extended
					// the device address is 16 bit
					rec_ext_addr=(uint16_t) rec_addr_inv<<8; //high byte
					rec_ext_addr|=rec_addr; // low byte
					rec_addr = 0;
					rec_addr_inv = 0;
				} 
																
				// new valid packet is received
				on_new_packet_received();
			} 
			else
			{
				// 2 bytes of command are incorrect, because they are not inverted to each other
				reset_ir_receiver(); // reset
			}
		} 
	}		
}

// process repeat command, it happens when a button is pressed for a longer period of time, this command is iterative
void on_repeat_command()
{
	if (receiver_allow_repeat == REPEAT_COMMAND_ALLOWED && (receiver_state == RECEIVER_STATE_READY || receiver_state == RECEIVER_STATE_START_REPEAT))
	{
		if (rec_repeat < REPEAT_COMMAND_MAX_QTY)
		{			
			rec_repeat++; // repeat counter up, repeat command maybe send it sequence
		}
		receiver_state = RECEIVER_STATE_READY;
		
		start_ir_timer(MAX_DELAY_FOR_NEXT_REPEAT_COMMAND); // wait next repeat command
	} 
	else 
	{
		//  problem, invalid protocol, reset receiver
		reset_ir_receiver();
	}
}

// recognize received chunk
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
				// on start bit
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
		else
		{
			// something wrong with packet : reset receiver
			reset_ir_receiver();
		}
	}
	
	// zero time counters for the next signals
	pulse_time_counter = 0;
	pause_time_counter = 0;
}

// check if new data packet received
// 0 - no, otherwise yes
// received_packet is a pointer to the IR_Packet structure to receive the data
// the packet updated only if the function result is not 0
extern uint8_t check_new_packet(struct IR_Packet * received_packet)
{
	if (receiver_state == RECEIVER_STATE_READY)
	{	
		if (rec_ext_addr > 0) 
		{
			// extended NEC protocol, get 2 bytes address
			received_packet->addr = rec_ext_addr;
		}
		else
		{
			// standard NEC protocol
			received_packet->addr = rec_addr;
		}
		received_packet->command = rec_command;
		received_packet->repeat = rec_repeat;
		
		// the packet is read successfully
		receiver_state = RECEIVER_STATE_START_REPEAT;
		
		return 1; // return positive status, because we have event (data)
	}
	return 0;
}

// external interrupt, catches rising and falling edges pulses to read the data according to the NEC protocol
ISR(INT0_vect)
{
	// skip next packets if the current packet not read by client application yet
	if (receiver_state == RECEIVER_STATE_READY)
	{
		return;
	}
	// read current signal edge
	uint8_t rising_edge = (IRR_PIN_PORT & (1<<IRR_PIN));
	
	if (!rising_edge) 
	{				
		// STEP: 1	
		// ON falling edge interrupt
		if (pulse_time_counter == 0)
		{	
			// new data signal start receiving			
			if (receiver_state == RECEIVER_STATE_READING)
			{
				// we are receving data packet bit by bit, set max delay for that
				start_ir_timer(MAX_BIT_TRANSMISSION_DELAY);	
			} 
			else
			{
				// we are receiving new packet or repeat command
				start_ir_timer(MAX_INITIAL_PULSE_TRANSMISSION_DELAY);
			}
		} 
		else 
		{		
			// STEP: 3
			// save duration of the pause
			pause_time_counter = TCNT1 - pulse_time_counter;
			// start 			
			start_ir_timer(MAX_BIT_TRANSMISSION_DELAY);	
			// read the piece of data received
			read_chunk();
		}
	} 
	else 
	{			
		// STEP : 2			
		// ON rising edge interrupt, read the counter value -> duration of pulse
		pulse_time_counter = TCNT1;
		if (pulse_time_counter == 0) {
			// something goes wrong
			reset_ir_receiver();
		}
	}
}

// When time is out
ISR(TIMER1_COMPA_vect)
{
	stop_ir_timer(); // we set timer only for one time
	
	if (receiver_state == RECEIVER_STATE_READY)
	{
		// timeout for the next repeat command, do not allow any new repeat command more
		receiver_allow_repeat = REPEAT_COMMAND_NOT_ALLOWED;
	}
	else
	{
		// something wrong with signals
		// reset IR receiver on the timer interrupt
		reset_ir_receiver();
	}	
}

// init receiver
extern void init_receiver()
{
	char cSREG;
	cSREG = SREG; /* store SREG value */
	/* disable interrupts during timed sequence */
	cli();
			
	#ifdef IR_STATUS_LED
	IR_STATUS_LED_DDR|=(1<<IR_STATUS_LED_PIN);
	#endif
	
	// IR port init
	init_IRR_PIN();
	//	
	reset_ir_receiver();
	
	SREG = cSREG; /* restore SREG value (I-bit) */
}


