/*
*Filename: 	can.h
*Author: 	Ing. Stanislav Subrt
*Year:		2016
*/


#ifndef CAN_H
#define CAN_H

extern void can_Init(void);
extern util_ErrTd can_SendMsg(const uint16_t msgId, const uint8_t* data, uint8_t len);

//RTOS task which handles data reception and parsing
#include "cmsis_os.h"
extern osThreadId can_QueryTaskId;
extern void can_QueryTask(void const *argument);

#endif  //end of CAN_H
