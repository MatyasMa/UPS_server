CC = gcc
CFLAGS = -Wall -Wextra -pedantic -g -std=c17
SOURCES = game_control.c players.c sender.c server.c
HEADERS = game_control.h players.h sender.h server.h
TARGET = server

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) -lpthread -lm

clean:
	rm -f $(TARGET)

.PHONY: all clean