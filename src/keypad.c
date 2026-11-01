#include "pico/stdlib.h"
#include <hardware/gpio.h>
#include <stdio.h>
#include "queue.h"

// Global column variable
int col = -1;

// Global key state
static bool state[16]; // Are keys pressed/released

// Keymap for the keypad
const char keymap[17] = "DCBA#9630852*741";

// Defined here to avoid circular dependency issues with autotest
// You can see the struct definition in queue.h
KeyEvents kev = { 
    .head = 0, 
    .tail = 0 
};

void keypad_drive_column();
void keypad_isr();

/********************************************************* */
// Implement the functions below.

void keypad_init_pins() {
    for (int i = 2; i <= 5; i++)
    {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
    }

    for (int i = 6; i <= 9; i++)
    {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
        sio_hw->gpio_clr = (1u << i);
    }
    
}

void keypad_init_timer() {
    irq_set_exclusive_handler(TIMER0_IRQ_0, keypad_drive_column);
    irq_set_exclusive_handler(TIMER0_IRQ_1, keypad_isr);

    hw_set_bits(&timer0_hw->inte, (1u << 0 | 1u << 1));

    nvic_hw->icpr[TIMER0_IRQ_0 / 32] = (1u << (TIMER0_IRQ_0 % 32));
    nvic_hw->iser[TIMER0_IRQ_0 / 32] = (1u << (TIMER0_IRQ_0 % 32));

    nvic_hw->icpr[TIMER0_IRQ_1 / 32] = (1u << (TIMER0_IRQ_1 % 32));
    nvic_hw->iser[TIMER0_IRQ_1 / 32] = (1u << (TIMER0_IRQ_1 % 32));

    // irq_set_enabled(TIMER0_IRQ_0, true);
    // irq_set_enabled(TIMER0_IRQ_1, true);

    uint64_t target0 = timer0_hw->timerawl + 1000000;
    uint64_t target1 = timer0_hw->timerawl + 1010000;

    timer0_hw->alarm[0] = (uint32_t) target0;
    timer0_hw->alarm[1] = (uint32_t) target1;
}

void keypad_drive_column() {
    // fill in
    timer0_hw->intr = (1u << 0);

    col = (col + 1) % 4;

    uint32_t col_mask = (0xF << 6);
    sio_hw->gpio_togl = (sio_hw->gpio_out & col_mask) ^ (1u << (6 + col));

    timer0_hw->alarm[0] = timer0_hw->timerawl + 25000;
}

uint8_t keypad_read_rows() {
    // Make GP2 bit 0 and then only keep last 4 bits
    return (uint8_t) ((sio_hw->gpio_in >> 2) & (0x0F));
}

void keypad_isr() {
    timer0_hw->intr = (1u << 1);
    
    uint8_t row = keypad_read_rows();

    for (int i = 0; i <= 3; i++)
    {
        bool pressed = (row & (1u << i));
        
        if (pressed)
        {
            int key_index = i + (col * 4);
            if (!state[key_index])
            {
                state[key_index] = true;
                uint16_t event = (1u << 8) | (uint8_t) keymap[key_index];
                key_push(event);
            }
        }
        else
        {
            int key_index = i + (col * 4);
            if (state[key_index])
            {
                state[key_index] = false;
                uint16_t event =  (uint8_t) keymap[key_index];
                key_push(event);
            }
        }

    }

    timer0_hw->alarm[1] = timer0_hw->timerawl + 25000;
}