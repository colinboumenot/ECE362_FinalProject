
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "hardware/structs/sio.h"
#include <stdint.h>
#include <stdbool.h>

#include "i2c_storage.h"

//Define the gpio pin numbers for seven segment display, switch to what is available
//D1-D8 = GPIO 10-17
//SEL0 = GPIO 18
//SEL1 = GPIO 19
//SEL2 = GPIO 20



#define DISPLAY_FIRST_PIN 10
#define DISPLAY_PIN_MASK (0x7FFu << 10)


//Provided by font.s
//Maps ascii character to its display on seven segment
extern char font[];

//Display buffer
//msg contains the characters we want to display on each segment which is trasferred
//to correct display by font
static uint8_t msg[8] = {0};
static int index = 0;

// Current high score
static uint16_t high_score = 0;


//Initialize SS display pins
void display_init_pins() {
    for (int i = DISPLAY_FIRST_PIN;
         i < DISPLAY_FIRST_PIN + 11;
         i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
        gpio_put(i, 0);
    }
}

//Convert high score to seven segment display patterns
void display_high_score(uint16_t score) {
    uint8_t digits[8];

    //Extract decimal digits
    for (int i = 7; i >= 0; i--) {
        digits[i] = score % 10;
        score /= 10;
    }

    //Convert digits to seven-segment patterns
    for (int i = 0; i < 8; i++) {
        msg[i] = (uint8_t)font['0' + digits[i]];
    }
}

//Seven segment interrupt
//Constantly refreshing the seven segment display
void display_isr() {
    //Acknowledge TIMER1 alarm 0 interrupt
    timer1_hw->intr = 1u << 0;

    //Combine digit index and segment pattern
    uint32_t value =
        ((uint32_t)index << 8) | msg[index];

    //Shift to GPIO 10-20
    value <<= DISPLAY_FIRST_PIN;

    //Clear old display output
    sio_hw->gpio_clr = DISPLAY_PIN_MASK;

    //Set new display output
    sio_hw->gpio_set = value;

    //Move to next digit, wrap after 7
    index = (index + 1) & 0x7;

    //Schedule next interrupt in 3 milliseconds
    uint32_t time = timer1_hw->timerawl;
    timer1_hw->alarm[0] = time + 3000;
}


//Initialize timer 1
void display_init_timer() {
    // Register interrupt handler
    irq_set_exclusive_handler(
        TIMER1_IRQ_0,
        display_isr
    );

    // Enable TIMER1 alarm 0 interrupt
    hw_set_bits(&timer1_hw->inte, 1u << 0);

    // Clear any pending interrupt
    irq_set_enabled(TIMER1_IRQ_0, false);
    irq_clear(TIMER1_IRQ_0);

    // Schedule first interrupt
    uint32_t time = timer1_hw->timerawl;
    timer1_hw->alarm[0] = time + 3000;

    // Enable interrupt
    irq_set_enabled(TIMER1_IRQ_0, true);
}


//Initialize seven segment display
void init_ss() {
    display_init_pins();

    // Load high score from EEPROM
    bool success = load_high_score(&high_score);

    //If ECEPROM is erased or load high score is not a success
    if (!success || high_score == 0xFFFF) {
        high_score = 0;
    }

    // Convert high score into display patterns
    display_high_score(high_score);

    // Start multiplexing
    display_init_timer();
}


//Update high score if current score > high score
bool update_high_score(uint16_t current_score) {
    if (current_score <= high_score) {
        return true;
    }

    //Save new score to EEPROM
    if (!save_high_score(current_score)) {
        printf("HighSCORE SAVED\n");
        return false;
    }

    high_score = current_score;

    //Update seven-segment display
    display_high_score(high_score);

    return true;
}