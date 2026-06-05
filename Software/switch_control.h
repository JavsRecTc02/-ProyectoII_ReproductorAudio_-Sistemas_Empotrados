#ifndef SWITCH_CONTROL_H
#define SWITCH_CONTROL_H

#include <stdint.h>
#include "audio_filter.h"

#define SW_REVERB     0x1  /* SW0 */
#define SW_BAND_PASS  0x2  /* SW1 */
#define SW_HIGH_PASS  0x4  /* SW2 */
#define SW_LOW_PASS   0x8  /* SW3 */

void switches_init(void);
uint32_t switches_read_raw(void);
AudioFilterType switches_get_selected_filter(void);
const char *switches_get_filter_name(AudioFilterType filter);

#endif