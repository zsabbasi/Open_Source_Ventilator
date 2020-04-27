
/*************************************************************
 * Open Ventilator
 * Copyright (C) 2020 - Dr. Bill Schmidt-J68HZ, Jack Purdum and Marcelo Varanda
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 **************************************************************
*/

/*
 * History:
 *
  Release 1.01  April 1, 2020, Jack Purdum, W8TEE. Adjusted mapping to 5V from 3V
    Release 1.0   March 30, 2020 by Dr. Bill Schmidt-J68HZ.  This program ws written to interface a MP3V7010xx
                  gauge pressure sensor and will calculate instantaneous average pressures for the
                  Inhale or exhale cycle.  See comments at end for integrating variables to be displayed into the LCD.

LUA code:

int to InchH2O:

function getp(a)
  local p = 4.01463
  local d = 614
  local r = p * ((a / d) - 0.08) / 0.09
  return r
end

InchH2O to int
function geta(v)
  local p = 4.01463
  local d = 614
  local r = (((v * .09) / p) + 0.08 ) * d
  return r
end
*/

#include "pressure.h"
#include "log.h"
#include "hal.h"
#include "config.h"
#include "bmp280_int.h"
#include "toyotaMafSensor.h"
#include <stdint.h>

#ifdef VENTSIM
#include <QRandomGenerator>
#define TEST_RAND_MIN 300
#define TEST_RAND_MAX 400
#endif

#if ((USE_Mpxv7002DP_PRESSURE_SENSOR == 1) || (USE_Mpxv7002DP_FLOW_SENSOR == 1) || (USE_CAR_FLOW_SENSOR == 1))

//#define SHOW_VAL

#define TM_LOG 2000

static float accumulator[NUM_P_SENSORS];
static float binCounts[NUM_P_SENSORS];
static float peaks[NUM_P_SENSORS];
static float last[NUM_P_SENSORS];

static uint64_t pressureTimer;

static float volumeInCurrentCycle = 0;
static uint16_t tidalVolume = 0;

static float av[NUM_P_SENSORS];
static uint64_t volumeStartTimerRef;

#ifdef SHOW_VAL
static uint64_t tm_log;
#endif

/* keeps a moving average of pressure values */
void CalculateAveragePressure(pressureSensor_t sensor) {
    int i;
    float rawSensorValue;

    for (i = 0; i < NUM_P_SENSORS; i++) {
        rawSensorValue = 0;
        if (i == PRESSURE) {
#if (USE_Mpxv7002DP_PRESSURE_SENSOR == 1)
            /*****************************************
             *
             *      Analog NXP Mpxv7002DP pressure sensor
             *
             *****************************************/
            rawSensorValue = halGetAnalogPressure();

#elif (USE_BMP280_PRESSURE_SENSOR == 1)
            /*****************************************
             *
             *      BOSH BMP280 pressure sensor
             *
             *****************************************/
            bpm280GetPressure();
            rawSensorValue = getCmH2OGauge();

#else
#warning "No pressure sensor defined in config.h"
#endif

        } // if i == 0
        else {
#if (USE_Mpxv7002DP_FLOW_SENSOR == 1)
            /*****************************************
             *
             *      Analog NXP Mpxv7002DP pressure sensor
             *
             *****************************************/
            rawSensorValue = halGetAnalogFlow();
#elif (USE_CAR_FLOW_SENSOR == 1)
            /*****************************************
             *
             *      Analog NXP Mpxv7002DP pressure sensor
             *
             *****************************************/
            rawSensorValue = getFlowRate();
#endif
        }

        last[i] = rawSensorValue;
        // clamp it to the max (max value provided by the sensor)
        if (rawSensorValue >= peaks[i])
            peaks[i] = rawSensorValue;

        accumulator[i] = accumulator[i] + rawSensorValue;
        binCounts[i] = binCounts[i] + 1;

        av[i] = accumulator[i] / binCounts[i];

        //Reset average to the first bin if already crossed the limit
        if (binCounts[i] >= AVERAGE_BIN_NUMBER) {
            accumulator[i] = rawSensorValue;
            binCounts[i] = 1;
            peaks[i] = rawSensorValue;
        }

        if (i == FLOW) {
            uint64_t timerNow = halStartTimerRef();
            float slotVolume = ((av[i] * 100.0) * (timerNow - volumeStartTimerRef));
            volumeInCurrentCycle = volumeInCurrentCycle + slotVolume;
            // Serial.println(volumeInCurrentCycle);
            // Serial.println(slotVolume);
            volumeStartTimerRef = timerNow;
        }
    } // for loop
}

/* initialize all pressure variables */
void pressInit() {
#ifndef VENTSIM
#if (USE_Mpxv7002DP_PRESSURE_SENSOR == 1 || USE_CAR_FLOW_SENSOR == 1)
    analogReference(DEFAULT); // Arduino function
#endif
#endif

#if (USE_BMP280_PRESSURE_SENSOR == 1)
    bpm280Init();
#endif

    pressureTimer = halStartTimerRef();

#ifdef SHOW_VAL
    tm_log = pressureTimer;
#endif
}

/* clears variables for tidal volume calculation */
void startTidalVolumeCalculation() {
    volumeInCurrentCycle = 0.0;
    tidalVolume = 0;
    volumeStartTimerRef = halStartTimerRef();
}

/* calculate tidal volume */
void endTidalVolumeCalculation() {
    tidalVolume = volumeInCurrentCycle * (1000 /* litres to cm3*/) /
                  (60000 * 100); // litres per min instead of per milliseconds
    volumeInCurrentCycle = 0;
}

/* main pressure loop */
void pressLoop() {
    if (halCheckTimerExpired(pressureTimer, PRESSURE_READ_DELAY)) {
        CalculateAveragePressure(PRESSURE);
        pressureTimer = halStartTimerRef();
    }

#ifdef SHOW_VAL
    char buf[24];
    if (halCheckTimerExpired(tm_log, TM_LOG))
    {
      LOGV("av = %d", av);
#ifndef VENTSIM
      dtostrf(cmH2O[PRESSURE], 8, 2, buf);
      LOGV("Pa = %s\n", buf);
#else
      LOGV("Pa = %f\n", cmH2O[PRESSURE]);
#endif
      tm_log = halStartTimerRef();
    }
#endif
}

/* getter method for pressure */
float pressGetVal(pressureSensor_t sensor) {
    return last[sensor];
}

/* getter method for tidal volume */
uint16_t pressGetTidalVolume() {
    return tidalVolume;
}

#else
// Stubbs
void pressInit() {}
void pressLoop() {}
float pressGetFloatVal(pressureSensor_t sensor) { return 0.0; }

#endif //#if ( (USE_Mpxv7002DP_PRESSURE_SENSOR == 1) || (USE_Mpxv7002DP_FLOW_SENSOR == 1) )
