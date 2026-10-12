CC      = gcc
CFLAGS  = -Wall -Wextra -O2
TARGET  = bin/ls
SRC     = src/ls-v1.0.0.c

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)