#include "assets/error.h"
#include <string.h>

static char* load_err_buf[256];

const char* get_last_asset_load_error() {
    return (const char*) load_err_buf;
}

void set_last_asset_load_error(const char* err) {
    strncpy((char*)load_err_buf, err, sizeof(load_err_buf));
}
