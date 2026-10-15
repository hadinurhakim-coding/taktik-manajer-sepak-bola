"""Reference reader for FSIM v1, using explicit little-endian fields (stdlib only)."""
import argparse
import json
import math
from pathlib import Path
import struct

MAGIC = b"FSIM\r\n\x1a\n"
HEADER = struct.Struct("<8s8I")
PREFIX = struct.Struct("<Q4IQi6f")
PLAYER = struct.Struct("<3fI")
FRAME_BYTES = PREFIX.size + 22 * PLAYER.size  # 412, no native struct padding


def decode_fsim(data):
    if len(data) < HEADER.size:
        raise ValueError("Truncated FSIM header")
    magic, version, header_size, meta_size, count, frame_size, sample_ticks, real_rate, game_rate = HEADER.unpack_from(data)
    if version not in (1, 2) or (magic, header_size, frame_size, sample_ticks, real_rate, game_rate) != (MAGIC, 40, FRAME_BYTES, 3, 60, 10):
        raise ValueError("Unsupported FSIM header/version/rates")
    if not 1 <= count <= (301 if version == 1 else 18001) or meta_size > 1024 * 1024 or len(data) != header_size + meta_size + count * frame_size:
        raise ValueError("Invalid FSIM lengths/frame count")
    metadata = json.loads(data[header_size:header_size + meta_size].decode("utf-8"))
    if metadata.get("format") != f"fsim.replay.v{version}" or metadata.get("time_base") != "presentation" or metadata.get("time_scale") != 6:
        raise ValueError("Unsupported replay metadata")
    identities = metadata.get("players", [])
    if len(identities) != 22 or len({p["id"] for p in identities}) != 22:
        raise ValueError("Expected 22 distinct player identities")
    for i, p in enumerate(identities):
        if p["team"] != i // 11 or p["lineup_index"] != i % 11:
            raise ValueError("Player identity order mismatch")
    frames = []
    offset = header_size + meta_size
    for _ in range(count):
        tick, home, away, phase, flags, goal_seq, owner, *ball = PREFIX.unpack_from(data, offset)
        offset += PREFIX.size
        if tick > 54000 or tick % 3 or phase not in (0, 1, 2) or flags & ~(7 if version == 1 else 63) or not -1 <= owner < 22 or goal_seq != home + away:
            raise ValueError("Invalid frame state")
        if phase != (2 if tick == 54000 else 1 if tick >= 27000 else 0):
            raise ValueError("Phase disagrees with clock")
        if frames and (tick != frames[-1]["tick"] + 3 or home < frames[-1]["score"][0] or away < frames[-1]["score"][1]):
            raise ValueError("Replay timeline is not consecutive/monotonic")
        if frames and goal_seq > frames[-1]["goal_sequence"] and not flags & 1:
            raise ValueError("Missing goal marker")
        players = []
        for _ in range(22):
            x, y, z, action = PLAYER.unpack_from(data, offset)
            offset += PLAYER.size
            if action > 4 or not all(math.isfinite(v) for v in (x, y, z)):
                raise ValueError("Invalid player state")
            players.append({"position": [x, y, z], "action": action})
        if not all(math.isfinite(v) for v in ball):
            raise ValueError("Invalid ball coordinates")
        frames.append({"tick": tick, "score": [home, away], "phase": phase, "flags": flags,
                       "goal_sequence": goal_seq, "ball_owner": owner, "ball": ball[:3], "ball_velocity": ball[3:], "players": players})
    if (metadata.get("first_tick"), metadata.get("last_tick")) != (frames[0]["tick"], frames[-1]["tick"]):
        raise ValueError("Metadata timeline mismatch")
    if version == 2:
        segments = metadata.get('segments', [])
        if metadata.get('source_ticks_per_second') != 10 or not 1 <= len(segments) <= 256:
            raise ValueError('Invalid highlight timeline')
        duration = 0
        for s in segments:
            a,b=s.get('first_frame'),s.get('last_frame')
            rate,hold=s.get('rate'),s.get('hold_seconds')
            if type(a) is not int or type(b) is not int or not 0 <= a <= b < count:
                raise ValueError('Segment frame range outside archive')
            if rate not in (0.5,1.0) or not isinstance(hold,(int,float)) or not math.isfinite(hold) or not 0 <= hold <= 10:
                raise ValueError('Invalid segment playback rate or hold')
            if s.get('kind') not in ('live','replay','halftime') or s.get('camera') not in ('broadcast','alternate'):
                raise ValueError('Invalid segment kind/camera')
            length=(frames[b]['tick']-frames[a]['tick'])/10/rate+hold
            if length <= 0: raise ValueError('Empty timeline segment')
            duration += length
        target=metadata.get('timeline_seconds')
        if not isinstance(target,(int,float)) or not math.isfinite(target) or abs(target-duration)>1e-5:
            raise ValueError('Timeline duration mismatch')
    return {"metadata": metadata, "frames": frames}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path)
    args = parser.parse_args()
    result = decode_fsim(args.file.read_bytes())
    frames = result["frames"]
    print(json.dumps({"file": str(args.file.resolve()), "format": result["metadata"]["format"],
                      "frames": len(frames), "first_tick": frames[0]["tick"], "last_tick": frames[-1]["tick"],
                      "presentation_seconds": (frames[-1]["tick"] - frames[0]["tick"]) / 60,
                      "score": frames[-1]["score"], "goal_markers": sum(bool(f["flags"] & 1) for f in frames)}, indent=2))


if __name__ == "__main__":
    main()
