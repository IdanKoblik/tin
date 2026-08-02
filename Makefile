CC      := cc
CFLAGS  := -Wall -Wextra -std=c17 -D_POSIX_C_SOURCE=200809L -Isrc
LDFLAGS := -ludev -lpulse-simple -lpulse

SRC_DIR := src
OBJ_DIR := build
BIN     := tin

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS.o=.d)

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN)

-include $(DEPS)
