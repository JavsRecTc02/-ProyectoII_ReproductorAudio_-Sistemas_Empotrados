#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#define H2F_BASE        0xC0000000u
#define SHARED_OFFSET   0x00000000u
#define SHARED_SIZE     0x00010000u   // 64 KB

#define MAGIC_OFFSET    0
#define STATUS_OFFSET   1
#define COMMAND_OFFSET  2
#define COUNTER_OFFSET  3

#define CMD_NONE        0
#define CMD_INIT_AUDIO  1
#define CMD_PLAY_TONE   2
#define CMD_STOP        3
#define CMD_PLAY_PCM    4

int main(void) {
    int fd;
    void *virtual_base;
    volatile uint32_t *shared;

    off_t target = H2F_BASE + SHARED_OFFSET;

    printf("[HPS] Opening /dev/mem...\n");

    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        printf("[ERROR] open /dev/mem failed: %s\n", strerror(errno));
        return 1;
    }

    printf("[HPS] Mapping Shared_RAM at physical address 0x%08lX...\n",
           (unsigned long)target);

    virtual_base = mmap(
        NULL,
        SHARED_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        target
    );

    if (virtual_base == MAP_FAILED) {
        printf("[ERROR] mmap failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    shared = (volatile uint32_t *)virtual_base;

    printf("[HPS] Shared_RAM mapped successfully.\n\n");

    while (1) {
        uint32_t magic = shared[MAGIC_OFFSET];
        uint32_t status = shared[STATUS_OFFSET];
        uint32_t command = shared[COMMAND_OFFSET];
        uint32_t counter = shared[COUNTER_OFFSET];

        printf("[HPS] magic=0x%08X status=0x%08X command=0x%08X counter=%u\n",
               magic, status, command, counter);

        printf("Select command:\n");
        printf("  0 = none\n");
        printf("  1 = LED pattern 1\n");
        printf("  2 = LED pattern 2\n");
        printf("  3 = LED pattern 4\n");
        printf("  4 = clear LEDs\n");
        printf("  q = quit\n");
        printf("> ");

        char input[16];

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        if (input[0] == 'q') {
            break;
        }

        int cmd = atoi(input);

        if (cmd < 0 || cmd > 4) {
            printf("[HPS] Invalid command.\n\n");
            continue;
        }

        shared[COMMAND_OFFSET] = (uint32_t)cmd;

        printf("[HPS] Wrote command %d to Shared_RAM.\n\n", cmd);
        usleep(200000);
    }

    munmap(virtual_base, SHARED_SIZE);
    close(fd);

    printf("[HPS] Done.\n");

    return 0;
}