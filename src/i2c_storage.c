
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdint.h>
#include <stdbool.h>

#define EEPROM_ADDR 0x50 //Address of the EEPROM
#define EEPROM_MEM_ADDR 0x0000 //Where we are storing the score in EEPROM

void init_i2c() {
    i2c_init(i2c0, 100 * 1000);

    gpio_set_function(4, GPIO_FUNC_I2C); //SDA
    gpio_set_function(5, GPIO_FUNC_I2C); //SCL

    gpio_pull_up(4);
    gpio_pull_up(5);
}

//Save the high score to EEPROm memory adress
//Transfer 4 bytes, first 2 select the address to store the score, last two contain the score
bool save_high_score(uint16_t score) {
    uint8_t data[4];

    //EEPROM memory address
    data[0] = 0x00;
    data[1] = 0x00;

    //High score (2 bytes)
    data[2] = (score >> 8) & 0xFF;
    data[3] = score & 0xFF;

    //wrute to the memory address of where to store the score
    //Store the score at data[0] and data[1]
    int result = i2c_write_blocking(
        i2c0,
        EEPROM_ADDR,
        data,
        4,
        false
    );

    if (result != 4) {
        return false;
    }

    //Allow eeprom internal write cycle to finush
    sleep_ms(5);
    return true;
}

bool load_high_score(uint16_t *score) {
    uint8_t addr[2] = {0x00, 0x00};//Address of where data is stored
    uint8_t data[2];

    //Set eeprom memory address
    //Address that we read from to load the score
    int result = i2c_write_blocking(
        i2c0,
        EEPROM_ADDR,
        addr,
        2,
        true
    );

    if (result != 2) {
        return false;
    }

    //read 2 bytes from eeeprom containing the score
    result = i2c_read_blocking(
        i2c0,
        EEPROM_ADDR,
        data,
        2,
        false
    );

    if (result != 2) {
        return false;
    }

    //Set the value of score
    //score is 16 bits
    *score = (data[0] << 8) | data[1];
    return true;
}


//Write and display score on seven segment display
void init_ss(){
    //Load the high score
    uint32_t high_score = 0;
    bool success = load_high_score(&high_score);

}


