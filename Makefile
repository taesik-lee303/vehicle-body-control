# make        → build/vbc (데모)
# make test   → tests/test_*.c 를 각각 빌드해서 실행 (파일을 추가하면 자동 포함)
# make scenarios → scenarios/*.txt 를 차례로 실행 (expect 확인)
# make wasm      → sim/build/vbc.js (emcc 필요: source ~/emsdk/emsdk_env.sh)
# make sim       → wasm 빌드 후 http://localhost:8080/sim/ (저장소 루트 서빙, SIM_PORT로 변경)
# make wasm-ecu  → sim/build/ecu.js (ECU 시뮬레이터 엔진, emcc 필요)
# make wasm-check → 터미널 콘솔 = WASM 엔진 출력 동일성 확인 (node 필요)
# make sim-ecu   → wasm-ecu 후 http://localhost:8080/sim/ecu.html (BCU system 구성도, 저장소 루트 서빙)
# make consoles  → 모듈별 대화형 테스트 콘솔 4개 빌드 (build/con_doors 등)
# make con-doors / con-dashboard / con-lighting / con-wipers → 해당 콘솔 실행
# make tc        → 모듈별 TC 전체 실행, 결과 문서 docs/test/tc-results.md 생성
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

.PHONY: all test run scenarios wasm sim consoles tc clean wasm-ecu wasm-check sim-ecu

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

scenarios: $(BUILD)/vbc
	@fail=0; for f in scenarios/*.txt; do \
	  if ./$(BUILD)/vbc $$f > $(BUILD)/$$(basename $$f .txt).log; then echo "  PASS $$f"; \
	  else echo "  FAIL $$f  (로그: $(BUILD)/$$(basename $$f .txt).log)"; fail=1; fi; done; \
	exit $$fail

SIM_OUT := sim/build/vbc.js
EMCC ?= emcc
EMFLAGS := -O2 -sMODULARIZE -sEXPORT_ES6 -sSINGLE_FILE -sENVIRONMENT=web \
	-sEXPORTED_FUNCTIONS=_sim_init,_sim_exec,_sim_advance,_sim_state \
	-sEXPORTED_RUNTIME_METHODS=cwrap,UTF8ToString

wasm: $(SIM_OUT)

$(SIM_OUT): $(LIB_SRCS) $(HEADERS)
	@command -v $(EMCC) >/dev/null || { echo "emcc가 없습니다: source ~/emsdk/emsdk_env.sh"; exit 1; }
	mkdir -p sim/build
	$(EMCC) $(CPPFLAGS) -std=c11 $(EMFLAGS) -o $@ $(LIB_SRCS)

SIM_PORT ?= 8080

sim: wasm
	@echo "브라우저에서 http://localhost:$(SIM_PORT)/sim/ (종료: Ctrl+C)"
	python3 -m http.server $(SIM_PORT)

# ---- 모듈별 대화형 테스트 콘솔 (tools/consoles) ----
CON_SRCS := $(wildcard tools/consoles/con_*.c)
CON_BINS := $(CON_SRCS:tools/consoles/%.c=$(BUILD)/%)

consoles: $(CON_BINS)

$(BUILD)/modcon.o: tools/consoles/modcon.c tools/consoles/modcon.h $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD)/con_%: tools/consoles/con_%.c $(BUILD)/modcon.o $(LIB_OBJS) tools/consoles/modcon.h | $(BUILD)
	$(CC) $(CPPFLAGS) -Itools/consoles $(CFLAGS) -o $@ $< $(BUILD)/modcon.o $(LIB_OBJS)

con-%: $(BUILD)/con_%
	./$<

# ---- ECU 시뮬레이터 엔진 (modcon을 WASM으로) ----
ECU_OUT := sim/build/ecu.js
ECU_SRCS := src/common.c src/wipers.c src/lighting.c src/doors.c src/dashboard.c \
	tools/consoles/modcon.c $(wildcard tools/consoles/con_*.c) tools/consoles/ecu_wasm.c
ECU_EMFLAGS := -O2 -sMODULARIZE -sEXPORT_ES6 -sSINGLE_FILE -sENVIRONMENT=web,node \
	-sEXPORTED_FUNCTIONS=_ecu_select,_ecu_exec,_ecu_advance,_ecu_state,_ecu_finish \
	-sEXPORTED_RUNTIME_METHODS=cwrap,UTF8ToString

wasm-ecu: $(ECU_OUT)

$(ECU_OUT): $(ECU_SRCS) $(HEADERS) tools/consoles/modcon.h tools/consoles/consoles.h
	@command -v $(EMCC) >/dev/null || { echo "emcc가 없습니다: source ~/emsdk/emsdk_env.sh"; exit 1; }
	mkdir -p sim/build
	$(EMCC) $(CPPFLAGS) -Itools/consoles -std=c11 -DMODCON_NO_MAIN $(ECU_EMFLAGS) -o $@ $(ECU_SRCS)

# BCU system 구성도 화면. TC 파일(tools/consoles/tc)을 fetch로 읽으므로 저장소 루트를 서빙한다
sim-ecu: wasm-ecu
	@echo "브라우저에서 http://localhost:$(SIM_PORT)/sim/ecu.html (종료: Ctrl+C)"
	python3 -m http.server $(SIM_PORT)

# 터미널 콘솔 출력과 WASM 엔진(재생기) 출력이 같은지 확인
wasm-check: wasm-ecu consoles
	node tools/consoles/wasm_check.mjs

# 모듈별 TC 전체 실행 → docs/test/tc-results.md, docs/test/logs/*.log
tc: consoles
	python3 tools/consoles/tc/run_tc.py

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD) sim/build
