#include "errors.h"

const char *error_code_to_string(error_code_t code) {
    switch (code) {
        case ERR_OK:
            return "ERR_OK";
        case ERR_INVALID_ARGS:
            return "ERR_INVALID_ARGS";
        case ERR_UNAUTHORIZED:
            return "ERR_UNAUTHORIZED";
        case ERR_FORBIDDEN:
            return "ERR_FORBIDDEN";
        case ERR_BUSY:
            return "ERR_BUSY";
        case ERR_TIMEOUT:
            return "ERR_TIMEOUT";
        case ERR_LOW_BATTERY:
            return "ERR_LOW_BATTERY";
        case ERR_WIFI_UNAVAILABLE:
            return "ERR_WIFI_UNAVAILABLE";
        case ERR_WIFI_NOT_CONNECTED:
            return "ERR_WIFI_NOT_CONNECTED";
        case ERR_WIFI_CONNECT_FAILED:
            return "ERR_WIFI_CONNECT_FAILED";
        case ERR_SD_NOT_MOUNTED:
            return "ERR_SD_NOT_MOUNTED";
        case ERR_NOT_FOUND:
            return "ERR_NOT_FOUND";
        case ERR_UNSUPPORTED:
            return "ERR_UNSUPPORTED";
        case ERR_INTERNAL:
            return "ERR_INTERNAL";
        case ERR_POLICY_BLOCKED:
            return "ERR_POLICY_BLOCKED";
        default:
            return "ERR_UNKNOWN";
    }
}
