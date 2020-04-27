
/*************************************************************
 * Open Ventilator
 * Copyright (C) 2020 - Marcelo Varanda
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
#include "config.h"

#ifndef STEPPER_MOTOR_STEP_PIN // compile this file only if not using Motor Stepper (squeezer)

#include "breather.h"
#include "properties.h"
#include "bmp280_int.h"
#include "hal.h"
#include "log.h"
#include "pressure.h"
#include "event.h"
#include "alarm.h"
#include "serialWriter.h"

#define MINUTE_MILLI 60000
#define TIME_WAIT_TO_OUT 200 //200 milliseconds
#define TIME_STOPPING 4000 // 4 seconds to stop

#define TIME_FAST_CALIBRATION 4000 // 4 seconds
#define TIME_DATA_LOG_DELAY 500 // 500 milliseconds

static int currPause;
static int currRate;
static int currInMilli;
static int currOutMilli;
static int currTotalCycleMilli;
static int currProgress;
static uint64_t timerStart;
static uint64_t timerSerialLog;
static int16_t highPressure;
static int16_t lowPressure;
static int16_t highTidalVolume;
static int16_t lowTidalVolume;
static int16_t tidalVolume;
static int8_t desiredPeep;

static bool fastCalibration;

static const int rate[4] = {1,2,3,4} ;

static breatherState_t breatherState = BREATHER_STATE_STOPPED;

/* set flag for fast calibration */
void breatherRequestFastCalibration() {
    if (propGetVent() == 0) {
        LOG("Ignore Fast Calib.");
        return;
    }
    fastCalibration = true;
    CEvent::post(EVENT_ALARM, ALARM_INDEX_FAST_CALIB_TO_START);
}

/* getter method for currProgress property */
int breatherGetProgress() {
    return currProgress;
}

/* initialize all variables to start breather cycle */
void breatherStartCycle() {
    currTotalCycleMilli = MINUTE_MILLI / propGetBpm();
    currPause = propGetPause();
    currRate = propGetDutyCycle();
    int inOutTime = currTotalCycleMilli - (currRate + TIME_WAIT_TO_OUT);
    currInMilli = (inOutTime / 2) / rate[currRate];
    currOutMilli = inOutTime - currInMilli;
    currProgress = 0;
    timerStart = halStartTimerRef();
    timerSerialLog = halStartTimerRef();
    breatherState = BREATHER_STATE_IN;
    halValveOutClose();
    halValveInOpen();
    fastCalibration = false;
    startTidalVolumeCalculation();
    highPressure = propGetHighPressure();
    lowPressure = propGetLowPressure();
    highTidalVolume = propGetHighTidal();
    lowTidalVolume = propGetLowTidal();
    desiredPeep = propGetDesiredPeep();

#if 0
    LOG("Ventilation ON:");
    LOGV(" currTotalCycleMilli = %d", currTotalCycleMilli);
    LOGV(" currPause = %d", currPause);
    LOGV(" currInMilli = %d", currInMilli);
    LOGV(" currOutMilli = %d", currOutMilli);
#endif
}

/* getter method for breatherState property */
breatherState_t breatherGetState() {
    return breatherState;
}

/* start breather cycle if ventilator != 0
 * ??? what does ventilator == 0 mean ??? */
static void fsmStopped() {
    if (propGetVent()) {
        //breatherStartCycle();
        // lets do a fast calibration
        fastCalibration = false;
        timerStart = halStartTimerRef();
        breatherState = BREATHER_STATE_INITIAL_FAST_CALIBRATION;
        halValveOutOpen();
    }
}

/* checks if breathe in time is up and starts wait to out if so.
 * Detects high/low pressure */
static void fsmIn() {
    uint64_t currTime = halStartTimerRef();
    if (timerStart + currInMilli < currTime) {
        // in valve off
        halValveInClose();
        timerStart = halStartTimerRef();
        breatherState = BREATHER_STATE_WAIT_TO_OUT;
        endTidalVolumeCalculation();
        tidalVolume = pressGetTidalVolume();
    } else {
        currProgress = ((currTime - timerStart) * 100) / currInMilli;
        //currProgress = 100 - (100 * timerStart + currInMilli) / currTime;

        //--------- we check for low pressure at 50% or grater
        // low pressure hardcode to 3 InchH2O -> 90 int
        if (timerStart + currInMilli / 2 < currTime) {
            if (getCmH2OGauge() < lowPressure) {
                CEvent::post(EVENT_ALARM, ALARM_INDEX_LOW_PRESSURE);
            }
        }
    }
    //------ check for high pressure hardcode to 35 InchH2O -> 531 int
    if (getCmH2OGauge() > highPressure) {
        CEvent::post(EVENT_ALARM, ALARM_INDEX_HIGH_PRESSURE);
    }
}

/* checks if wait time is up, and starts breathe out if so */
static void fsmWaitToOut() {
    if (halCheckTimerExpired(timerStart, TIME_WAIT_TO_OUT)) {
        // switch valves
        timerStart = halStartTimerRef();
        breatherState = BREATHER_STATE_OUT;
        //LOG("Wait to out");
    }
}

/* checks if breathe out time is up and enters pause state if so */
static void fsmOut() {
    uint64_t currTime = halStartTimerRef();
    float currentPressure = pressGetVal(PRESSURE);
    if (timerStart + currOutMilli < currTime) {

        //if we have fast calibration request then we keep the valve open
        if (fastCalibration) {
            fastCalibration = false;
            timerStart = halStartTimerRef();
            breatherState = BREATHER_STATE_FAST_CALIBRATION;
            return;
        }

        // switch valves
        timerStart = halStartTimerRef();
        breatherState = BREATHER_STATE_PAUSE;
        halValveOutClose();

        //------ check for high tidal volume between 3-25 cmH2O
        if (tidalVolume < lowTidalVolume) {
            CEvent::post(EVENT_ALARM, ALARM_INDEX_LOW_TIDAL_VOLUME);
        }

        if (tidalVolume > highTidalVolume) {
            CEvent::post(EVENT_ALARM, ALARM_INDEX_HIGH_TIDAL_VOLUME);
        }
    } else {
        currProgress = 100 - ((currTime - timerStart) * 100) / currOutMilli;
        if (currProgress > 100) currProgress = 100;

        if (currentPressure > (float) desiredPeep)
            halValveOutOpen(); // drop the pressure
        if (currentPressure <= (float) desiredPeep)
            halValveOutClose(); //don't drop thepressure       
    }
}

static void fsmFastCalibration() {
    if (halCheckTimerExpired(timerStart, TIME_FAST_CALIBRATION)) {
        // switch valves
        timerStart = halStartTimerRef();
        breatherState = BREATHER_STATE_PAUSE;
        bmp280SetReference();
        halValveOutClose();
        CEvent::post(EVENT_ALARM, ALARM_INDEX_FAST_CALIB_DONE);
    }
}

static void fsmInitialFastCalibration() {
    if (halCheckTimerExpired(timerStart, TIME_FAST_CALIBRATION)) {
        // switch valves
        timerStart = halStartTimerRef();
        breatherState = BREATHER_STATE_PAUSE;
        bmp280SetReference();
        halValveOutClose();
    }
}

/* checks if stopping time is up and enters stopped state if so */
static void fsmStopping() {
    if (halCheckTimerExpired(timerStart, TIME_STOPPING)) {
        // switch valves
        timerStart = halStartTimerRef();
        breatherState = BREATHER_STATE_STOPPED;
        halValveOutOpen();
        halValveInClose();
    }
}

/* checks if pause time is up and resets the cycle */
static void fsmPause() {
    if (halCheckTimerExpired(timerStart, currPause)) {
        breatherStartCycle();
    }
}

/* main breather loop */
void breatherLoop() {
    if (halCheckTimerExpired(timerSerialLog, TIME_DATA_LOG_DELAY)) {
        sendDataViaSerial();
        timerSerialLog = halStartTimerRef();
    }

    if (breatherState != BREATHER_STATE_STOPPED && breatherState != BREATHER_STATE_STOPPING && propGetVent() == 0) {
        // force stop
        timerStart = halStartTimerRef();
        breatherState = BREATHER_STATE_STOPPING;
        currProgress = 0;
        halValveInClose();
        halValveOutOpen();
    }

/* all of these methods basically do the same thing:
 * check if the time is up for the current state and if so,
 * update the variables according to the state we're in,
 * and move onto the next state */
    switch(breatherState) {
        case BREATHER_STATE_STOPPED:
            fsmStopped();
            break;

        case BREATHER_STATE_IN:
            fsmIn();
            break;

        case BREATHER_STATE_WAIT_TO_OUT:
            fsmWaitToOut();
            break;

        case BREATHER_STATE_OUT:
            fsmOut();
            break;

        case BREATHER_STATE_INITIAL_FAST_CALIBRATION:
            fsmInitialFastCalibration();
            break;

        case BREATHER_STATE_FAST_CALIBRATION:
            fsmFastCalibration();
            break;

        case BREATHER_STATE_PAUSE:
            fsmPause();
            break;

        case BREATHER_STATE_STOPPING:
            fsmStopping();
            break;

        default:
            LOG("breatherLoop: unexpected state");
    }
}
//---------------------------------------------------------
#endif //#ifndef STEPPER_MOTOR_STEP_PIN
