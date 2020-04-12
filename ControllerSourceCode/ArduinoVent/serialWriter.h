#ifndef SERIALWRITER_H
#define SERIALWRITER_H

#include <SoftwareSerial.h>

SoftwareSerial monitor(RX_PIN, TX_PIN);

float serialSendParams[10];

void addtoSerialBuff(float gets[5])
{
    for (int i = 0; i < 5; i++)
        serialSendParams[i] = gets[i];
}

//send valuestoserialbuff
bool sendSerialBuff()
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

#endif // SERIALWRITER_H