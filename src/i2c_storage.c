
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "i2c_storage.h"
#define EEPROM_ADDR 0x57 //24AA32A 7-bit address with A0, A1, A2 high
#define EEPROM_MEM_ADDR 0x0000 //Where we are storing the score in EEPROM

void init_i2c() {
    //Initialize i2c1 controller with 100000 Hz clock speed
    i2c_init(i2c1, 100 * 1000);

    gpio_set_function(34, GPIO_FUNC_I2C); //SDA
    gpio_set_function(35, GPIO_FUNC_I2C); //SCL

    gpio_pull_up(34);
    gpio_pull_up(35);
}

//Save the high score to EEPROm memory adress
//Transfer 4 bytes, first 2 select the address to store the score, last two contain the score
bool save_high_score(uint16_t score) {
    uint8_t data[4];

    //EEPROM memory address
    data[0] = (EEPROM_MEM_ADDR >> 8) & 0xFF;
    data[1] = EEPROM_MEM_ADDR & 0xFF;

    //High score (2 bytes)
    data[2] = (score >> 8) & 0xFF;
    data[3] = score & 0xFF;

    //wrute to the memory address of where to store the score
    //Store the score starting at EEPROM memory address 0x0000
    int result = i2c_write_timeout_us(
    i2c1,
    EEPROM_ADDR,
    data,
    4,
    false,
    100000
    );

    if (result != 4) {
        printf("EEPROM write to 0x%02X failed: result=%d (expected 4)\n",
               EEPROM_ADDR, result);
        return false;
    }
    // Allow the internal write cycle to finish, then verify persistence.
    // An acknowledged transfer alone does not prove the memory was written.
    sleep_ms(5);
    uint16_t stored_score;
    for (int attempt = 0; attempt < 10; attempt++) {
        if (load_high_score(&stored_score) && stored_score == score) {
            return true;
        }
        sleep_ms(1);
    }
    printf("EEPROM write verification failed for score %u\n", (unsigned)score);
    return false;
}

bool load_high_score(uint16_t *score) {
    uint8_t addr[2] = {(EEPROM_MEM_ADDR >> 8) & 0xFF,
    EEPROM_MEM_ADDR & 0xFF};//Address of where data is stored
    uint8_t data[2];

    //Set eeprom memory address
    //Address that we read from to load the score
    //Use i2c1 on GPIO 34 (SDA) and GPIO 35 (SCL).
    int result = i2c_write_timeout_us(
    i2c1,
    EEPROM_ADDR,
    addr,
    2,
    true,
    100000  // 100 ms timeout
);
    
    if (result != 2) {
        printf("EEPROM address setup at 0x%02X failed: result=%d (expected 2)\n",
               EEPROM_ADDR, result);
        return false;
    }

    //read 2 bytes from eeeprom containing the score
    result = i2c_read_timeout_us(
    i2c1,
    EEPROM_ADDR,
    data,
    2,
    false,
    100000
);
    
    if (result != 2) {
        printf("EEPROM read from 0x%02X failed: result=%d (expected 2)\n",
               EEPROM_ADDR, result);
        return false;
    }
    //Set the value of score
    //score is 16 bits
    *score = (data[0] << 8) | data[1];
    return true;
}

bool reset_high_score(void) {
    return save_high_score(0);
}


