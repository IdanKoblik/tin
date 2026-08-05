CC      := cc

SODIUM_CFLAGS := $(shell pkg-config --cflags libsodium)
SODIUM_LIBS   := $(shell pkg-config --libs libsodium)

CFLAGS  := -Wall -Wextra -std=c17 -D_POSIX_C_SOURCE=200809L -Isrc $(SODIUM_CFLAGS)
LDLIBS  := -lncurses -ludev -lpulse-simple -lpulse $(SODIUM_LIBS)

SRC_DIR := src
OBJ_DIR := build
BIN     := tin
LOG     := tin.log

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

TEST_DIR  := tests
TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)
TEST_BIN  := $(OBJ_DIR)/test_runner
# Link every production object except main.o (which owns its own main()).
TEST_OBJS := $(filter-out $(OBJ_DIR)/main.o,$(OBJS))

# Coverage: instrument the production sources (except main.c).
COV_DIR  := coverage
COV_OBJ  := $(COV_DIR)/obj
COV_SRCS := $(filter-out $(SRC_DIR)/main.c,$(SRCS)) $(TEST_SRCS)
COV_OBJS := $(COV_SRCS:%.c=$(COV_OBJ)/%.o)

CLANG_FORMAT := clang-format
# greatest.h is vendored third-party; leave it alone.
FORMAT_SRCS := $(shell find $(SRC_DIR) $(TEST_DIR) -name '*.c' -o -name '*.h' \
                 | grep -v '^$(TEST_DIR)/greatest\.h$$')

.PHONY: all clean test coverage logs format format-check

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

logs:
	journalctl -t $(BIN) -o short-iso > $(LOG)
	@echo "Logs written to $(LOG)"

test: $(TEST_OBJS) $(TEST_SRCS)
	$(CC) $(CFLAGS) -I$(TEST_DIR) $(TEST_SRCS) $(TEST_OBJS) -o $(TEST_BIN) $(LDLIBS)
	./$(TEST_BIN)

# Requires lcov
coverage: $(COV_OBJS)
	$(CC) $(COV_OBJS) --coverage -o $(COV_DIR)/test_runner $(LDLIBS)
	./$(COV_DIR)/test_runner
	lcov --capture --directory $(COV_OBJ) --output-file $(COV_DIR)/coverage.info
	lcov --remove $(COV_DIR)/coverage.info '*/tests/*' '/usr/*' --output-file $(COV_DIR)/coverage.info
	genhtml $(COV_DIR)/coverage.info --output-directory $(COV_DIR)/html
	@echo "Coverage report: $(COV_DIR)/html/index.html"

$(COV_OBJ)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$(TEST_DIR) --coverage -c $< -o $@

format:
	$(CLANG_FORMAT) -i $(FORMAT_SRCS)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_SRCS)

clean:
	rm -rf $(OBJ_DIR) $(BIN) $(LOG) $(COV_DIR)

-include $(DEPS)

