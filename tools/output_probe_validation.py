"""Verify readback artifacts and reported window geometry; not human comfort."""
import hashlib
import json
import re

FRAMES = (1000, 3000, 6000)
ROLES = ('left', 'center', 'right')

def validate(folder, physical=False):
    result = {'validated': False, 'errors': [], 'records': [],
              'scope': 'CPU source and default OpenGL backbuffer before swap; not desktop scanout'}
    try:
        rows = [json.loads(line) for line in (folder/'probes.jsonl').read_text().splitlines()]
        expected = {(frame, role) for frame in FRAMES for role in ROLES}
        if len(rows) != 9 or {(row['frame'],row['role']) for row in rows} != expected:
            raise ValueError('Expected exactly three roles at all three recorded frames')
        for row in rows:
            if not all(row[key] for key in ('written','display_ok','shader_loaded')) or row['minimized']:
                raise ValueError('Unavailable, unshaded or minimized output')
            if (row['gl_width'],row['gl_height']) != (2560,1440) or row['coloured_pixels'] <= 0:
                raise ValueError('Unexpected drawable or blank output')
            for key in ('cpu_sha256','gl_sha256'):
                if not re.fullmatch('[0-9a-f]{64}', row[key]):
                    raise ValueError('Invalid pixel hash')
            raw = (folder/f"{row['frame']}-{row['role']}.rgba").read_bytes()
            if len(raw) != row['gl_width']*row['gl_height']*4 or hashlib.sha256(raw).hexdigest()!=row['gl_sha256']:
                raise ValueError('Truncated or altered readback')
            x = {'left':-2560,'center':0,'right':2560}[row['role']] if physical else {'left':0,'center':2560,'right':5120}[row['role']]
            if row['window_bounds'] != [x,0,2560,1440]:
                raise ValueError('Window is not at its requested panel bounds')
            display_bounds = [x,0,2560,1440] if physical else [0,0,7680,1440]
            if row['display_bounds'] != display_bounds:
                raise ValueError('Window assigned to an unexpected actual display')
            if physical and row['display_primary'] != (row['role']=='center'):
                raise ValueError('Center-primary assignment differs')
            if physical:
                for key in ('keyboard_focus','input_focus','mouse_focus','native_foreground'):
                    if row[key] is not (row['role']=='center'):
                        raise ValueError('Expected center-only '+key)
            if type(row['window_id']) is not int or row['window_id'] <= 0:
                raise ValueError('Invalid SDL window identity')
        for frame in FRAMES:
            frame_rows = [row for row in rows if row['frame']==frame]
            ids = {row['display_id'] for row in frame_rows}
            if len(ids) != (3 if physical else 1):
                raise ValueError('Actual SDL monitor identities differ from requested topology')
            if len({row['window_id'] for row in frame_rows}) != 3:
                raise ValueError('Panel window identities are not distinct')
        for role in ROLES:
            if len({row['window_id'] for row in rows if row['role']==role}) != 1:
                raise ValueError('Panel window identity changed between checkpoints')
        result['records'] = rows
        result['validated'] = True
    except Exception as error:
        result['errors'].append(type(error).__name__+': '+str(error))
    return result

def compare(first, second):
    if not first['validated'] or not second['validated']:
        raise ValueError('Cannot compare unvalidated probes')
    a = {(row['frame'],row['role']):row for row in first['records']}
    b = {(row['frame'],row['role']):row for row in second['records']}
    changed = [{'frame':frame,'role':role,'field':field} for frame,role in a for field in
               ('cpu_sha256','gl_sha256','cpu_width','cpu_height','gl_width','gl_height') if a[(frame,role)][field]!=b[(frame,role)][field]]
    return {'equivalent': not changed, 'comparedOutputs':len(a), 'differences':changed}
