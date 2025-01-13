CC = gcc
CFLAGS = -Wall -Wextra -pedantic -ansi -g
SOURCES = server.c
HEADERS = 
TARGET = server

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) -lm

clean:
	rm -f $(TARGET)

.PHONY: all clean