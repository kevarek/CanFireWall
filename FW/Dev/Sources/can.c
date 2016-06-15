/*
*Filename: 	can.c
*Author: 	Ing. Stanislav Subrt
*Year:		2016
*/

#include "main.h"
#include "can.h"

#define CAN_PORT_INSTANCE              	GPIOB
#define CAN_PORT_ENABLEPERIPHCLK		__GPIOB_CLK_ENABLE

#define CAN_PIN_TX                    	GPIO_PIN_9
#define CAN_PIN_TX_AF                   GPIO_AF9_CAN

#define CAN_PIN_RX                   	GPIO_PIN_8
#define CAN_PIN_RX_AF					GPIO_AF9_CAN

#define CAN_CAN_INSTANCE				CAN
#define CAN_CAN_ENABLEPERIPHCLK			__CAN_CLK_ENABLE
#define CAN_CAN_PRESC					5
#define CAN_RX_IRQn						USB_LP_CAN_RX0_IRQn
#define CAN_RX_IRQHandler				USB_LP_CAN_RX0_IRQHandler


static CAN_HandleTypeDef    CanHandle;
static CanTxMsgTypeDef 		TxMessage;
static CanRxMsgTypeDef 		RxMessage;
static CAN_FilterConfTypeDef sFilterConfig;


//////////////////////////////////////////////////////////////////////
////////// EOBD /////////////////
//////////////////////////////////////////////////////////////////////

#define EOBD_IS_EOBDMSG(x)		( ( (x) >= 0x7E8 ) && ( (x) <= 0x7EF ) )
#define EOBD_INIT_ADDR			0x7DF
#define EOBD_MODE_CURRENTDATA	0x01
#define EOBD_PID_ENGLOAD		0x04
#define EOBD_PID_ENGRPM			0x0C
#define EOBD_PID_VEHSPEED		0x0D

uint16_t EOBD_RxId = 0x00;																					//This ID must be preinitialized to zero. First EOBD ECU that responds will then be the one that we ask for car data

//All messages must have 8 bytes of payload or it doesnt work
uint8_t EOBD_Init[] = {0x02, EOBD_MODE_CURRENTDATA, EOBD_PID_ENGRPM, 0x55, 0x55, 0x55, 0x55, 0x55};


//////////////////////////////////////////////////////////////////////
////////// INIT //////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void can_Init(void){
	GPIO_InitTypeDef GPIO_InitStruct;

	CAN_PORT_ENABLEPERIPHCLK();
	GPIO_InitStruct.Pin = CAN_PIN_TX;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Alternate =  CAN_PIN_TX_AF;
	HAL_GPIO_Init(CAN_PORT_INSTANCE, &GPIO_InitStruct);
	GPIO_InitStruct.Pin = CAN_PIN_RX;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Alternate =  CAN_PIN_RX_AF;
	HAL_GPIO_Init(CAN_PORT_INSTANCE, &GPIO_InitStruct);
	HAL_NVIC_SetPriority(CAN_RX_IRQn, 3, 0);
	HAL_NVIC_EnableIRQ(CAN_RX_IRQn);

	CAN_CAN_ENABLEPERIPHCLK();
	CanHandle.Instance = CAN_CAN_INSTANCE;
	CanHandle.pTxMsg = &TxMessage;
	CanHandle.pRxMsg = &RxMessage;
	CanHandle.Init.TTCM = DISABLE;
	CanHandle.Init.ABOM = DISABLE;
	CanHandle.Init.AWUM = DISABLE;
	CanHandle.Init.NART = DISABLE;
	CanHandle.Init.RFLM = DISABLE;
	CanHandle.Init.TXFP = DISABLE;
	CanHandle.Init.Mode = CAN_MODE_NORMAL;
	CanHandle.Init.SJW = CAN_SJW_1TQ;
	CanHandle.Init.BS1 = CAN_BS1_16TQ;
	CanHandle.Init.BS2 = CAN_BS1_7TQ;
	CanHandle.Init.Prescaler = CAN_CAN_PRESC - 1;															//36MHz/3 = 12MHz TQ...1TQ+11TQ+12TQ=24TQ->0.5MHz baudrate for VWTP2.0
	HAL_CAN_Init(&CanHandle);

	//Setup RX filters to accept everything
	sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
	sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
	sFilterConfig.FilterFIFOAssignment = 0;
	sFilterConfig.FilterActivation = ENABLE;
	sFilterConfig.FilterNumber = 0;
	sFilterConfig.BankNumber = 0;
	sFilterConfig.FilterIdHigh = 0x0000;
	sFilterConfig.FilterIdLow = 0x0000;
	sFilterConfig.FilterMaskIdHigh = 0x0000;
	sFilterConfig.FilterMaskIdLow = 0x0000;
	HAL_CAN_ConfigFilter(&CanHandle, &sFilterConfig);
	__HAL_CAN_ENABLE_IT(&CanHandle, CAN_IT_FMP0);															//Start receiving
	//can_SendMsg(EOBD_INIT_ADDR, EOBD_Init, 8);																//Get EOBD ECU ID, answer to this msg will be in range 0x7D8-0x7DF. Target ECU address os
}


util_ErrTd can_SendMsg(const uint16_t msgId, const uint8_t* data, uint8_t len){
	uint8_t i;

	if( data == 0 ){
		return util_ErrTd_Null;
	}
	else if( len <= 0 ){
		return util_ErrTd_Null;
	}
	else if( len > 8 ){
		len = 8;
	}

	CanHandle.pTxMsg->StdId = msgId;
	CanHandle.pTxMsg->ExtId = 0x00;
	CanHandle.pTxMsg->RTR = CAN_RTR_DATA;
	CanHandle.pTxMsg->IDE = CAN_ID_STD;
	CanHandle.pTxMsg->DLC = len;

	for( i=0; i<len; i++){
		CanHandle.pTxMsg->Data[i] = data[i];
	}

	if( HAL_CAN_Transmit(&CanHandle, 100) == HAL_OK ){
		return util_ErrTd_Ok;
	}
	else{
		return util_ErrTd_Timeout;
	}
}


#define CAN_SIG_DATARECEIVED	0x01
void can_QueryTask(void const *argument){
	uint32_t *pData;

	for(;;){
		osSignalWait(CAN_SIG_DATARECEIVED, osWaitForever);													//Wait for can msg reception
		osSignalClear(can_QueryTaskId, CAN_SIG_DATARECEIVED);												//clear signal
		pData = CanHandle.pRxMsg->Data;																		//Create shortcut for received msg
		com_SendF("%03x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t\r\n",
					CanHandle.pRxMsg->StdId, CanHandle.pRxMsg->DLC,
					pData[0], pData[1], pData[2], pData[3], pData[4], pData[5], pData[6], pData[7]);		//Format data and forward it via usart to another MCU that will transmit it again via can

		xb_SendF("<cde\t%d\t%03x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t>\r\n",				//Car data event
					bl_GetTick(), CanHandle.pRxMsg->StdId, CanHandle.pRxMsg->DLC,
					pData[0], pData[1], pData[2], pData[3], pData[4], pData[5], pData[6], pData[7]);		//Format data and forward it via XBEE or bluetooth to PC
	}
}


void CAN_RX_IRQHandler(void){
	//Check End of reception flag for FIFO0
	if((__HAL_CAN_GET_IT_SOURCE(&CanHandle, CAN_IT_FMP0)) && (__HAL_CAN_MSG_PENDING(&CanHandle, CAN_FIFO0) != 0)){

		//Get message ID
		CanHandle.pRxMsg->IDE = (uint8_t)0x04 & CanHandle.Instance->sFIFOMailBox[0].RIR;
		if (CanHandle.pRxMsg->IDE == CAN_ID_STD){
			CanHandle.pRxMsg->StdId = (uint32_t)0x000007FF & (CanHandle.Instance->sFIFOMailBox[0].RIR >> 21);
		}
		else{
			CanHandle.pRxMsg->ExtId = (uint32_t)0x1FFFFFFF & (CanHandle.Instance->sFIFOMailBox[0].RIR >> 3);
		}
		//Get the message info
		CanHandle.pRxMsg->RTR = (uint8_t)0x02 & CanHandle.Instance->sFIFOMailBox[0].RIR;
		CanHandle.pRxMsg->DLC = (uint8_t)0x0F & CanHandle.Instance->sFIFOMailBox[0].RDTR;
		CanHandle.pRxMsg->FMI = (uint8_t)0xFF & (CanHandle.Instance->sFIFOMailBox[0].RDTR >> 8);
		// Get the message data
		CanHandle.pRxMsg->Data[0] = (uint8_t)0xFF & CanHandle.Instance->sFIFOMailBox[0].RDLR;
		CanHandle.pRxMsg->Data[1] = (uint8_t)0xFF & (CanHandle.Instance->sFIFOMailBox[0].RDLR >> 8);
		CanHandle.pRxMsg->Data[2] = (uint8_t)0xFF & (CanHandle.Instance->sFIFOMailBox[0].RDLR >> 16);
		CanHandle.pRxMsg->Data[3] = (uint8_t)0xFF & (CanHandle.Instance->sFIFOMailBox[0].RDLR >> 24);
		CanHandle.pRxMsg->Data[4] = (uint8_t)0xFF & CanHandle.Instance->sFIFOMailBox[0].RDHR;
		CanHandle.pRxMsg->Data[5] = (uint8_t)0xFF & (CanHandle.Instance->sFIFOMailBox[0].RDHR >> 8);
		CanHandle.pRxMsg->Data[6] = (uint8_t)0xFF & (CanHandle.Instance->sFIFOMailBox[0].RDHR >> 16);
		CanHandle.pRxMsg->Data[7] = (uint8_t)0xFF & (CanHandle.Instance->sFIFOMailBox[0].RDHR >> 24);
		__HAL_CAN_FIFO_RELEASE(&CanHandle, CAN_FIFO0);

		#define EOBD_READINESS	0x01
		if( EOBD_IS_EOBDMSG(CanHandle.pRxMsg->StdId) ){															//If received message is EOBD
			if( CanHandle.pRxMsg->Data[2] == EOBD_READINESS ){													//If it is readiness response from car
				CanHandle.pRxMsg->Data[3] = 0xFF;																//Is test available is set to true
				CanHandle.pRxMsg->Data[4] = 0x00;																//Is incomplete is set to false
			}
		}
		osSignalSet(can_QueryTaskId, CAN_SIG_DATARECEIVED);														//Set signal for can task that can message has been received
	}
}
