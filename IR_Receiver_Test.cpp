/*
 * Tests IR Receiver library.
 *
 * Created: 28.04.2017 22:29:35
 *  Author: Sergei Biletnikov
 */ 

#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>

#include "IR_Receiver.h"

// define some macros
#define BAUD 9600                                   // define baud
#define UBRR ((F_CPU)/(BAUD*16UL)-1)            // set baud rate value for UBRR

// function to initialize UART
void uart_init (void)
{
	//UBRR0H = (UBRR>>8);                      // shift the register right by 8 bits
	//UBRR0L = UBRR;                           // set baud rate
	UBRR0 = UBRR;
	UCSR0B|= (1<<TXEN0)|(1<<RXEN0);                // enable receiver and transmitter
	UCSR0C|= (1<<UCSZ00)|(1<<UCSZ01);   // 8bit data format
}

void usart_transmit(uint8_t data)
{
	/* Wait for empty transmit buffer */
	while ( !( UCSR0A & (1<<UDRE0)) );
	/* Put data into buffer, sends the data */
	UDR0 = data;
}

void usart_transmit_str(const char* str)
{
	int len = strlen(str);
	int i;
	for (i = 0; i < len; i++) {
		usart_transmit(str[i]);
	}
}

struct IR_Packet received_packet;

int main(void)
{
	
	// init uart
	uart_init();
	
	init_receiver();
	
	sei();
	
	while (1)
	{
		cli();
		uint8_t check_result = check_new_packet(&received_packet);
		sei();
		
		usart_transmit_str("Ready :");
		usart_transmit_str("\n\r");
		
		if (check_result)
		{
			char buff[10];
			utoa(received_packet.addr, buff, 16);
			usart_transmit_str("A : ");
			usart_transmit_str(buff);
			usart_transmit_str(" ");
			utoa(received_packet.command, buff, 16);
			usart_transmit_str("C : ");
			usart_transmit_str(buff);
			usart_transmit_str(" ");
			utoa(received_packet.repeat, buff, 10);
			usart_transmit_str("R : ");
			usart_transmit_str(buff);
			usart_transmit_str("\n\r");
		}
	}
}