# make        → build/vbc (데모)
# make test   → tests/test_*.c 를 각각 빌드해서 실행 (파일을 추가하면 자동 포함)
# make scenarios → scenarios/*.txt 를 차례로 실행 (expect 확인)
# make wasm      → sim/build/vbc.js (emcc 필요: source ~/emsdk/emsdk_env.sh)
# make sim       → wasm 빌드 후 http://localhost:8080/sim/ (저장소 루트 서빙, SIM_PORT로 변경)
# make clean

CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -g
CPPFLAGS += -Isrc
LDLIBS += -lm
BUILD := build

LIB_SRCS := $(filter-out src/main.c,$(wildcard src/*.c))
LIB_OBJS := $(LIB_SRCS:src/%.c=$(BUILD)/%.o)
HEADERS := $(wildcard src/*.h)
TEST_SRCS := $(wildcard tests/test_*.c)
TEST_BINS := $(TEST_SRCS:tests/%.c=$(BUILD)/%)

.PHONY: all test run scenarios wasm sim clean

all: $(BUILD)/vbc

run: $(BUILD)/vbc
	./$(BUILD)/vbc

$(BUILD)/vbc: $(LIB_OBJS) $(BUILD)/main.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD)/test_%: tests/test_%.c $(LIB_OBJS) $(wildcard tests/*.h) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LIB_OBJS) $(LDLIBS)

test: $(TEST_BINS)
	@fail=0; for t in $(TEST_BINS); do ./$$t || fail=1; done; \
	if [ $$fail -eq 0 ]; then echo "ALL TESTS PASSED"; else echo "SOME TESTS FAILED"; fi; \
	exit $$fail

scenarios: $(BUILD)/vbc
	@fail=0; for f in scenarios/*.txt; do \
	  if ./$(BUILD)/vbc $$f > $(BUILD)/$$(basename $$f .txt).log; then echo "  PASS $$f"; \
	  else echo "  FAIL $$f  (로그: $(BUILD)/$$(basename $$f .txt).log)"; fail=1; fi; done; \
	exit $$fail

SIM_OUT := sim/build/vbc.js
EMCC ?= emcc
EMFLAGS := -O2 -sMODULARIZE -sEXPORT_ES6 -sSINGLE_FILE -sENVIRONMENT=web \
	-sEXPORTED_FUNCTIONS=_sim_init,_sim_exec,_sim_advance,_sim_state,_sim_adas,_sim_points,_sim_points_count,_sim_set_path,_sim_set_ego,_malloc,_free \
	-sEXPORTED_RUNTIME_METHODS=cwrap,UTF8ToString,HEAPF32

wasm: $(SIM_OUT)

$(SIM_OUT): $(LIB_SRCS) $(HEADERS)
	@command -v $(EMCC) >/dev/null || { echo "emcc가 없습니다: source ~/emsdk/emsdk_env.sh"; exit 1; }
	mkdir -p sim/build
	$(EMCC) $(CPPFLAGS) -std=c11 $(EMFLAGS) -o $@ $(LIB_SRCS)

SIM_PORT ?= 8080

sim: wasm
	@echo "브라우저에서 http://localhost:$(SIM_PORT)/sim/ (종료: Ctrl+C)"
	python3 -m http.server $(SIM_PORT)

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD) sim/build
