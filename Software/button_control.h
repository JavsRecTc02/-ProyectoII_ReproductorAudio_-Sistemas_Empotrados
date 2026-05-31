#ifndef BUTTON_CONTROL_H
#define BUTTON_CONTROL_H

#include <stdint.h>

#define BTN_PLAY_PAUSE 0x1  // KEY0
#define BTN_STOP       0x2  // KEY1
#define BTN_NEXT       0x4  // KEY2
#define BTN_PREV       0x8  // KEY3

void buttons_init(void);
uint32_t buttons_get_events(void);

#endif