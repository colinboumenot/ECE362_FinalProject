#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "lcd.h"
#include "keypad.h"
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <math.h>   

/****************************************** */
#define PIN_SDI    27
#define PIN_CS     29
#define PIN_SCK    30
#define PIN_DC     32
#define PIN_nRESET 33


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

    while (true)
    {
        game_tick();
        sleep_ms(30);
    }
}