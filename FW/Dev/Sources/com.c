/*
*Filename: 	com.c
*Author: 	Ing. Stanislav Subrt
*Year:		2016
*/


#include "main.h"
#include "can.h"
#include <stdarg.h>


//////////////////////////////////////////////////////////////////////
////////// HARDWARE ABSTRACTION LAYER ////////////////////////////////
//////////////////////////////////////////////////////////////////////

#define COM_PORT_INST     				GPIOA
#define COM_PORT_ENPERCLK				__GPIOA_CLK_ENABLE
#define COM_PIN_TX    					GPIO_PIN_2
#define COM_PIN_RX    					GPIO_PIN_3
#define COM_PORT_AF						GPIO_AF7_USART2

#define COM_USART_INST					USART2
#define COM_USART_ENPERCLK				__USART2_CLK_ENABLE
#define COM_USART_BAUDRATE				115200
#define COM_USART_IRQN					USART2_IRQn
#define COM_USART_IRQH					USART2_IRQHandler

#define COM_DMA_INST					DMA1
#define COM_DMA_ENPERCLK				__DMA1_CLK_ENABLE

#define COM_DMA_TX						DMA1_Channel7
#define COM_DMA_TX_IRQN					DMA1_Channel7_IRQn
#define COM_DMA_TX_IRQH					DMA1_Channel7_IRQHandler

#define COM_DMA_RX						DMA1_Channel6
#define COM_DMA_RX_IRQN					DMA1_Channel6_IRQn
#define COM_DMA_RX_IRQH					DMA1_Channel6_IRQHandler


#define COM_RXBUFSIZE					1024																//Must be power of two because of some math magic
#define COM_RXBUFTAILMASK				( COM_RXBUFSIZE - 1 )
#define COM_SINGLECMDBUFSIZE			512
#define COM_ENDOFCMDCHAR				'\n'

#define COM_TXBUFSIZE					1024																//Must be power of two because of some math magic
#define COM_TXBUFHEADMASK				( COM_TXBUFSIZE - 1 )



static uint8_t	com_RxBuffer[COM_RXBUFSIZE];
static uint32_t com_RxTail = 0;
static int32_t com_RxCmdCnt = 0;
static int32_t com_HandledRxCmdCnt = 0;

static uint8_t com_Cmd[COM_SINGLECMDBUFSIZE];

static uint8_t com_TxBuffer[COM_TXBUFSIZE];
static uint32_t com_TxHead = 0;
static uint32_t com_TxTail = 0;

static char com_TmpStr[COM_SINGLECMDBUFSIZE];

static UART_HandleTypeDef com_ComPort = {0};
static DMA_HandleTypeDef com_hDMA_TX = {0};

osMutexId com_MutexId;
osMutexDef (com_Mutex);
osStatus com_MutexStatus;


//////////////////////////////////////////////////////////////////////
////////// INITIALIZATION ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

//Initializes communication peripheral and buffers
void com_Init(void){
	GPIO_InitTypeDef GPIO_InitStructure = {0};
	DMA_HandleTypeDef hDMA_RX = {0};

	//Configure GPIO pins for UART
	COM_PORT_ENPERCLK();
	GPIO_InitStructure.Pin = COM_PIN_TX | COM_PIN_RX;
	GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.Alternate = COM_PORT_AF;
	HAL_GPIO_Init(COM_PORT_INST, &GPIO_InitStructure);

	//Configure USART peripheral
	COM_USART_INST->CR2 = 0xFF000000;																	//I wish I could remember what this line was supposed to do then ... :)
	COM_USART_ENPERCLK();
	com_ComPort.Instance = COM_USART_INST;
	com_ComPort.Init.BaudRate = COM_USART_BAUDRATE;
	com_ComPort.Init.WordLength = UART_WORDLENGTH_8B;
	com_ComPort.Init.StopBits = UART_STOPBITS_1;
	com_ComPort.Init.Parity = UART_PARITY_NONE;
	com_ComPort.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	com_ComPort.Init.Mode = UART_MODE_TX_RX;
	HAL_UART_Init(&com_ComPort);

	//Set character match and disable error interrupts (this can be done only while Receiver is enabled and USART is disabled (RE=1, UE=0)
	__HAL_UART_DISABLE(&com_ComPort);
	//__HAL_UART_DISABLE_IT(&ComPort, UART_IT_ERR);
	MODIFY_REG(COM_USART_INST->CR3, USART_CR3_OVRDIS, USART_CR3_OVRDIS);
	MODIFY_REG(COM_USART_INST->CR2, USART_CR2_ADD, ((unsigned)COM_ENDOFCMDCHAR << UART_CR2_ADDRESS_LSB_POS));
	__HAL_UART_ENABLE(&com_ComPort);

	//Configure DMA for TX and RX
	COM_DMA_ENPERCLK();
	com_hDMA_TX.Instance = COM_DMA_TX;
	com_hDMA_TX.Init.Direction = DMA_MEMORY_TO_PERIPH;
	com_hDMA_TX.Init.PeriphInc = DMA_PINC_DISABLE;
	com_hDMA_TX.Init.MemInc = DMA_MINC_ENABLE;
	com_hDMA_TX.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
	com_hDMA_TX.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
	com_hDMA_TX.Init.Mode = DMA_NORMAL;
	com_hDMA_TX.Init.Priority = DMA_PRIORITY_MEDIUM;
	HAL_DMA_Init(&com_hDMA_TX);
	__HAL_LINKDMA(&com_ComPort, hdmatx, com_hDMA_TX);
	HAL_NVIC_SetPriority(COM_DMA_TX_IRQN, 3, 1);
	HAL_NVIC_EnableIRQ(COM_DMA_TX_IRQN);

	hDMA_RX.Instance = COM_DMA_RX;
	hDMA_RX.Init.Direction = DMA_PERIPH_TO_MEMORY;
	hDMA_RX.Init.PeriphInc = DMA_PINC_DISABLE;
	hDMA_RX.Init.MemInc = DMA_MINC_ENABLE;
	hDMA_RX.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
	hDMA_RX.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
	hDMA_RX.Init.Mode = DMA_CIRCULAR;
	hDMA_RX.Init.Priority = DMA_PRIORITY_MEDIUM;
	HAL_DMA_Init(&hDMA_RX);
	__HAL_LINKDMA(&com_ComPort, hdmarx, hDMA_RX);

	//Enable USART interrupts
	HAL_NVIC_SetPriority(COM_USART_IRQN, 3, 3);
	HAL_NVIC_EnableIRQ(COM_USART_IRQN);
	__HAL_UART_ENABLE_IT(&com_ComPort, UART_IT_CM);

	//Starts port reading
	HAL_UART_Receive_DMA(&com_ComPort, com_RxBuffer, COM_RXBUFSIZE);

	com_MutexId = osMutexCreate(osMutex(com_Mutex));
}


//Transmits data from Tx circular buffer either in one part if data is linear or in two parts if data is broken by circular buffers end.
static void TransmitTxBuffer(void){

	HAL_NVIC_DisableIRQ(COM_DMA_TX_IRQN);																	//DMA interrupt must be disabled - if transfer is complete immediately (one byte only, DMA transmit function invokes interrupt before tail position is updated)
	if( com_TxHead != com_TxTail ){																			//If there is data to transfer
		if( com_TxHead > com_TxTail ){																		//If all data in circular buffer is linear
			if( HAL_UART_Transmit_DMA(&com_ComPort, com_TxBuffer + com_TxTail , com_TxHead - com_TxTail) == HAL_OK ){
				com_TxTail = com_TxHead;																	//Transmit all data and move tail to head position
			}
		}
		else if( com_TxHead < com_TxTail ){																	//If data is separated by circular buffer end
			if( HAL_UART_Transmit_DMA(&com_ComPort, com_TxBuffer + com_TxTail , COM_TXBUFSIZE - com_TxTail) == HAL_OK ){
				com_TxTail = 0;																				//Transmit first end of buffer data part and then linear part from beginning of circular buffer
			}
		}
	}

	if( com_TxTail >= COM_TXBUFSIZE ){
		com_TxTail = 0;
	}
	if( com_TxHead >= COM_TXBUFSIZE ){
		com_TxHead = 0;
	}

	HAL_NVIC_EnableIRQ(COM_DMA_TX_IRQN);
}


//Send data in specified format. Same syntax as printf
util_ErrTd com_SendF(char *format, ...){
	util_ErrTd Status = util_ErrTd_Ok;
	va_list va;
	int32_t i;

	if( osMutexWait(com_MutexId, 100) == osOK ){															//Wait for shared resource (tx buffer access)
		HAL_NVIC_DisableIRQ(COM_DMA_TX_IRQN);																//Disable DMA interrupts (TXC could acces TxBuffer in the middle of writing data to it

		va_start(va, format);																				//Start reading of parameters
		vsnprintf(com_TmpStr, sizeof(com_TmpStr), format, va);												//Format new string and save its length
		va_end(va);																							//End of reading parameters

	//	if( Cnt <= 0 || Cnt >= sizeof(com_TmpStr) ){														//If formatted string doesnt fit into buffer
	//		snprintf(com_TmpStr, sizeof(com_TmpStr), "<erre %d %d>\r\n", (int)Cnt, (int)sizeof(com_TmpStr));	//Format error message instead
	//		Status = util_ErrTd_Overflow;
	//	}

		for( i=0; i<strlen(com_TmpStr); i++ ){																//Copy byte by byte into tx buffer
			com_TxBuffer[ com_TxHead++ ] = com_TmpStr[i];
			com_TxHead &= COM_TXBUFHEADMASK;
		}

		HAL_NVIC_EnableIRQ(COM_DMA_TX_IRQN);																//Enable DMA interrupts again
		TransmitTxBuffer();																					//Transmit TX buffer


		osMutexRelease(com_MutexId);																		//Release shared resource
	}
	return Status;
}


//////////////////////////////////////////////////////////////////////
////////// RTOS TASK TO HANDLE RX/TX DATA ////////////////////////////
//////////////////////////////////////////////////////////////////////

static void HandleReceivedData(void){
	uint8_t tmp;
	unsigned i = 0, Id, ByteCnt, Data[8];
	uint8_t ByteData[8];

	while( ( ( tmp = com_RxBuffer[ (com_RxTail++) & COM_RXBUFTAILMASK ] ) != COM_ENDOFCMDCHAR ) ){
		if( i < ( COM_SINGLECMDBUFSIZE - 2 ) ){
			com_Cmd[i++] = tmp;																				//Copy byte by byte to command buffer until end of command byte is found or command buffer is full
		}
	}
	com_Cmd[i++] = COM_ENDOFCMDCHAR;
	com_Cmd[i] = 0;

	sscanf((char*)com_Cmd, "%03x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t\r\n", &Id, &ByteCnt,
					&(Data[0]), &(Data[1]), &(Data[2]), &(Data[3]), &(Data[4]), &(Data[5]), &(Data[6]), &(Data[7]));

	for( i=0; i<UTIL_SIZEOFARRAY(ByteData); i++){
		ByteData[i] = Data[i] & 0xFF;
	}
	can_SendMsg(Id, ByteData, ByteCnt);																		//Data received from car or diag are just forwarded to the other

	xb_SendF("<dde\t%d\t%03x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t>\r\n",					//Diag data event
				bl_GetTick(), Id, ByteCnt,
				Data[0], Data[1], Data[2], Data[3], Data[4], Data[5], Data[6], Data[7]);					//Format data and forward it via XBEE or bluetooth to PC
}


#define COM_SIG_DATARECEIVED	0x01
void com_ReceiveTask(void const *argument){
	for(;;){
		osSignalWait(COM_SIG_DATARECEIVED, osWaitForever);													//Wait until end of command char arrives
		osSignalClear(com_ReceiveTaskId, COM_SIG_DATARECEIVED);
		while( com_RxCmdCnt > com_HandledRxCmdCnt ){														//If there is more received than parsed commands
			com_HandledRxCmdCnt++;																			//Increment handled command counter
			HandleReceivedData();																			//Handle command
		}
	}
}


//////////////////////////////////////////////////////////////////////
////////// USART INTERRUPT ROUTINE ///////////////////////////////////
//////////////////////////////////////////////////////////////////////

void COM_USART_IRQH(void){
	if( __HAL_USART_GET_IT(&com_ComPort, UART_IT_CM ) ){													//If character match interrupt is pending
		__HAL_UART_CLEAR_IT(&com_ComPort, UART_CLEAR_CMF);													//Clear interrupt pending bit

		com_RxCmdCnt++;																						//Increment counter of received commands in buffer
		osSignalSet(com_ReceiveTaskId, COM_SIG_DATARECEIVED);												//Set signal to receiving task
	}
}


void COM_DMA_TX_IRQH(void){
	HAL_DMA_IRQHandler(com_ComPort.hdmatx);																	//Generic HAL DMA interrupt handler
}


//Tx transfer complete callback, that transmits all data from TxBuffer in linear blocks via dma
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
	if( huart == &com_ComPort ){
		TransmitTxBuffer();
	}
	if( huart == &xb_ComPort ){
		xb_TransmitTxBuffer();
	}
}
