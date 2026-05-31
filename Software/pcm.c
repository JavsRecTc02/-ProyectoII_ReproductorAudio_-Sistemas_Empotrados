#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "pcm.h"
#include "audio_control.h"
#include "button_control.h"

typedef enum
{
    PLAYER_PLAYING = 0,
    PLAYER_PAUSED
} PlayerState;

PlayResult play_PCM(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "[ERROR] Open %s failed.\n", filename);
        return PLAY_RESULT_ERROR;
    }

    uint8_t sample[4];
    PlayerState state = PLAYER_PLAYING;

    printf("[INFO] Now playing PCM: %s\n", filename);

    while (1)
    {
        uint32_t events = buttons_get_events();

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
    }
}