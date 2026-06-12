#ifndef AUDIO_FILTER_H
#define AUDIO_FILTER_H

#include <stdint.h>

typedef enum
{
    AUDIO_FILTER_NONE = 0,
    AUDIO_FILTER_LOW_PASS_FIR = 1,
    AUDIO_FILTER_HIGH_PASS_IIR = 2,
    AUDIO_FILTER_REVERB = 3,
    AUDIO_FILTER_BAND_PASS = 4
} AudioFilterType;

void audio_filter_init(AudioFilterType type);
void audio_filter_process_stereo(int16_t *left, int16_t *right);

#endif