#include <Arduino.h>
#include <SoftwareSerial.h>
#include "config.h"
#include "serialWriter.h"
#include "properties.h"
#include "pressure.h"
#include "log.h"
#include "breather.h"

#define byte uint8_t

static SoftwareSerial monitor(RX_PIN, TX_PIN);
struct telemetryEvent
{
  uint8_t ventStatus;
  uint8_t dutyCycle;
  uint8_t bpm;
  uint8_t peep;
  uint8_t phase;
  float pressure;
  float flow;
  uint16_t tidalVolume;
};

typedef struct telemetryEvent TelemetryEvent;

void serialInit()
{
    monitor.begin(9600);
}

void sendDataViaSerial()
{
    TelemetryEvent evt;

    evt.dutyCycle = propGetDutyCycle();
    evt.bpm = propGetBpm();
    evt.ventStatus = propGetVent();
    evt.tidalVolume = pressGetTidalVolume();
    evt.pressure = pressGetVal(PRESSURE);
    evt.flow = pressGetVal(FLOW);
    evt.peep = propGetDesiredPeep();
    evt.phase = (uint8_t)breatherGetState();
    uint8_t *evtBytes = (uint8_t*)&evt;
    
    monitor.write(0x23);
    monitor.write(0x23);
    monitor.write(evtBytes, sizeof(evt));
    monitor.write(0x24);
    monitor.write(0x24);
}