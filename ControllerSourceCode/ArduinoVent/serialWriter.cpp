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
    int dutyCycle = propGetDutyCycle();
    int bpm = propGetBpm();
    float pressure = pressGetVal(PRESSURE);
    float flow = pressGetVal(FLOW);
    uint16_t tidalVolume = pressGetTidalVolume();

    monitor.write(0x23);
    monitor.write((uint8_t *) &dutyCycle, 4); //send to serial
    monitor.write((uint8_t *) &bpm, 4);
    monitor.write((uint8_t *) &pressure, 4);
    monitor.write((uint8_t *) &flow, 4);
    monitor.write((uint8_t *) &tidalVolume, 2);
    monitor.write(0x24);
}