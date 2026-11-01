
#ifndef KEYPAD_H
#define KEYPAD_H

#include <hardware/timer.h>
#include "pico/stdlib.h"

// Basic queue structure for tracking events
// Tracks 32 events, with each event being {pressed, key}
typedef struct {
    uint16_t q[32];
    uint16_t head;
    uint16_t tail;
} KeyEvents;

extern KeyEvents kev; // Global key event queue 

bool key_available();
uint16_t key_pop();
void key_push(uint16_t value);


#endif