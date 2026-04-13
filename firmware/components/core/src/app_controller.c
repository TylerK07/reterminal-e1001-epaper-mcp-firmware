#include "app_controller.h"

static app_state_t g_state = STATE_BOOTING;

error_code_t app_controller_init(void) {
    g_state = STATE_BOOTING;
    // TODO: initialize modules in boot order
    return ERR_OK;
}

error_code_t app_controller_run(void) {
    // TODO: implement boot/provision/connect/idle/sleep state machine
    g_state = STATE_UNPROVISIONED;
    return ERR_OK;
}

app_state_t app_controller_get_state(void) {
    return g_state;
}
