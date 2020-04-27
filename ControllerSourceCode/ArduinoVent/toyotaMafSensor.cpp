

#include <Arduino.h>
#include "toyotaMafSensor.h"
#include <math.h> //M_PI
#include "config.h"
#include "log.h"
#include "hal.h"

#ifdef USE_CAR_FLOW_SENSOR

#define TIME_INIT_RETRY 200

static uint8_t state;
static uint64_t timer;

static float fFlow;
static float fRefFlow;

void mafSetReference() {
    LOGV("Set Flow Ref %d.", fFlow);
    fRefFlow = fFlow;
}

void updateRawFlowRate() {
    uint16_t val = analogRead(FLOW_SENSOR_PIN);
    float volt = val * (5.0) / (1023L) * 1000; //Calibrated to mV
    fFlow = (volt - FLOW_RELATION_INTERCEPT) / FLOW_RELATION_SLOPE;
}

static void checkInit() {
    if (state >= 4)
        return; // error or OK

    if (halCheckTimerExpired(timer, TIME_INIT_RETRY)) {
        updateRawFlowRate();
        mafSetReference();

        timer = halStartTimerRef();
        state = 10; // init is completed
        return;
    }

    timer = halStartTimerRef();
}

float getFlowRate() {
    checkInit();
    updateRawFlowRate();
    return fFlow - fRefFlow;
}
#endif