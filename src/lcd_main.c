#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "lcd.h"
#include "keypad.h"
#include "game.h"
#include <stdio.h>
#include "i2c_storage.h"
#include "score_ss7.h" 

/****************************************** */
#define PIN_SDI    27
#define PIN_CS     29
#define PIN_SCK    30
#define PIN_DC     32
#define PIN_nRESET 33
#define FRAME_US 33000 // Micro Second Gap Between Frames ~30 fps

//////////////////////////////////////////////////////////////////////////////
const int ADC_X = 45; //Up: high adc x value, vice versa
const int DMA_ADC_CH = 0; 
const int DMA_I2C_CH = 1;
/////////////////////////////////////////////////////////////////////////////

uint32_t adc_fifo_out = 0;

void init_adc_dma();

void init_spi_lcd() {
    gpio_set_function(PIN_CS, GPIO_FUNC_SIO);
    gpio_set_function(PIN_DC, GPIO_FUNC_SIO);
    gpio_set_function(PIN_nRESET, GPIO_FUNC_SIO);

    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_set_dir(PIN_nRESET, GPIO_OUT);

    gpio_put(PIN_CS, 1); // CS high
    gpio_put(PIN_DC, 0); // DC low
    gpio_put(PIN_nRESET, 1); // nRESET high

    // initialize SPI1 with 48 MHz clock
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SDI, GPIO_FUNC_SPI);
    spi_init(spi1, 100 * 1000 * 1000);
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

int main() {
    stdio_init_all();
    init_spi_lcd();
    LCD_Setup();
    LCD_Clear(0x0000); // Clear the screen to black

    keypad_init_pins();
    keypad_init_timer();
    game_init();
    init_adc_dma();


    //Initialize I2C to EEPROM
    init_i2c();

    //Initialize seven segment display
    //Load high score from EEPROM
    init_ss();

    //One note: currently the high score is set to be uint16 but the score tracker
    //in game is set to be uint32 which would cause truncation
    while (true)
    {
        uint32_t frame_start = time_us_32();

        game_tick();
        // Might want to move these out of main loop to avoid uneccesary calls
        uint32_t current_score = get_score(); 
        update_high_score(current_score);

        uint32_t frame_elapse = time_us_32() - frame_start;

        if (frame_elapse < FRAME_US)
        {
            sleep_us(FRAME_US - frame_elapse);
        }
    }
}