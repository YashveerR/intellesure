
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h> 
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <sys/timerfd.h>
#include <time.h>
#include <poll.h>
#include <sys/inotify.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include "control_algo.h"
#include "io_defs.h"
//#include "general.h"

//File descriptor commands
#define IO_STATES_FILE "/home/pi/gpio_fd"
#define SYS_STATES_FILE "/home/pi/intellesure/system_states.fd"
#define SMS_COMMAND_FILE "/home/pi/intellesure/sms_tx_fd"
#define SMS_IN_COMMAND_FD "/home/pi/intellesure/sms_rx_fd"

#define SMS_RX_CUT	(1)
#define SMS_RX_DISABLE_CUT	(2)
#define SMS_RX_SWAP_PHONE	(3)

#define MAX_DISARM_TO	(30)
#define BACK_OFF_TIME	(300)
#define MAX_BACKOFF		(3)

#define WAKE_UP_TIME	(5)

	//for child process call
	pid_t jamm_pid = 0;
	pid_t sms_pid = 0;
	pid_t io_pid = 0;
	
uint8_t check_intrusion(int );
int create_child(int num, ...);

//TODO: MAKE ALL PRINTF's into SYSLOG MESSAGES

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
	
	int sms_to_cnt = 0;
	int sms_bo_time = 0;
	//file descriptor for the timer and poll
	struct itimerspec time_str;
	int timer_fd;
	uint64_t timer_val;
	
	uint8_t sms_ign_to_flag = 0;
	
	struct pollfd fds[1]; 
	int timeout_msecs = 2000;
	int poll_ret; 

	int wake_timer = 0;
	
	int disarm_to = 0; //disarm timeout count
	uint8_t delay_count = 0;

	int sms_tx_fd = 0;	

	 openlog("control_main", LOG_NDELAY, LOG_PID);
	//file descriptor for the io states file, written to by python program
	syslog(LOG_INFO, "Start of Log ");
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
	if ((fd_sms = open(SMS_IN_COMMAND_FD, O_RDONLY |O_NONBLOCK)) < 0)
	{
		syslog(LOG_ERR, "unable to open IO file, Main");
	}
	
	//read the IO states
	//read last known state from file, need to always start system in known state
	//either we know the last state OR we read from the outputs and based on the IO's
	


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
	
	//spawn child processes here IO and SMS functionality
	//SDR functionality gets spawned only after certain conditions
	//io_pid = create_child(2, "/home/pi/gpio_driver.py", NULL);
	sms_pid = create_child(1, "/home/pi/intellesure/serialport_write", NULL);
	
	//printf("PID's %d ; %d\r\n", io_pid, sms_pid);
	//printf("Creating the IO process %d: \r\n", create_child("/home/yash/gpio_driver.py"));
	//printf("Creating the SMS process child %d: \r\n", create_child("/home/yash/serialport_write"));
	//TODO: add the sms portions
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
				syslog(LOG_ERR, "unable to read sms, Main");
								
			}
			sms_comm = sms_command - '0';
			
			io_state = atoi(io_states);
			printf("system_state:  %d\r\n", sys_state);
			printf("IO States %d\r\n", io_state);
			printf("SMS States %d\r\n", sms_comm);
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
					if (sms_comm & SMS_RX_CUT)
					{
						sys_state = REACTION;
						printf("reaction state\r\n");
					}
					//if the sms to disable the cut 
					if (sms_comm & SMS_RX_DISABLE_CUT)
					{
						//the sms daemon will already be running on its own, part of start up scripts
						printf("disabling the cut\r\n");
					}
					if (sms_comm & SMS_RX_SWAP_PHONE)
					{
						//same comment as above, daemon will be running on its own
						// Does the main process need to know about this?? Really? 
						//We will have a DB to hold this value.. 
						
					}
					
					break;
				}
				case DISARMED: 
				{
					printf("states of things %d:%d:%d \r\n", io_state, IGN, disarm_to);
					//TODO: perhaps make sure the system came from an IGN ON state before this........
					if ( !(io_state & IGN) && (jamm_pid <= 0) && (disarm_to == 0))
					{
						
						jamm_pid = create_child(1, "/home/pi/intellesure/rtlsdr_driver.py", NULL);			
						printf("jamming PID: \r\n", jamm_pid);														
					}
					
					if (io_state & ARM)
					{
						//arm detected then move to the armed state
						sys_state = ARMED;
						disarm_to = 0;
						sms_bo_time = 0;
						sms_to_cnt = 0;
						sms_ign_to_flag = 0;
//kill python scripts here.... and any process that should not be running
						printf("Killing jammer %d: \r\n", kill(jamm_pid, SIGTERM)); //SIGTERM gives process time to clean up and then exit
						printf("Killing sms %d: \r\n",kill(sms_pid, SIGTERM)); //kill the child processes
						jamm_pid = 0;
	 					sms_pid = 0;
						sms_tx_fd = 0;					
						create_child(2, "/home/pi/intellesure/power_saving.sh", "stop");	
						break;
					}
					
					if (((io_state & IGN) == 0) && (!sms_ign_to_flag) && (disarm_to++ >= MAX_DISARM_TO))
					{
						
						
						//need to send sms that the cars has been disarmed for too long
						//then start the power savings mode
						//TODO: PLEASE YOU ARE USING THIS TWICE, SO PLACE IN A FUNCTION OF ITS OWN!!!!!!!
						
						char sms_out_buf[2] = {'1', '\0'};
						if ((sms_tx_fd == 0) && (sms_tx_fd = open(SMS_COMMAND_FILE, O_WRONLY |O_TRUNC)) < 0)
						{
							syslog(LOG_ERR, "Error opening file for sms out %d", errno);
						}
						else if (delay_count == 0)
						{
							printf(" MAX timeout reached sending SMS \r\n");
							write(sms_tx_fd, sms_out_buf, sizeof(sms_out_buf));
							close(sms_tx_fd); //close the file after use, do not need it anymore
						
						}	
						if (delay_count++ >= 5)
						{
							sms_bo_time = 0;
							sms_to_cnt = 0;
							//NEED TO WAIT UNTIL THE SMS HAS BEEN SENT
							//kill python scripts here.... and any process that should not be running
							printf("PID Termination %d:%d \r\n", jamm_pid, sms_pid);
							printf("Killing jammer %d: \r\n", kill(jamm_pid, SIGTERM)); //SIGTERM gives process time to clean up and then exit
							printf("Killing sms %d: \r\n",kill(sms_pid, SIGTERM)); //kill the child processes
							jamm_pid = 0;
							sms_pid = 0;
							create_child(2, "/home/pi/intellesure/power_saving.sh",
 "stop");					
							sms_ign_to_flag = 1;	
							delay_count = 0;
							
						}				

					}
					
					if ((io_state & IGN) == 1)
					{
                        //TODO: STOP ANY SCRIPTS THAT SHOULD NOT BE RUNNING; 
						//TODO: start the battery charging stuff - might already be handled for us using a power bank
					}
					
					
					
					
					break;
				}
				case INTRUSION: 
				{
					//in the event of an intrusion, we need to then send out an alert ASAPs
					//TODO: disable power savings
					//need to send sms that there has been an instrusion
					create_child(2, "/home/pi/intellesure/power_savings.sh", "start"); //need to give time to 
						//the USB hub and stuff to come up again before we start them again 
					if ((system("lsusb -d 10c4:ea60")) == 0 )
					{
						if (sms_pid == 0)
						{
							sms_pid = create_child(1, "/home/pi/intellesure/serialport_write", NULL);
						}					
						//then start the power savings mode
						
						//TODO: revise this to make it a single send as NO SPAM MUST BE sent
						if ((sms_bo_time++ >= (sms_to_cnt * BACK_OFF_TIME )) && (sms_to_cnt++ < MAX_BACKOFF))
						{	
				
							int sms_tx_fd = 0;
							char sms_out_buf[2] = {'3', '\0'};
							if ((sms_tx_fd = open(SMS_COMMAND_FILE, O_WRONLY |O_TRUNC)) < 0)
							{
								syslog(LOG_ERR, "Error opening file for sms out %d", errno);
							}
							else
							{
								write(sms_tx_fd, sms_out_buf, sizeof(sms_out_buf));
								close(sms_tx_fd); //close the file after use, do not need it anymore
								printf("SENDING SMS's\r\n");
							}					
		
						}
					}
					if (io_state & DISARM)
					{
						sys_state = DISARMED;
						printf("dISARM DETECTED\r\n");
					}
					//if a sms command comes through to "cut" the car
					if (sms_command & SMS_RX_CUT)
					{
						//sys_state = REACTION;
						printf("reaction state from intrusion\r\n");
					}
									
					break;
				}
				case REACTION: 
				{	//TODO: disable power savings
					
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

pid_t create_child(int num, ...)
{	
	int i = 0;
	int retval = 0;
	int ind = 0;
	/* List used to store a variable number of arguments */
	va_list ap;
	pid_t pid;
	char proc[64] = {0};
	char *args[num + 1];

	/* Initializes the list */
	va_start(ap, num);

	for (i = 0; i < num; i++) {
		/* va_arg(va_list, arg_type) - Returns the next argument in
		 * the list */
		args[i] = va_arg(ap, char *);
		if (ind < (sizeof(proc) - 2)) {
			strncpy(&proc[ind], args[i], sizeof(proc) - ind - 2);
			ind += strlen(&proc[ind]);
			proc[ind++] = ' ';
		}
	}
	args[num] = NULL;

	/*
	 * Explicitly ignore the SIGCHLD signal so that dead children will be
	 * silently reaped, instead of turning into annoying zombies. (I hope.)
	 */
	signal(SIGCHLD, SIG_IGN);

	syslog(LOG_INFO, "Attempting to create child process: %s", proc);
	/* Creates a child process */
	if ((pid = fork()) < 0) {
		syslog(LOG_ERR, "Error forking: %d", errno);
		retval = -1;
	} else if (!pid) {
		signal(SIGCHLD, SIG_DFL);
		if (execvp(args[0], args) < 0) {
			syslog(LOG_ERR, "Error executing %s: %d", args[0], errno);
		}
		exit(0);
	}

	/* Cleans up the variable argument list */
	va_end(ap);
		
	printf("PID: %d\r\n", pid);
	
	return pid;
}




