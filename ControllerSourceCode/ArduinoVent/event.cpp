
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

#include "event.h"
#include <string.h>
#include "log.h"

//----------- Locals -------------

#define QUEUE_SIZE 6
#define NUM_MAX_LISTENERS 4

//extern void LOG(const char * txt);

//------------ Global -----------
static event_t eventQueue[QUEUE_SIZE];
static int eventQueueIndexIn = 0;
static int eventQueueIndexOut = 0;
static int eventQueueIndexCount = 0;

static CEvent * listeners[NUM_MAX_LISTENERS];
static int numListeners = 0;

/* dispatch all events */
void eventDispatchAll() {
    int i;
    propagate_t ret;
    while (eventQueueIndexCount > 0) {
        for (i = 0; i < numListeners; i++) {
            ret = listeners[i]->onEvent(&eventQueue[eventQueueIndexOut]);
            if (ret == PROPAGATE_STOP)
                break;
        }

        eventQueueIndexOut++;
        if (eventQueueIndexOut >= QUEUE_SIZE) eventQueueIndexOut = 0;
        eventQueueIndexCount--;
    }
}

/* ??? */
CEvent::CEvent() {
    if (numListeners < NUM_MAX_LISTENERS) {
        listeners[numListeners++] = this;
    } else {
        LOG("critical error, no room for CEvent");
    }
}

/* ??? */
void CEvent::post(eventType_t type, int iParam) {
    event_t event;
    event.type = type;
    event.param.iParam = iParam;
    post(&event);
}

/* ??? */
void CEvent::post(eventType_t type, uint64_t lParam) {
    event_t event;
    event.type = type;
    event.param.lParam = lParam;
    post(&event);
}

/* ??? */
void CEvent::post(eventType_t type, char * tParam) {
    event_t event;
    event.type = type;
    memcpy(event.param.tParam, tParam, sizeof(event.param.tParam));
    post(&event);
}

/* ??? */
void CEvent::post (event_t * event) {
    if (eventQueueIndexCount >= NUM_MAX_LISTENERS) {
        LOG("critical error: Event queue full");
        return;
    }
    event_t *queueIn = &eventQueue[eventQueueIndexIn++];
    eventQueueIndexCount++;
    if (eventQueueIndexIn >= QUEUE_SIZE) eventQueueIndexIn = 0;

    *queueIn = *event;
}

CEvent::~CEvent() {

}

/* ??? why pass the event parameter ??? */
propagate_t CEvent::onEvent(event_t * event) {
    return PROPAGATE;
}
