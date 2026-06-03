#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/stat.h>

#include "hwlib.h"
#include "socal/hps.h"
#include "socal/socal.h"
#include "hps_0.h"
#include "audio_control.h"
#include "pcm.h"
#include "button_control.h"
#include "display_control.h"

#define HW_REGS_BASE (ALT_STM_OFST)
#define HW_REGS_SPAN (0x04000000)
#define HW_REGS_MASK (HW_REGS_SPAN - 1)
#define MAX_SONGS 128
#define MAX_PATH_LEN 256

//base addr
static volatile unsigned long *h2p_lw_axi_addr = NULL;
volatile unsigned long *oc_i2c_audio_addr = NULL;
volatile unsigned long *audio_addr = NULL;
volatile unsigned long *button_pio_addr = NULL;
volatile unsigned long *hex_low_pio_addr = NULL;
volatile unsigned long *hex_high_pio_addr = NULL;

static int has_extension(const char *filename, const char *ext)
{
    const char *dot = strrchr(filename, '.');

    if (dot == NULL)
        return 0;

    while (*dot && *ext)
    {
        if (tolower((unsigned char)*dot) != tolower((unsigned char)*ext))
            return 0;

        dot++;
        ext++;
    }

    return (*dot == '\0' && *ext == '\0');
}

static int is_audio_file(const char *filename)
{
    return has_extension(filename, ".mp3") ||
           has_extension(filename, ".wav") ||
           has_extension(filename, ".pcm");
}

static int is_regular_file(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0)
        return 0;

    return S_ISREG(st.st_mode);
}

static int is_directory(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0)
        return 0;

    return S_ISDIR(st.st_mode);
}

static int build_playlist_from_directory(const char *directory,
                                         char songs[MAX_SONGS][MAX_PATH_LEN])
{
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    dir = opendir(directory);
    if (dir == NULL)
    {
        fprintf(stderr, "[ERROR] Could not open directory: %s\n", directory);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL && count < MAX_SONGS)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (!is_audio_file(entry->d_name))
            continue;

        snprintf(songs[count], MAX_PATH_LEN, "%s/%s", directory, entry->d_name);
        count++;
    }

    closedir(dir);

    return count;
}

static int build_playlist(const char *path, char songs[MAX_SONGS][MAX_PATH_LEN])
{
    if (is_regular_file(path))
    {
        if (!is_audio_file(path))
        {
            fprintf(stderr, "[ERROR] File is not .mp3, .wav or .pcm: %s\n", path);
            return -1;
        }

        strncpy(songs[0], path, MAX_PATH_LEN - 1);
        songs[0][MAX_PATH_LEN - 1] = '\0';
        return 1;
    }

    if (is_directory(path))
    {
        return build_playlist_from_directory(path, songs);
    }

    fprintf(stderr, "[ERROR] Path does not exist: %s\n", path);
    return -1;
}

static int get_pcm_file(const char *input_file, char *pcm_file, size_t pcm_file_size)
{
    if (has_extension(input_file, ".pcm"))
    {
        strncpy(pcm_file, input_file, pcm_file_size - 1);
        pcm_file[pcm_file_size - 1] = '\0';
        return 0;
    }

    snprintf(pcm_file, pcm_file_size, "%s.pcm", input_file);

    FILE *fp = fopen(pcm_file, "rb");
    if (fp != NULL)
    {
        fclose(fp);
        printf("[INFO] PCM already exists: %s\n", pcm_file);
        return 0;
    }

    printf("[INFO] Converting to PCM: %s\n", input_file);

    char cmd[512];

    snprintf(cmd, sizeof(cmd),
             "/home/root/ffmpeg -i \"%s\" -f s16le -ar 32000 -ac 2 "
             "-acodec pcm_s16le -af volume=0.35 \"%s\" -y",
             input_file, pcm_file);

    if (system(cmd) != 0)
    {
        fprintf(stderr, "[ERROR] PCM conversion failed: %s\n", input_file);
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    void *virtual_base;
    int fd;
    int return_code = 0;

    if ((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1)
    {
        fprintf(stderr, "[ERROR] Fail to open \"/dev/mem\"...\n");
        return 1;
    }

    virtual_base = mmap(NULL, HW_REGS_SPAN,
                        (PROT_READ | PROT_WRITE),
                        MAP_SHARED,
                        fd,
                        HW_REGS_BASE);

    if (virtual_base == MAP_FAILED)
    {
        fprintf(stderr, "[ERROR] mmap() failed...\n");
        close(fd);
        return 1;
    }

    h2p_lw_axi_addr = virtual_base +
        ((unsigned long)(ALT_LWFPGASLVS_OFST) &
         (unsigned long)(HW_REGS_MASK));

    oc_i2c_audio_addr = virtual_base +
        ((unsigned long)(ALT_LWFPGASLVS_OFST + OC_I2C_MASTER_0_BASE) &
         (unsigned long)(HW_REGS_MASK));

    audio_addr = virtual_base +
        ((unsigned long)(ALT_LWFPGASLVS_OFST + AUDIO_IF_0_BASE) &
         (unsigned long)(HW_REGS_MASK));

    button_pio_addr = virtual_base +
        ((unsigned long)(ALT_LWFPGASLVS_OFST + BUTTON_PIO_BASE) &
         (unsigned long)(HW_REGS_MASK));

    hex_low_pio_addr = virtual_base +
        ((unsigned long)(ALT_LWFPGASLVS_OFST + HEX_LOW_PIO_BASE) &
        (unsigned long)(HW_REGS_MASK));

    hex_high_pio_addr = virtual_base +
        ((unsigned long)(ALT_LWFPGASLVS_OFST + HEX_HIGH_PIO_BASE) &
        (unsigned long)(HW_REGS_MASK));

    printf("[INFO] i2c_audio_addr:  %04Xh\n", (unsigned int)oc_i2c_audio_addr);
    printf("[INFO] audio_addr:      %04Xh\n", (unsigned int)audio_addr);
    printf("[INFO] button_pio_addr: %04Xh\n", (unsigned int)button_pio_addr);
    printf("[INFO] hex_low_pio_addr:  %04Xh\n", (unsigned int)hex_low_pio_addr);
    printf("[INFO] hex_high_pio_addr: %04Xh\n", (unsigned int)hex_high_pio_addr);

    oc_i2c_audio_init();
    init_audio();
    buttons_init();
    display_init();

    usleep(500 * 1000); // Delay necesario antes de iniciar reproducción

    AUDIO_SetSampleRate(RATE_ADC32K_DAC32K);

    // Cargar playlist desde argumento o usar ruta por defecto
    char input_path[MAX_PATH_LEN] = "./music";

    if (argc >= 2 && argv[1])
    {
        strncpy(input_path, argv[1], MAX_PATH_LEN - 1);
        input_path[MAX_PATH_LEN - 1] = '\0';
    }

    char songs[MAX_SONGS][MAX_PATH_LEN];

    int song_count = build_playlist(input_path, songs);

    if (song_count <= 0)
    {
        fprintf(stderr, "[ERROR] No audio files found in: %s\n", input_path);
        munmap(virtual_base, HW_REGS_SPAN);
        close(fd);
        return 1;
    }

    printf("[INFO] Playlist loaded\n");
    printf("[INFO] Songs found: %d\n", song_count);

	int i;

	for (i = 0; i < song_count; i++)
	{
		printf("[INFO] Song %d: %s\n", i + 1, songs[i]);
	}

    printf("\n[INFO] Controls:\n");
    printf("       KEY0: Play/Pause\n");
    printf("       KEY1: Stop\n");
    printf("       KEY2: Next\n");
    printf("       KEY3: Previous\n\n");

    int current_song = 0;
    int running = 1;

    while (running)
    {
        char pcm_file[MAX_PATH_LEN + 8];

        printf("\n[INFO] Selected song [%d/%d]: %s\n",
               current_song + 1,
               song_count,
               songs[current_song]);

        if (get_pcm_file(songs[current_song], pcm_file, sizeof(pcm_file)) != 0)
        {
            fprintf(stderr, "[WARN] Skipping song due to conversion error\n");

            current_song = (current_song + 1) % song_count;
            continue;
        }

        printf("[INFO] Now playing: %s\n", songs[current_song]);

        int duration_seconds = display_get_pcm_duration_seconds(pcm_file);

        printf("[INFO] Duration: %02d:%02d\n",
            duration_seconds / 60,
            duration_seconds % 60);

        PlayResult result = play_PCM(pcm_file, current_song + 1);

        if (result == PLAY_RESULT_FINISHED)
        {
            printf("[INFO] Song finished. Playing next song...\n");
            current_song = (current_song + 1) % song_count;
        }
        else if (result == PLAY_RESULT_NEXT)
        {
            printf("[INFO] Moving to next song...\n");
            current_song = (current_song + 1) % song_count;
        }
        else if (result == PLAY_RESULT_PREVIOUS)
        {
            printf("[INFO] Moving to previous song...\n");

            current_song--;

            if (current_song < 0)
                current_song = song_count - 1;
        }
        else if (result == PLAY_RESULT_STOP)
        {
            printf("[INFO] Stop received. Exiting player...\n");
            running = 0;
        }
        else
        {
            fprintf(stderr, "[ERROR] Playback error. Exiting player...\n");
            running = 0;
            return_code = 1;
        }
    }

    printf("[INFO] Player stopped\n");

    if (munmap(virtual_base, HW_REGS_SPAN) != 0)
    {
        fprintf(stderr, "[ERROR] munmap() failed...\n");
        close(fd);
        return 1;
    }

    close(fd);
    return return_code;
}

