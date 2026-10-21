#!/usr/bin/env python3
"""Transcribe the game's movies into a subtitles table.

The movies (movies/*.RMV, Bink) have no text for what is said in them. This takes
each one's soundtracks (English, French, German, Spanish: FFmpeg decodes each,
mono, 16 kHz), cuts them where someone speaks
(Silero's voice activity detector) and hands each stretch to NVIDIA's Parakeet run
on this machine's graphics card, as tools/transcribe_dialogue.py does the lines.
The text is machine heard, imperfect in places.

The table, one line a stretch, tab-separated: the movie, its soundtrack (0 English,
1 French, 2 German, 3 Spanish, as the game picks them), when it starts and ends
(seconds), its text:

    FMV1A.RMV<TAB>1<TAB>5.25<TAB>7.58<TAB>Je soutiens un nouveau soda, les enfants !

Usage, in the virtual environment of tools/transcribe_dialogue.py:

    python tools/transcribe_movies.py --model-dir <parakeet's folder>
        [--movies movies] [--out art/audio/dialog/movie_subtitles.tsv]

A movie's soundtrack already in the output is not heard again, so an interrupted run
carries on.
"""

import argparse
import os
import subprocess
import sys

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# The studios' logos say nothing worth a line.
SILENT = {"RADLOGO.RMV", "FOXLOGO.RMV", "GRACIE.RMV", "VUGLOGO.RMV"}


def sound(path, track):
    """The movie's soundtrack `track`, mono, 16 kHz, as floats; empty when it has none."""
    run = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-map", f"0:a:{track}", "-vn", "-ac", "1", "-ar", "16000",
                          "-f", "s16le", "-"], capture_output=True)
    raw = run.stdout if run.returncode == 0 else b""
    return np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--movies", default=os.path.join(ROOT, "movies"))
    parser.add_argument("--out", default=os.path.join(ROOT, "art/audio/dialog/movie_subtitles.tsv"))
    parser.add_argument("--tracks", default="0,1,2,3", help="the soundtracks to hear")
    parser.add_argument("--model-dir", required=True,
                        help="Parakeet's files (istupakov/parakeet-tdt-0.6b-v3-onnx), in a plain folder")
    args = parser.parse_args()

    done = set()
    if os.path.exists(args.out):
        with open(args.out, encoding="utf-8") as table:
            done = {tuple(line.split("\t", 2)[:2]) for line in table if line.strip()}
    tracks = [int(track) for track in args.tracks.split(",")]
    todo = [(name, track) for name in sorted(os.listdir(args.movies))
            if name.upper().endswith(".RMV") and name.upper() not in SILENT for track in tracks
            if (name, str(track)) not in done]
    print(f"{len(todo)} soundtracks to hear, {len(done)} already heard", file=sys.stderr)

    import onnxruntime
    onnxruntime.preload_dlls()
    import onnx_asr
    providers = ["CUDAExecutionProvider", "CPUExecutionProvider"]
    model = onnx_asr.load_model("nemo-parakeet-tdt-0.6b-v3", args.model_dir, providers=providers)
    vad = onnx_asr.load_vad("silero", providers=providers)
    hear = model.with_vad(vad)

    # A movie at a time, its stretches in order.
    with open(args.out, "a", encoding="utf-8") as table:
        for count, (name, track) in enumerate(todo, 1):
            wave = sound(os.path.join(args.movies, name), track)
            lines = 0
            if len(wave):
                for segment in hear.recognize(wave, sample_rate=16000):
                    text = segment.text.strip().replace("\t", " ")
                    if not text:
                        continue
                    table.write(f"{name}\t{track}\t{segment.start:.2f}\t{segment.end:.2f}\t{text}\n")
                    lines += 1
            table.flush()
            print(f"[{count}/{len(todo)}] {name} track {track}: {len(wave) / 16000:.0f}s, {lines} lines",
                  file=sys.stderr)


if __name__ == "__main__":
    main()
