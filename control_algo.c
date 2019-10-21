
#include <stdint.h>
#include <stdio.h> 
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/timerfd.h>
#include <time.h>
#include <poll.h>
#include <sys/inotify.h>
#include <errno.h>
#include <signal.h>

#include "control_algo.h"
#include "io_defs.h"
//#include "general.h"

//File descriptor commands
#define IO_STATES_FILE "io_states.fd"
#define SYS_STATES_FILE "system_states.fd"
#define SMS_COMMAND_FILE "sms_tx_fd"

#define SMS_RX_CUT	(1)
#define SMS_RX_DISABLE_CUT	(2)
#define SMS_RX_SWAP_PHONE	(3)

#define MAX_DISARM_TO	(1200)

uint8_t check_intrusion(int );

//fdopen

uint8_t main(void)
{
	//char io_states; 
	char io_states[3];
	int io_state;	//post conversion value
	uint8_t fd_io; 
	uint8_t fd;
	uint8_t fd_sys_state; 
	uint8_t system_state;
	int sys_state;  
	uint8_t fd_sms; 
	uint8_t sms_command;
	int sms_comm; 
	
	//file descriptor for the timer and poll
	struct itimerspec time_str;
	int timer_fd;
	uint64_t timer_val;
	
	
	struct pollfd fds[1]; 
	int timeout_msecs = 2000;
	int poll_ret; 

	//for child process call
	int pid = 0;

	int disarm_to = 0; //disarm timeout count


	 openlog("control_main", LOG_NDELAY, LOG_PID);
	//file descriptor for the io states file, written to by python program
	if ((fd = open(IO_STATES_FILE, O_RDONLY |O_NONBLOCK )) < 0)
	{
		syslog(LOG_ERR, "unable to open IO file, Main");
	}
	
	if (read(fd, &io_states, sizeof(io_states)) < 0)
	{
		syslog(LOG_ERR, "unable to read io's, Main");
	}
	//convert whats read to an integer
	io_state = atoi(io_states);

	//file descriptor for the system states
	if ((fd_sys_state = open(SYS_STATES_FILE, O_RDONLY |O_NONBLOCK)) < 0)
	{
		syslog(LOG_ERR, "unable to open IO file, Main");
	}
	
	if (read(fd_sys_state, &system_state, sizeof(system_state)) < 0)
	{
		syslog(LOG_ERR, "unable to read io's, Main");
	}
	sys_state = system_state - '0';
	printf("Initial system state %d\r\n", sys_state);

	//file descriptor for the received sms commands
	if ((fd_sms = open(SMS_COMMAND_FILE, O_RDONLY |O_NONBLOCK)) < 0)
	{
		syslog(LOG_ERR, "unable to open IO file, Main");
	}
	
	if (read(fd_sms, &sms_command, sizeof(sms_command)) < 0)
	{
		syslog(LOG_ERR, "unable to read io's, Main");
	}
	
	//read the IO states
	//read last known state from file, need to always start system in known state
	//either we know the last state OR we read from the outputs and based on the IO's
	
	if ((io_state & IGN_ON) == 1)
	{
		//This means that the ignition is on 
	}

	//we have this timer here as we need to stop stealing all the resources
	timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
	
	time_str.it_interval.tv_sec = 1;
	time_str.it_interval.tv_nsec = 0;
	time_str.it_value.tv_sec = 1; 
	time_str.it_value.tv_nsec = 0;
	
	//if ( < 0) 
	{
		printf("%d\r\n",timerfd_settime(timer_fd, 0, &time_str, NULL));
	}
	
	
	fds[0].fd = timer_fd;
	fds[0].events = POLLIN;
	
	while (1)
	{
		//smart thi
		poll_ret = poll(fds, 1, timeout_msecs); //poll the timer for 1 second events
		if (fds[0].revents & POLLIN)
		{
			
			read(fds[0].fd, &timer_val, sizeof(timer_val));
			lseek (fd, 0, SEEK_SET); //set the file pointer back to the beginning of the file
			if (read(fd, &io_states, sizeof(io_states)) < 0)
			{
				syslog(LOG_ERR, "unable to read io's, Main");
			}
			lseek (fd_sms, 0, SEEK_SET); //set the file pointer back to the beginning of the file
			if (read(fd_sms, &sms_command, sizeof(sms_command)) < 0)
			{
				syslog(LOG_ERR, "unable to read io's, Main");
								
			}
			sms_comm = sms_command - '0';
			
			io_state = atoi(io_states);
			printf("system_state:  %d\r\n", sys_state);
			//read system states - make this a return procedure - actually make this one procedure, takes an file descriptor arg, void* var and length, returns variable value/values
			//read io's - make this a return procedure 
			switch (sys_state)
			{

				case ARMED: 
				{
					printf("armed state\r\n");
					//if any of the IO's return as open during this stage, 
					//then this is an un-authorized entry
					if (check_intrusion(io_state))
					{
						sys_state = INTRUSION;
						printf("intrusion detected\r\n");
					}
					//if the disarm pulse is received, disarm the device
					else if (io_state & DISARM)
					{
						sys_state = DISARMED;
						printf("dISARM DETECTED\r\n");
					}
					//if a sms command comes through to "cut" the car
					if (sms_command & SMS_RX_CUT)
					{
						sys_state = REACTION;
						printf("reaction state\r\n");
					}
					//if the sms to disable the cut 
					if (sms_command & SMS_RX_DISABLE_CUT)
					{
						//the sms daemon will already be running on its own, part of start up scripts
						//send the signal to dis_able the output
						//write to IO_FD_OUT, probably want a single struct IO FD
						printf("disabling the cut\r\n");
					}
					if (sms_command & SMS_RX_SWAP_PHONE)
					{
						//same comment as above, daemon will be running on its own
						// Does the main process need to know about this?? Really? 
						//We will have a DB to hold this value.. 
						
					}
					
					break;
				}
				case DISARMED: 
				{
					printf("states of things %d:%d\r\n", io_state, IGN);
					//TODO: perhaps make sure the system came from an IGN ON state before this........
					if (!(io_state & !(IGN)) && (pid <= 0))
					{
						printf("Attempting to start the process \r\n");
						char *cmd = "/home/yash/rtlsdr_driver.py";
						char *argv[3];
						//if ignition if on need to call the jamming daemon
						syslog(LOG_INFO, "Attempting to create child process: %s", "jamming detect");
						/* Creates a child process */
						if ((pid = fork()) < 0) {
							syslog(LOG_ERR, "Error forking: %d", errno);
								
						} else if (!pid) {
							signal(SIGCHLD, SIG_DFL);
							if (execvp(cmd, argv) < 0) {
								syslog(LOG_ERR, "Error executing %s: %d", cmd, errno);
							}
						}
						
					}
					
					if (io_state & ARM)
					{
						//arm detected then move to the armed state
						sys_state = ARMED;
						disarm_to = 0;
						break;
					}
					
					if (disarm_to++ >= MAX_DISARM_TO)
					{
						//need to send sms that the cars has been disarmed for too long
						//then start the power savings mode
						int sms_tx_fd = 0;
						char sms_out_buf[2] = {'1', '\0'};
						if ((sms_tx_fd = open(SMS_COMMAND_FILE, O_WRONLY |O_TRUNC)) < 0)
						{
							syslog(LOG_ERR, "Error opening file for sms out %d", errno);
						}
						else
						{
							write(sms_tx_fd, sms_out_buf, sizeof(sms_out_buf));
							close(sms_tx_fd); //close the file after use, do not need it anymore
							disarm_to = 0;
						}
						
						//kill python scripts here.... and any process that should not be running
						//kill(PID, SIGNAL);
					}
					
					
					
					break;
				}
				case INTRUSION: 
				{
					//in the event of an intrusion, we need to then send out an alert ASAPs
					
					break;
				}
				case REACTION: 
				{	
					break;
				}
			
				default: break; //do something a bit more meaningful here please
			}//end case
		}//end poll
	
	}
	
	return 0;
}

uint8_t check_intrusion(int io_value)
{
	uint8_t retval; 

	retval = 0; 
	
	if (	(io_value & D_DOOR) ||
		(io_value & P_DOOR) ||
		(io_value & RD_DOOR) ||
		(io_value & LP_DOOR) ||
		(io_value & BOOT) ||
		(io_value & BONNET)	)
	{
		retval = INTRUSION;
	}
	
	return retval; 

}
