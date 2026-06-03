CC = arm-linux-gnueabihf-gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu99
TARGET = hps_audio_player
SRC = hps_audio_player.c

all: $(TARGET)

$(TARGET): $(SRC) shared_protocol.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)