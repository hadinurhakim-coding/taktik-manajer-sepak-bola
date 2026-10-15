#include <cstdint>
#include <iostream>
#include <type_traits>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// Probe types only; this does not define the future .fsim wire format.
struct Vec3 {
    float x;
    float y;
    float z;
};

struct PlayerState {
    std::uint32_t player_id;
    Vec3 position;
    float stamina;
};

static_assert(__cplusplus >= 202002L, "C++20 with /Zc:__cplusplus is required");
static_assert(std::is_standard_layout_v<PlayerState>);
static_assert(sizeof(void*) == 8, "Build the x64 target");

int main() {
    const bool sdk_ok = GetCurrentProcessId() != 0;
    std::cout << "__cplusplus=" << __cplusplus << '\n'
              << "_MSC_VER=" << _MSC_VER << '\n'
              << "sizeof(void*)=" << sizeof(void*) << '\n'
              << "sizeof(Vec3)=" << sizeof(Vec3) << '\n'
              << "sizeof(PlayerState)=" << sizeof(PlayerState) << '\n'
              << "alignof(PlayerState)=" << alignof(PlayerState) << '\n'
              << "windows_sdk_probe=" << (sdk_ok ? "OK" : "FAILED") << '\n'
              << "TOOLCHAIN_CHECK=" << (sdk_ok ? "PASS" : "FAIL") << '\n';
    return sdk_ok ? 0 : 1;
}
