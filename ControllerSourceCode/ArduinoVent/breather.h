#ifndef BREATHER_H
#define BREATHER_H

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
typedef enum {
    BREATHER_STATE_STOPPED,
    BREATHER_STATE_IN,
    BREATHER_STATE_WAIT_TO_OUT,
    BREATHER_STATE_OUT,
    BREATHER_STATE_INITIAL_FAST_CALIBRATION,
    BREATHER_STATE_FAST_CALIBRATION,
    BREATHER_STATE_PAUSE,
    BREATHER_STATE_STOPPING,
} breatherState_t;

void breatherLoop();
void breatherStartCycle();
breatherState_t breatherGetState();
int breatherGetProgress();
void breatherRequestFastCalibration();

#endif // BREATHER_H
