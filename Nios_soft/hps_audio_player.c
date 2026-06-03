#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>

#include "shared_protocol.h"

/*
 * H2F bridge.
 */
#define H2F_BASE        0xC0000000u
#define SHARED_OFFSET   0x00030000u
#define SHARED_SIZE     0x00010000u

/*
 * Mientras depuramos, regenerar siempre el PCM.
 */
#define FORCE_REGENERATE_PCM 1

/*
 * Tu HPS viejo usaba volume=0.35 y sonaba decente.
 * Volvemos a ese valor para comparar mejor.
 * Si queda muy fuerte, prueba 0.20.
 */
#define FFMPEG_VOLUME "0.20"

static volatile uint32_t *g_shm = NULL;
static uint32_t g_local_words[SHM_BUFFER_WORDS];

static inline void mem_barrier(void) {
    __sync_synchronize();
}

static inline uint32_t shm_read(uint32_t offset) {
    mem_barrier();
    return g_shm[offset];
}

static inline void shm_write(uint32_t offset, uint32_t value) {
    g_shm[offset] = value;
    mem_barrier();
}

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int convert_to_pcm(const char *input_name, const char *pcm_name) {
    char cmd[1024];

#if FORCE_REGENERATE_PCM
    if (file_exists(pcm_name)) {
        printf("[HPS] Removing old PCM: %s\n", pcm_name);
        if (remove(pcm_name) != 0) {
            fprintf(stderr, "[HPS][WARN] Could not remove old PCM %s: %s\n",
                    pcm_name, strerror(errno));
        }
    }
#else
    if (file_exists(pcm_name)) {
        printf("[HPS] PCM already exists: %s\n", pcm_name);
        return 0;
    }
#endif

    printf("[HPS] Converting to PCM using old-HPS compatible settings...\n");

    snprintf(
        cmd,
        sizeof(cmd),
        "/home/root/ffmpeg -hide_banner -loglevel error -y "
        "-i \"%s\" "
        "-vn "
        "-f s16le "
        "-ar 32000 "
        "-ac 2 "
        "-acodec pcm_s16le "
        "-af volume=%s "
        "\"%s\"",
        input_name,
        FFMPEG_VOLUME,
        pcm_name
    );

    printf("[HPS] Command: %s\n", cmd);

    int ret = system(cmd);

    if (ret != 0) {
        fprintf(stderr, "[HPS][ERROR] ffmpeg conversion failed\n");
        return -1;
    }

    if (!file_exists(pcm_name)) {
        fprintf(stderr, "[HPS][ERROR] PCM was not created: %s\n", pcm_name);
        return -1;
    }

    return 0;
}

static void inspect_pcm_file(const char *pcm_name) {
    FILE *fp = fopen(pcm_name, "rb");

    if (!fp) {
        fprintf(stderr, "[HPS][WARN] Could not inspect PCM file %s\n", pcm_name);
        return;
    }

    int16_t samples[8192];
    size_t n = fread(samples, sizeof(int16_t), 8192, fp);
    fclose(fp);

    if (n == 0) {
        fprintf(stderr, "[HPS][WARN] PCM inspect: empty file\n");
        return;
    }

    int16_t min_sample = INT16_MAX;
    int16_t max_sample = INT16_MIN;
    uint32_t clipped = 0;
    uint32_t zeros = 0;

    for (size_t i = 0; i < n; i++) {
        int16_t s = samples[i];

        if (s < min_sample) min_sample = s;
        if (s > max_sample) max_sample = s;

        if (s == 0) zeros++;

        if (s == INT16_MAX || s == INT16_MIN) {
            clipped++;
        }
    }

    printf("[HPS] PCM inspect:\n");
    printf("      samples_checked = %zu\n", n);
    printf("      min_sample      = %d\n", min_sample);
    printf("      max_sample      = %d\n", max_sample);
    printf("      clipped_samples = %u\n", clipped);
    printf("      zero_samples    = %u\n", zeros);
}

static int wait_for_nios_ready(void) {
    printf("[HPS] Waiting for Nios Shared_RAM magic...\n");

    for (int i = 0; i < 200; i++) {
        uint32_t magic = shm_read(SHM_MAGIC);
        uint32_t status = shm_read(SHM_STATUS);

        if (magic == MAGIC_VALUE &&
            (status == STATUS_READY ||
             status == STATUS_STOPPED ||
             status == STATUS_FINISHED)) {
            printf("[HPS] Nios ready. status=%u\n", status);
            return 0;
        }

        usleep(100000);
    }

    fprintf(stderr, "[HPS][ERROR] Nios not ready or wrong magic\n");
    fprintf(stderr, "[HPS][ERROR] magic=0x%08X status=%u command=%u\n",
            shm_read(SHM_MAGIC),
            shm_read(SHM_STATUS),
            shm_read(SHM_COMMAND));

    return -1;
}

/*
 * Lee PCM raw por bytes.
 *
 * Archivo generado:
 * frame = 4 bytes:
 *   byte0 = left low
 *   byte1 = left high
 *   byte2 = right low
 *   byte3 = right high
 *
 * Empaque en Shared_RAM:
 *   bits [15:0]  = left raw
 *   bits [31:16] = right raw
 *
 * El byte swap final, si se necesita, lo hace Nios.
 */
static size_t read_pcm_words_explicit(FILE *fp) {
    uint8_t frame[4];
    size_t words = 0;

    memset(g_local_words, 0, sizeof(g_local_words));

    while (words < SHM_BUFFER_WORDS) {
        size_t n = fread(frame, 1, 4, fp);

        if (n == 0) {
            break;
        }

        if (n < 4) {
            /*
             * Frame incompleto al final.
             * Lo rellenamos con cero para no meter basura.
             */
            for (size_t i = n; i < 4; i++) {
                frame[i] = 0;
            }
        }

        uint16_t left_raw  = (uint16_t)frame[0] | ((uint16_t)frame[1] << 8);
        uint16_t right_raw = (uint16_t)frame[2] | ((uint16_t)frame[3] << 8);

        g_local_words[words] = ((uint32_t)right_raw << 16) | (uint32_t)left_raw;
        words++;

        if (n < 4) {
            break;
        }
    }

    return words;
}

static int wait_until_buffer_free(uint32_t ready_offset) {
    while (shm_read(ready_offset) != 0) {
        uint32_t status = shm_read(SHM_STATUS);

        if (status == STATUS_ERROR) {
            fprintf(stderr, "[HPS][ERROR] Nios reported STATUS_ERROR. last_error=%u\n",
                    shm_read(SHM_LAST_ERROR));
            return -1;
        }

        if (status == STATUS_READY || status == STATUS_BOOTING) {
            fprintf(stderr, "[HPS][ERROR] Nios left playback while waiting buffer. status=%u\n",
                    status);
            return -1;
        }

        usleep(1000);
    }

    return 0;
}

static int publish_buffer(FILE *fp,
                          uint32_t buffer_word_offset,
                          uint32_t buffer_size_offset,
                          uint32_t buffer_ready_offset,
                          const char *buffer_name,
                          int initial_load,
                          size_t *out_words) {
    if (!initial_load) {
        if (wait_until_buffer_free(buffer_ready_offset) != 0) {
            return -1;
        }
    }

    size_t words = read_pcm_words_explicit(fp);
    *out_words = words;

    if (words == 0) {
        return 0;
    }

    /*
     * Escribir palabra por palabra a Shared_RAM.
     */
    for (size_t i = 0; i < words; i++) {
        g_shm[buffer_word_offset + i] = g_local_words[i];
    }

    mem_barrier();

    shm_write(buffer_size_offset, (uint32_t)words);
    mem_barrier();

    shm_write(buffer_ready_offset, 1);
    mem_barrier();

    shm_write(SHM_TOTAL_WORDS_SENT,
              shm_read(SHM_TOTAL_WORDS_SENT) + (uint32_t)words);

    if (initial_load) {
        printf("[HPS] Initial %s loaded: %zu words\n", buffer_name, words);
    } else {
        printf("[HPS] Refilled %s: %zu words\n", buffer_name, words);
    }

    return 0;
}

static int wait_for_nios_playing(void) {
    for (int i = 0; i < 1000; i++) {
        uint32_t status = shm_read(SHM_STATUS);

        if (status == STATUS_PLAYING || status == STATUS_WAITING_DATA) {
            printf("[HPS] Nios accepted playback. status=%u\n", status);
            return 0;
        }

        if (status == STATUS_ERROR) {
            fprintf(stderr, "[HPS][ERROR] Nios error before playback. last_error=%u\n",
                    shm_read(SHM_LAST_ERROR));
            return -1;
        }

        usleep(1000);
    }

    fprintf(stderr,
            "[HPS][ERROR] Nios did not enter PLAYING. "
            "status=%u command=%u buffer0_ready=%u buffer1_ready=%u\n",
            shm_read(SHM_STATUS),
            shm_read(SHM_COMMAND),
            shm_read(SHM_BUFFER0_READY),
            shm_read(SHM_BUFFER1_READY));

    return -1;
}

static int stream_pcm_file(const char *pcm_name) {
    FILE *fp = fopen(pcm_name, "rb");

    if (!fp) {
        fprintf(stderr, "[HPS][ERROR] Could not open PCM file %s: %s\n",
                pcm_name,
                strerror(errno));
        return -1;
    }

    printf("[HPS] Streaming PCM file: %s\n", pcm_name);

    shm_write(SHM_COMMAND, CMD_NONE);
    shm_write(SHM_FLAGS, 0);

    shm_write(SHM_BUFFER0_READY, 0);
    shm_write(SHM_BUFFER1_READY, 0);
    shm_write(SHM_BUFFER0_SIZE, 0);
    shm_write(SHM_BUFFER1_SIZE, 0);

    shm_write(SHM_TOTAL_WORDS_SENT, 0);
    shm_write(SHM_TOTAL_WORDS_PLAYED, 0);
    shm_write(SHM_UNDERRUNS, 0);
    shm_write(SHM_LAST_ERROR, ERR_NONE);

    mem_barrier();

    size_t words0 = 0;
    if (publish_buffer(fp,
                       SHM_BUFFER0_WORD,
                       SHM_BUFFER0_SIZE,
                       SHM_BUFFER0_READY,
                       "buffer0",
                       1,
                       &words0) != 0) {
        fclose(fp);
        return -1;
    }

    size_t words1 = 0;
    if (publish_buffer(fp,
                       SHM_BUFFER1_WORD,
                       SHM_BUFFER1_SIZE,
                       SHM_BUFFER1_READY,
                       "buffer1",
                       1,
                       &words1) != 0) {
        fclose(fp);
        return -1;
    }

    if (words0 == 0 && words1 == 0) {
        fprintf(stderr, "[HPS][ERROR] PCM file is empty\n");
        fclose(fp);
        return -1;
    }

    if (words0 < SHM_BUFFER_WORDS || words1 < SHM_BUFFER_WORDS) {
        shm_write(SHM_FLAGS, shm_read(SHM_FLAGS) | FLAG_EOF);
        printf("[HPS] EOF already reached during initial preload\n");
    }

    printf("[HPS] Sending CMD_PLAY_PCM\n");

    mem_barrier();
    shm_write(SHM_COMMAND, CMD_PLAY_PCM);
    mem_barrier();

    if (wait_for_nios_playing() != 0) {
        fclose(fp);
        return -1;
    }

    int eof_sent = (shm_read(SHM_FLAGS) & FLAG_EOF) ? 1 : 0;

    while (1) {
        uint32_t status = shm_read(SHM_STATUS);

        if (status == STATUS_FINISHED) {
            printf("[HPS] Nios finished playback\n");
            break;
        }

        if (status == STATUS_STOPPED) {
            printf("[HPS] Nios stopped playback\n");
            break;
        }

        if (status == STATUS_ERROR) {
            fprintf(stderr, "[HPS][ERROR] Nios status error. last_error=%u\n",
                    shm_read(SHM_LAST_ERROR));
            fclose(fp);
            return -1;
        }

        if (status == STATUS_READY || status == STATUS_BOOTING) {
            fprintf(stderr,
                    "[HPS][ERROR] Nios left playback unexpectedly. "
                    "status=%u command=%u words_sent=%u words_played=%u\n",
                    status,
                    shm_read(SHM_COMMAND),
                    shm_read(SHM_TOTAL_WORDS_SENT),
                    shm_read(SHM_TOTAL_WORDS_PLAYED));
            fclose(fp);
            return -1;
        }

        if (!eof_sent && shm_read(SHM_BUFFER0_READY) == 0) {
            size_t words = 0;

            if (publish_buffer(fp,
                               SHM_BUFFER0_WORD,
                               SHM_BUFFER0_SIZE,
                               SHM_BUFFER0_READY,
                               "buffer0",
                               0,
                               &words) != 0) {
                fclose(fp);
                return -1;
            }

            if (words < SHM_BUFFER_WORDS) {
                shm_write(SHM_FLAGS, shm_read(SHM_FLAGS) | FLAG_EOF);
                eof_sent = 1;
                printf("[HPS] EOF sent after buffer0\n");
            }
        }

        if (!eof_sent && shm_read(SHM_BUFFER1_READY) == 0) {
            size_t words = 0;

            if (publish_buffer(fp,
                               SHM_BUFFER1_WORD,
                               SHM_BUFFER1_SIZE,
                               SHM_BUFFER1_READY,
                               "buffer1",
                               0,
                               &words) != 0) {
                fclose(fp);
                return -1;
            }

            if (words < SHM_BUFFER_WORDS) {
                shm_write(SHM_FLAGS, shm_read(SHM_FLAGS) | FLAG_EOF);
                eof_sent = 1;
                printf("[HPS] EOF sent after buffer1\n");
            }
        }

        usleep(1000);
    }

    printf("[HPS] Playback stats:\n");
    printf("      words_sent   = %u\n", shm_read(SHM_TOTAL_WORDS_SENT));
    printf("      words_played = %u\n", shm_read(SHM_TOTAL_WORDS_PLAYED));
    printf("      underruns    = %u\n", shm_read(SHM_UNDERRUNS));
    printf("      last_error   = %u\n", shm_read(SHM_LAST_ERROR));

    fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    int fd;
    void *virtual_base;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <audio_file.mp3|wav|...>\n", argv[0]);
        return 1;
    }

    char pcm_name[256];

    snprintf(pcm_name, sizeof(pcm_name), "%s.pcm", argv[1]);

    if (convert_to_pcm(argv[1], pcm_name) != 0) {
        return 1;
    }

    inspect_pcm_file(pcm_name);

    printf("[HPS] Opening /dev/mem...\n");

    fd = open("/dev/mem", O_RDWR | O_SYNC);

    if (fd < 0) {
        fprintf(stderr, "[HPS][ERROR] open /dev/mem failed: %s\n", strerror(errno));
        return 1;
    }

    virtual_base = mmap(
        NULL,
        SHARED_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        H2F_BASE + SHARED_OFFSET
    );

    if (virtual_base == MAP_FAILED) {
        fprintf(stderr, "[HPS][ERROR] mmap failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    g_shm = (volatile uint32_t *)virtual_base;

    printf("[HPS] Shared_RAM mapped at H2F 0x%08X\n",
       H2F_BASE + SHARED_OFFSET);

    if (wait_for_nios_ready() != 0) {
        munmap(virtual_base, SHARED_SIZE);
        close(fd);
        return 1;
    }

    int ret = stream_pcm_file(pcm_name);

    munmap(virtual_base, SHARED_SIZE);
    close(fd);

    return ret == 0 ? 0 : 1;
}