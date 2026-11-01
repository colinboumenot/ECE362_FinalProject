#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"

// Base library headers ncluded for your convenience.
// ** You may have to add more depending on your practical. **
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/rand.h"

//////////////////////////////////////////////////////////////////////////////
const int ADC_X = 45; //Up: high adc x value, vice versa
const int DMA_ADC_CH = 0; 
const int DMA_I2C_CH = 1;
/////////////////////////////////////////////////////////////////////////////

uint32_t adc_fifo_out = 0;

/////////////////////////////////////////////////////////////////////////////
void init_adc_dma();

int main()
{
    stdio_init_all();
    init_adc_dma();

    for(;;) {
        printf("adc test: %ld\n", adc_fifo_out);
        fflush(stdout);
        sleep_ms(1000);
    }
    return 0;
}