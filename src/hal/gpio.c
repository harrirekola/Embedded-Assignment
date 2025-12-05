#include <avr/io.h>
#include "gpio.h"

static volatile uint8_t* ddr_for_pin(GpioPin p) {
    switch (p) {
        case PIN_D0: return &DDRD;
        case PIN_D1: return &DDRD;
        case PIN_D2: return &DDRD;
        case PIN_D3: return &DDRD;
        case PIN_D4: return &DDRD;
        case PIN_D5: return &DDRD;
        case PIN_D6: return &DDRD;
        case PIN_D7: return &DDRD;
        case PIN_D8: return &DDRB;
        case PIN_D9: return &DDRB;
        case PIN_D10: return &DDRB;
        case PIN_D11: return &DDRB;
        case PIN_D12: return &DDRB;
        case PIN_D13: return &DDRB;
        case PIN_A0: return &DDRC;
        case PIN_A1: return &DDRC;
        case PIN_A2: return &DDRC;
        case PIN_A3: return &DDRC;
        case PIN_A4: return &DDRC;
        case PIN_A5: return &DDRC;
    }
    return &DDRD;
}
static volatile uint8_t* port_for_pin(GpioPin p) {
    switch (p) {
        case PIN_D0: return &PORTD;
        case PIN_D1: return &PORTD;
        case PIN_D2: return &PORTD;
        case PIN_D3: return &PORTD;
        case PIN_D4: return &PORTD;
        case PIN_D5: return &PORTD;
        case PIN_D6: return &PORTD;
        case PIN_D7: return &PORTD;
        case PIN_D8: return &PORTB;
        case PIN_D9: return &PORTB;
        case PIN_D10: return &PORTB;
        case PIN_D11: return &PORTB;
        case PIN_D12: return &PORTB;
        case PIN_D13: return &PORTB;
        case PIN_A0: return &PORTC;
        case PIN_A1: return &PORTC;
        case PIN_A2: return &PORTC;
        case PIN_A3: return &PORTC;
        case PIN_A4: return &PORTC;
        case PIN_A5: return &PORTC;
    }
    return &PORTD;
}
static volatile uint8_t* pin_for_pin(GpioPin p) {
    switch (p) {
        case PIN_D0: return &PIND;
        case PIN_D1: return &PIND;
        case PIN_D2: return &PIND;
        case PIN_D3: return &PIND;
        case PIN_D4: return &PIND;
        case PIN_D5: return &PIND;
        case PIN_D6: return &PIND;
        case PIN_D7: return &PIND;
        case PIN_D8: return &PINB;
        case PIN_D9: return &PINB;
        case PIN_D10: return &PINB;
        case PIN_D11: return &PINB;
        case PIN_D12: return &PINB;
        case PIN_D13: return &PINB;
        case PIN_A0: return &PINC;
        case PIN_A1: return &PINC;
        case PIN_A2: return &PINC;
        case PIN_A3: return &PINC;
        case PIN_A4: return &PINC;
        case PIN_A5: return &PINC;
    }
    return &PIND;
}
static uint8_t bit_for_pin(GpioPin p) {
    switch (p) {
        case PIN_D0: return 0;
        case PIN_D1: return 1;
        case PIN_D2: return 2;
        case PIN_D3: return 3;
        case PIN_D4: return 4;
        case PIN_D5: return 5;
        case PIN_D6: return 6;
        case PIN_D7: return 7;
        case PIN_D8: return 0;
        case PIN_D9: return 1;
        case PIN_D10: return 2;
        case PIN_D11: return 3;
        case PIN_D12: return 4;
        case PIN_D13: return 5;
        case PIN_A0: return 0;
        case PIN_A1: return 1;
        case PIN_A2: return 2;
        case PIN_A3: return 3;
        case PIN_A4: return 4;
        case PIN_A5: return 5;
    }
    return 0;
}

void gpio_pin_mode(GpioPin pin, GpioMode mode) {
    volatile uint8_t* ddr = ddr_for_pin(pin);
    volatile uint8_t* port = port_for_pin(pin);
    uint8_t bit = bit_for_pin(pin);
    if (mode == GPIO_OUTPUT) {
        *ddr |= (1<<bit);
    } else {
        *ddr &= ~(1<<bit);
        if (mode == GPIO_INPUT_PULLUP) { 
            *port |= (1<<bit); 
        } 
        else { 
            *port &= ~(1<<bit); 
        }
    }
}

void gpio_write(GpioPin pin, GpioLevel level) {
    volatile uint8_t* port = port_for_pin(pin);
    uint8_t bit = bit_for_pin(pin);
    if (level == GPIO_HIGH) {
         *port |= (1<<bit); 
        } 
    else { *port &= ~(1<<bit); 
    }
}

GpioLevel gpio_read(GpioPin pin) {
    volatile uint8_t* in = pin_for_pin(pin);
    uint8_t bit = bit_for_pin(pin);
    return ((*in & (1<<bit)) ? GPIO_HIGH : GPIO_LOW);
}
