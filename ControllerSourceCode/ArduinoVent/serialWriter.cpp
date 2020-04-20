#include "serialWriter.h"
#include <SoftwareSerial.h>
#include "config.h"
#include "properties.h"
#include "pressure.h"
#define byte uint8_t

SoftwareSerial monitor(RX_PIN, TX_PIN);

bool sendSerialBuff(float serialSendParams[5])
{
    monitor.write(0x23);
    for (int i = 0; i < 5; i++)
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
    float gets[5] = {
        propGetDutyCycle(),
        propGetBpm(),
        pressGetRawVal(PRESSURE),
        pressGetRawVal(FLOW),
        (float)propGetVent()};
    sendSerialBuff(gets); //send to serial the float array
}