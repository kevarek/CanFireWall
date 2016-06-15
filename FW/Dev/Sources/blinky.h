/*
*Filename: 	blinky.h
*Author: 	Ing. Stanislav Subrt
*Year:		2015
*/


#ifndef BLINKY_H
#define BLINKY_H

#include <stdint.h>

typedef struct{
	uint32_t TotalRAM;
	uint32_t FreeRAM;
	uint32_t StackUsage;
	uint32_t DataUsage;
	uint32_t CpuLoadVal;
	float CPULoad;
}bl_PerfTd;


typedef struct bl_TimeoutTd{
	uint32_t Expiration;
	struct bl_TimeoutTd* Prev;
	struct bl_TimeoutTd* Next;
	void (*ExpiredFn)(void);
	void (*NotExpiredFn)(void);
}bl_TimeoutTd;


extern void bl_Init(void);
extern unsigned bl_GetTick(void);
extern inline void bl_UpdatePerfCntr(void);


//Timeouts
extern void bl_InitTimeout(bl_TimeoutTd* timeoutObj, uint32_t timeoutMs, void (*expiredCallbackFn)(void), void (*notExpiredCallbackFn)(void));
extern void bl_DeInitTimeout(bl_TimeoutTd* timeoutObj);
extern void bl_SetTimeout(bl_TimeoutTd* timeoutObj, uint32_t timeoutMs);


#include "cmsis_os.h"
extern osThreadId bl_TaskId;
extern void bl_Task(void const *argument);

#endif  //end of BLINKY_H
