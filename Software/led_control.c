#include <stdio.h>
#include <stdint.h>

#include "socal/socal.h"
#include "led_control.h"

#define PIO_DATA_REG 0

#define LED_FILTER_ACTIVE_MASK (1u << 8)
#define LED_ALL_MASK           0x3FFu

extern volatile unsigned long *led_pio_addr;

void leds_init(void)
{
    uint32_t value;

    /*
     * Leer estado actual de LEDs.
     * El PIO tiene 10 bits, por eso se usa mascara 0x3FF.
     */
    value = alt_read_word(led_pio_addr + PIO_DATA_REG) & LED_ALL_MASK;

    /*
     * Apagar LED9 al iniciar.
     */
    value &= ~LED_FILTER_ACTIVE_MASK;

    alt_write_word(led_pio_addr + PIO_DATA_REG, value);

    printf("[INFO] LED filter indicator initialized\n");
    printf("[INFO] LED9 = Filter active indicator\n");
    printf("[INFO] LED initial value = 0x%03X\n", value);
}

void led_filter_set_active(int active)
{
    uint32_t value;

    /*
     * Leer el valor actual para no modificar LED0-LED8.
     */
    value = alt_read_word(led_pio_addr + PIO_DATA_REG) & LED_ALL_MASK;

    if (active)
    {
        value |= LED_FILTER_ACTIVE_MASK;
    }
    else
    {
        value &= ~LED_FILTER_ACTIVE_MASK;
    }

    alt_write_word(led_pio_addr + PIO_DATA_REG, value);

    /*
     * Debug opcional.
     * Puedes dejarlo mientras pruebas y luego comentarlo.
     */
    printf("[DEBUG] LED9 %s, led_pio = 0x%03X\n",
           active ? "ON" : "OFF",
           value);
}

void led_filter_on(void)
{
    led_filter_set_active(1);
}

void led_filter_off(void)
{
    led_filter_set_active(0);
}

void led_debug_write(uint32_t value)
{
    value &= LED_ALL_MASK;
    alt_write_word(led_pio_addr + PIO_DATA_REG, value);

    printf("[DEBUG] LED write = 0x%03X\n", value);
}

uint32_t led_debug_read(void)
{
    uint32_t value;

    value = alt_read_word(led_pio_addr + PIO_DATA_REG) & LED_ALL_MASK;

    printf("[DEBUG] LED read = 0x%03X\n", value);

    return value;
}