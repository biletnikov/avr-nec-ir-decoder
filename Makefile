F_CPU = 16000000UL
CC = /usr/bin/avr-gcc
CFLAGS = -g -Os -Wall -mcall-prologues -mmcu=atmega88
CFLAGS += -DF_CPU=$(F_CPU)
OBJ2HEX = /usr/bin/avr-objcopy
UISP = /usr/bin/avrdude
TARGET = main


main.hex : main.obj
	$(OBJ2HEX) -R .eeprom -O ihex main.obj main.hex

main.obj : main.o ir.o
	$(CC) $(CFLAGS) main.o ir.o -o main.obj

ir.o : IR_Receiver.cpp IR_Receiver.h
	$(CC) $(CFLAGS) IR_Receiver.cpp -c -o ir.o

main.o : IR_Receiver_Test.cpp
	$(CC) $(CFLAGS) IR_Receiver_Test.cpp -c -o main.o

prog : $(TARGET).hex
	@echo 'progaem'
	$(UISP) -c usbasp -p m8 -U flash:w:$(TARGET).hex:a

clean :
	rm -f *.hex *.obj *.o

#%.obj : %.o
#	$(CC) $(CFLAGS) $< -o $@
#$@ — имя цели (%.obj)
#$< — имя зависимости (%.o)

#%.hex : %.obj
#	$(OBJ2HEX) -R .eeprom -O ihex $< $@
