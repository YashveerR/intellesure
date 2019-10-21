//control_algo.h
#ifndef __CONTROL_ALGO_H
#define __CONTROL_ALGO_H

enum system_states
{
	ARMED = 5, 
	DISARMED, 
	INTRUSION,
	REACTION
};

enum io_states
{
	IGN_ON = 0x01
};



extern int g_state;


#endif /* __CONTROL_ALGO_H */
