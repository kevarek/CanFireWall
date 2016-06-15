/*
*Filename: utility.h
*Author: Ing. Stanislav Subrt
*Year: 2015
*/


#ifndef UTILITY_H
#define UTILITY_H

#define UTIL_ISNAN(x)			        ( (x) != (x) )
#define UTIL_SIZEOFARRAY(x)		        ( sizeof( (x) ) / sizeof( (x[0]) ) )

#define UTIL_VALINRANGE(val,min,max)    ( ( val >= (min) ) && ( val <= (max) ) )


typedef enum{
	util_LogicTd_Normal = 0,
	util_LogicTd_Inverted = 1,
}util_LogicTd;


typedef enum{
	util_StateTd_NotImplemented = -2,
	util_StateTd_NotInitialized = -1,
	util_StateTd_Off = 0,
	util_StateTd_On = 1,
}util_StateTd;


typedef enum{
	util_OutModeTd_NotImplemented = -1,
	util_OutModeTd_None = 0,
	util_OutModeTd_Bin = 1,
	util_OutModeTd_PWM = 2,
	util_OutModeTd_PWMDE = 3,
	util_OutModeTd_PDE = 4,
	util_OutModeTd_Analog = 5,
}util_OutModeTd;


typedef enum{
	util_ErrTd_Ok = 0,
	util_ErrTd_Err = -1,
	util_ErrTd_InvalidParam = -2,
	util_ErrTd_Ack = -3,
	util_ErrTd_Nack = -4,
	util_ErrTd_Uninitialized = -5,
	util_ErrTd_Busy = -6,
	util_ErrTd_Timeout = -7,
	util_ErrTd_Saturation = -8,
	util_ErrTd_OutOfRange = -9,
	util_ErrTd_NotExisting = -10,
	util_ErrTd_Null = -11,
	util_ErrTd_NotImplemented = -12,
	util_ErrTd_NotFound = -13,				//RESERVED only for purpose of command parsing
	util_ErrTd_NotSupported = -14,
	util_ErrTd_NotValid = -15,
	util_ErrTd_Overflow = -16,
}util_ErrTd;


typedef enum{
	util_PidAntiWindupModeTd_None = 0,
	util_PidAntiWindupModeTd_Sat = 1,
	util_PidAntiWindupModeTd_ILim = 2,
	util_PidAntiWindupModeTd_OutLimSat = 3,
}util_PidAntiWindupModeTd;

typedef struct{
	float FF;
	float P;
	float I;
	float D;
	util_PidAntiWindupModeTd AntiWindupMode;
	float ModeLim[2];
	float Integral;
	float PrevErr;

}util_PidTd;


extern inline float util_DegToRad(float deg);
extern inline float util_RadToDeg(float rad);

extern inline float util_ExpMovAvg(float alpha, float prevVal, float newVal);

extern inline float util_UpdatePid(util_PidTd *h, float desired, float actual, float valToFF);
extern inline void util_InitPid(util_PidTd *h, float ff, float pp, float ii, float dd, util_PidAntiWindupModeTd m, float modeLimMin, float modeLimMax);
extern inline void util_ResetPid(util_PidTd *h);

#endif  //end of UTILITY_H
