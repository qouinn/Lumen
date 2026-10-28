#!/usr/bin/env bash
# Makes the files test_formats checks: 4 s, 330 Hz left / 495 Hz right, in every common audio and video format.
#   bash plugins/Lumen/tests/make_format_files.sh build/fmt && build/test_formats build/fmt
set -euo pipefail
D="${1:-fmt}"; mkdir -p "$D"; cd "$D"
F="ffmpeg -v error -y"
$F -f lavfi -i "sine=f=330:d=4" -f lavfi -i "sine=f=495:d=4" -filter_complex "[0][1]join=inputs=2:channel_layout=stereo,volume=0.5" -ar 44100 ref.wav
$F -i ref.wav ref.aiff
$F -i ref.wav ref.flac
$F -i ref.wav -b:a 192k ref.mp3
$F -i ref.wav -c:a libvorbis ref.ogg
$F -i ref.wav -c:a libopus -b:a 128k ref.opus
$F -i ref.wav -c:a aac -b:a 192k ref.m4a
$F -i ref.wav -c:a alac alac.m4a
$F -i ref.wav -c:a aac -f adts ref.aac
$F -i ref.wav -c:a wmav2 ref.wma
V="-f lavfi -i testsrc=size=320x240:rate=25:duration=4"
$F $V -i ref.wav -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest video.mp4
$F $V -i ref.wav -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest video.mov
$F $V -i ref.wav -c:v libx264 -pix_fmt yuv420p -c:a libvorbis -shortest video.mkv
$F $V -i ref.wav -c:v libvpx -c:a libopus -shortest video.webm
$F $V -i ref.wav -c:v mpeg4 -c:a libmp3lame -shortest video.avi
$F $V -i ref.wav -c:v wmv2 -c:a wmav2 -shortest video.wmv
# 5.1 with 330 Hz in the centre only: the stereo downmix must put it in both sides
$F -i ref.wav -filter_complex "[0]pan=5.1|FL=0*c0|FR=0*c0|FC=c0|LFE=0*c0|BL=0*c0|BR=0*c0" -c:a aac surround.m4a
$F $V -c:v libx264 -pix_fmt yuv420p noaudio.mp4
echo "this is not audio" > notaudio.txt
ls
