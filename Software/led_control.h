#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdint.h>

void leds_init(void);
void led_filter_set_active(int active);
void led_filter_on(void);
void led_filter_off(void);
void led_debug_write(uint32_t value);
uint32_t led_debug_read(void);

#endif /* LED_CONTROL_H */