# Build Chrysalis OS for Linux (userspace port)

CC = gcc
CFLAGS = -Wall -Wextra -O2 -I. -I../include
LDFLAGS = -lc -lm

# Output
TARGET = chrysalis_daemon
BUILD_DIR = build
INSTALL_DIR = /opt/chrysalis

# Source files
SRCS = $(wildcard *.c)
OBJS = $(addprefix $(BUILD_DIR)/,$(SRCS:.c=.o))

# Targets
all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "✓ Built: $(TARGET)"

install: $(TARGET)
	@echo "Installing Chrysalis daemon..."
	mkdir -p $(INSTALL_DIR)
	cp $(TARGET) $(INSTALL_DIR)/
	chmod +x $(INSTALL_DIR)/$(TARGET)
	@echo "✓ Installed to $(INSTALL_DIR)/$(TARGET)"

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	@echo "✓ Cleaned"

.PHONY: all clean install
