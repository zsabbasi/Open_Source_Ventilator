
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

#ifdef STEPPER_MOTOR_STEP_PIN // compile this file only when using Motor Stepper (squeezer)

#include "breather.h"
#include "properties.h"
#include "hal.h"
#include "log.h"
#include "pressure.h"
#include "event.h"
#include "alarm.h"
#include "motor.h"

#define MINUTE_MILLI 60000
#define TIME_WAIT_TO_OUT 200 //200 milliseconds
#define TIME_STOPPING 4000 // 4 seconds to stop

#define TIME_FAST_CALIBRATION 4000 // 4 seconds


static int currPause;
static int currRate;
static int currInMilli;
static int currOutMilli;
static int currTotalCycleMilli;
static int currProgress;
static uint64_t timerStart;
static int16_t highPressure;
static int16_t lowPressure;

static bool fastCalibration;

static const int rate[4] = {1,2,3,4} ;

static breatherState_t breatherState = BREATHER_STATE_STOPPED;

void breatherRequestFastCalibration() {
    if (propGetVent() == 0) {
        LOG("Ignore Fast Calib.");
        return;
    }
    fastCalibration = true;
    CEvent::post(EVENT_ALARM, ALARM_INDEX_FAST_CALIB_TO_START);
}

int breatherGetProgress() {
    return currProgress;
}

void breatherStartCycle() {
    currTotalCycleMilli = MINUTE_MILLI / propGetBpm();
    currPause = propGetPause();
    currRate = propGetDutyCycle();
    int inOutTime = currTotalCycleMilli - (currRate + TIME_WAIT_TO_OUT);
    currInMilli = (inOutTime / 2) / rate[currRate];
    currOutMilli = inOutTime - currInMilli;
    currProgress = 0;
    timerStart = halStartTimerRef();
    breatherState = BREATHER_STATE_IN;
    halValveOutClose();
    halValveInOpen();
    fastCalibration = false;
    highPressure = propGetHighPressure();
    lowPressure = propGetLowPressure();

    motorStartInspiration(currInMilli);

#if 0
    LOG("Ventilation ON:");
    LOGV(" currTotalCycleMilli = %d", currTotalCycleMilli);
    LOGV(" currPause = %d", currPause);
    LOGV(" currInMilli = %d", currInMilli);
    LOGV(" currOutMilli = %d", currOutMilli);
#endif
}

breatherState_t breatherGetState() {
    return breatherState;
}

static void fsmStopped() {
    if (propGetVent()) {
        breatherStartCycle();
    }
}

static void fsmIn() {
    currProgress = motorGetProgress();

    if (currProgress == 100) {
        // in valve off
        halValveInClose();
        timerStart = halStartTimerRef();
        breatherState = BREATHER_STATE_WAIT_TO_OUT;
    }

    //--------- we check for low pressure at 50% or grater
    // low pressure hardcode to 3 InchH2O -> 90 int
    if (currProgress < 50) {
        if (getCmH2OGauge(PRESSURE) < lowPressure) {
            CEvent::post(EVENT_ALARM, ALARM_INDEX_LOW_PRESSURE);
        }
    }

    //------ check for high pressure hardcode to 35 InchH2O -> 531 int
    if (getCmH2OGauge(PRESSURE) > highPressure) {
        CEvent::post(EVENT_ALARM, ALARM_INDEX_HIGH_PRESSURE);
    }
}

static void fsmWaitToOut() {
    if (halCheckTimerExpired(timerStart, TIME_WAIT_TO_OUT)) {
        // switch valves
        timerStart = halStartTimerRef();
        breatherState = BREATHER_STATE_OUT;
        halValveOutOpen();
        motorStartExhalation(currOutMilli);
    }
}

static void fsmOut()
{
  int p = motorGetProgress();
  currProgress = 100 - p;
  if (currProgress >  100) currProgress = 100;
  if (p == 100) {

        //if we have fast calibration request then we keep the valve open
        if (fastCalibration) {
            fastCalibration = false;
            timerStart = halStartTimerRef();
            breatherState = BREATHER_STATE_FAST_CALIBRATION;
            return;
        }
    timerStart = halStartTimerRef();
    breatherState = BREATHER_STATE_PAUSE;
    halValveOutClose();    
  }
}

static void fsmFastCalibration() {
    if (halCheckTimerExpired(timerStart, TIME_FAST_CALIBRATION)) {
        // switch valves
        timerStart = halStartTimerRef();
        breatherState = BREATHER_STATE_PAUSE;
        halValveOutClose();
        CEvent::post(EVENT_ALARM, ALARM_INDEX_FAST_CALIB_DONE);
    }
}

static void fsmStopping() {
    if (halCheckTimerExpired(timerStart, TIME_STOPPING)) {
        // switch valves
        timerStart = halStartTimerRef();
        breatherState = BREATHER_STATE_STOPPED;
        halValveOutClose();
        halValveInClose();
    }
}

static void fsmPause() {
    if (halCheckTimerExpired(timerStart, currPause)) {
        breatherStartCycle();
    }
}

void breatherLoop() {
    if (breatherState != BREATHER_STATE_STOPPED && breatherState != BREATHER_STATE_STOPPING && propGetVent() == 0) {
        // force stop
        timerStart = halStartTimerRef();
        breatherState = BREATHER_STATE_STOPPING;
        currProgress = 0;
        halValveInClose();
        halValveOutOpen();
    }

    switch (breatherState) {
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
