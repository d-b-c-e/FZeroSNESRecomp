"""Isolated DLSS NR bridge runner. Native ABI from ComfyUI-DLSS5-NR v0.3.0 (MIT).

No ComfyUI/Torch dependency. Live mode uses a single-slot Windows shared-memory
mailbox; process mode provides a reproducible still/sequence capability test.
"""
import argparse
import ctypes as c
import hashlib
import json
import mmap
import os
from pathlib import Path
import struct
import sys
import time

import numpy as np
from PIL import Image

MAX_PIXELS = 1280 * 960
HEADER = 1024
SIZE = HEADER + MAX_PIXELS * 8


class Bridge:
    def __init__(self, root):
        self.root = Path(root).resolve()
        self.handles = [os.add_dll_directory(str(self.root / p))
                        for p in ('native/bin', 'runtime', 'runtime/caller')]
        self.lib = c.CDLL(str(self.root / 'native/bin/dlss5nr_bridge.dll'))
        self.error = c.create_string_buffer(4096)
        self.lib.dlss5nr_init.argtypes = [c.c_int, c.c_wchar_p, c.c_char_p, c.c_int]
        self.lib.dlss5nr_init.restype = c.c_int
        self.lib.dlss5nr_process.argtypes = [c.POINTER(c.c_float)] * 2 + [c.c_int] * 4 + [c.c_float] * 4 + [c.c_int] * 3 + [c.c_char_p, c.c_int]
        self.lib.dlss5nr_process.restype = c.c_int
        self.lib.dlss5nr_gpu_name.restype = c.c_char_p
        self.lib.dlss5nr_version.restype = c.c_char_p
        self.lib.dlss5nr_shutdown.restype = None
        self.check(self.lib.dlss5nr_init(0, str(self.root / 'runtime'), self.error, len(self.error)))

    def check(self, result):
        if not result:
            raise RuntimeError(self.error.value.decode('utf-8', 'replace'))

    def info(self):
        runtime = self.root / 'runtime/nvngx_dlssnr.dll'
        with runtime.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        return dict(gpu=self.lib.dlss5nr_gpu_name().decode(),
                    bridge=self.lib.dlss5nr_version().decode(), runtime_sha256=digest,
                    optical_flow=bool(self.lib.dlss5nr_nvof_available()))

    def process(self, rgb, reset, temporal, args):
        src = np.ascontiguousarray(rgb, dtype=np.float32) / 255.0
        dst = np.empty_like(src)
        h, w, _ = src.shape
        start = time.perf_counter()
        self.check(self.lib.dlss5nr_process(
            src.ctypes.data_as(c.POINTER(c.c_float)), dst.ctypes.data_as(c.POINTER(c.c_float)),
            w, h, args.style, 3, args.intensity, args.tone, args.structure, -1.0,
            0, int(reset), int(temporal), self.error, len(self.error)))
        elapsed = (time.perf_counter() - start) * 1000
        if not np.isfinite(dst).all():
            raise RuntimeError('Neural output contains non-finite pixels')
        # Pin interpretation for the sequence, rather than switching each frame.
        if not hasattr(self, 'swap'):
            self.swap = bool(np.mean(np.abs(dst[..., ::-1] - src)) < np.mean(np.abs(dst - src)))
        if self.swap:
            dst = dst[..., ::-1]
        return (np.clip(dst, 0, 1) * 255).round().astype(np.uint8), elapsed


def live(args):
    kernel = c.WinDLL('kernel32', use_last_error=True)
    kernel.OpenEventW.argtypes = [c.c_uint32, c.c_int, c.c_wchar_p]
    kernel.OpenEventW.restype = c.c_void_p
    kernel.WaitForMultipleObjects.argtypes = [c.c_uint32, c.POINTER(c.c_void_p), c.c_int, c.c_uint32]
    kernel.SetEvent.argtypes = [c.c_void_p]
    kernel.CloseHandle.argtypes = [c.c_void_p]
    handles = [kernel.OpenEventW(0x1F0003, False, args.mapping + suffix)
               for suffix in ('-stop', '-request', '-done')]
    if not all(handles):
        raise OSError(c.get_last_error(), 'OpenEvent failed')
    memory = mmap.mmap(-1, SIZE, tagname=args.mapping)
    log_root = Path(args.root) if Path(args.root).is_dir() else Path(__file__).resolve().parent
    log = open(log_root / 'live.jsonl', 'a', buffering=1)
    bridge = None
    try:
        bridge = Bridge(args.root)
        log.write(json.dumps(dict(event='init', **bridge.info())) + '\n')
        struct.pack_into('<I', memory, 12, 1)
        kernel.SetEvent(handles[2])
        last_size = None
        waits = (c.c_void_p * 2)(*handles[:2])
        while True:
            result = kernel.WaitForMultipleObjects(2, waits, False, 60000)
            if result != 1:
                break
            w, h, reset, _, _, sequence = struct.unpack_from('<6I', memory)
            if not (0 < w <= 1280 and 0 < h <= 960):
                raise ValueError('Invalid mailbox dimensions')
            packed = np.frombuffer(memory, '<u4', w * h, HEADER).copy().reshape(h, w)
            rgb = np.stack([(packed >> 16) & 255, (packed >> 8) & 255, packed & 255], axis=-1).astype(np.uint8)
            out, ms = bridge.process(rgb, not args.temporal or reset or last_size != (w, h), args.temporal, args)
            last_size = (w, h)
            packed_out = (out[..., 0].astype(np.uint32) << 16) | (out[..., 1].astype(np.uint32) << 8) | out[..., 2]
            memory[HEADER + MAX_PIXELS * 4:HEADER + MAX_PIXELS * 4 + w * h * 4] = packed_out.tobytes()
            struct.pack_into('<I', memory, 12, 2)
            struct.pack_into('<I', memory, 16, round(ms))
            struct.pack_into('<I', memory, 24, sequence)
            log.write(json.dumps(dict(event='evaluated', sequence=sequence, milliseconds=ms,
                                      reset=not args.temporal or bool(reset), temporal=args.temporal,
                                      width=w, height=h, swap_rb=bridge.swap)) + '\n')
            if sequence % 120 == 0:
                Image.fromarray(rgb).save(Path(args.root) / 'live-original.png')
                Image.fromarray(out).save(Path(args.root) / 'live-neural.png')
            kernel.SetEvent(handles[2])
    except Exception as error:
        message = str(error).encode('utf-8')[:899]
        memory[64:64 + len(message) + 1] = message + b'\0'
        struct.pack_into('<i', memory, 12, -1)
        log.write(json.dumps(dict(event='error', error=str(error))) + '\n')
        kernel.SetEvent(handles[2])
    finally:
        if bridge:
            bridge.lib.dlss5nr_shutdown()
        memory.close()
        for handle in handles:
            kernel.CloseHandle(handle)
        log.close()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('mode', choices=['probe', 'process', 'live'])
    p.add_argument('--root', required=True)
    p.add_argument('--mapping')
    p.add_argument('--input', type=Path)
    p.add_argument('--output', type=Path)
    p.add_argument('--width', type=int, default=640)
    p.add_argument('--height', type=int, default=480)
    p.add_argument('--style', type=int, default=1)
    p.add_argument('--temporal', action='store_true', help='Experimental NVOFA temporal path; off for live Ampere compatibility')
    p.add_argument('--intensity', type=float, default=1)
    p.add_argument('--tone', type=float, default=1)
    p.add_argument('--structure', type=float, default=1)
    args = p.parse_args()
    if args.mode == 'live':
        if not args.mapping:
            p.error('live requires --mapping')
        return live(args)
    bridge = Bridge(args.root)
    try:
        info = bridge.info()
        print(json.dumps(info), flush=True)
        if args.mode == 'probe':
            return
        if not args.input or not args.output:
            p.error('process requires --input and --output')
        paths = sorted(args.input.glob('*.ppm')) if args.input.is_dir() else [args.input]
        if not paths:
            p.error('no input frames')
        args.output.mkdir(parents=True, exist_ok=False)
        timings = []
        for i, path in enumerate(paths):
            rgb = np.array(Image.open(path).convert('RGB').resize((args.width, args.height), Image.Resampling.NEAREST))
            out, ms = bridge.process(rgb, not args.temporal or i == 0, args.temporal, args)
            Image.fromarray(out).save(args.output / f'{i:06d}.png')
            if i == 0:
                Image.fromarray(rgb).save(args.output / 'original.png')
            timings.append(ms)
            print(json.dumps(dict(frame=i, milliseconds=ms, changed=int(np.count_nonzero(out != rgb)))), flush=True)
        info.update(settings=vars(args) | {'input': str(args.input), 'output': str(args.output)},
                    milliseconds=timings, swap_rb=bridge.swap)
        (args.output / 'report.json').write_text(json.dumps(info, indent=2))
    finally:
        # The pinned bridge can hang in NGX shutdown on this driver. This
        # disposable CLI process owns no persistent GPU state; terminate it
        # after flushing results, bypassing DLL detach callbacks.
        if sys.exc_info()[1] is not None:
            print(str(sys.exc_info()[1]), file=sys.stderr)
        sys.stdout.flush()
        sys.stderr.flush()
        kernel = c.WinDLL('kernel32')
        kernel.GetCurrentProcess.restype = c.c_void_p
        kernel.TerminateProcess.argtypes = [c.c_void_p, c.c_uint]
        kernel.TerminateProcess(kernel.GetCurrentProcess(), 0 if sys.exc_info()[0] is None else 1)


if __name__ == '__main__':
    main()
