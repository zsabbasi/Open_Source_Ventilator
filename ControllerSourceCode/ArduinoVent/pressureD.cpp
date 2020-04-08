

#include <Arduino.h>
#include "pressureD.h"
#include <math.h> //M_PI
#include "config.h"
//#include <stdio.h>
//float vals[2];
//std::String valstr;
//char * vals=new char[30];
#define PTFCC               63.639
#define MF                  1.0
#define VSOURCE             5.0

#define AIR_DENSITY 		1.225 //kgm/m^3

float mapfloat(long x, long in_min, long in_max, long out_min, long out_max)
{
	return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}

float getFlowRate()
{
	float kpa = getPsi(DIFF_PRESSURE_SENSOR_PIN); //for nano A9 FOR MEGA
	
	float p = fabs(kpa * 1000);
	/*
	float correction = 0;
	if (delP < 0)
    	correction = -1.0;
  	else
    	correction = 1.0;

	//
	fluidRate = MF * PTFCC * sqrt(kpa * correction);
	*/
	float fluidRate = sqrt(p * 2 / AIR_DENSITY) * (PI * 0.0127 * 0.0127);

	/*https://sciencing.com/convert-differential-pressure-flow-7994015.html
	fluidvelocity=sqrt(differentialpressure*2 /fluidDensity )
	flowrate=fluidvelocity*area of pipe;(surface area pi* r * r)

	0.00635;//for half inch pipe

	*/
	float flowRate = fluidRate * 60000; //m^3/s --> litre/minute
	Serial.print("flow ");
	Serial.print(kpa);
	Serial.print(", ");
	Serial.println(flowRate);
	return kpa;
}

float getPsi(int p)
{

	int val = analogRead(p);
	float volt = val * 0.004887586;

	//float kpa = (0.21 - 0.2) / 4.5 * 700.0;
	float kpa = (5.0 * volt / VSOURCE) - 2.5;

	Serial.print("volt ");
	Serial.println(volt);
	return volt;
}