#include "app_controller.h"
#include "core/errors.h"

void app_main(void) {
    if (app_controller_init() != ERR_OK) {
        // TODO: add fatal boot indication/logging
        return;
    }

    (void)app_controller_run();
}
