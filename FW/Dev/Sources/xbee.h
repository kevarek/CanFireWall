/*
*Filename: 	xbee.h
*Author: 	Ing. Stanislav Subrt
*Year:		2015
*/

#ifndef	XBEE_H
#define XBEE_H

extern void xb_Init(void);																					//Initializes communication peripheral and buffers
extern util_ErrTd xb_SendF(char *format, ...);																//Sends data via communication peripheral

extern UART_HandleTypeDef xb_ComPort;																		//Share port handle for dma tx complete callback
extern void xb_TransmitTxBuffer(void);																		//Share port transmit function for dma tx complete callback

#include "cmsis_os.h"
extern osThreadId xb_ReceiveTaskId;
extern void xb_ReceiveTask(void const *argument);															//RTOS task which handles data reception and parsing


#endif	//XBEE_H
