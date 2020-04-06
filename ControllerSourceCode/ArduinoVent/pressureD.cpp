

#include <Arduino.h>
#include "pressureD.h"
#include <math.h> //M_PI
//#include <stdio.h>
//float vals[2];
//std::String valstr;
//char * vals=new char[30];
float airDensity = 1.225; //kgm/m^3
float mapfloat(long x, long in_min, long in_max, long out_min, long out_max)
{
	return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}

float getFlowRate()
{
	float kpa = getPsi(A7); //for nano A9 FOR MEGA
	float p = fabs(kpa * 1000);
	float fluidRate;
	fluidRate = (sqrt(p * 2 / airDensity)) * (M_PI * 0.00635 * 0.00635);
	/*https://sciencing.com/convert-differential-pressure-flow-7994015.html
	fluidvelocity=sqrt(differentialpressure*2 /fluidDensity )
	flowrate=fluidvelocity*area of pipe;(surface area pi* r * r)

	0.00635;//for half inch pipe

	*/
	return fluidRate * 60000; //m^3/s --> litre/minute
}

float getPsi(int p)
{

	int val = analogRead(p);
	float volt = val * 0.004887586;

	float psi = 0.0;

	float kpa = (volt - 0.2) / 4.5 * 700.0;

	return kpa;
}