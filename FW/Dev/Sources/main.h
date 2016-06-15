/*
*Filename: main.h
*Author: Ing. Stanislav Subrt
*Year: 2016
*/


#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "stm32f3xx.h"
#include "cmsis_os.h"

#include "utility.h"
#include "blinky.h"
#include "com.h"
#include "xbee.h"



/*
	HW----------------------------------
		1HW0 - prototype from spare parts glued together



	FW---------------------------------
		1FW0 - Initial firmware


*/


#define VERSIONSTRING	"Kevarek CanFireWall 000 1HW0 1FW0"					//Company name, device name, device ID, HW version, FW version, reserved, reserved reserved

#endif  //end of MAIN_H
