"""FSIM v2 edit-decision timeline. Match ticks remain immutable; playback is in game seconds.

900 seconds includes a 6-second halftime card and repeated 7.2-second moments at
0.5x (14.4 seconds on screen). Normal clips play at physical/game-time 1x.
The old 6x simulation clock remains compatible with v1 ring-buffer clips.
"""
import copy
import json
import random
import struct
from read_fsim import decode_fsim, HEADER, FRAME_BYTES, MAGIC

def encode_metadata(original, metadata, version=2, first=0, last=None):
    header=HEADER.unpack_from(original)
    last=header[4]-1 if last is None else last
    start=40+header[3]+first*FRAME_BYTES
    body=bytearray(original[start:start+(last-first+1)*FRAME_BYTES])
    if version==1:
        for offset in range(0,len(body),FRAME_BYTES):
            flags=struct.unpack_from('<I',body,offset+20)[0]
            struct.pack_into('<I',body,offset+20,flags&7)
    meta=json.dumps(metadata,separators=(',',':'),ensure_ascii=False,allow_nan=False).encode('utf-8')
    return HEADER.pack(MAGIC,version,40,len(meta),last-first+1,FRAME_BYTES,3,60,10)+meta+body

def slice_highlights(archive, target_seconds=900):
    replay=decode_fsim(archive)
    frames=replay['frames']; n=len(frames)-1
    if frames[0]['tick']!=0 or frames[-1]['tick']!=54000: raise ValueError('Slicing requires a complete 90-minute archive')
    goals=[i for i,f in enumerate(frames) if f['flags']&1]
    saves=[]
    for i,f in enumerate(frames):
        if f['flags']&32 and all(abs(i-j)>200 for j in goals+saves) and len(saves)<4: saves.append(i)
    slow=[]
    for i in sorted(goals+saves):
        lo,hi=(0,9000) if i<9000 else (9000,n)
        a,b=max(lo,i-17),min(hi,i+7)
        if (b-a)*0.3>=5: slow.append((i,a,b))
    replay_duration=sum((b-a)*0.3/0.5 for _,a,b in slow)
    budget=round((target_seconds-6-replay_duration)/0.3)
    if budget<=0: raise ValueError('Target duration cannot contain requested replays')
    selected=[False]*n
    def add(a,b,mandatory=False):
        a,b=max(0,a),min(n,b)
        new=[j for j in range(a,b) if not selected[j]]
        remaining=budget-sum(selected)
        if mandatory and len(new)>remaining: raise ValueError('Required events exceed highlight budget')
        for j in new[:max(0,remaining)]: selected[j]=True
    add(0,70,True); add(8950,9000,True); add(9000,9050,True); add(n-70,n,True)
    for i,a,b in slow: add(i-40,b,True)
    for i in goals: add(i-40,i+10,True)
    for i,f in enumerate(frames):
        if f['flags']&8: add(i-30,i+10)
    # Distributed buildup clips fill remaining time from both halves; no
    # manufactured goals, repeated filler, freeze-frame padding or altered scores.
    centres=list(range(90,n,180)); random.Random(31).shuffle(centres)
    for i in centres: add(i-30,i+30)
    if sum(selected)<budget:
        for i in range(n): add(i,i+1)
    runs=[]; start=None
    for i in range(n+1):
        if i==9000 and start is not None: runs.append((start,i)); start=None
        value=i<n and selected[i]
        if value and start is None: start=i
        if not value and start is not None: runs.append((start,i)); start=None
    segments=[]; halftime=False
    def segment(a,b,kind='live',rate=1.0,hold=0.0,event=None):
        if b<=a and hold<=0: return
        segments.append({'first_frame':a,'last_frame':b,'rate':rate,'hold_seconds':hold,'kind':kind,
                         'camera':'alternate' if kind=='replay' else 'broadcast', 'event_frame':event})
    for a,b in runs:
        if a>=9000 and not halftime:
            segment(9000,9000,'halftime',hold=6.0); halftime=True
        cursor=a
        for event,sa,sb in slow:
            if a < sb <= b:
                segment(cursor,sb); segment(sa,sb,'replay',0.5,event=event); cursor=sb
        segment(cursor,b)
    meta=copy.deepcopy(replay['metadata'])
    meta.update(segments=segments,timeline_seconds=sum((s['last_frame']-s['first_frame'])*0.3/s['rate']+s['hold_seconds'] for s in segments),
                target_seconds=target_seconds,source_ticks_per_second=10,highlight_policy='all goals, up to four major saves, shots, distributed buildup',
                slow_motion_window='5.1 seconds before + 2.1 seconds after event; inserted after event',statistics_source='unaltered C++ match')
    if abs(meta['timeline_seconds']-target_seconds)>0.00001: raise AssertionError('Highlight budget mismatch')
    output=encode_metadata(archive,meta)
    decode_fsim(output)
    return output

def legacy_clip(archive, end_frame):
    replay=decode_fsim(archive); frames=replay['frames']
    first=max(0,end_frame-300)
    meta=copy.deepcopy(replay['metadata'])
    for key in ('segments','timeline_seconds','source_ticks_per_second'): meta.pop(key,None)
    meta.update(format='fsim.replay.v1',first_tick=frames[first]['tick'],last_tick=frames[end_frame]['tick'])
    result=encode_metadata(archive,meta,1,first,end_frame)
    decode_fsim(result)
    return result
