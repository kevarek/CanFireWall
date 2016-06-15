/*
*Filename: xbee.c
*Author: Ing. Stanislav Subrt
*Year: 2015
*/

#include "main.h"
#include "comprotocol.h"
#include <stdarg.h>

//////////////////////////////////////////////////////////////////////
////////// HARDWARE ABSTRACTION LAYER ////////////////////////////////
//////////////////////////////////////////////////////////////////////

#define XB_PORT_INST     				GPIOB
#define XB_PORT_ENPERCLK				__GPIOB_CLK_ENABLE
#define XB_PIN_TX    					GPIO_PIN_6
#define XB_PIN_RX    					GPIO_PIN_7
#define XB_PORT_AF						GPIO_AF7_USART1

#define XB_USART_INST					USART1
#define XB_USART_ENPERCLK				__USART1_CLK_ENABLE
#define XB_USART_BAUDRATE				115200
#define XB_USART_IRQN					USART1_IRQn
#define XB_USART_IRQH					USART1_IRQHandler

#define XB_DMA_INST						DMA1
#define XB_DMA_ENPERCLK					__DMA1_CLK_ENABLE

#define XB_DMA_TX						DMA1_Channel4
#define XB_DMA_TX_IRQN					DMA1_Channel4_IRQn
#define XB_DMA_TX_IRQH					DMA1_Channel4_IRQHandler

#define XB_DMA_RX						DMA1_Channel5
#define XB_DMA_RX_IRQN					DMA1_Channel5_IRQn
#define XB_DMA_RX_IRQH					DMA1_Channel5_IRQHandler


#define XB_RXBUFSIZE					1024																//Must be power of two because of some math magic
#define XB_RXBUFTAILMASK				( XB_RXBUFSIZE - 1 )
#define XB_SINGLECMDBUFSIZE				512
#define XB_ENDOFCMDCHAR					'\n'

#define XB_TXBUFSIZE					1024																//Must be power of two because of some math magic
#define XB_TXBUFHEADMASK				( XB_TXBUFSIZE - 1 )



static uint8_t	xb_RxBuffer[XB_RXBUFSIZE];
static uint32_t xb_RxTail = 0;
static int32_t xb_RxCmdCnt = 0;
static int32_t xb_HandledRxCmdCnt = 0;

static uint8_t xb_Cmd[XB_SINGLECMDBUFSIZE];

static uint8_t xb_TxBuffer[XB_TXBUFSIZE];
static uint32_t xb_TxHead = 0;
static uint32_t xb_TxTail = 0;

static char xb_TmpStr[XB_SINGLECMDBUFSIZE];

UART_HandleTypeDef xb_ComPort = {0};
static DMA_HandleTypeDef xb_hDMA_TX = {0};

osMutexId xb_MutexId;
osMutexDef (xb_Mutex);
osStatus xb_MutexStatus;


//////////////////////////////////////////////////////////////////////
////////// INITIALIZATION ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

//Initializes communication peripheral and buffers
void xb_Init(void){
	GPIO_InitTypeDef GPIO_InitStructure = {0};
	DMA_HandleTypeDef hDMA_RX = {0};

	//Configure GPIO pins for UART
	XB_PORT_ENPERCLK();
	GPIO_InitStructure.Pin = XB_PIN_TX | XB_PIN_RX;
	GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.Alternate = XB_PORT_AF;
	HAL_GPIO_Init(XB_PORT_INST, &GPIO_InitStructure);

	//Configure USART peripheral
	XB_USART_INST->CR2 = 0xFF000000;																		//I wish I could remember what this line was supposed to do then ... :)
	XB_USART_ENPERCLK();
	xb_ComPort.Instance = XB_USART_INST;
	xb_ComPort.Init.BaudRate = XB_USART_BAUDRATE;
	xb_ComPort.Init.WordLength = UART_WORDLENGTH_8B;
	xb_ComPort.Init.StopBits = UART_STOPBITS_1;
	xb_ComPort.Init.Parity = UART_PARITY_NONE;
	xb_ComPort.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	xb_ComPort.Init.Mode = UART_MODE_TX_RX;
	HAL_UART_Init(&xb_ComPort);

	//Set character match and disable error interrupts (this can be done only while Receiver is enabled and USART is disabled (RE=1, UE=0)
	__HAL_UART_DISABLE(&xb_ComPort);
	//__HAL_UART_DISABLE_IT(&ComPort, UART_IT_ERR);
	MODIFY_REG(XB_USART_INST->CR3, USART_CR3_OVRDIS, USART_CR3_OVRDIS);
	MODIFY_REG(XB_USART_INST->CR2, USART_CR2_ADD, ((unsigned)XB_ENDOFCMDCHAR << UART_CR2_ADDRESS_LSB_POS));
	__HAL_UART_ENABLE(&xb_ComPort);

	//Configure DMA for TX and RX
	XB_DMA_ENPERCLK();
	xb_hDMA_TX.Instance = XB_DMA_TX;
	xb_hDMA_TX.Init.Direction = DMA_MEMORY_TO_PERIPH;
	xb_hDMA_TX.Init.PeriphInc = DMA_PINC_DISABLE;
	xb_hDMA_TX.Init.MemInc = DMA_MINC_ENABLE;
	xb_hDMA_TX.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
	xb_hDMA_TX.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
	xb_hDMA_TX.Init.Mode = DMA_NORMAL;//DMA_CIRCULAR;
	xb_hDMA_TX.Init.Priority = DMA_PRIORITY_MEDIUM;
	HAL_DMA_Init(&xb_hDMA_TX);
	__HAL_LINKDMA(&xb_ComPort, hdmatx, xb_hDMA_TX);
	HAL_NVIC_SetPriority(XB_DMA_TX_IRQN, 3, 1);
	HAL_NVIC_EnableIRQ(XB_DMA_TX_IRQN);

	hDMA_RX.Instance = XB_DMA_RX;
	hDMA_RX.Init.Direction = DMA_PERIPH_TO_MEMORY;
	hDMA_RX.Init.PeriphInc = DMA_PINC_DISABLE;
	hDMA_RX.Init.MemInc = DMA_MINC_ENABLE;
	hDMA_RX.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
	hDMA_RX.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
	hDMA_RX.Init.Mode = DMA_CIRCULAR;
	hDMA_RX.Init.Priority = DMA_PRIORITY_MEDIUM;
	HAL_DMA_Init(&hDMA_RX);
	__HAL_LINKDMA(&xb_ComPort, hdmarx, hDMA_RX);

	//Enable USART interrupts
	HAL_NVIC_SetPriority(XB_USART_IRQN, 3, 3);
	HAL_NVIC_EnableIRQ(XB_USART_IRQN);
	__HAL_UART_ENABLE_IT(&xb_ComPort, UART_IT_CM);

	//Starts port reading
	HAL_UART_Receive_DMA(&xb_ComPort, xb_RxBuffer, XB_RXBUFSIZE);

	xb_MutexId = osMutexCreate(osMutex(xb_Mutex));
}


//Transmits data from Tx circular buffer either in one part if data is linear or in two parts if data is broken by circular buffers end.
void xb_TransmitTxBuffer(void){

	HAL_NVIC_DisableIRQ(XB_DMA_TX_IRQN);																	//DMA interrupt must be disabled - if transfer is complete immediately (one byte only, DMA transmit function invokes interrupt before tail position is updated)
	if( xb_TxHead != xb_TxTail ){																			//If there is data to transfer
		if( xb_TxHead > xb_TxTail ){																		//If all data in circular buffer is linear
			if( HAL_UART_Transmit_DMA(&xb_ComPort, xb_TxBuffer + xb_TxTail , xb_TxHead - xb_TxTail) == HAL_OK ){
				xb_TxTail = xb_TxHead;																		//Transmit all data and move tail to head position
			}
		}
		else if( xb_TxHead < xb_TxTail ){																	//If data is separated by circular buffer end
			if( HAL_UART_Transmit_DMA(&xb_ComPort, xb_TxBuffer + xb_TxTail , XB_TXBUFSIZE - xb_TxTail) == HAL_OK ){
				xb_TxTail = 0;																				//Transmit first end of buffer data part and then linear part from beginning of circular buffer
			}
		}
	}

	if( xb_TxTail >= XB_TXBUFSIZE ){
		xb_TxTail = 0;
	}
	if( xb_TxHead >= XB_TXBUFSIZE ){
		xb_TxHead = 0;
	}

	HAL_NVIC_EnableIRQ(XB_DMA_TX_IRQN);
}


//Send data in specified format. Same syntax as printf
util_ErrTd xb_SendF(char *format, ...){
	util_ErrTd Status = util_ErrTd_Ok;
	va_list va;
	int32_t i;


	if( osMutexWait(xb_MutexId, 100) == osOK ){																//Wait for shared resource (tx buffer access)
		HAL_NVIC_DisableIRQ(XB_DMA_TX_IRQN);																//Disable DMA interrupts (TXC could acces TxBuffer in the middle of writing data to it

		va_start(va, format);																				//Start reading of parameters
		vsnprintf(xb_TmpStr, sizeof(xb_TmpStr), format, va);												//Format new string and save its length
		va_end(va);																							//End of reading parameters

	//	if( Cnt <= 0 || Cnt >= sizeof(com_TmpStr) ){						//If formatted string doesnt fit into buffer
	//		snprintf(com_TmpStr, sizeof(com_TmpStr), "<erre %d %d>\r\n", (int)Cnt, (int)sizeof(com_TmpStr));	//Format error message instead
	//		Status = util_ErrTd_Overflow;
	//	}

		for( i=0; i<strlen(xb_TmpStr); i++ ){																//Copy byte by byte into tx buffer
			xb_TxBuffer[ xb_TxHead++ ] = xb_TmpStr[i];
			xb_TxHead &= XB_TXBUFHEADMASK;
		}

		HAL_NVIC_EnableIRQ(XB_DMA_TX_IRQN);																	//Enable DMA interrupts again
		xb_TransmitTxBuffer();																				//Transmit TX buffer

		osMutexRelease(xb_MutexId);																			//Release shared resource
	}
	return Status;
}


//////////////////////////////////////////////////////////////////////
////////// RTOS TASK TO HANDLE RX/TX DATA ////////////////////////////
//////////////////////////////////////////////////////////////////////

static void HandleReceivedData(void){
	uint8_t tmp;
	unsigned i = 0;
	while( ( ( tmp = xb_RxBuffer[ (xb_RxTail++) & XB_RXBUFTAILMASK ] ) != XB_ENDOFCMDCHAR ) ){
		if( i < ( XB_SINGLECMDBUFSIZE - 2 ) ){
			xb_Cmd[i++] = tmp;																				//Copy byte by byte to command buffer until end of command byte is found or command buffer is full
		}
	}
	xb_Cmd[i++] = XB_ENDOFCMDCHAR;
	xb_Cmd[i] = 0;

	cp_HandleCmd((char*)xb_Cmd);

}


#define XB_SIG_DATARECEIVED	0x01
void xb_ReceiveTask(void const *argument){
	for(;;){
		osSignalWait(XB_SIG_DATARECEIVED, osWaitForever);													//Wait until end of command char arrives
		osSignalClear(xb_ReceiveTaskId, XB_SIG_DATARECEIVED);
		while( xb_RxCmdCnt > xb_HandledRxCmdCnt ){															//If there is more received than parsed commands
			xb_HandledRxCmdCnt++;																			//Increment handled command counter
			HandleReceivedData();																			//Handle command
		}
	}
}


//////////////////////////////////////////////////////////////////////
////////// USART INTERRUPT ROUTINE ///////////////////////////////////
//////////////////////////////////////////////////////////////////////

void XB_USART_IRQH(void){
	if( __HAL_USART_GET_IT(&xb_ComPort, UART_IT_CM ) ){														//If character match interrupt is pending
		__HAL_UART_CLEAR_IT(&xb_ComPort, UART_CLEAR_CMF);													//Clear interrupt pending bit

		xb_RxCmdCnt++;																						//Increment counter of received commands in buffer
		osSignalSet(xb_ReceiveTaskId, XB_SIG_DATARECEIVED);													//Set signal to receiving task
	}
}


void XB_DMA_TX_IRQH(void){
	HAL_DMA_IRQHandler(xb_ComPort.hdmatx);																	//Generic HAL DMA interrupt handler
}
