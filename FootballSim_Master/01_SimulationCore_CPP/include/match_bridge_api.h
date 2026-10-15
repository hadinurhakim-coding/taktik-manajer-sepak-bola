#ifndef FSIM_MATCH_BRIDGE_API_H
#define FSIM_MATCH_BRIDGE_API_H
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
# if defined(FSIM_BRIDGE_EXPORTS)
#  define FSIM_API __declspec(dllexport)
# else
#  define FSIM_API __declspec(dllimport)
# endif
# define FSIM_CALL __cdecl
#else
# define FSIM_API __attribute__((visibility("default")))
# define FSIM_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t fsim_match_handle; /* opaque process-local token; zero is invalid */
enum fsim_status {
    FSIM_OK = 0,
    FSIM_INVALID_ARGUMENT = 1,
    FSIM_INVALID_HANDLE = 2,
    FSIM_BUFFER_TOO_SMALL = 3,
    FSIM_INTERNAL_ERROR = 4
};

FSIM_API uint32_t FSIM_CALL get_match_api_version(void);
/* UTF-8 fsim.match_input.v1 JSON, exact byte count (excluding NUL), maximum 1 MiB.
   Input is copied. Caller owns its memory. On failure *out_handle is zero. */
FSIM_API int32_t FSIM_CALL init_match(const char* input_json, size_t input_size, fsim_match_handle* out_handle);
/* Each tick advances 1/60 real second and 0.1 game second; clamps at full time.
   Calls are serialized internally; count=0 is a no-op. No real-world sleep. */
FSIM_API int32_t FSIM_CALL tick_match_fixed(fsim_match_handle handle, uint32_t tick_count);
/* required_size includes terminating NUL. Query with NULL/0, then allocate and
   retry. Query/undersized buffers return FSIM_BUFFER_TOO_SMALL, never truncate.
   Snapshot is JSON, not a legacy friendly-* replay or .fsim file. */
FSIM_API int32_t FSIM_CALL get_match_snapshot(fsim_match_handle handle, char* output, size_t capacity, size_t* required_size);
/* JSON latest goal + monotonic sequence and score. Reading does not consume it.
   Compare sequence with the caller's previous value, not a transient boolean. */
FSIM_API int32_t FSIM_CALL get_match_goal_status(fsim_match_handle handle, char* output, size_t capacity, size_t* required_size);
/* Binary FSIM v1: required_size is exact bytes, WITHOUT a NUL. Same sizing
   protocol as snapshots. Caller may write the returned bytes to a .fsim file.
   Last 15 presentation seconds, sampled every 0.05 s, inclusive endpoints. */
FSIM_API int32_t FSIM_CALL export_match_replay(fsim_match_handle handle, uint8_t* output, size_t capacity, size_t* required_size);
/* FSIM v2 full archive plus source-time segments. Same two-call buffer contract. */
FSIM_API int32_t FSIM_CALL export_match_archive(fsim_match_handle handle, uint8_t* output, size_t capacity, size_t* required_size);
FSIM_API int32_t FSIM_CALL get_engine_schema(char* output, size_t capacity, size_t* required_size);
FSIM_API int32_t FSIM_CALL destroy_match(fsim_match_handle handle);
/* Thread-local message for the last call; success clears it. Copy failures
   immediately on the same thread.
   Calling this function does not replace the error being queried. */
FSIM_API int32_t FSIM_CALL get_match_last_error(char* output, size_t capacity, size_t* required_size);
#ifdef __cplusplus
}
#endif
#endif
