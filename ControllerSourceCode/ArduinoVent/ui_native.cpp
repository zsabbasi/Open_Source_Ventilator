
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


#include "ui_native.h"
#include "hal.h"
#include "properties.h"
#include "bmp280_int.h"
#include "breather.h"
#include <stdio.h>
#include <string.h>
#include "alarm.h"
#include "languages.h"
#include "toyotaMafSensor.h"
#include "pressure.h"

//#define TEST_WDT // Debug only... it makes Watchdor to trigger reset when Set button is pressed

#define TM_BLINK                    400   // milliseconds
#define TM_FUNC_HOLD                500  // twoseconds
#define BLINK_PARAMETER_VAL         1
#define BLINK_SATUS                 2

//static int paramsIndex = 0;

#define LCD_STATUS_ROW              0
#define LCD_PARAMS_FIRST_ROW        1

#if (LCD_CFG_2_ROWS == 1)
  #define LCD_PARAMS_LAST_ROW         1
#elif (LCD_CFG_4_ROWS == 1)
  #define LCD_PARAMS_LAST_ROW         3
#else
  #error "At least one LCD_CFG_x_ROWS must be set to 1 in config.h"
#endif
#if ((LCD_CFG_2_ROWS == 1) && (LCD_CFG_4_ROWS == 1))
  #error "Only one LCD_CFG_x_ROWS must be set to 1 in config.h"
#endif


#if (LCD_CFG_20_COLS == 1)
  #define PROGRESS_NUM_CHARS  6
#elif (LCD_CFG_16_COLS == 1)
  #define PROGRESS_NUM_CHARS  6
#else
  #error "At least one LCD_CFG_XX_COLS must be set to 1 in config.h"
#endif
#if ((LCD_CFG_20_COLS == 1) && (LCD_NUM_COLS == 1))
  #error "Only one LCD_CFG_XX_COLS must be set to 1 in config.h"
#endif

#define LCD_PARAMS_NUM_ROWS         ( (LCD_PARAMS_LAST_ROW - LCD_PARAMS_FIRST_ROW) + 1)

#define PARAM_VAL_MAX_SIZE  5
#define PARAM_VAL_START_COL (LCD_NUM_COLS- PARAM_VAL_MAX_SIZE)

#define PROGRESS_ROW        0
#define PROGRESS_COL       (LCD_NUM_COLS - PROGRESS_NUM_CHARS)
#define PROGRESS_CHARACTER '|'

static const char * st_txt[3] = {
        (const char *) STR_IDLE,
        (const char *) STR_RUN,
        (const char *) STR_ERR
};

typedef enum {
    PARAM_INT = 1,
    PARAM_CHOICES,
    PARAM_TEXT_GETTER,

    PARAM_END
} paramType_t;

typedef void (*propchangefunc_t)(int);
typedef int (*propgetfunc_t)();
typedef char * (*txtGetterfunc_t)();


typedef struct params_st {
    paramType_t type;
    const char *name;
    int *val;
    int step;
    int min;
    int max;
    const char **options;
    bool quickUpdate;
    propchangefunc_t handler;
    union {
        propgetfunc_t propGetter;
        txtGetterfunc_t txtGetter;  // valid for PARAM_TEXT_GETTER. Value is update periodicly on screen
    } getter;

} params_t;

static CUiNative * uiNative;

// -------------  prototypes --------------
static void handleChangeCalibration(int val);


void uiNativeInit() {
    uiNative = new CUiNative();
}

void uiNativeLoop() {
    uiNative->loop();
}

void CUiNative::updateStatus(bool blank) {
    char buf[LCD_NUM_COLS + 1];
    memset(buf, 0x20, LCD_NUM_COLS);
    buf[LCD_NUM_COLS] = 0;
    int len;

    if (alarmMode) {
        if (!blank) {
            strcpy(buf, alarmMessage);
            len = strlen(buf);
        } else len = 0;
    } else {
        len = sprintf(buf, "st=%s", (const char *) st_txt[(int) stateIndex]);
    }
    buf[len] = 0x20;
    halLcdWrite(0, LCD_STATUS_ROW, buf);
}

//------ parameter values holders -------
static int valVent;
static int valBpm;
static int valDutyCycle;
static int valPause;
static int valLowPressure;
static int valHighPressure;
static int valLowTidal;
static int valHighTidal;
static int valCalibration;
static int valDesiredPeep;

//----------- Setters ----------

static void handleChangeVent(int val) {
    propSetVent(val);
    if (val) {
        alarmResetAll();
        uiNative->stateIndex = STATE_RUN;
    } else {
        uiNative->stateIndex = STATE_IDLE;
    }
    uiNative->updateStatus(false);
}

static void handleChangeBpm(int val) {
    propSetBpm(val);
}

static void handleChangeDutyCycle(int val) {
    propSetDutyCycle(val);
}

static void handleChangePause(int val) {
    propSetPause(val);
}

static void handleChangeLowPressure(int val) {
    propSetLowPressure(val);
}

static void handleChangeHighPressure(int val) {
    propSetHighPressure(val);
}

static void handleChangeLowTidal(int val) {
    propSetLowTidal(val);
}

static void handleChangeHighTidal(int val) {
    propSetHighTidal(val);
}

static void handleChangeDesiredPeep(int val) {
    propSetDesiredPeep(val);
}

//-------- getters ------

static int handleGetVent() {
    return propGetVent();
}

static int handleGetBpm() {
    return propGetBpm();
}

static int handleGetDutyCycle() {
     return propGetDutyCycle();
}

static int handleGetPause() {
    return propGetPause();
}

static int handleGetLowPressure() {
    return propGetLowPressure();
}

static int handleGetHighPressure() {
    return propGetHighPressure();
}

static int handleGetLowTidal() {
    return propGetLowTidal();
}

static int handleGetHighTidal() {
    return propGetHighTidal();
}

static int handleGetDesiredPeep() {
    return propGetDesiredPeep();
}

static char *  getFlow () {
    static char buf[8];
    buf[sizeof(buf) - 1] = 0;
    float f = pressGetVal(FLOW);
#ifndef VENTSIM
    dtostrf(f, 2, 2, buf);
#else
    snprintf(buf, sizeof(buf) - 1, "%f", f);
#endif
    return buf;
}

static char *  getTidalVolume() {
    static char buf[8];
    buf[sizeof(buf) - 1] = 0;
    uint16_t f = pressGetTidalVolume();
#ifndef VENTSIM
    itoa(f, buf, 10);
#else
    snprintf(buf, sizeof(buf) - 1, "%i", f);
#endif
    return buf;
}

static char *  getPressure() {
    static char buf[8];
    buf[sizeof(buf) - 1] = 0;
    float f = pressGetVal(PRESSURE);
#ifndef VENTSIM
    dtostrf(f, 2, 2, buf);
#else
    snprintf(buf, sizeof(buf) - 1, "%i", f);
#endif
    return buf;
}

static const char * onOffTxt[] = {
        STR_OFF,
        STR_ON,
};

static const char * yesNoTxt[] = {
        STR_NO,
        STR_YES,
};

#ifndef VENTSIM
  static const params_t params[]  PROGMEM =  {
#else
  static params_t params[]  =  {
#endif
        {PARAM_CHOICES,            // type
         STR_VENTILATOR,           // name
         &valVent,                 // val
         1,                        // step
         0,                        // min
         1,                        // max
         onOffTxt,                 // text array for options
         false,                    // no dynamic changes
         handleChangeVent,         // change prop function
         {handleGetVent},          // propGetter
        },

        {PARAM_INT,                // type
         STR_BPM,                  // name
         &valBpm,                  // val
         5,                        // step
         10,                       // min
         30,                       // max
         0,                        // text array for options
         true,                     // no dynamic changes
         handleChangeBpm,          // change prop function
         {handleGetBpm}            // propGetter
        },

        {PARAM_CHOICES,            // type
         STR_DUTY_CYCLE,           // name
         &valDutyCycle,            // val
         1,                        // step
         0,                        // min
         PROT_DUTY_CYCLE_SIZE - 1, // max
         propDutyCycleTxt,         // text array for options
         true,                     // no dynamic changes
         handleChangeDutyCycle,    // change prop function
         {handleGetDutyCycle}      // propGetter
        },

        {PARAM_INT,                // type
         STR_PAUSE,                // name
         &valPause,                // val
         50,                       // step
         0,                        // min
         2000,                     // max
         0,                        // text array for options
         true,                     // no dynamic changes
         handleChangePause,        // change prop function
         {handleGetPause}          // propGetter
        },

        {PARAM_TEXT_GETTER,        // type
         STR_PRESSURE,             // name
         0,                        // val
         1,                        // step
         0,                        // min
         1,                        // max
         0,                        // text array for options
         false,                    // no dynamic changes
         0,  // change prop function
         {(propgetfunc_t) getPressure} // despite this is a txtGetter (note that this is a PARAM_TEXT_GETTER)
                // different compilers have particular syntax in how to set union. Casting
                // with the first function prototype works for all. Hack but better than add lots of #ifdef's

        },

        {PARAM_TEXT_GETTER,        // type
         STR_FLOW,                 // name
         0,                        // val
         1,                        // step
         0,                        // min
         1,                        // max
         0,                        // text array for options
         false,                    // no dynamic changes
         0,  // change prop function
         {(propgetfunc_t) getFlow} // despite this is a txtGetter (note that this is a PARAM_TEXT_GETTER)
                // different compilers have particular syntax in how to set union. Casting
                // with the first function prototype works for all. Hack but better than add lots of #ifdef's

        },

        {PARAM_TEXT_GETTER,        // type
         STR_TIDAL,                // name
         0,                        // val
         1,                        // step
         0,                        // min
         1,                        // max
         0,                        // text array for options
         false,                    // no dynamic changes
         0,  // change prop function
         {(propgetfunc_t) getTidalVolume} // despite this is a txtGetter (note that this is a PARAM_TEXT_GETTER)
                // different compilers have particular syntax in how to set union. Casting
                // with the first function prototype works for all. Hack but better than add lots of #ifdef's

        },

        {PARAM_INT,                // type
         STR_PEEP,                 // name
         &valDesiredPeep,          // val
         1,                        // step
         1,                        // min
         25,                       // max
         0,                        // text array for options
         true,                     // no dynamic changes
         handleChangeDesiredPeep,  // change prop function
         {handleGetDesiredPeep}    // propGetter
        },

        {PARAM_INT,                // type
         STR_LOW_PRESSURE,         // name
         &valLowPressure,          // val
         1,                        // step
         1,                        // min
         15,                       // max
         0,                        // text array for options
         true,                     // no dynamic changes
         handleChangeLowPressure,  // change prop function
         {handleGetLowPressure}    // propGetter
        },

        {PARAM_INT,                // type
         STR_HIGH_PRESSURE,        // name
         &valHighPressure,         // val
         2,                        // step
         10,                       // min
         40,                       // max
         0,                        // text array for options
         true,                     // no dynamic changes
         handleChangeHighPressure, // change prop function
         {handleGetHighPressure}   // propGetter
        },

        {PARAM_INT,                // type
         STR_LOW_TIDAL,            // name
         &valLowTidal,             // val
         100,                      // step
         0,                        // min
         1400,                     // max
         0,                        // text array for options
         true,                     // no dynamic changes
         handleChangeLowTidal,     // change prop function
         {handleGetLowTidal}       // propGetter
        },

        {PARAM_INT,                // type
         STR_HIGH_TIDAL,           // name
         &valHighTidal,            // val
         100,                      // step
         0,                        // min
         1400,                     // max
         0,                        // text array for options
         true,                     // no dynamic changes
         handleChangeHighTidal,    // change prop function
         {handleGetHighTidal}      // propGetter
        },

        // *******************************************
        // NOTE: THIS MUST BE THE VERY LAST PARAMETER
        // *******************************************
        {PARAM_CHOICES,            // type
         STR_CALIB_PRESSURES,      // name
         &valCalibration,          // val
         1,                        // step
         0,                        // min
         1,                        // max
         onOffTxt,                 // text array for options
         false,                    // no dynamic changes
         handleChangeCalibration,  // change prop function
         0,        // propGetter
        },
};

#define NUM_PARAMS (sizeof(params)/sizeof(params_t))

#ifndef VENTSIM
params_t * loadParamRecord(int idx) {
    static params_t par;
    int i;
    uint8_t *srcPtr = (uint8_t *) &params[idx];
    uint8_t *dstPtr = (uint8_t *) &par;

    for (i = 0; i < sizeof(params_t); i++) {
        *dstPtr = pgm_read_byte_near(srcPtr);
        srcPtr++;
        dstPtr++;
    }
    return &par;
}
#else
params_t * loadParamRecord(int idx) {
    static params_t par;
    par = params[idx];
    return &par;
}
#endif

static void handleChangeCalibration(int val) {
    breatherRequestFastCalibration();
    params_t *par = loadParamRecord(NUM_PARAMS - 1);
    *par->val = 0; // reset val
}

//------------ Global -----------
CUiNative::CUiNative() {
    paramsIndex = 0;
    uiState = SHOW_MODE;
    checkSetHold = false;
    checkDecrementHold = false;
    shortcutToTopDone = false;
    ignoreRelease = 0;
    stateIndex = STATE_IDLE;
    blinkMask = 0;
    blinkPhase = 0;
    alarmMode = false;
    bpm = 10;
    dutyCycle = 0.1f;

    initParams();
    blinkTimer = halStartTimerRef();
    updateStatus(false);
    updateParams();
}

CUiNative::~CUiNative() {

}

void CUiNative::initParams() {
    unsigned int i;
    params_t *par;
    for (i = 0; i < NUM_PARAMS; i++) {
        par = loadParamRecord(i);
        if (par->type == PARAM_TEXT_GETTER) continue;
        if (par->getter.propGetter) {
            *par->val = par->getter.propGetter();
        }
    }

    if (propGetVent()) {
        stateIndex = STATE_RUN;
    } else {
        stateIndex = STATE_IDLE;
    }
}

void CUiNative::fillValBuf(char * buf, int index) {
    params_t *par = loadParamRecord(index);

    if (par->type == PARAM_INT)
        sprintf(buf, "%5d", *par->val);
    else if (par->type == PARAM_CHOICES)
        strcpy(buf, par->options[*par->val]);
    else if (par->type == PARAM_TEXT_GETTER)
        sprintf(buf, "%s", par->getter.txtGetter());
    else
        LOG("blinker: Unexpected type");
}

void CUiNative::loop() {
    checkFuncHold();
    blinker();
    updateProgress();
}

void CUiNative::blinker() {
    char buf[(LCD_NUM_COLS - PARAM_VAL_START_COL) + 1];
    int len = LCD_NUM_COLS - PARAM_VAL_START_COL;
    memset(buf, 0x20, (size_t) len); // spaces
    buf[len] = 0; // NULL terminate

    if (halCheckTimerExpired(blinkTimer, TM_BLINK)) {
        blinkTimer = halStartTimerRef();

//        if (blinkMask == 0) return;

        blinkPhase++;
        blinkPhase &= 1;

        //-------- paramater Blinking ------------
        if (blinkMask & BLINK_PARAMETER_VAL) {
            if (blinkPhase) {
                fillValBuf(buf, paramsIndex);
            }
            halLcdWrite(PARAM_VAL_START_COL, LCD_PARAMS_FIRST_ROW, buf);
        }

            //-------- other Blinking... ------------


        else {
            updateParams();
        }

        //---------- Alarm blink ------------
        if (alarmMode) {
            updateStatus(blinkPhase != 0);
        }
    }
}

void CUiNative::refreshValue(bool force) {
    char buf[(LCD_NUM_COLS - PARAM_VAL_START_COL) + 1];
    int len = LCD_NUM_COLS - PARAM_VAL_START_COL;

    params_t *par = loadParamRecord(paramsIndex);

    if (par->quickUpdate || force) {
        if (par->handler) {
            par->handler(*par->val);
        }
    }

    memset(buf, 0x20, (size_t) len); // spaces
    buf[len] = 0; // NULL terminate

    blinkTimer = halStartTimerRef();
    fillValBuf(buf, paramsIndex);
    halLcdWrite(PARAM_VAL_START_COL, LCD_PARAMS_FIRST_ROW, buf);
}

void CUiNative::updateProgress() {
    if (alarmMode) return;

    int i;
    char buf[PROGRESS_NUM_CHARS + 1];
    memset(buf, 0x20, sizeof(buf)); // spaces
    buf[sizeof(buf) - 1] = 0;

    //breatherState_t s = breatherGetState();
    int p = (breatherGetProgress() * PROGRESS_NUM_CHARS) / 90;
    if (p > PROGRESS_NUM_CHARS) p = PROGRESS_NUM_CHARS;
    if (p == progress) return;

    progress = p;
    for (i = 0; i < progress; i++) {
        buf[i] = PROGRESS_CHARACTER;
    }
    halLcdWrite(PROGRESS_COL, PROGRESS_ROW, buf);
}

void CUiNative::updateParameterValue() {

}


void CUiNative::blinkOn(int mask) {
    blinkMask |= mask;
}

void CUiNative::blinkOff(int mask) {
    blinkMask &= ~mask;
    updateParams();
}

void CUiNative::updateParams() {
    unsigned int index = paramsIndex;
    unsigned int i;
    char buf[LCD_NUM_COLS + 1];
    params_t *par;

    for (i = 0; i < LCD_PARAMS_NUM_ROWS; i++) {
        par = loadParamRecord(index);
        memset(buf, 0x20, LCD_NUM_COLS);
        buf[LCD_NUM_COLS] = 0;
        if (i == 0) {
            buf[0] = '>';
        }
        memcpy(&buf[1], par->name, strlen(par->name));
        fillValBuf(&buf[PARAM_VAL_START_COL], index);
        halLcdWrite(0, LCD_PARAMS_FIRST_ROW + i, buf);

        index++;
        if (index >= NUM_PARAMS) index = 0;
    }
}

void CUiNative::scroolParams( bool down) {
    if (down) {
        paramsIndex++;
        if (paramsIndex >= (int) NUM_PARAMS)
            paramsIndex = 0;
    } else {
        paramsIndex--;
        if (paramsIndex < 0)
            paramsIndex = NUM_PARAMS - 1;
    }

    updateParams();
}

void CUiNative::checkFuncHold() {
    if (uiState != SHOW_MODE) return;

    //-------- process KEY_SET hold ------
    if (checkSetHold) {
        if (halCheckTimerExpired(timerSetHold, TM_FUNC_HOLD)) {
            blinkOn(BLINK_PARAMETER_VAL);
            LOG("** ENTER mode");
            uiState = ENTER_MODE;
        }
    }

    //-------- process KEY_DECREMENT hold ------
    if (checkDecrementHold && !shortcutToTopDone) {
        if (halCheckTimerExpired(timerDecrementHold, TM_FUNC_HOLD)) {
            paramsIndex = -1;
            scroolParams(true);
            shortcutToTopDone = true;
        }
    }
}

propagate_t CUiNative::onEvent(event_t * event) {
    //char b[64];
    //sprintf(b, "onEvent: type = %d, key = %d\n", event->type, event->iParam);
    //LOG( (char *) b);

    if (event->type == EVT_ALARM_DISPLAY_ON) {
        alarmMode = true;
        strcpy(alarmMessage, event->param.tParam);
        return PROPAGATE_STOP;
    }

    if (event->type == EVENT_ALARM_DISPLAY_OFF) {
        alarmMode = false;
        updateStatus(false);
        return PROPAGATE_STOP;
    }

    //============== SHOW Mode =============
    if (uiState == SHOW_MODE) {

        //--------- SET Key -------------
        if (event->param.iParam == KEY_SET) {
            if (event->type == EVENT_KEY_RELEASE) {
                if (ignoreRelease) {
                    ignoreRelease--;
                    return PROPAGATE;
                }
                // scroolParams(true);
                checkSetHold = false;
            } else if (event->type == EVENT_KEY_PRESS) {
                timerSetHold = halStartTimerRef();
                checkSetHold = true;
#ifdef TEST_WDT
                // test WDT
                delay(2000); // triggers reset via WDT
#endif
            }
        }

        //------------- Decrement key --------------
        if (event->param.iParam == KEY_DECREMENT) {
            if (event->type == EVENT_KEY_PRESS) {
                scroolParams(false);
                timerDecrementHold = halStartTimerRef();
                checkDecrementHold = true;
            } else {
                checkDecrementHold = false;
                shortcutToTopDone = false;
            }
        }

        //------------- increment key --------------
        if (event->param.iParam == KEY_INCREMENT && event->type == EVENT_KEY_PRESS) {
            scroolParams(true);
        }
    }

        //============== ENTER Mode =============
    else if (uiState == ENTER_MODE) {

        //--------- Function Key -------------
        if (event->param.iParam == KEY_SET && event->type == EVENT_KEY_PRESS) {
            blinkOff(BLINK_PARAMETER_VAL);
            LOG("** SHOW mode");
            uiState = SHOW_MODE;
            checkSetHold = false;
            ignoreRelease = 1;
            refreshValue(true);
        }

        params_t *par = loadParamRecord(paramsIndex);

        //------- Right (UP) Key
        if (event->param.iParam == KEY_INCREMENT && event->type == EVENT_KEY_PRESS) {
            if (*par->val < par->max) {
                *par->val += par->step;
                refreshValue(false);
            }
        }
        if (event->param.iParam == KEY_DECREMENT && event->type == EVENT_KEY_PRESS) {
            if (*par->val > par->min) {
                *par->val -= par->step;
                refreshValue(false);
            }
        }
    }

    return PROPAGATE;
}
