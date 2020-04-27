
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

#include "hal.h"
#include "event.h"
#include "properties.h"
#include "pressure.h"
#include "serialWriter.h"

#ifndef LCD_CFG_I2C
  #include "LcdMv.h"
#endif

#include "EEPROM.h"

#ifdef WATCHDOG_ENABLE
  #if defined(__AVR__)
    #include <avr/wdt.h>
  #else 
    error "MPU not supported... either add support or disable watchdog."
  #endif
#endif

//---------- Constants ---------
#define EEPROM_DATA_BLOCK_ADDRESS 0

static monitorLED_t monitorLedNormal = MONITOR_LED_NORMAL;

//-------- variables --------
static uint64_t timerLED;
static bool alarm = false;
static int alarmPhase = 0;
static uint64_t timerAlarm;

static uint64_t timeKeySampling;

static char lcdBuffer [LCD_NUM_ROWS][LCD_NUM_COLS];
static int cursor_col = 0, cursor_row = 0;

static int ledState = 0;

#define FREQ1 440
#define FREQ2 740
#define FREQ3 880

//--------- local prototypes ------
static void motorInit();

/* turn alarm on/off */
void halBeepAlarmOnOff( bool on) {
#ifndef  NO_ALARM_SOUND

    if (on) {
        alarm = true;
        alarmPhase = 0;
        timerAlarm = halStartTimerRef();
        // turn tone on
        alarmPhase = 0;
        tone(ALARM_SOUND_PIN, FREQ1);

    } else {
        alarm = false;
        noTone(ALARM_SOUND_PIN);
        // turn tone off
    }

#endif
}

//----------- Locals -------------

  #ifdef LCD_CFG_I2C
    LiquidCrystal_I2C lcd(0x27,4,4);
  #else
    LcdMv         lcd(  LCD_CFG_RS, 
                        LCD_CFG_E, LCD_CFG_D4, 
                        LCD_CFG_D5, 
                        LCD_CFG_D6, 
                        LCD_CFG_D7);
  #endif

#define TIME_WAIT_TO_ENABLE_WATCHDOG 3000

uint64_t watchdogTimer = 0;
static int watchdogState;

//-------------------------------------------------------  
//-------         Milliseconds Timer
//-------------------------------------------------------  

/* current time in milliseconds */
uint64_t halStartTimerRef() {
    static uint32_t low32, high32 = 0;
    uint32_t newLow32 = millis();
    if (newLow32 < low32) high32++;
    low32 = newLow32;
    return (uint64_t) high32 << 32 | low32;
}

/* is the timer started at time <timerRef>, with duration <time> expired?
   in other words is timerRef + time > current time? */
bool halCheckTimerExpired(uint64_t timerRef, uint64_t time) {
    uint64_t now = halStartTimerRef();
    return timerRef + time < now;
}

//-------------------------------------------------------  
//------- Microsecond timer implementation (needed for motor support)
//-------------------------------------------------------  
#ifdef ENABLE_MICROSEC_TIMER

#define OVER_32BITS 4294967296
static uint64_t   microFreeRunningTimer;
static uint64_t   lastMicros; // to check overflow

static void updateMicroFreeRunningTimer() {
    uint64_t currTime = micros();
    uint64_t elapse;

    if (currTime < lastMicros) {
        LOG("micro overflow"); // happens every 70 minutes
        elapse = (currTime + OVER_32BITS) - lastMicros;
    } else {
        elapse = currTime - lastMicros;
    }

    microFreeRunningTimer += elapse;
    lastMicros = currTime;
}

uint64_t halStartMicroTimerRef() {
    return microFreeRunningTimer;
}

bool halCheckMicroTimerExpired(uint64_t microTimerRef, uint64_t time) {
    return (microTimerRef + time) < microFreeRunningTimer;
}
#endif // ENABLE_MICROSEC_TIMER
//-------------------------------------------------------

/* initializes watchdog */
static void initWdt(uint8_t resetValue) {
#ifdef WATCHDOG_ENABLE
    watchdogState = 0;
    watchdogTimer = halStartTimerRef();

    // the following line always return zero as bootloader clears the bit.
    // see hack at: https://www.reddit.com/r/arduino/comments/29kev1/a_question_about_the_mcusr_and_the_wdrf_after_a/
    //if ( MCUSR & (1<<WDRF) ) { // if we are starting dua watchdog recover LED will be fast
    if (resetValue & (1 << WDRF)) {
        halSetMonitorLED(MONITOR_LED_FAST);
    } else {
        halSetMonitorLED(MONITOR_LED_NORMAL);
    }
    wdt_disable(); // keep WDT disable for a couple seconds
#else
    halSetMonitorLED(MONITOR_LED_SLOW);
#endif
}

/* check if watchdog timer is up and trigger it if so
 * if triggered, call the wdt_reset method */
static void loopWdt() {
#ifdef WATCHDOG_ENABLE
    if (watchdogState == 0) { // wait to enable WDT
        if (halCheckTimerExpired(watchdogTimer, TIME_WAIT_TO_ENABLE_WATCHDOG)) {
            watchdogTimer = halStartTimerRef();
            watchdogState = 1;
            wdt_enable(WDTO_1S); //WDTO_2S Note: LCD Library is TOO SLOW... need to fix to get real time and lower WDT

            /* WDT possible values for ATMega 8, 168, 328, 1280, 2560
                 WDTO_15MS, WDTO_30MS, WDTO_60MS, WDTO_120MS, WDTO_250MS, WDTO_500MS, WDTO_1S, WDTO_2S
               WDT possible values for  ATMega 168, 328, 1280, 2560
                 WDTO_4S, WDTO_8S */
        }
        return;
    }
    //------- if we are here then WDT is enabled... kick it
    wdt_reset();
#endif
}

/* initializes all hal variables */
void halInit(uint8_t resetValue) {
    int r, c;
#ifdef DEBUG_SERIAL_LOGS
    Serial.begin(9600);
    LOG("Starting...");
#endif
    pinMode(MONITOR_LED_PIN, OUTPUT);
    timerLED = halStartTimerRef();

    propInit();
#ifdef LCD_CFG_I2C
    lcd.init();                      // initialize the lcd
    lcd.backlight();
#else
#if (LCD_CFG_4_ROWS == 1)
    r = 4;
#if (LCD_CFG_20_COLS == 1)
        c = 20;
#else
        c = 16;
#endif
#else
    r = 2;
#if (LCD_CFG_20_COLS == 1)
        c = 20;
#else
        c = 16;
#endif
#endif
  lcd.begin(c, r);
  lcd.setFrameBuffer( (uint8_t *) lcdBuffer, r, c);
#endif
    halLcdClear();

    // -----  keys -------
    pinMode(KEY_SET_PIN, INPUT_PULLUP);           // set pin to input
    pinMode(KEY_INCREMENT_PIN, INPUT_PULLUP);           // set pin to input
    pinMode(KEY_DECREMENT_PIN, INPUT_PULLUP);           // set pin to input

// ------ valves -------
    pinMode(VALVE_IN_PIN, OUTPUT);           // set pin to input
    pinMode(VALVE_OUT_PIN, OUTPUT);           // set pin to input
    halValveInClose();
    halValveOutOpen();

    timeKeySampling = halStartTimerRef();
    initWdt(resetValue);
    pressInit();
    motorInit();
    serialInit();
}

/* ??? */
static void testKey() {
#if 1
    digitalWrite(VALVE_IN_PIN, digitalRead(KEY_SET_PIN));
#else
    digitalWrite(VALVE_IN_PIN, digitalRead(KEY_DECREMENT_PIN));
#endif
    digitalWrite(VALVE_OUT_PIN, digitalRead(KEY_INCREMENT_PIN));
}

/* setter for monitorLedSpeed property */
void halSetMonitorLED (monitorLED_t speed) {
    monitorLedNormal = speed;
}

/* getter for monitorLedSpeed property */
monitorLED_t halGetMonitorLED () {
    return monitorLedNormal;
}

/* turns led on/off at the appropriate time */
void halBlinkLED() {
    uint64_t time;
    if (monitorLedNormal == MONITOR_LED_FAST) {
        time = TIME_MONITOR_LED_FAST;
    } else if (monitorLedNormal == MONITOR_LED_SLOW) {
        time = TIME_MONITOR_LED_SLOW;
    } else {
        time = TIME_MONITOR_LED_NORMAL;
    }

    if (halCheckTimerExpired(timerLED, time)) {
        timerLED = halStartTimerRef();

        if (ledState) {
            ledState = 0;
            digitalWrite(MONITOR_LED_PIN, LOW);
        } else {
            ledState = 1;
            digitalWrite(MONITOR_LED_PIN, HIGH);
        }
    }
}

/* ??? */
static void alarmToggler() {
    if (!alarm) return;

    if (halCheckTimerExpired(timerAlarm, TIME_ALARM_PERIOD / 5)) {
        timerAlarm = halStartTimerRef();
        switch (alarmPhase) {
            case 0:
                noTone(ALARM_SOUND_PIN);
                tone(ALARM_SOUND_PIN, FREQ2);
                break;
            case 1:
                noTone(ALARM_SOUND_PIN);
                tone(ALARM_SOUND_PIN, FREQ3);
                break;
            case 2:
                noTone(ALARM_SOUND_PIN);
                break;
            case 6:
                tone(ALARM_SOUND_PIN, FREQ1);
                alarmPhase = 0;
                return;
            default:
                break;
        }
        alarmPhase++;
    }
}

//-------- display --------

/* updates screen */
static void lcdUpdate() {
    int i, r;
    char *s, *d;
    char out[LCD_NUM_COLS + 1];

    for (r = 0; r < LCD_NUM_ROWS; r++) {
        d = out;
        s = &lcdBuffer[r][0];
        for (i = 0; i < LCD_NUM_COLS; i++) {
            *d++ = *s++;
        }
        *d++ = 0;

#ifdef LCD_CFG_I2C          // only for I2C as LcdMv updates by refresh
        lcd.setCursor(0, r);
        lcd.print(out);
#endif

    }
}

/* clears screen */
void halLcdClear() {
    memset(lcdBuffer, 0x20, sizeof(lcdBuffer));
    cursor_col = 0;
    cursor_row = 0;
}

/* set position of the cursor */
void halLcdSetCursor(int col, int row) {
    if (cursor_col >= LCD_NUM_COLS) {
        LOG("halLcdSetCursor: bad cursor_col");
        return;
    }
    if (cursor_row >= LCD_NUM_ROWS) {
        LOG("halLcdSetCursor: bad cursor_row");
        return;
    }
    cursor_col = col;
    cursor_row = row;
}

/* writes new data on the screen */
void halLcdWrite(const char * txt) {
    int n;
    if (cursor_col >= LCD_NUM_COLS) {
        LOG("halLcdWrite: bad cursor_col");
        return;
    }
    if (cursor_row >= LCD_NUM_ROWS) {
        LOG("halLcdWrite: bad cursor_row");
        return;
    }
    n = strlen(txt);
    if (n > (LCD_NUM_COLS - cursor_col)) {
        //LOG("halLcdWrite: clipping");
        n = LCD_NUM_COLS - cursor_col;
    }
    memcpy(&lcdBuffer[cursor_row][cursor_col], txt, n);
    // TODO: row overflow check or clipping
    lcdUpdate();
}

/* sets cursor position and then writes */
void halLcdWrite(int col, int row, const char * txt) {
    halLcdSetCursor(col, row);
    halLcdWrite(txt);
}

//---------- valves Real

/* open in valve */
void halValveInOpen() {
#ifdef VALVE_IN_ACTIVE_LOW
    digitalWrite(VALVE_IN_PIN, LOW);
#else
    digitalWrite(VALVE_IN_PIN, HIGH);
#endif
}

/* close in valve */
void halValveInClose() {
#ifdef VALVE_IN_ACTIVE_LOW
    digitalWrite(VALVE_IN_PIN, HIGH);
#else
    digitalWrite(VALVE_IN_PIN, LOW);
#endif
}

/* open out valve */
void halValveOutOpen() {
#ifdef VALVE_OUT_ACTIVE_LOW
    digitalWrite(VALVE_OUT_PIN, LOW);
#else
    digitalWrite(VALVE_OUT_PIN, HIGH);
#endif
}

/* close out valve */
void halValveOutClose() {
#ifdef VALVE_OUT_ACTIVE_LOW
    digitalWrite(VALVE_OUT_PIN, HIGH);
#else
    digitalWrite(VALVE_OUT_PIN, LOW);
#endif
}

//---------- Stepper Motor ---------

/* initialize stepper motor */
static void motorInit() {
#ifdef STEPPER_MOTOR_STEP_PIN
    pinMode(STEPPER_MOTOR_STEP_PIN, OUTPUT);
    pinMode(STEPPER_MOTOR_DIR_PIN, OUTPUT);
#ifdef STEPPER_MOTOR_EOC_PIN
    pinMode(STEPPER_MOTOR_EOC_PIN, INPUT_PULLUP);
#endif
    halMotorStep(false);
#endif
}

/* ??? */
void halMotorStep(bool on) {
#ifdef STEPPER_MOTOR_STEP_PIN
    digitalWrite(STEPPER_MOTOR_STEP_PIN, on);
#endif
}

/* ??? */
void halMotorDir(bool dir) {
#ifdef STEPPER_MOTOR_STEP_PIN
#ifndef STEPPER_MOTOR_INVERT_DIR
    digitalWrite(STEPPER_MOTOR_DIR_PIN, dir);
#else
    digitalWrite(STEPPER_MOTOR_DIR_PIN, !dir);
#endif
#endif
}

/* ??? */
bool halMotorEOC() {
#ifdef STEPPER_MOTOR_STEP_PIN
#ifdef STEPPER_MOTOR_EOC_PIN
    return digitalRead(STEPPER_MOTOR_EOC_PIN);
#else
    return false;
#endif
#endif
}

/* Read analog pressure */
uint16_t halGetAnalogPressure() {
    return (uint16_t) analogRead(PRESSURE_SENSOR_PIN);  //Raw digital input from pressure sensor
}

/* Read analog flow */
uint16_t halGetAnalogFlow() {
    return (uint16_t) analogRead(FLOW_SENSOR_PIN);  //Raw digital input from pressure sensor
}

/* Save data in non-volatil storage */
bool halSaveDataBlock(uint8_t * data, int _size) {
    unsigned int i, eeprom_addr = EEPROM_DATA_BLOCK_ADDRESS;
    LOG("halSaveDataBlock:");
    for (i = 0; i < _size; i++) {
        //LOGV("addr = %d, d = 0x%x", eeprom_addr, *data);
        EEPROM.update(eeprom_addr, *data);
        eeprom_addr++, data++;
    }
}

/* Restore data from non-volatil storage */
bool halRestoreDataBlock(uint8_t * data, int _size) {
    unsigned int i, eeprom_addr = EEPROM_DATA_BLOCK_ADDRESS;
    LOG("halRestoreDataBlock:");
    for (i = 0; i < _size; i++) {
        *data = EEPROM.read(eeprom_addr);
        //LOGV("addr = %d, d = 0x%x", eeprom_addr, *data);
        eeprom_addr++, data++;
    }
}

/* process keys */
#if KEYS_JOYSTICK == 0
  #define   DEBOUNCING_N    4
#elif KEYS_JOYSTICK == 1
  #define   DEBOUNCING_N    2
#endif
typedef struct keys_st {
    int state; // 0-> released
    int count;
    int pin;
    int keyCode;
} keys_t;

static keys_t keys[3] = {
        {0, 0, KEY_DECREMENT_PIN, KEY_DECREMENT},
        {0, 0, KEY_INCREMENT_PIN, KEY_INCREMENT},
        {0, 0, KEY_SET_PIN,       KEY_SET},
};

/* ??? */
bool keyPressed(keys_t key) {
#if (KEYS_JOYSTICK == 1)
    if (key.keyCode == KEY_DECREMENT) {
        uint16_t value = analogRead(KEY_INCREMENT);
        // Serial.print("DECREMENT: ");
        // Serial.println(value);
        return value <= 60;
    }
    if (key.keyCode == KEY_INCREMENT) {
        uint16_t value = analogRead(KEY_INCREMENT);
        // Serial.print("INCREMENT: ");
        // Serial.println(value);
        return value >= 800;
    }
    if (key.keyCode == KEY_SET) {
        int value = digitalRead(key.pin);
        return value == LOW;
    }
    return false;
#elif (KEYS_BUTTONS == 1)
    return digitalRead(key.pin) == LOW;
#endif
}

/* ??? */
bool keyReleased(keys_t key) {
#if (KEYS_JOYSTICK == 1)
    if (key.keyCode == KEY_DECREMENT) {
        int value = analogRead(KEY_INCREMENT);
        return value >= 400 && value < 600;
    }
    if (key.keyCode == KEY_INCREMENT) {
        int value = analogRead(KEY_INCREMENT);
        return value <= 700 && value > 300;
    }
    if (key.keyCode == KEY_SET) {
        return digitalRead(key.pin) == HIGH;
    }
    return false;
#elif (KEYS_BUTTONS == 1)
    return digitalRead(key.pin) == HIGH;
#endif
}

/* handle pressed keys */
static void processKeys() {
    int i;
    if (!halCheckTimerExpired(timeKeySampling, TIME_KEY_SAMPLING)) return;

    timeKeySampling = halStartTimerRef();
    for (i = 0; i < 3; i++) {
        if (keys[i].state == 0) {
            // ------- key is release state -------
            if (keyPressed(keys[i])) { // if key is pressed
                keys[i].count++;
                if (keys[i].count >= DEBOUNCING_N) {
                    Serial.print("Pressed: ");
                    Serial.println(keys[i].keyCode);
                    //declare key pressed
                    keys[i].count = 0;
                    keys[i].state = 1;
                    CEvent::post(EVENT_KEY_PRESS, keys[i].keyCode);
                }

            } else {
                keys[i].count = 0;
            }
        } else {
            // ------- key is pressed state -------
            if (keyReleased(keys[i])) { // if key is release
                keys[i].count++;
                if (keys[i].count >= DEBOUNCING_N) {
                    Serial.print("Released: ");
                    Serial.println(keys[i].keyCode);
                    //declare key released
                    keys[i].count = 0;
                    keys[i].state = 0;
                    CEvent::post(EVENT_KEY_RELEASE, keys[i].keyCode);
                }
            } else {
                keys[i].count = 0;
            }
        }
    }
}

/* main hal loop */
void halLoop() {
    halBlinkLED();
    processKeys();
    propLoop();
    pressLoop();
    alarmToggler();

#ifdef WATCHDOG_ENABLE
    loopWdt();
#endif

#ifdef ENABLE_MICROSEC_TIMER
    updateMicroFreeRunningTimer();
#endif

#ifndef LCD_CFG_I2C
    lcd.stepRefresh();
#endif
}

/* ??? */
void halWriteSerial(char * s) {
#ifndef VENTSIM
    Serial.print(s);
#endif
}
