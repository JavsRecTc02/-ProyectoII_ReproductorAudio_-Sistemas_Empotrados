#include <stdio.h>
#include <stdint.h>

#include "socal/socal.h"
#include "switch_control.h"

#define PIO_DATA_REG 0

extern volatile unsigned long *dipsw_pio_addr;

void switches_init(void)
{
    printf("[INFO] Switch filter control initialized\n");
    printf("[INFO] SW0 = Reverb\n");
    printf("[INFO] SW1 = Band Pass\n");
    printf("[INFO] SW2 = High Pass\n");
    printf("[INFO] SW3 = Low Pass\n");
    printf("[INFO] SW0-SW3 OFF = No filter\n");
}

/*
 * Lee los switches fisicos.
 * DIPSW_PIO tiene 10 bits, pero para filtros solo usamos SW0-SW3.
 */
uint32_t switches_read_raw(void)
{
    return alt_read_word(dipsw_pio_addr + PIO_DATA_REG) & 0xF;
}

AudioFilterType switches_get_selected_filter(void)
{
    uint32_t sw = switches_read_raw();

    /*
     * Todos los switches abajo:
     * audio normal, sin filtro.
     */
    if (sw == 0)
        return AUDIO_FILTER_NONE;

    /*
     * Si hay mas de un switch activo, se usa prioridad:
     *
     * SW0 > SW1 > SW2 > SW3
     *
     * Reverb > Pasa-banda > Pasa-altos > Pasa-bajos
     */
    if (sw & SW_REVERB)
        return AUDIO_FILTER_REVERB;

    if (sw & SW_BAND_PASS)
        return AUDIO_FILTER_BAND_PASS;

    if (sw & SW_HIGH_PASS)
        return AUDIO_FILTER_HIGH_PASS_IIR;

    if (sw & SW_LOW_PASS)
        return AUDIO_FILTER_LOW_PASS_FIR;

    return AUDIO_FILTER_NONE;
}

const char *switches_get_filter_name(AudioFilterType filter)
{
    switch (filter)
    {
    case AUDIO_FILTER_REVERB:
        return "REVERB";

    case AUDIO_FILTER_BAND_PASS:
        return "BAND_PASS";

    case AUDIO_FILTER_HIGH_PASS_IIR:
        return "HIGH_PASS_IIR";

    case AUDIO_FILTER_LOW_PASS_FIR:
        return "LOW_PASS_FIR";

    case AUDIO_FILTER_NONE:
    default:
        return "NONE";
    }
}