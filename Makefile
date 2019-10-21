serialport_write: serial_write.o poll_write_read.o serial_set.o
		  gcc -o serialport_write serial_write.o serial_set.o poll_write_read.o

serial_write.o: serial_write.c
		gcc -c serial_write.c

serial_set.o: serial_set.c
	      gcc -c serial_set.c

poll_write_read.o: poll_write_read.c
	gcc -c poll_write_read.c

clean: 
	rm *.o

