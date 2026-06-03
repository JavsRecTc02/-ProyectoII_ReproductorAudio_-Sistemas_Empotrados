#ifndef DISPLAY_CONTROL_H
#define DISPLAY_CONTROL_H

#include <stdint.h>

void display_init(void);
void display_clear(void);
void display_show_song_time(int song_number, int elapsed_seconds);
int display_get_pcm_duration_seconds(const char *pcm_file);

#endif