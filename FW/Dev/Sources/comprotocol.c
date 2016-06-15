/*
*Filename: 	comprotocol.h
*Author: 	Ing. Stanislav Subrt
*Year:		2016
*/

/* DESCRIPTION

General com protocol info :
	Each command string is split to tokens with "space" and "\r" delimitters.

	Square brackets in command description mean one token, which must NOT include delimitters!

	Response from device may contain spaces or tabs (\t) interchangably (for some commands, typically measurement results, it makes sense to use \t to allow direct copy and pasting to excel)

	Each command starts with [CommandName], [CmdId] and then other parameters separated by "space" delimitter:
		[CommandName] [CmdId] ...\r\n

	[CmdId] is sent to device from PC and can be any number in range od 32bit signed integer ~ +-2 billions. When device recieves command,
	it answers with ack message with [DeviceTimestamp] and [CmdId].

	Each command is first executed (if found in list) and then acked(acknowledged).
		PC : i 12345\r\n
		DEV: <AeroscannerEngines R2 NA A>\r\n
		DEV: <ack 1592901 i 12345 0>

	If command is supposed to return a data, than data is first returned and than command is acked.

	Ack has following syntax:
		<ack [DeviceTimestamp] [CommandName] [CmdId] [util_ErrTd]>\r\n
			[DeviceTimestamp] is time [ms] counting from device start.
			[CommandName] is first token in command string.
			[CmdId] see above.
			[util_ErrTd] is error code returned while handling command. Negative value means error occured. Always check latest implementation in utility.h file.

	Events:

*/


#include "comprotocol.h"
#include "main.h"
#include "can.h"


//////////////////////////////////////////////////////////////////////
////////// CALLBACKS /////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

//Returns device info.
static util_ErrTd InfoCB(char *cmdName, int cmdId){
	xb_SendF("<%s %d %s>\r\n", cmdName, cmdId, VERSIONSTRING);
	return util_ErrTd_Ok;
}


//Returns device info.
static util_ErrTd RestartCB(char *cmdName, int cmdId){
	int32_t i;

	i = atoi(strtok(0, " \r"));												//Read restart required flag
	if( i ){
		xb_SendF("<ack %d %s %d>\r\n", bl_GetTick(), cmdName, cmdId, 0);	//Fake ack since this function never returns
		for( i=0; i< 720000; i++);
		NVIC_SystemReset();
	}
	return util_ErrTd_Ok;
}


static util_ErrTd SendCarCB(char *cmdName, int cmdId){
	uint32_t i, Id, ByteCnt;
	uint8_t Data[8];

	Id = strtol(strtok(0, " \r"), 0, 16);
	ByteCnt = strtol(strtok(0, " \r"), 0, 16);
	for( i=0; i<UTIL_SIZEOFARRAY(Data); i++){
		Data[i] = strtol(strtok(0, " \r"), 0, 16);
	}
	return can_SendMsg(Id, Data, ByteCnt);
}



static util_ErrTd SendDiagCb(char *cmdName, int cmdId){
	uint32_t i, Id, ByteCnt;
	uint8_t Data[8];

	Id = strtol(strtok(0, " \r"), 0, 16);
	ByteCnt = strtol(strtok(0, " \r"), 0, 16);
	for( i=0; i<UTIL_SIZEOFARRAY(Data); i++){
		Data[i] = strtol(strtok(0, " \r"), 0, 16);
	}

	return com_SendF("%03x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t\r\n",
				Id, ByteCnt,
				Data[0], Data[1], Data[2], Data[3], Data[4], Data[5], Data[6], Data[7]);		//Format data and forward it via usart to another MCU that will transmit it again via can
}


//////////////////////////////////////////////////////////////////////
////////// COMMAND LIST //////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

typedef struct{
	char* CmdName;
	util_ErrTd (*CallbackFn)(char *cmdName, int cmdId);
}cp_CommandTd;


/*
Following table shows command list of device with syntax and answers and events.
Parameter data types are signed 32-bit integer by default or specified if otherwise.
*/
static cp_CommandTd CmdList[] = {
	{"i", InfoCB},						//i [CmdId]\r\n
											//Requests device info
											//Answer: <[CommandName] [CmdId] [CompanyName] [DeviceName] [SerialNumber] [HWVersion] [FWVersion]>\r\n

	{"rr", RestartCB},					//rr [CmdId] [IsRestartRequired]\r\n

	{"sc", SendCarCB},					//sc [CmdId] [MsgId] [ByteCnt] [Byte0_0x00] [Byte1_0x00] [Byte2_0x00] [Byte3_0x00] [Byte4_0x00] [Byte5_0x00] [Byte6_0x00] [Byte7_0x00]\r\n
											//Send can message to car

	{"sd", SendDiagCb},					//sd [CmdId] [MsgId] [ByteCnt] [Byte0_0x00] [Byte1_0x00] [Byte2_0x00] [Byte3_0x00] [Byte4_0x00] [Byte5_0x00] [Byte6_0x00] [Byte7_0x00]\r\n
											//Send can message to diagnostics
};


//////////////////////////////////////////////////////////////////////
////////// PARSER ////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void cp_HandleCmd(char *str){
	unsigned i;
	char *CmdName;
	int CmdId = 0, ErrNo = util_ErrTd_NotExisting;

	CmdName = strtok(str, " \r");																		//Get command name
	CmdId = atoi(strtok(0, " \r"));																		//Get command ID
	for( i=0; i<UTIL_SIZEOFARRAY(CmdList); i++ ){														//Repeat through whole list of defined commands
		if( strcmp(CmdList[i].CmdName, CmdName ) == 0 ){												//If command name from defined list is equal to current command name
			if( CmdList[i].CallbackFn != 0 ){
				ErrNo = CmdList[i].CallbackFn(CmdName, CmdId);											//Call callback function to parse remaining data
			}
			break;																						//Break lookup cycle if command has been found
		}
	}																									//Command not found in list
	xb_SendF("<ack %d %s %d %d>\r\n", bl_GetTick(), CmdName, CmdId, ErrNo);								//Send it
}
