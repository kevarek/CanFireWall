/*
*Filename: utility.c
*Author: Ing. Stanislav Subrt
*Year: 2015
*/

#include "main.h"
#include "utility.h"


//Converts value in degrees to radians
inline float util_DegToRad(float deg){
	return deg * 0.0174532925;		//2*PI*1RADIAN = 360deg
}


//Converts value in radians to degrees
inline float util_RadToDeg(float rad){
	return rad * 57.2957795;		//2*PI*1RADIAN = 360deg
}


//Calculates exponential moving average
//Output = (1-Alpha) * value + Alpha * PrevValue;
//Tau ... time constant, T ... sampling period
//Alpha = exp(-T/Tau);
//Tau = -T/ln(Alpha); T = -Tau * ln(Alpha);
inline float util_ExpMovAvg(float alpha, float prevVal, float newVal){
	return (1-alpha) * newVal + alpha * prevVal;
}


//////////////////////////////////////////////////////////////////////
////////// PID REGULATOR /////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

//Calculates action value coming from PID algorithm with simple proportional feed forwarding.
inline float util_UpdatePid(util_PidTd *h, float desired, float actual, float valToFF){
	float Err, Dif, Integral, Output;

	if( h == 0 ){
		return 0;
	}

	Err = desired - actual;
	Dif = Err - h->PrevErr;
	Integral = h->Integral + Err;


	switch( h->AntiWindupMode ){
		case util_PidAntiWindupModeTd_Sat:																	//Saturation antiwindup technique prevents integral updates when output is saturated
			Output = h->FF*valToFF + h->P*Err + h->I*Integral + h->D*Dif;									//Calculate output value
			if( Output >= h->ModeLim[0] && Output <= h->ModeLim[1] ){
				h->Integral = Integral;																		//Update integral if output is not saturated
			}
			h->PrevErr = Err;																				//Update previous error value for next iteration
		break;

		case util_PidAntiWindupModeTd_ILim:																	//Integral limitation antiwindup technique limits integral value
			if( Integral < h->ModeLim[0] ){																	//Check and correct integral according to limits
				Integral = h->ModeLim[0];
			}
			else if( Integral > h->ModeLim[1] ){
				Integral = h->ModeLim[1];
			}
			Output = h->FF*valToFF + h->P*Err + h->I*Integral + h->D*Dif;									//Calculate output value
			h->PrevErr = Err;																				//Update previous error value for next iteration
			h->Integral = Integral;
		break;

		case util_PidAntiWindupModeTd_OutLimSat:
			Output = h->FF*valToFF + h->P*Err + h->I*Integral + h->D*Dif;									//Calculate output value
			if( Output < h->ModeLim[0] ){																	//Check and correct integral according to limits
				Output = h->ModeLim[0];
			}
			else if( Output > h->ModeLim[1] ){
				Output = h->ModeLim[1];
			}
			else{
				h->Integral = Integral;																		//Update integral only if output is not saturated
			}
			h->PrevErr = Err;																				//Update previous error value for next iteration
		break;

		default:
			Output = h->FF*valToFF + h->P*Err + h->I*Integral + h->D*Dif;
			h->PrevErr = Err;																				//Update previous error value for next iteration
			h->Integral = Integral;
		break;
	}
	return Output;
}


//Initialize PID regulator structure
inline void util_InitPid(util_PidTd *h, float ff, float pp, float ii, float dd, util_PidAntiWindupModeTd m, float modeLimMin, float modeLimMax){
	if( h == 0 ){
		return;
	}

	h->FF = ff;
	h->P = pp;
	h->I = ii;
	h->D = dd;
	h->AntiWindupMode = m;
	h->ModeLim[0] = modeLimMin;
	h->ModeLim[1] = modeLimMax;

	h->Integral = 0;
	h->PrevErr = 0;
}


inline void util_ResetPid(util_PidTd *h){
	if( h == 0 ){
		return;
	}

	h->Integral = 0;
	h->PrevErr = 0;
}


//////////////////////////////////////////////////////////////////////
////////// DYNAMIC MEMORY ALLOCATION /////////////////////////////////
//////////////////////////////////////////////////////////////////////

//This is sbrk implementation which stops hardfault each time when sprintf with float param is called
caddr_t _sbrk(int incr){
	extern char end asm("end");
	static char *heap_end;
	char *prev_heap_end;

	if (heap_end == 0){
		heap_end = &end;
	}
	prev_heap_end = heap_end;
	heap_end += incr;
	return (caddr_t)prev_heap_end;
}

