#include "hardware/adc.h"
#include "hardware/dma.h"

extern const int ADC_X;

void init_adc() {
    // fill in
    adc_init();
    adc_gpio_init(ADC_X);
    adc_select_input(ADC_X-40);
}

uint16_t read_adc() {
    // fill in / replace
    return adc_read();
}
