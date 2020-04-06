//required bmp280 (can be found from ide->manage libraries->install new)
//s to ms = /1000
#ifndef PRESSURE_H
#define PRESSURE_H

#define AVERAGE_BIN_NUMBER 10   // Number of averaging bins for the averaging routine
#define PRESSURE_READ_DELAY 20L // wait 20 ms between reads

void pressInit();
void pressLoop();

float pressGetFloatVal(); // in cmh2o
int pressGetRawVal();
// float getcmh20();
#endif // PRESSURE_H
