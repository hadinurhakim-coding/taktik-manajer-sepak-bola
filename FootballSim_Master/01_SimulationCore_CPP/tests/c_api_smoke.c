#include "match_bridge_api.h"
#include <stdio.h>
int main(void) {
    fsim_match_handle handle = 99;
    size_t required = 0;
    if (get_match_api_version() != 1) return 1;
    if (init_match(NULL, 0, &handle) != FSIM_INVALID_ARGUMENT || handle != 0) return 2;
    if (tick_match_fixed(0, 1) != FSIM_INVALID_HANDLE) return 3;
    if (get_engine_schema(NULL, 0, &required) != FSIM_BUFFER_TOO_SMALL || required == 0) return 4;
    if (destroy_match(0) != FSIM_INVALID_HANDLE) return 5;
    puts("PASS: C header, C linkage, exported functions and status codes");
    return 0;
}
