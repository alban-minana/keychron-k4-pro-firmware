# Host unit tests for the K4 Pro board logic.
#
#   make -C k4pro/tests          build and run every test suite (non-zero exit on failure)
#   make -C k4pro/tests clean
#
# Each suite #includes the real firmware source it tests (so static helpers and state
# are reachable) and is linked against the fakes in stubs/. Only a C compiler is needed.

CC     ?= gcc
ifeq ($(CC),cc)
CC := gcc
endif

ROOT   := ../..
BT     := $(ROOT)/keyboards/keychron/bluetooth
K4     := $(ROOT)/keyboards/keychron/k4_pro
OUT    := build

# Same feature flags as the firmware build (info.json features + bluetooth.mk OPT_DEFS)
DEFS   := -DKC_BLUETOOTH_ENABLE -DBLUETOOTH_NKRO_ENABLE -DNKRO_ENABLE -DRAW_ENABLE
CFLAGS := -std=gnu11 -O0 -g -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter \
          -Wno-unused-but-set-variable -Werror=implicit-function-declaration \
          -Istubs -I. -I$(BT) $(DEFS)

SUITES := battery report_buffer bluetooth indicator k4_pro
EXE    := $(if $(filter Windows_NT,$(OS)),.exe,)
BINS   := $(SUITES:%=$(OUT)/test_%$(EXE))

.PHONY: all test clean
all: test

test: $(BINS)
	@fail=0; for t in $(BINS); do ./$$t || fail=1; done; \
	if [ $$fail -ne 0 ]; then echo "UNIT TESTS FAILED"; exit 1; fi; echo "All unit tests passed"

$(OUT):
	mkdir -p $(OUT)

HDRS := harness.h $(wildcard stubs/*.h)

$(OUT)/test_battery$(EXE): test_battery.c $(BT)/battery.c stubs/fakes.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) -o $@ test_battery.c stubs/fakes.c

$(OUT)/test_report_buffer$(EXE): test_report_buffer.c $(BT)/report_buffer.c stubs/fakes.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) -o $@ test_report_buffer.c stubs/fakes.c

$(OUT)/test_bluetooth$(EXE): test_bluetooth.c $(BT)/bluetooth.c $(BT)/report_buffer.c stubs/fakes.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) -o $@ test_bluetooth.c $(BT)/report_buffer.c stubs/fakes.c

$(OUT)/test_indicator$(EXE): test_indicator.c $(BT)/indicator.c stubs/fakes.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) -DRGB_MATRIX_ENABLE -o $@ test_indicator.c stubs/fakes.c

$(OUT)/test_k4_pro$(EXE): test_k4_pro.c $(K4)/k4_pro.c stubs/fakes.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) -o $@ test_k4_pro.c stubs/fakes.c

clean:
	rm -rf $(OUT)
