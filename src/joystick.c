#include "hardware/adc.h"
#include "hardware/dma.h"

extern const int ADC_X;
extern const int DMA_ADC_CH; 
extern int adc_fifo_out;

void init_adc() {
    // fill in
    adc_init();
    adc_gpio_init(ADC_X);
    adc_select_input(ADC_X-40);
    adc_run(true);
}

void init_dma() {
    // fill in
    dma_hw->ch[DMA_ADC_CH].read_addr = &adc_hw->fifo;
    dma_hw->ch[DMA_ADC_CH].write_addr = &adc_fifo_out;
    dma_hw->ch[DMA_ADC_CH].transfer_count = (1u << 28) | 1u;

    dma_hw->ch[DMA_ADC_CH].ctrl_trig = 0;
    uint32_t temp = 0;
    temp |= (1u << 2);
    temp |= (DREQ_ADC << 17);
    temp |= (1u << 0);

    dma_hw->ch[DMA_ADC_CH].ctrl_trig = temp;
}

void init_adc_dma() {
    // fill in
    init_dma();
    init_adc();
    adc_hw->fcs |= (1u << 0) + (1u << 3);
}
