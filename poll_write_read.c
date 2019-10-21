#include <stdio.h>
#include <unistd.h>  /* UNIX Standard Definitions 	   */ 
#include <errno.h>   /* ERROR Number Definitions           */
#include <stropts.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include "poll_read.h"

int poll_write_read(struct pollfd *fd, char *write_string, int wr_len, char *buptr, int *rd_len ,int time_out)
{
	int ret_val = 0;
	int poll_val = 0;
	int bytes_wr = 0;
	int in_bytes = 0;
	int total_read = 0;
	int nbytes = 0;
	char buffer[255];  /* Input buffer */
	char *bufptr;      /* Current char in buffer */
	int loop = 0;

	printf("Poll settings: %d ; timeout: %d\r\n", fd[0].events, time_out);
	printf("File descriptor %d ", fd[0].fd);
//	if ((bytes_wr = write(fd[0].fd, write_string, wr_len)) < 0)
//	{
//		ret_val = -1; 
//		printf("Error in writing\r\n");
//	}
//	else
//	{
		poll_val = poll(fd, 1, time_out);
		if (poll_val > 0)
		{
			printf("Poll value: %d\r\n", poll_val);
			if (fd[0].revents & POLLIN)
			{
				ioctl(fd[0].fd, FIONREAD, &in_bytes);
				printf("Number of bytes read: %d \r\n", in_bytes);
				bufptr = buffer;
				while (total_read < in_bytes)
				{
					nbytes = read(fd[0].fd, &bufptr,in_bytes);
					total_read += nbytes;
					bufptr += nbytes;
				}
				bufptr = '\0'; //null terminate string					
				//*rd_len = in_bytes;
			}
			else
			{
				printf("Poll Errors\r\n");
			}
		}
		else
		{
			printf("Poll error value: %d\r\n", poll_val);
		}
//	}
	printf("Read bytes %d \r\n", total_read);
	printf("Buffer %s\r\n", buffer);
	for (loop = 0; loop < (total_read + 1); loop ++)
	{
		printf("%2X", buffer[loop]);
	}
	printf("\r\n");
	strncpy(buptr, buffer, total_read);
	return ret_val;
}
