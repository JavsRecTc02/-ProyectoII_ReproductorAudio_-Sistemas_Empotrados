#ifndef PCM_H
#define PCM_H

typedef enum
{
    PLAY_RESULT_ERROR = -1,
    PLAY_RESULT_FINISHED = 0,
    PLAY_RESULT_STOP,
    PLAY_RESULT_NEXT,
    PLAY_RESULT_PREVIOUS
} PlayResult;

PlayResult play_PCM(const char *filename, int song_number);

#endif