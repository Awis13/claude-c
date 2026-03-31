CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS = -lcurl
INCLUDES = -Isrc -Ideps/cJSON

# readline: попробовать libedit, потом readline, потом без
READLINE_FLAGS := $(shell pkg-config --cflags libedit 2>/dev/null || echo "")
READLINE_LIBS := $(shell pkg-config --libs libedit 2>/dev/null || echo "-lreadline")

CFLAGS += $(READLINE_FLAGS)
LDFLAGS += $(READLINE_LIBS)

SRC_DIR = src
DEPS_DIR = deps/cJSON
BUILD_DIR = build

# исходники
SRCS = $(wildcard $(SRC_DIR)/*.c) $(DEPS_DIR)/cJSON.c
OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SRCS)))

TARGET = claude-c

.PHONY: all clean install

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# src/*.c
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# deps/cJSON/cJSON.c
$(BUILD_DIR)/cJSON.o: $(DEPS_DIR)/cJSON.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# статическая сборка (Linux + musl)
static:
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) -o $(TARGET) -static -lcurl -lssl -lcrypto -lz -lpthread
