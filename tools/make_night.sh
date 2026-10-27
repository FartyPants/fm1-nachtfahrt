#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# "4 a.m. to 5 a.m.": the hour-long listening video (tests/film/20-night.amb), 1920x1080, the screen alone
# (tools/film.py --layout center), in build/film/channel/long_videos/night/. An hour of film is ~12 GB, so it
# never goes to disk: the simulator is deterministic (the same script and build, the same audio and frames,
# sample for sample), so it runs twice.
#   1. the film to /dev/null: the WAV and the cue sheet (each `bar` printed); mastered (the levels, fades,
#      a true-peak limiter); the captions and the chapters from the cues
#   2. the film into a named pipe, tools/film.py reading it as it comes and encoding the picture alone;
#      its WAV compared with the first (the proof the two runs are the same); then the mastered audio muxed in
#   tools/make_night.sh            (after host/build_host.sh; WORK=dir for the temporary files)
set -e
cd "$(dirname "$0")/.."
ROOT=$PWD
PY="${PYTHON:-.venv/bin/python3}"
N=20-night
SCRIPT="${SCRIPT:-tests/film/$N.amb}"     # (SCRIPT=file: a short test of the same pipeline, its wav and film named as here)
OUT="${OUT:-build/film/channel/long_videos/night}"
WORK="${WORK:-build/film/night-work}"
# the levels (dB), a piece each (Night Drive, Pressure, Fog, Haze), raw -23.3 -22.6 -21.5 -17.9 LUFS: each
# brought near -16 (Fog, the peakiest, to -17.8), ramped over 8 s around each change (under the dub hold); the
# limiter (0.80, 192 kHz) catches the rest, over 2 dB on about 1% of the time in Night Drive and Fog only
GAINS="${GAINS:-3.0 3.5 1.5 -0.5}"   # (each piece kept out of the limiter but for rare peaks: louder distorted)
mkdir -p "$OUT" "$WORK/p1" "$WORK/p2"

echo "== pass 1: the audio and the cue sheet"
ln -sf /dev/null "$WORK/p1/$N.film"
build/host/amb_host "$SCRIPT" "$WORK/p1" > "$WORK/cues.log"
if grep -q FAIL "$WORK/cues.log"; then
    grep FAIL "$WORK/cues.log"
    exit 1
fi
grep '^@' "$WORK/cues.log" > "$OUT/cues.txt"

# the switches (the bars labelled "4:15 PRESSURE" ...) and the end, from the cues: captions, chapters, the envelope
"$PY" - "$OUT/cues.txt" "$WORK/captions.tsv" "$OUT/chapters.txt" "$WORK/gain.txt" $GAINS <<'EOF'
import re, sys
cues, caps, chap, gain = sys.argv[1:5]
G = [float(g) for g in sys.argv[5:9]]
T = []
for line in open(cues, encoding="utf-8"):
    m = re.match(r"@ (\d+):(\d+\.\d)\s+bar (\d+)\s+(\d:\d\d) ([A-Z][A-Z ]+?)(:|$)", line.strip())
    if m:
        T.append((int(m[1]) * 60 + float(m[2]), m[4], m[5].strip().title()))
pieces = [t for t in T if t[1] != "5:00"]
assert [p[2] for p in pieces] == ["Night Drive", "Pressure", "Fog", "Haze"][:len(pieces)], pieces
with open(caps, "w", encoding="utf-8") as f:            # each piece's name a few seconds in (after the launch
    for i, (t, clock, name) in enumerate(pieces):        # screen; after the held echo is let go)
        f.write(f"{3.0 if i == 0 else t + 4.5:.1f}\t{clock} a.m. \u00b7 {name}\n")
with open(chap, "w", encoding="utf-8") as f:
    for i, (t, clock, name) in enumerate(pieces):
        s = 0 if i == 0 else int(t)
        f.write(f"{s // 60}:{s % 60:02d} {clock} a.m. \u00b7 {name}\n")
X, R = [p[0] for p in pieces[1:]], 4.0
def env(i):
    if i == len(X):
        return f"{G[i]}"
    a, b = X[i] - R, X[i] + R
    return f"if(lt(t,{a:.2f}),{G[i]},if(lt(t,{b:.2f}),{G[i]}+({G[i + 1] - G[i]:.2f})*(t-{a:.2f})/{2 * R},{env(i + 1)}))"
end = [t for t in T if t[1] == "5:00"][0][0]
open(gain, "w").write(f"pow(10,({env(0)})/20)\n{end:.1f}\n")
EOF
cat "$OUT/chapters.txt"
E=$(sed -n 1p "$WORK/gain.txt")
FADE=$(sed -n 2p "$WORK/gain.txt")
echo "== mastering (fade-in 10 s, fade-out 20 s from $FADE s, the levels $GAINS dB, the limiter)"
ffmpeg -nostats -loglevel error -y -i "$WORK/p1/$N.wav" -af "volume='$E':eval=frame,afade=t=in:st=0:d=10,afade=t=out:st=$FADE:d=20,aresample=192000,alimiter=limit=0.80:attack=5:release=300:level=disabled,aresample=48000" -c:a pcm_s24le "$OUT/DUB-1-night-1h-master.wav"
ffmpeg -nostats -i "$OUT/DUB-1-night-1h-master.wav" -af ebur128=peak=true -f null - 2>&1 | grep -A14 Summary | grep -E ' I:| LRA:| Peak:'

echo "== pass 2: the picture, through a pipe"
rm -f "$WORK/p2/$N.film"
mkfifo "$WORK/p2/$N.film"
"$PY" tools/film.py "$WORK/p2/$N.film" - "$WORK/video.mp4" --layout center --captions "$WORK/captions.tsv" \
    --crf 16 --preset slow &
FP=$!
build/host/amb_host "$SCRIPT" "$WORK/p2" > "$WORK/cues2.log"
wait $FP
rm -f "$WORK/p2/$N.film"
cmp "$WORK/p1/$N.wav" "$WORK/p2/$N.wav"
grep '^@' "$WORK/cues2.log" | cmp - "$OUT/cues.txt"
echo "  the two runs are the same, sample for sample"

echo "== mux"
ffmpeg -nostats -loglevel error -y -i "$WORK/video.mp4" -i "$OUT/DUB-1-night-1h-master.wav" -map 0:v -map 1:a \
    -c:v copy -c:a aac -b:a 320k -movflags +faststart "$OUT/DUB-1-night-1h.mp4"
ls -la "$OUT"
