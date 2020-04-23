#include "serialWriter.h"
#include <SoftwareSerial.h>
#include "config.h"
#include "properties.h"
#include "pressure.h"
#include "log.h"

#define byte uint8_t

static SoftwareSerial monitor(RX_PIN, TX_PIN);

void serialInit()
{
    monitor.begin(9600);
}

void sendDataViaSerial()
{
    uint8_t dutyCycle = propGetDutyCycle();
    uint8_t bpm = propGetBpm();
    uint8_t ventStatus = propGetVent();
    uint16_t tidalVolume = pressGetTidalVolume();
    float pressure = pressGetVal(PRESSURE);
    float flow = pressGetVal(FLOW);

    monitor.write(0x23);
    monitor.write((uint8_t *)&ventStatus, sizeof(&ventStatus)); //send to serial
    monitor.write((uint8_t *)&dutyCycle, sizeof(&dutyCycle));  //send to serial
    monitor.write((uint8_t *)&bpm, sizeof(&bpm));
    monitor.write((uint8_t *)&pressure, sizeof(&pressure));
    monitor.write((uint8_t *)&flow, sizeof(&flow));
    monitor.write((uint8_t *)&tidalVolume, sizeof(&tidalVolume));
    monitor.write(0x24);
}