/*
*Filename: com.h
*Author: Ing. Stanislav Subrt
*Year: 2016
*/


#ifndef	COM_H
#define COM_H

extern void com_Init(void);																					//Initializes communication peripheral and buffers
extern util_ErrTd com_SendF(char *format, ...);																//Sends data via communication peripheral

#include "cmsis_os.h"
extern osThreadId com_ReceiveTaskId;
extern void com_ReceiveTask(void const *argument);															//RTOS task which handles data reception and parsing
#endif	//COM_H

