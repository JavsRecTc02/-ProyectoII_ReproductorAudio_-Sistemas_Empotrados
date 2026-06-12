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
#include <string.h>
#include "volume_peakmeter.h"
#include "lcd_i2c.h"

/*
 * Offset  Tamaño  Descripcion
 *   0       4     "RIFF"
 *   4       4     Tamaño total - 8 (little endian)
 *   8       4     "WAVE"
 *  12       4     "fmt "
 *  16       4     Tamaño del chunk fmt (16 para PCM)
 *  20       2     Audio format (1 = PCM)
 *  22       2     Num canales
 *  24       4     Sample rate
 *  28       4     Byte rate
 *  32       2     Block align
 *  34       2     Bits per sample
 *  36       4     "data"
 *  40       4     Tamaño del chunk data
 */
typedef struct
{
    char    title[64];
    char    artist[64];
    char    album[64];
    int     sample_rate;
    int     num_channels;
    int     bits_per_sample;
    int     duration_seconds;
} WavMetadata;

typedef enum {
    META_TITLE = 0,
    META_ARTIST,
    META_ALBUM,
    META_DURATION,
    META_COUNT
} MetaField;


#define PCM_SAMPLE_RATE 32000

typedef enum
{
    PLAYER_PLAYING = 0,
    PLAYER_PAUSED
} PlayerState;

static const char *meta_field_name(MetaField field)
{
    switch (field)
    {
        case META_TITLE:    return "TITLE";
        case META_ARTIST:   return "ARTIST";
        case META_ALBUM:    return "ALBUM";
        case META_DURATION: return "DURATION";
        default:            return "UNKNOWN";
    }
}

static void lcd_show_meta(const WavMetadata *meta, MetaField field)
{
    char line1[17];
    char line2[17];

    memset(line1, 0, sizeof(line1));
    memset(line2, 0, sizeof(line2));

    lcd_clear();

    switch (field)
    {
        case META_TITLE:
            snprintf(line1, sizeof(line1), "Titulo:");
            strncpy(line2, meta->title, 16);
            break;

        case META_ARTIST:
            snprintf(line1, sizeof(line1), "Artista:");
            strncpy(line2, meta->artist, 16);
            break;

        case META_ALBUM:
            snprintf(line1, sizeof(line1), "Album:");
            strncpy(line2, meta->album, 16);
            break;

        case META_DURATION:
            snprintf(line1, sizeof(line1), "Duracion:");
            snprintf(line2, sizeof(line2), "%02d:%02d",
                     meta->duration_seconds / 60,
                     meta->duration_seconds % 60);
            break;

        default:
            return;
    }

    line1[16] = '\0';
    line2[16] = '\0';

    printf("[LCD] field=%s | line1=\"%s\" | line2=\"%s\"\n",
           meta_field_name(field), line1, line2);

    lcd_set_cursor(0, 0);
    lcd_print(line1);

    lcd_set_cursor(0, 1);
    lcd_print(line2);
}

static void wav_get_path_from_pcm(const char *pcm_path,
                                   char *wav_path,
                                   int maxlen)
{
    /*
     * El pcm se genera como: "cancion.wav.pcm"
     * El wav original es:    "cancion.wav"
     * Solo hay que quitar el ".pcm" del final.
     */
    int len;

    strncpy(wav_path, pcm_path, maxlen - 1);
    wav_path[maxlen - 1] = '\0';

    len = strlen(wav_path);

    if (len > 4 && strcmp(wav_path + len - 4, ".pcm") == 0)
        wav_path[len - 4] = '\0';
}

static void wav_parse_metadata(const char *wav_path, WavMetadata *meta)
{
    FILE   *fp;
    uint8_t buf[44];
    uint32_t chunk_id;
    uint32_t chunk_size;
    uint16_t audio_format;
    uint32_t data_size;

    /* Valores por defecto */
    strncpy(meta->title,   "Unknown", sizeof(meta->title)  - 1);
    strncpy(meta->artist,  "Unknown", sizeof(meta->artist) - 1);
    strncpy(meta->album,   "Unknown", sizeof(meta->album)  - 1);
    meta->sample_rate     = 0;
    meta->num_channels    = 0;
    meta->bits_per_sample = 0;
    meta->duration_seconds = 0;

    fp = fopen(wav_path, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "[WARN] Could not open WAV for metadata: %s\n", wav_path);

        /*
         * Intentar extraer titulo del nombre de archivo como fallback.
         * Formato soportado: "Artista - Titulo.wav"
         */
        const char *filename = strrchr(wav_path, '/');
        if (filename) filename++;
        else          filename = wav_path;

        const char *sep = strstr(filename, " - ");
        const char *dot = strrchr(filename, '.');

        if (sep && dot && sep < dot)
        {
            int len;

            len = sep - filename;
            if (len >= (int)sizeof(meta->artist))
                len = sizeof(meta->artist) - 1;
            strncpy(meta->artist, filename, len);
            meta->artist[len] = '\0';

            sep += 3;
            len  = dot - sep;
            if (len >= (int)sizeof(meta->title))
                len = sizeof(meta->title) - 1;
            strncpy(meta->title, sep, len);
            meta->title[len] = '\0';
        }
        else
        {
            /* Solo nombre sin extension como titulo */
            const char *dot2 = strrchr(filename, '.');
            int len = dot2 ? (int)(dot2 - filename) : (int)strlen(filename);
            if (len >= (int)sizeof(meta->title))
                len = sizeof(meta->title) - 1;
            strncpy(meta->title, filename, len);
            meta->title[len] = '\0';
        }

        return;
    }

    /* Leer los primeros 44 bytes del header */
    if (fread(buf, 1, 44, fp) != 44)
    {
        fprintf(stderr, "[WARN] WAV file too short: %s\n", wav_path);
        fclose(fp);
        return;
    }

    /* Verificar "RIFF" */
    if (buf[0] != 'R' || buf[1] != 'I' ||
        buf[2] != 'F' || buf[3] != 'F')
    {
        fprintf(stderr, "[WARN] Not a valid WAV file: %s\n", wav_path);
        fclose(fp);
        return;
    }

    /* Verificar "WAVE" */
    if (buf[8]  != 'W' || buf[9]  != 'A' ||
        buf[10] != 'V' || buf[11] != 'E')
    {
        fprintf(stderr, "[WARN] Not a valid WAVE chunk: %s\n", wav_path);
        fclose(fp);
        return;
    }

    /* Leer campos del fmt chunk (little endian) */
    audio_format          = (uint16_t)(buf[20] | (buf[21] << 8));
    meta->num_channels    = (int)(uint16_t)(buf[22] | (buf[23] << 8));
    meta->sample_rate     = (int)(uint32_t)(buf[24] | (buf[25] << 8) |
                                            (buf[26] << 16) | (buf[27] << 24));
    meta->bits_per_sample = (int)(uint16_t)(buf[34] | (buf[35] << 8));

    /* Tamaño del chunk data */
    data_size = (uint32_t)(buf[40] | (buf[41] << 8) |
                           (buf[42] << 16) | (buf[43] << 24));

    /* Calcular duracion
    if (meta->sample_rate > 0 && meta->num_channels > 0 &&
        meta->bits_per_sample > 0)
    {
        int bytes_per_second = meta->sample_rate *
                               meta->num_channels *
                               (meta->bits_per_sample / 8);

        if (bytes_per_second > 0)
            meta->duration_seconds = (int)(data_size / bytes_per_second);
    }*/

    /*
     * Intentar leer chunks LIST/INFO para metadata de texto.
     * Estos chunks vienen despues del chunk data en muchos WAVs.
     *
     * Estructura LIST/INFO:
     *   "LIST"  4 bytes
     *   size    4 bytes (little endian)
     *   "INFO"  4 bytes
     *   subchumks: INAM (titulo), IART (artista), IPRD (album)
     *     cada uno: ID(4) + size(4) + data(size)
     */
    fseek(fp, 36, SEEK_SET);

    while (1)
    {
        uint8_t  id[4];
        uint32_t size;
        char     text[64];
        int      read_size;

        if (fread(id, 1, 4, fp) != 4) break;
        if (fread(&size, 1, 4, fp) != 4) break;

        /* Buscar chunk LIST */
        if (id[0] == 'L' && id[1] == 'I' &&
            id[2] == 'S' && id[3] == 'T')
        {
            uint8_t list_type[4];

            if (fread(list_type, 1, 4, fp) != 4) break;

            if (list_type[0] == 'I' && list_type[1] == 'N' &&
                list_type[2] == 'F' && list_type[3] == 'O')
            {
                /* Leer sub-chunks INFO */
                uint32_t remaining = size - 4;

                while (remaining >= 8)
                {
                    uint8_t  sub_id[4];
                    uint32_t sub_size;

                    if (fread(sub_id, 1, 4, fp) != 4) break;
                    if (fread(&sub_size, 1, 4, fp) != 4) break;

                    remaining -= 8;

                    read_size = sub_size < sizeof(text) - 1
                                ? sub_size
                                : sizeof(text) - 1;

                    if (fread(text, 1, read_size, fp) != (size_t)read_size)
                        break;

                    text[read_size] = '\0';

                    /* Saltar bytes restantes del sub-chunk */
                    if (sub_size > (uint32_t)read_size)
                        fseek(fp, sub_size - read_size, SEEK_CUR);

                    /* Sub-chunk de tamaño impar tiene byte de padding */
                    if (sub_size % 2 != 0)
                    {
                        fseek(fp, 1, SEEK_CUR);
                        remaining--;
                    }

                    remaining -= sub_size;

                    /* INAM = titulo, IART = artista, IPRD = album */
                    if (sub_id[0]=='I' && sub_id[1]=='N' &&
                        sub_id[2]=='A' && sub_id[3]=='M')
                    {
                        strncpy(meta->title, text, sizeof(meta->title) - 1);
                        meta->title[sizeof(meta->title) - 1] = '\0';
                    }
                    else if (sub_id[0]=='I' && sub_id[1]=='A' &&
                             sub_id[2]=='R' && sub_id[3]=='T')
                    {
                        strncpy(meta->artist, text, sizeof(meta->artist) - 1);
                        meta->artist[sizeof(meta->artist) - 1] = '\0';
                    }
                    else if (sub_id[0]=='I' && sub_id[1]=='P' &&
                             sub_id[2]=='R' && sub_id[3]=='D')
                    {
                        strncpy(meta->album, text, sizeof(meta->album) - 1);
                        meta->album[sizeof(meta->album) - 1] = '\0';
                    }
                }
            }
            else
            {
                /* LIST que no es INFO, saltar */
                fseek(fp, size - 4, SEEK_CUR);
            }
        }
        else if (id[0] == 'd' && id[1] == 'a' &&
                 id[2] == 't' && id[3] == 'a')
        {
            /* Saltar chunk data */
            fseek(fp, size, SEEK_CUR);
        }
        else
        {
            /* Cualquier otro chunk, saltar */
            fseek(fp, size, SEEK_CUR);
        }
    }

    /*
     * Si no habia metadata LIST/INFO, usar nombre del archivo
     * como fallback con formato "Artista - Titulo.wav"
     */
    if (strcmp(meta->title, "Unknown") == 0)
    {
        const char *filename = strrchr(wav_path, '/');
        if (filename) filename++;
        else          filename = wav_path;

        const char *sep = strstr(filename, " - ");
        const char *dot = strrchr(filename, '.');

        if (sep && dot && sep < dot)
        {
            int len;

            len = sep - filename;
            if (len >= (int)sizeof(meta->artist))
                len = sizeof(meta->artist) - 1;
            strncpy(meta->artist, filename, len);
            meta->artist[len] = '\0';

            sep += 3;
            len  = dot - sep;
            if (len >= (int)sizeof(meta->title))
                len = sizeof(meta->title) - 1;
            strncpy(meta->title, sep, len);
            meta->title[len] = '\0';
        }
        else if (strcmp(meta->title, "Unknown") == 0)
        {
            const char *dot2 = strrchr(filename, '.');
            int len = dot2 ? (int)(dot2 - filename) : (int)strlen(filename);
            if (len >= (int)sizeof(meta->title))
                len = sizeof(meta->title) - 1;
            strncpy(meta->title, filename, len);
            meta->title[len] = '\0';
        }
    }

    fclose(fp);
}


PlayResult play_PCM(const char *filename, int song_number)
{
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "[ERROR] Open %s failed.\n", filename);
        return PLAY_RESULT_ERROR;
    }
    
     char        wav_path[512];
    WavMetadata meta;

    wav_get_path_from_pcm(filename, wav_path, sizeof(wav_path));
    wav_parse_metadata(wav_path, &meta);
    meta.duration_seconds = display_get_pcm_duration_seconds(filename);
    MetaField current_field = META_TITLE;
    int pause_lcd_counter = 0;

    lcd_show_meta(&meta, current_field);

    printf("\n");
    printf("========================================\n");
    printf("[META] Title:    %s\n",  meta.title);
    printf("[META] Artist:   %s\n",  meta.artist);
    printf("[META] Album:    %s\n",  meta.album);
    printf("[META] Rate:     %d Hz\n", meta.sample_rate);
    printf("[META] Channels: %d\n",  meta.num_channels);
    printf("[META] Bits:     %d\n",  meta.bits_per_sample);
    printf("[META] Duration: %02d:%02d\n",
           meta.duration_seconds / 60,
           meta.duration_seconds % 60);
    printf("========================================\n\n");
  

    uint32_t frames_played = 0;
    int elapsed_seconds = 0;
    int last_displayed_second = -1;
    int last_lcd_meta_second = 0;

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
                peakmeter_set_play_state(PLAY_STATE_PAUSE);
                printf("[INFO] Paused\n");
                AUDIO_FifoClear();
                pause_lcd_counter = 0;
                current_field = META_TITLE;
                lcd_show_meta(&meta, current_field);
            }
            else
            {
                state = PLAYER_PLAYING;
                peakmeter_set_play_state(PLAY_STATE_PLAY);
                printf("[INFO] Playing\n");
                pause_lcd_counter = 0;
                current_field = META_TITLE;
                lcd_show_meta(&meta, current_field);
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
            pause_lcd_counter++;

            /*
            * 10 ms * 500 = 3 segundos
            */
            if (pause_lcd_counter >= 300)
            {
                pause_lcd_counter = 0;

                current_field = (MetaField)((current_field + 1) % META_COUNT);
                lcd_show_meta(&meta, current_field);
            }

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
                peakmeter_set_play_state(PLAY_STATE_PAUSE);
                printf("[INFO] Paused\n");
                AUDIO_FifoClear();
                pause_lcd_counter = 0;
                current_field = META_TITLE;
                lcd_show_meta(&meta, current_field);
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

        peakmeter_write_audio_sample(sample_l);
        apply_volume_if_changed();
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
