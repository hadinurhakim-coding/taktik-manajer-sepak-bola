"""ctypes caller for the version-1 C ABI; no engine-specific Python packages."""
import ctypes as C
import json
from pathlib import Path


class MatchLibrary:
    def __init__(self, library_path):
        self.api = C.CDLL(str(Path(library_path).resolve()))
        handle = C.c_uint64
        buffer = C.POINTER(C.c_char)
        size_out = C.POINTER(C.c_size_t)
        signatures = {
            "get_match_api_version": ([], C.c_uint32),
            "init_match": ([C.c_char_p, C.c_size_t, C.POINTER(handle)], C.c_int32),
            "tick_match_fixed": ([handle, C.c_uint32], C.c_int32),
            "get_match_snapshot": ([handle, buffer, C.c_size_t, size_out], C.c_int32),
            "get_match_goal_status": ([handle, buffer, C.c_size_t, size_out], C.c_int32),
            "export_match_replay": ([handle, C.POINTER(C.c_uint8), C.c_size_t, size_out], C.c_int32),
            "get_engine_schema": ([buffer, C.c_size_t, size_out], C.c_int32),
            "destroy_match": ([handle], C.c_int32),
            "get_match_last_error": ([buffer, C.c_size_t, size_out], C.c_int32),
        }
        for name, (args, result) in signatures.items():
            method = getattr(self.api, name)
            method.argtypes, method.restype = args, result
        if self.api.get_match_api_version() != 1:
            raise RuntimeError("Unsupported MatchEngine C ABI")

    def check(self, status):
        if status:
            message = C.create_string_buffer(512)
            required = C.c_size_t()
            self.api.get_match_last_error(message, len(message), C.byref(required))
            raise RuntimeError(f"MatchEngine status {status}: {message.value.decode('utf-8', errors='replace')}")

    def read_json(self, method, *arguments):
        required = C.c_size_t()
        status = method(*arguments, None, 0, C.byref(required))
        if status != 3:
            self.check(status)
        for _ in range(4):
            result = C.create_string_buffer(required.value)
            status = method(*arguments, result, len(result), C.byref(required))
            if status == 3: # another thread may tick between sizing and copying
                continue
            self.check(status)
            return json.loads(result.value.decode("utf-8"))
        raise RuntimeError("Snapshot changed repeatedly; serialize calls from the host")

    def schema(self):
        return self.read_json(self.api.get_engine_schema)

    def create(self, data):
        encoded = json.dumps(data, ensure_ascii=False, allow_nan=False).encode("utf-8")
        handle = C.c_uint64()
        self.check(self.api.init_match(encoded, len(encoded), C.byref(handle)))
        return MatchSession(self, handle.value)


class MatchSession:
    def __init__(self, library, handle):
        self.library, self.handle = library, handle

    def tick(self, count=1):
        if type(count) is not int or not 0 <= count <= 2**32 - 1:
            raise ValueError("tick count must fit uint32")
        self.library.check(self.library.api.tick_match_fixed(self.handle, count))

    def snapshot(self):
        return self.library.read_json(self.library.api.get_match_snapshot, self.handle)

    def goals(self):
        return self.library.read_json(self.library.api.get_match_goal_status, self.handle)

    def replay_bytes(self):
        required = C.c_size_t()
        method = self.library.api.export_match_replay
        status = method(self.handle, None, 0, C.byref(required))
        if status != 3:
            self.library.check(status)
        for _ in range(4):
            output = (C.c_uint8 * required.value)()
            status = method(self.handle, output, len(output), C.byref(required))
            if status == 3:
                continue
            self.library.check(status)
            return bytes(output[:required.value])
        raise RuntimeError("Replay changed repeatedly; serialize calls from the host")

    def close(self):
        if self.handle:
            self.library.check(self.library.api.destroy_match(self.handle))
            self.handle = 0

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()
