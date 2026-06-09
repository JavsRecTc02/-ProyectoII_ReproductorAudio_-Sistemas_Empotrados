#ifndef VOLUME_PEAKMETER_H
#define VOLUME_PEAKMETER_H

#include <stdint.h>
#include "hps_0.h"

/*
 * volume_peakmeter.h
 * 
 * Interfaz de software para el módulo personalizado Volume & Peak Meter.
 * 
 * El módulo vive en la FPGA y es accesible desde el HPS mediante
 * escrituras/lecturas a direcciones de memoria mapeadas (Avalon-MM).
 * 
 * El puntero base debe ser inicializado antes de usar cualquier función.
 */

extern volatile uint32_t *peakmeter_base;

/* Inicializa el puntero base del módulo */
void peakmeter_init(void *lw_bridge_base);

/* Escribe el estado de reproducción al módulo (controla LEDR[9]) */
void peakmeter_set_play_state(uint8_t state);

/* Lee el nivel de volumen actual del encoder (0-31) */
uint8_t peakmeter_get_volume(void);

/* Envía una muestra de audio para el cálculo del peak meter */
void peakmeter_write_audio_sample(int16_t sample);

void apply_volume_if_changed(void);

/* Lee el peak actual (8 bits, uno por LED) */
uint8_t peakmeter_get_peak(void);

uint8_t peakmeter_get_button_event(void);

#endif /* VOLUME_PEAKMETER_H */