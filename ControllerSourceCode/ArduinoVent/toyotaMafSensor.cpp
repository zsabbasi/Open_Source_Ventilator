

#include <Arduino.h>
#include "toyotaMafSensor.h"
#include <math.h> //M_PI
#include "config.h"
#define PTFCC               63.639
#define MF                  1.0
#define VSOURCE             5.0

#define AIR_DENSITY 		1.225 //kgm/m^3
#define USE_ANALOG_FLOW_SENSOR

float mapfloat(long x, long in_min, long in_max, long out_min, long out_max)
{
	return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}

#ifdef USE_CAR_FLOW_SENSOR
float getFlowRate()
{
	int val = analogRead(FLOW_SENSOR_PIN);
	float volt = val * 0.004887586 * 1000; //Calibrated to mV
	
	return (FLOW_RELATION_INTERCEPT-volt)/FLOW_RELATION_SLOPE ;
}
#endif