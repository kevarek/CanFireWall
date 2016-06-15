/*
*Filename: main.c
*Author: Stanislav Subrt
*Year: 2014
*/

#include "main.h"
#include "clocks.h"
#include "xbee.h"
#include "can.h"

//////////////////////////////////////////////////////////////////////
////////// CMSIS RTX RTOS THREAD DEFINITIONS /////////////////////////
//////////////////////////////////////////////////////////////////////

osThreadId bl_TaskId;
osThreadDef(bl_Task, osPriorityLow, 1, 0);

osThreadId com_ReceiveTaskId;
osThreadDef(com_ReceiveTask, osPriorityNormal, 1, 0);

osThreadId xb_ReceiveTaskId;
osThreadDef(xb_ReceiveTask, osPriorityNormal, 1, 0);

osThreadId can_QueryTaskId;
osThreadDef(can_QueryTask, osPriorityNormal, 1, 0);



//////////////////////////////////////////////////////////////////////
////////// HARDWARE INITIALIZATION ///////////////////////////////////
//////////////////////////////////////////////////////////////////////


static void BoardInit(void){


	//Initialize hardware
	clk_Init();			//Initialize MCU clock
	bl_Init();			//Blinky module - timing for HAL and optional performance monitoring

	can_Init();
	com_Init();			//Communication with other MCU
	xb_Init();			//Communication with PC
}


//////////////////////////////////////////////////////////////////////
////////// MAIN + RTOS INIT AND START ////////////////////////////////
//////////////////////////////////////////////////////////////////////
int main(void){
	HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_2);
	osKernelInitialize();	//Initialize RTOS kernel
    BoardInit();			//Initialize hardware


	//Start RTOS tasks and kernel
    bl_TaskId = osThreadCreate(osThread(bl_Task), NULL);

    com_ReceiveTaskId = osThreadCreate(osThread(com_ReceiveTask), NULL);
    xb_ReceiveTaskId = osThreadCreate(osThread(xb_ReceiveTask), NULL);
	can_QueryTaskId = osThreadCreate(osThread(can_QueryTask), NULL);
    osKernelStart();

    return 0;
}


uint32_t HAL_GetTick(void)
{
	return bl_GetTick();
}
