CC = gcc
CFLAGS = -Wall -Wextra -Werror
TARGET = a.out
SRC = HexByteFinder.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)
