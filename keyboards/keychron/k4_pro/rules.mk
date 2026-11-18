# Enter lower-power sleep mode when on the ChibiOS idle thread
OPT_DEFS += -DCORTEX_ENABLE_WFI_IDLE=TRUE
OPT_DEFS += -DNO_USB_STARTUP_CHECK -DENABLE_FACTORY_TEST

# Community firmware: eager per-key debounce, a press is sent on the first contact
# (stock: sym_defer_g, which delays every press by DEBOUNCE ms).
DEBOUNCE_TYPE = sym_eager_pk

SRC += matrix.c

include keyboards/keychron/bluetooth/bluetooth.mk
