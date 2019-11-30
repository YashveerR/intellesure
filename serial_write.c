#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>   /* File Control Definitions           */
#include <termios.h> /* POSIX Terminal Control Definitions */
#include <unistd.h>  /* UNIX Standard Definitions 	   */ 
#include <errno.h>   /* ERROR Number Definitions           */
#include <stropts.h>
#include <poll.h>
#include <sys/inotify.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>
#include "serial_set.h"
#include "poll_read.h"
#include <syslog.h>

#define MASTER_PHONE "/home/pi/intellesure/master_phone"	
#define MASTER_CODE "/home/pi/intellesure/pass_code"
#define SMS_FD "/home/pi/intellesure/sms_tx_fd"
#define TEST
#define WORK

void msg_actions(void);
void poll_read(int timeout_msecs);
char* check_ok(char*, char*);
void read_msg();
int extract_msg_no();
void send_msg(void);

struct pollfd fds[2]; //poll filedescriptor
char buffer[255];  /* Input buffer */

int bytes;

int	phone_fd = 0;
char master_no[20];
int code_fd = 0;
char master_code[6];

char message[100];
int char_size = sizeof(char);
int msg_num = 0;
char number[20];

void main(void)
{
	
	int bytes_written = 0;
	int poll_ret = 0;
	int val_print = 0;

	int sms_poll_fd = 0;
	int wd;
	char buf[4096];

	openlog ("gsm_driver", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

   	fds[0].fd = open("/dev/ttyUSB0",O_RDWR);	/* ttyUSB0 is the FT232 based USB2SERIAL Converter   */
							
	fds[0].events = POLLIN;						                                        
								
  	if(fds[0].fd == -1)						/* Error Checking */
        	   printf("\n  Error! in Opening ttyUSB0  ");
   	else
        	   printf("\n  ttyUSB0 Opened Successfully %d\r\n", fds[0].fd);

	serial_settings(&fds[0].fd);
	printf("post setting the serial port");
	phone_fd = open(MASTER_PHONE, O_RDWR);
	code_fd = open(MASTER_CODE, O_RDWR);
	printf("phone fd %d, code fd %d\r\n", phone_fd, code_fd);

	/* For normal files, one has to "watch" them for changes, creating those event watchers*/

	/*creating the INOTIFY instance*/
	sms_poll_fd = inotify_init();

	/*checking for error*/
	if ( sms_poll_fd < 0 ) {
		perror( "inotify_init" );
	}
	/*adding the “/tmp” directory into watch list. Here, the suggestion is to validate the 		                existence of the directory before adding into monitoring list.*/
	wd = inotify_add_watch( sms_poll_fd, "/home/pi/intellesure/sms_tx_fd", IN_MODIFY );
	fds[1].fd = sms_poll_fd;
	fds[1].events = POLLIN;


#ifdef TEST	
	
	/*The following are standard setup messages, which do not
	  require additional checks, apart from the OK check */
	bytes_written = write(fds[0].fd,"ATE0\r",5);
	poll_read(500);
	check_ok(buffer, "OK");
	memset(buffer,0,sizeof(buffer));
	bytes_written = write(fds[0].fd,"AT+CMGF=1\r",10);
	poll_read(500);
	check_ok(buffer, "OK");
	memset(buffer,0,sizeof(buffer));
	bytes_written = write(fds[0].fd,"AT+CNMI=2,1,0,0,0\r",18);
	poll_read(500);
	check_ok(buffer, "OK");
	memset(buffer,0,sizeof(buffer));
#endif
	while (1)
	{

		//here we are going to poll UNTIL there is data to read... 
		poll_ret = poll(fds, 2 , 500);
		if (poll_ret > 0)
		{
			if (fds[0].revents & POLLIN)
			{
				memset(message,0,sizeof(message));
				//a new message can break us out of this here.... 
				//Deal with CMTI, extract message number then read the message
				poll_read(500);
				if ((msg_num = extract_msg_no()) >= 0)
				{
				
					memset(message,0,sizeof(message));
					/* Received messages require a bit more complex handling */
					printf("retrieved msg numger %d\r\n", msg_num);
					val_print = sprintf(message,"AT+CMGR=%d\r", msg_num);
					bytes_written = write(fds[0].fd,message,val_print);
	  				poll_read(500);
					read_msg();
					msg_actions();
					memset(message,0,sizeof(message));
					//check the message number and the message
					//if it is the number on the database, send the data
					//for patterns recogs and then thats it, delete the message
					printf("Going to delete msg number %d\r\n", msg_num);
					val_print = sprintf(message,"AT+CMGD=%d,0\r", msg_num);
					bytes_written = write(fds[0].fd,message,val_print);
					poll_read(500);
					check_ok(buffer, "OK");
				}						
				
			}
			if (fds[1].revents & POLLIN)
			{
				read(sms_poll_fd, buf, sizeof(buf)); // need to read to clear inotify
				memset(message,0,sizeof(message));
				printf("reading the main number meow\r\n");
				char sms_no[13];
				char some_buffer[50];
				read(phone_fd, sms_no ,sizeof(sms_no));
				printf("%s: ", sms_no);
				send_msg();
				//send message based on what the number says here
			}
		}
		else if (poll_ret == EINTR)
		{
			printf("EXITING\r\n");
			//we got a sig term action close up and exit
			break;
		}

	}

	printf("EXITING\r\n");
	close(fds[0].fd );
	close(fds[1].fd );
	closelog();

	return;
}

void poll_read(int timeout_msecs)
{
	int poll_ret = 0; 
	int nbytes = 0;       /* Number of bytes read */
	int total_read = 0;
	char *bufptr;      /* Current char in buffer */
	//int timeout_msecs = 500;

	bytes = 0;
	poll_ret = poll(fds, 1, timeout_msecs);
	if (poll_ret > 0)
	{
		if (fds[0].revents & POLLIN)
		{
			ioctl(fds[0].fd, FIONREAD, &bytes);		
			printf("bytes to read here %d\r\n", bytes);
			bufptr = buffer;

			while (total_read < bytes)
			{
				nbytes = read(fds[0].fd, bufptr,bytes);
				total_read += nbytes;
				bufptr += nbytes;				
			}			
		}
	}
	
	return;
}

char* check_ok(char* str_1, char* str_2)
{
	char *pos = NULL;
	int ret_val = 0;

	printf("%s : %s\r\n", str_1, str_2);
	if ((pos = strstr(str_1, str_2)) == NULL)
	{
		syslog(LOG_ERR, "Error RX MODEM: NOT OK");
		printf("error in check for ok \r\n");
	}
	
	return pos;
}

void read_msg()
{
	char *pos = NULL;
	char *pos_2 = NULL;
	char *i = NULL;
	int k = 0;
	
	
	printf("position of comma %d ", (pos = check_ok(buffer, ",")));
	printf("position of comma2 %d ", (pos_2 = check_ok((pos + sizeof(char)), ",")));
	
	for (i = (pos + (2 * char_size)); i < (pos_2 - (1 * char_size)); i+= sizeof(char))
	{
		number[k++] =  *i;
	}
	number[k] = '\0';
	printf("number = %s\r\n", number);
	//if this message from the CMTI indication is not a learnt number, just delete it
	//kill a AT + CMGD = number -- need to compare to DB value
	
	pos = strchr(pos_2, '\n');
	pos_2 = strchr((pos + sizeof(char)), '\n');
	printf ("%d : %d ", pos, pos_2);
	k = 0;
	memset(message,0,sizeof(message));
	for (i = (pos + (1 * char_size)); i < (pos_2); i+= sizeof(char))
	{
		message[k++] =  *i;
	}
	message[k] = '\0';
	printf("message: %s\r\n", message);

	return;
}

int extract_msg_no()
{
	char *pos = NULL;
	char *pos_2 = NULL;
	char *i = NULL;
	int k = 0;
	int ret_val = -1;

	if ((strstr(buffer, "CMTI") > 0) & (strstr(buffer, "ERROR") == NULL))
	{
		pos = check_ok(buffer, ",");
		pos_2 = strchr((pos + sizeof(char)), '\n');
		k = 0;
		for (i = (pos + (1 * char_size)); i < (pos_2); i+= sizeof(char))
		{
			message[k++] =  *i;
		}
		printf("message: %s\r\n", message);
		ret_val = atoi(message);
	}
	return ret_val;
}

/* Messages will follow a very specific format: 
	action, passcode, optional extras */
void msg_actions(void)
{
	int client_msg = 0;
	
	read(code_fd, master_code,sizeof(master_code));
	master_code[5] = '\0';
	switch(message[0])
	{
		case '1': 
				if (check_ok(message, "2134") > 0)
				{
					//write to cut FD to cut the car
					printf("Cut the car\r\n");
					client_msg = 1;
					
				}
				
				break;
		case '2':
				
				if (check_ok(message,"2134") > 0)
				{
					//write to cut FD to undo the cut
					printf("Uncut the car\r\n");
					client_msg = 1;
				}
				break;

		case '3':
				if (check_ok(message, "2134") > 0)
				{
					//write to master number fd to change the number
					printf("changing the number\r\n");
					write(phone_fd, &message[7], sizeof(number));
					client_msg = 1;
				}
				break;
				
		default: 
				printf("No action taken");
				break;
	}
	if (client_msg)
	{
		//send confirmation msg to current learnt in number.....AT+CMGS=+27...
	}
	
	return; 
}

void send_msg()
{
	int bytes_written;
	int sms_fd = 0;
	char buffer[3];
	int num_comm;
	char msg_out_buf[50];
	
	if ((sms_fd = open(SMS_FD, O_RDONLY)) < 0)
	{
		syslog(LOG_ERR, "Error cannot open sms command file");
	}
	else
	{
		read(sms_fd, buffer, sizeof(buffer));
		num_comm = atoi(buffer);
		switch (num_comm) //GET RID OF MAGIC NUMBER PLZ
		{
			case 1:
				printf("case 1 received for sms send\r\n");
				break;
			case 2: printf("case 2 received for sms send\r\n");
				break;
			case 3:printf("case 3 received for sms send\r\n");
				break;
			default:
				break;
		}
	/*
		bytes_written = write(fds[0].fd,"AT+CMGS=\"+27835783897\"\r",23);
		poll_read(500);
		check_ok(buffer, ">");
		bytes_written = write(fds[0].fd,"Message here\r",13);
		bytes_written = write(fds[0].fd,"\x1A",2);
		poll_read(500);
		check_ok(buffer, "OK");
		poll_read(500); */
	}
	return;
}



