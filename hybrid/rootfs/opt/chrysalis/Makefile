# Build Chrysalis OS inside rootfs

CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include
LDFLAGS = -lc -lm

TARGET = chrysalis_daemon
SRCS = ../../../os/linux_port/*.c
OBJS = build/*.o

all: $(TARGET)

$(TARGET):
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)
	@chmod +x $@
	@echo "✓ Built Chrysalis daemon"

clean:
	rm -rf build $(TARGET)

.PHONY: all clean
