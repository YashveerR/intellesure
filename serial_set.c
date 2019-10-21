#include <stdio.h>
#include <fcntl.h>   /* File Control Definitions           */
#include <termios.h> /* POSIX Terminal Control Definitions */
#include <unistd.h>  /* UNIX Standard Definitions 	   */ 
#include "serial_set.h"

int serial_settings(int *fd)
{
	int ret_val = 0;

/*---------- Setting the Attributes of the serial port using termios structure --------- */
	
	struct termios sp_set;	/* Create the structure                          */

	tcgetattr(*fd, &sp_set);	/* Get the current attributes of the Serial port */
	sp_set.c_cflag |= CLOCAL | CREAD;
    sp_set.c_cflag &= ~CSIZE;
    sp_set.c_cflag |= CS8;         /* 8-bit characters */
    sp_set.c_cflag &= ~PARENB;     /* no parity bit */
    sp_set.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    sp_set.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    sp_set.c_lflag |= ICANON | ISIG;  /* canonical input */
    sp_set.c_lflag &= ~(ECHO | ECHOE | ECHONL | IEXTEN);
   	sp_set.c_iflag &= ~INPCK;
   	sp_set.c_iflag |= IGNCR;
   	sp_set.c_iflag &= ~(INLCR | ICRNL | IUCLC | IMAXBEL);
   	sp_set.c_iflag &= ~(IXON | IXOFF | IXANY);   /* no SW flowcontrol */

   	sp_set.c_oflag &= ~OPOST;

   	cfsetospeed(&sp_set, B115200);       //setting communication speed and other attributes
   	cfsetispeed(&sp_set, B115200);
   	tcflush(*fd, TCIOFLUSH);
	                
	if((tcsetattr(*fd,TCSANOW,&sp_set)) != 0) /* Set the attributes to the termios structure*/
		ret_val = 1;
	else
		ret_val = 0;
		tcflush(*fd, TCIFLUSH);   /* Discards old data in the rx buffer            */

	printf("Termios retval : %d\r\n", ret_val);
	return ret_val;
}
