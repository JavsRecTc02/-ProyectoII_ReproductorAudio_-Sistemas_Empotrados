#include "volume_peakmeter.h"
#include "hps_0.h"
#include <stddef.h>

volatile uint32_t *peakmeter_base = NULL;

/*
 * Inicializa el puntero base.
 * lw_bridge_base es la dirección virtual del lightweight HPS-to-FPGA bridge.
 */
void peakmeter_init(void *lw_bridge_base) {
    peakmeter_base = (volatile uint32_t *)(
        (uint8_t *)lw_bridge_base + VOLUME_PEAKMETER_0_BASE
    );
}

/*
 * Escribe el estado de reproducción en el registro CTRL.
 * state: PLAY_STATE_STOP, PLAY_STATE_PLAY, PLAY_STATE_PAUSE
 * 
 * El hardware usa este valor para controlar LEDR[9]:
 *   - PLAY:  parpadea a 1Hz
 *   - PAUSE: fijo encendido
 *   - STOP:  apagado
 */
void peakmeter_set_play_state(uint8_t state) {
    if (peakmeter_base == NULL) return;
    /* CTRL está en offset 0x04 → índice 1 (uint32_t*) */
    peakmeter_base[1] = (uint32_t)(state & 0x3);
}

/*
 * Lee el nivel de volumen actual.
 * El encoder en hardware actualiza este valor de forma autónoma.
 * El software lo lee para enviarlo al WM8731 vía I2C.
 * Retorna un valor entre 0 y 31.
 */
uint8_t peakmeter_get_volume(void) {
    if (peakmeter_base == NULL) return 16;
    /* VOLUME está en offset 0x08 → índice 2 */
    return (uint8_t)(peakmeter_base[2] & 0x1F);
}

/*
 * Envía una muestra de audio al módulo para calcular el peak.
 * Se llama en el loop de reproducción por cada muestra enviada al DAC.
 * El hardware calcula el valor absoluto y actualiza los LEDs.
 */
void peakmeter_write_audio_sample(int16_t sample) {
    if (peakmeter_base == NULL) return;
    /* AUDIO está en offset 0x0C → índice 3 */
    peakmeter_base[3] = (uint32_t)(uint16_t)sample;
}

/*
 * Lee el peak actual (para mostrar en pantalla si se desea).
 * 8 bits, cada bit corresponde a un LED del peak meter.
 */
uint8_t peakmeter_get_peak(void) {
    if (peakmeter_base == NULL) return 0;
    /* STATUS está en offset 0x00 → índice 0 */
    return (uint8_t)(peakmeter_base[0] & 0xFF);
}