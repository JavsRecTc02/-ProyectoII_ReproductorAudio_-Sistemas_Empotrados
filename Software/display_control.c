#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>

#include "socal/socal.h"
#include "display_control.h"

#define PCM_SAMPLE_RATE      32000
#define PCM_CHANNELS         2
#define PCM_BYTES_PER_SAMPLE 2
#define PCM_FRAME_BYTES      (PCM_CHANNELS * PCM_BYTES_PER_SAMPLE)
#define PCM_BYTES_PER_SECOND (PCM_SAMPLE_RATE * PCM_FRAME_BYTES)

/*
 * HEX displays de la DE1-SoC normalmente son activos en bajo:
 * bit en 0 = segmento encendido
 * bit en 1 = segmento apagado
 *
 * Orden típico: gfedcba
 */
static uint8_t seven_seg_encode_digit(int digit)
{
    switch (digit)
    {
    case 0: return 0x40;
    case 1: return 0x79;
    case 2: return 0x24;
    case 3: return 0x30;
    case 4: return 0x19;
    case 5: return 0x12;
    case 6: return 0x02;
    case 7: return 0x78;
    case 8: return 0x00;
    case 9: return 0x10;
    default: return 0x7F; // apagado
    }
}

extern volatile unsigned long *hex_low_pio_addr;
extern volatile unsigned long *hex_high_pio_addr;

void display_init(void)
{
    display_clear();
    printf("[INFO] 7-segment display initialized\n");
}

void display_clear(void)
{
    /*
     * 0x7F apaga un display.
     * HEX0-HEX3: 4 displays = 28 bits.
     * HEX4-HEX5: 2 displays = 14 bits.
     */
    uint32_t low_blank = 0;
    uint32_t high_blank = 0;

    low_blank |= ((uint32_t)0x7F << 0);
    low_blank |= ((uint32_t)0x7F << 7);
    low_blank |= ((uint32_t)0x7F << 14);
    low_blank |= ((uint32_t)0x7F << 21);

    high_blank |= ((uint32_t)0x7F << 0);
    high_blank |= ((uint32_t)0x7F << 7);

    alt_write_word(hex_low_pio_addr, low_blank);
    alt_write_word(hex_high_pio_addr, high_blank);
}

void display_show_song_time(int song_number, int elapsed_seconds)
{
    int minutes;
    int seconds;

    int song_tens;
    int song_units;
    int min_tens;
    int min_units;
    int sec_tens;
    int sec_units;

    uint32_t low_value;
    uint32_t high_value;

    if (song_number < 0)
        song_number = 0;

    if (song_number > 99)
        song_number = 99;

    if (elapsed_seconds < 0)
        elapsed_seconds = 0;

    minutes = elapsed_seconds / 60;
    seconds = elapsed_seconds % 60;

    if (minutes > 99)
        minutes = 99;

    song_tens  = song_number / 10;
    song_units = song_number % 10;

    min_tens  = minutes / 10;
    min_units = minutes % 10;

    sec_tens  = seconds / 10;
    sec_units = seconds % 10;

    /*
     * Distribución física:
     *
     * HEX5 HEX4 HEX3 HEX2 HEX1 HEX0
     *  #    #    M    M    S    S
     *
     * En el arreglo bajo:
     * HEX0 = sec_units
     * HEX1 = sec_tens
     * HEX2 = min_units
     * HEX3 = min_tens
     *
     * En el arreglo alto:
     * HEX4 = song_units
     * HEX5 = song_tens
     */

    low_value = 0;
    low_value |= ((uint32_t)seven_seg_encode_digit(sec_units) << 0);
    low_value |= ((uint32_t)seven_seg_encode_digit(sec_tens)  << 7);
    low_value |= ((uint32_t)seven_seg_encode_digit(min_units) << 14);
    low_value |= ((uint32_t)seven_seg_encode_digit(min_tens)  << 21);

    high_value = 0;
    high_value |= ((uint32_t)seven_seg_encode_digit(song_units) << 0);
    high_value |= ((uint32_t)seven_seg_encode_digit(song_tens)  << 7);

    alt_write_word(hex_low_pio_addr, low_value);
    alt_write_word(hex_high_pio_addr, high_value);
}

int display_get_pcm_duration_seconds(const char *pcm_file)
{
    struct stat st;
    long bytes;
    int duration;

    if (stat(pcm_file, &st) != 0)
    {
        fprintf(stderr, "[WARN] Could not read PCM metadata: %s\n", pcm_file);
        return 0;
    }

    bytes = (long)st.st_size;

    /*
     * PCM generado como:
     * 32000 Hz, stereo, signed 16-bit little endian.
     *
     * bytes por segundo = 32000 * 2 canales * 2 bytes = 128000
     */
    duration = (int)(bytes / PCM_BYTES_PER_SECOND);

    return duration;
}