#ifndef UI_NATIVE_H
#define UI_NATIVE_H

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
#ifdef VENTSIM
#else
  #include <Arduino.h> 
#endif

#include "config.h"
#include "event.h"

void uiNativeInit();
void uiNativeLoop();

typedef enum {
    SHOW_MODE = 0,
    ENTER_MODE,
} uiState_t;

typedef  enum {
    STATE_IDLE = 0,
    STATE_RUN,
    STATE_ERROR
} runState_t;

class CUiNative : CEvent {
public:
    CUiNative();

    ~CUiNative();

    void loop();

    //void updateStatus();
    void updateParams();

    void updateParameterValue();

    void scroolParams(bool down);

    void blinker();

    void blinkOff(int mask);

    void blinkOn(int mask);

    void refreshValue(bool force);

    void checkFuncHold();

    void updateProgress();

    void initParams();

    void fillValBuf(char *buf, int index);

    void updateStatus(bool blank);

    virtual propagate_t onEvent(event_t *event);

    // --- public var (neede for static function) ---
    runState_t stateIndex; // = STATE_IDLE;

private:
    int paramsIndex;
    int progress;

    uiState_t uiState; // = SHOW_MODE;
    unsigned long timerSetHold;
    bool checkSetHold; // = false;
    unsigned long timerDecrementHold;
    bool checkDecrementHold; // = false;
    bool shortcutToTopDone;
    int ignoreRelease; // = 0;

    int blinkMask; // = 0;
    unsigned long blinkTimer;
    int blinkPhase; // = 0;

    bool alarmMode; // = false;
    char alarmMessage[LCD_NUM_COLS + 1];

    int bpm; // = 10;
    float dutyCycle; // = 0.1f;
};

#endif // UI_NATIVE_H
