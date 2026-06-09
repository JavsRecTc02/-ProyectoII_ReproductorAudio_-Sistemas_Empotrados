#include <stdio.h>
#include <stdint.h>

#include "led_control.h"
#include "volume_peakmeter.h"

/*
 * led_control.c
 *
 * El indicador de filtro activo ahora se controla via el modulo
 * volume_peakmeter (registro CTRL, bit 2) en lugar del led_pio
 * generico. Esto permite que LEDR[0] sea controlado por hardware
 * de forma consistente con el resto de los LEDs.
 */

void leds_init(void)
{
    /* El modulo peakmeter ya inicializa filter_active=0 en reset */
    printf("[INFO] LED filter indicator initialized (via peakmeter module)\n");
    printf("[INFO] LEDR[0] = Filter active indicator\n");
}

void led_filter_set_active(int active)
{
    if (peakmeter_base == NULL)
        return;

    /*
     * Leer play_state actual del registro CTRL para no pisarlo.
     * CTRL[1:0] = play_state
     * CTRL[2]   = filter_active
     */
    uint32_t ctrl = peakmeter_base[1] & 0x3;

    if (active)
        ctrl |= 0x4;
    else
        ctrl &= ~0x4;

    peakmeter_base[1] = ctrl;

    printf("[DEBUG] LED filtro %s (CTRL=0x%X)\n",
           active ? "ON" : "OFF", ctrl);
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
    if (peakmeter_base == NULL) return;
    /* Escribe directamente al registro CTRL preservando play_state */
    uint32_t ctrl = peakmeter_base[1] & 0x3;
    if (value & 0x1) ctrl |= 0x4;
    else ctrl &= ~0x4;
    peakmeter_base[1] = ctrl;
    printf("[DEBUG] LED write = 0x%03X\n", value);
}

uint32_t led_debug_read(void)
{
    if (peakmeter_base == NULL) return 0;
    uint32_t value = peakmeter_base[1] & 0x4 ? 1 : 0;
    printf("[DEBUG] LED read = 0x%03X\n", value);
    return value;
}