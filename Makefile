# make        → build/vbc (데모)
# make test   → tests/test_*.c 를 각각 빌드해서 실행 (파일을 추가하면 자동 포함)
# make clean

CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -g
CPPFLAGS += -Isrc
BUILD := build

LIB_SRCS := $(filter-out src/main.c,$(wildcard src/*.c))
LIB_OBJS := $(LIB_SRCS:src/%.c=$(BUILD)/%.o)
HEADERS := $(wildcard src/*.h)
TEST_SRCS := $(wildcard tests/test_*.c)
TEST_BINS := $(TEST_SRCS:tests/%.c=$(BUILD)/%)

.PHONY: all test run clean

all: $(BUILD)/vbc

run: $(BUILD)/vbc
	./$(BUILD)/vbc

$(BUILD)/vbc: $(LIB_OBJS) $(BUILD)/main.o
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD)/test_%: tests/test_%.c $(LIB_OBJS) $(wildcard tests/*.h) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LIB_OBJS)

test: $(TEST_BINS)
	@fail=0; for t in $(TEST_BINS); do ./$$t || fail=1; done; \
	if [ $$fail -eq 0 ]; then echo "ALL TESTS PASSED"; else echo "SOME TESTS FAILED"; fi; \
	exit $$fail

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
