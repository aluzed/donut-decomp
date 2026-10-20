#!/usr/bin/env python3
"""Transcribe the game's spoken lines into a subtitles table.

The game ships no text for its dialogue: its lines are sound only
(art/audio/dialog/DIALOGF.RCF, RSD files in a Radcore cement library). This reads
each one, decodes it (16-bit PCM or Radical's 4-bit ADPCM, "RADP") and hands it to
a speech recogniser run on this machine's graphics card: NVIDIA's Parakeet (by
default) or faster-whisper. The text is machine heard, imperfect in places
(names, shouts).

The table, one line a sound, tab-separated: its path in the library, then its text
(empty when nothing was heard):

    agnes\\p_hitbyc_agn_sack.rsd<TAB>Espèce de grossier personnage!

Usage, in a virtual environment with numpy and either onnx-asr
(`onnx-asr[gpu,hub]`, `onnxruntime-gpu[cuda,cudnn]`) or faster-whisper:

    python tools/transcribe_dialogue.py --model-dir <parakeet's folder>
        [--engine parakeet|whisper] [--limit N] [--out art/audio/dialog/subtitles.tsv]

Lines already in the output are kept, so an interrupted run carries on.
"""

import argparse
import os
import struct
import sys

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]
STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
    5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767,
]


def read_library(path):
    """The library's files: {name: bytes}, as src/RCL/RCFFile.cpp reads it."""
    with open(path, "rb") as file:
        data = file.read()
    if not data.startswith(b"RADCORE CEMENT LIBRARY"):
        raise ValueError(f"{path} is not a cement library")
    data_offset = struct.unpack_from("<I", data, 0x24)[0]
    count, table = struct.unpack_from("<II", data, data_offset)
    entries = {}
    for i in range(count):
        file_hash, offset, size = struct.unpack_from("<III", data, data_offset + 16 + 12 * i)
        entries[file_hash] = (offset, size)
    names_count = struct.unpack_from("<I", data, table)[0]
    at = table + 8
    files = {}
    for _ in range(names_count):
        length = struct.unpack_from("<I", data, at)[0]
        name = data[at + 4:at + 4 + length].split(b"\0")[0].decode("latin1")
        at += 4 + length + 4
        offset, size = entries[name_hash(name)]
        files[name] = data[offset:offset + size]
    return files


def name_hash(name):
    value = 0
    for c in name.encode("latin1"):
        if c < 97:
            c += 32
        value = (value * 0x1F + c) & 0xFFFFFFFF
    return value


def decode_sound(data):
    """(samples as float32 mono, sample rate) of an RSD file."""
    magic = data[:8]
    channels, bits, rate = struct.unpack_from("<III", data, 8)
    body = data[0x800:]
    if magic == b"RSD4PCM ":
        samples = np.frombuffer(body[: len(body) // 2 * 2], dtype="<i2").astype(np.float32)
    elif magic == b"RSD4RADP":
        samples = decode_radp(body, channels)
    else:
        raise ValueError(f"unknown sound format {magic!r}")
    if channels > 1:
        samples = samples[: len(samples) // channels * channels].reshape(-1, channels).mean(axis=1)
    return samples / 32768.0, rate


def decode_radp(body, channels):
    # Blocks of 32 samples a channel: each channel's step index and first
    # value (two int16), then the nibbles, channels interleaved byte by byte.
    block_size = channels * 20
    blocks = len(body) // block_size
    out = np.zeros(blocks * 32 * channels, dtype=np.float32)
    for block in range(blocks):
        base = block * block_size
        for channel in range(channels):
            step_index, value = struct.unpack_from("<hh", body, base + channel * 4)
            step_index = min(max(step_index, 0), 88)
            for i in range(32):
                byte = body[base + 4 * channels + channel + (i // 2) * channels]
                nibble = (byte >> (4 if i & 1 else 0)) & 0xF
                step = STEP_TABLE[step_index]
                delta = step >> 3
                if nibble & 1:
                    delta += step >> 2
                if nibble & 2:
                    delta += step >> 1
                if nibble & 4:
                    delta += step
                if nibble & 8:
                    delta = -delta
                value = min(max(value + delta, -32768), 32767)
                step_index = min(max(step_index + INDEX_TABLE[nibble], 0), 88)
                out[(block * 32 + i) * channels + channel] = value
    return out


def resample(samples, rate, to=16000):
    if rate == to or len(samples) == 0:
        return samples
    length = int(len(samples) * to / rate)
    return np.interp(np.linspace(0, len(samples) - 1, length), np.arange(len(samples)), samples).astype(np.float32)


def parakeet(model_dir):
    """NVIDIA's Parakeet TDT 0.6B v3 (25 European languages, French among them,
    heard without being told), run by onnx-asr on the card."""
    import onnxruntime
    onnxruntime.preload_dlls()
    import onnx_asr
    model = onnx_asr.load_model("nemo-parakeet-tdt-0.6b-v3", model_dir,
                                providers=["CUDAExecutionProvider", "CPUExecutionProvider"])
    return lambda waves: model.recognize(waves, sample_rate=16000)


def whisper(name, language, threads):
    """faster-whisper, on the card if CTranslate2 finds it, else on the CPU."""
    from faster_whisper import WhisperModel
    import ctranslate2
    device = "cuda" if ctranslate2.get_cuda_device_count() > 0 else "cpu"
    model = WhisperModel(name, device=device, compute_type="float16" if device == "cuda" else "int8",
                         cpu_threads=threads)

    def hear(waves):
        texts = []
        for wave in waves:
            segments, _ = model.transcribe(wave, language=language, beam_size=5, condition_on_previous_text=False)
            texts.append(" ".join(segment.text.strip() for segment in segments).strip())
        return texts

    return hear


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--library", default=os.path.join(ROOT, "art/audio/dialog/DIALOGF.RCF"))
    parser.add_argument("--out", default=os.path.join(ROOT, "art/audio/dialog/subtitles.tsv"))
    parser.add_argument("--engine", choices=["parakeet", "whisper"], default="parakeet")
    parser.add_argument("--model-dir", help="parakeet: the model's files (istupakov/parakeet-tdt-0.6b-v3-onnx), "
                                            "in a plain folder (ONNX Runtime refuses the hub cache's links)")
    parser.add_argument("--model", default="medium", help="whisper: its model")
    parser.add_argument("--language", default="fr", help="whisper: the voices' language")
    parser.add_argument("--limit", type=int, default=0, help="at most this many lines (a trial)")
    parser.add_argument("--threads", type=int, default=os.cpu_count() or 4)
    args = parser.parse_args()

    files = read_library(args.library)
    done = {}
    if os.path.exists(args.out):
        with open(args.out, encoding="utf-8") as table:
            for line in table:
                name, _, text = line.rstrip("\n").partition("\t")
                done[name] = text
    names = [name for name in sorted(files) if name.lower().endswith(".rsd") and name not in done]
    if args.limit:
        names = names[: args.limit]
    print(f"{len(files)} sounds, {len(done)} already heard, {len(names)} to hear", file=sys.stderr)

    hear = parakeet(args.model_dir) if args.engine == "parakeet" else whisper(args.model, args.language, args.threads)
    # A line at a time.
    with open(args.out, "a", encoding="utf-8") as table:
        for count, name in enumerate(names, 1):
            text = hear([resample(*decode_sound(files[name]))])[0].strip()
            table.write(f"{name}\t{text.replace(chr(9), ' ')}\n")
            table.flush()
            print(f"[{count}/{len(names)}] {name}: {text}", file=sys.stderr)


if __name__ == "__main__":
    main()
