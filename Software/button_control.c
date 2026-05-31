#include <stdio.h>
#include <stdint.h>

#include "socal/socal.h"
#include "button_control.h"

#define PIO_DATA_REG           0
#define PIO_DIRECTION_REG      1
#define PIO_INTERRUPT_MASK_REG 2
#define PIO_EDGE_CAPTURE_REG   3

extern volatile unsigned long *button_pio_addr;

void buttons_init(void)
{
    // Habilitar interrupciones para KEY0, KEY1, KEY2 y KEY3
    alt_write_word(button_pio_addr + PIO_INTERRUPT_MASK_REG,
                   BTN_PLAY_PAUSE | BTN_STOP | BTN_NEXT | BTN_PREV);

    // Limpiar cualquier evento pendiente
    alt_write_word(button_pio_addr + PIO_EDGE_CAPTURE_REG, 0xF);

    printf("[INFO] Buttons initialized\n");
    printf("[INFO] KEY0 = Play/Pause\n");
    printf("[INFO] KEY1 = Stop\n");
    printf("[INFO] KEY2 = Next\n");
    printf("[INFO] KEY3 = Previous\n");
}

uint32_t buttons_get_events(void)
{
    uint32_t edge;

    edge = alt_read_word(button_pio_addr + PIO_EDGE_CAPTURE_REG) & 0xF;

    if (edge != 0)
    {
        // Limpiar eventos capturados
        alt_write_word(button_pio_addr + PIO_EDGE_CAPTURE_REG, edge);
    }

    return edge;
}