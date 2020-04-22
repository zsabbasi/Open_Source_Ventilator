#include "serialWriter.h"
#include <SoftwareSerial.h>
#include "config.h"
#include "properties.h"
#include "pressure.h"
#include "log.h"

#define byte uint8_t

static SoftwareSerial monitor(RX_PIN, TX_PIN);

void serialInit() {
    monitor.begin(9600);
}

bool sendSerialBuff(float serialSendParams[6])
{
    monitor.write(0x23);
    for (int i = 0; i < 6; i++)
    {
        byte *b = (byte *)&serialSendParams[i];
        monitor.write(b[0]);
        monitor.write(b[1]);
        monitor.write(b[2]);
        monitor.write(b[3]);
    }
    monitor.write(0x24);
    return true;
}

void sendDataViaSerial()
{
    float gets[6] = {
        propGetDutyCycle(),
        propGetBpm(),
        pressGetVal(PRESSURE),
        pressGetVal(FLOW),
        pressGetTidalVolume(),
        propGetVent()};
    sendSerialBuff(gets); //send to serial the float array
}