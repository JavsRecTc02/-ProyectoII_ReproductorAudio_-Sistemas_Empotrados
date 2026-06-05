#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "pcm.h"
#include "audio_control.h"
#include "button_control.h"
#include "display_control.h"
#include "audio_filter.h"
#include "switch_control.h"
#include "led_control.h"
#define PCM_SAMPLE_RATE 32000

typedef enum
{
    PLAYER_PLAYING = 0,
    PLAYER_PAUSED
} PlayerState;

PlayResult play_PCM(const char *filename, int song_number)
{
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "[ERROR] Open %s failed.\n", filename);
        return PLAY_RESULT_ERROR;
    }

    uint32_t frames_played = 0;
    int elapsed_seconds = 0;
    int last_displayed_second = -1;

    display_show_song_time(song_number, 0);

    /*
     * Inicializar filtro 
     * AUDIO_FILTER_NONE
     * AUDIO_FILTER_LOW_PASS_FIR
     * AUDIO_FILTER_HIGH_PASS_IIR
     * AUDIO_FILTER_REVERB
     * AUDIO_FILTER_BAND_PASS
     */

    // audio_filter_init(AUDIO_FILTER_BAND_PASS);
    AudioFilterType current_filter = switches_get_selected_filter();
    audio_filter_init(current_filter);
    led_filter_set_active(current_filter != AUDIO_FILTER_NONE);
    printf("[INFO] Audio filter: %s\n", switches_get_filter_name(current_filter));

    uint8_t sample[4];
    PlayerState state = PLAYER_PLAYING;

    printf("[INFO] Now playing PCM: %s\n", filename);

    while (1)
    {
        uint32_t events = buttons_get_events();

        if ((frames_played % 1024) == 0)
        {
            AudioFilterType selected_filter = switches_get_selected_filter();

            if (selected_filter != current_filter)
            {
                current_filter = selected_filter;
                audio_filter_init(current_filter);

                led_filter_set_active(current_filter != AUDIO_FILTER_NONE);

                printf("[INFO] Audio filter changed: %s\n",
                    switches_get_filter_name(current_filter));

                AUDIO_FifoClear();
            }
        }

        if (events & BTN_PLAY_PAUSE)
        {
            if (state == PLAYER_PLAYING)
            {
                state = PLAYER_PAUSED;
                printf("[INFO] Paused\n");
                AUDIO_FifoClear();
            }
            else
            {
                state = PLAYER_PLAYING;
                printf("[INFO] Playing\n");
            }

            usleep(200 * 1000); // debounce
        }

        if (events & BTN_STOP)
        {
            printf("[INFO] Stop requested\n");
            AUDIO_FifoClear();
            fclose(fp);
            return PLAY_RESULT_STOP;
        }

        if (events & BTN_NEXT)
        {
            printf("[INFO] Next requested\n");
            AUDIO_FifoClear();
            fclose(fp);
            return PLAY_RESULT_NEXT;
        }

        if (events & BTN_PREV)
        {
            printf("[INFO] Previous requested\n");
            AUDIO_FifoClear();
            fclose(fp);
            return PLAY_RESULT_PREVIOUS;
        }

        if (state == PLAYER_PAUSED)
        {
            usleep(10 * 1000);
            continue;
        }

        /*
         * Cada frame PCM tiene 4 bytes:
         *
         * byte 0-1: canal izquierdo, signed 16-bit little endian
         * byte 2-3: canal derecho, signed 16-bit little endian
         */

        if (fread(sample, 1, 4, fp) != 4)
        {
            printf("[INFO] Song finished\n");
            fclose(fp);
            return PLAY_RESULT_FINISHED;
        }

        int16_t sample_l = (int16_t)((uint16_t)sample[0] |
                                     ((uint16_t)sample[1] << 8));

        int16_t sample_r = (int16_t)((uint16_t)sample[2] |
                                     ((uint16_t)sample[3] << 8));

        audio_filter_process_stereo(&sample_l, &sample_r);
        int try_cnt = 0;

        while (!AUDIO_DacFifoNotFull() && try_cnt < MAX_TRY_CNT)
        {
            events = buttons_get_events();

            if (events & BTN_PLAY_PAUSE)
            {
                state = PLAYER_PAUSED;
                printf("[INFO] Paused\n");
                AUDIO_FifoClear();
                usleep(200 * 1000);
                break;
            }

            if (events & BTN_STOP)
            {
                printf("[INFO] Stop requested\n");
                AUDIO_FifoClear();
                fclose(fp);
                return PLAY_RESULT_STOP;
            }

            if (events & BTN_NEXT)
            {
                printf("[INFO] Next requested\n");
                AUDIO_FifoClear();
                fclose(fp);
                return PLAY_RESULT_NEXT;
            }

            if (events & BTN_PREV)
            {
                printf("[INFO] Previous requested\n");
                AUDIO_FifoClear();
                fclose(fp);
                return PLAY_RESULT_PREVIOUS;
            }

            try_cnt++;
        }

        if (state == PLAYER_PAUSED)
        {
            continue;
        }

        if (try_cnt >= MAX_TRY_CNT)
        {
            fprintf(stderr, "[ERROR] Audio FIFO timeout...\n");
            fclose(fp);
            return PLAY_RESULT_ERROR;
        }

        AUDIO_DacFifoSetData(sample_l, sample_r);

        frames_played++;

        elapsed_seconds = frames_played / PCM_SAMPLE_RATE;

        if (elapsed_seconds != last_displayed_second)
        {
            display_show_song_time(song_number, elapsed_seconds);
            last_displayed_second = elapsed_seconds;
        }
    }
}