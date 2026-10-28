// Grainfield / Flock / Lumen — audio file loading: WAV, AIFF, FLAC, MP3, Ogg Vorbis built in; M4A/AAC, ALAC, WMA, Opus
// and the audio track of videos (MP4, MOV, MKV, WebM, AVI, WMV, …) through Windows Media Foundation (ffmpeg elsewhere)
#pragma once

#include "GrainEngine.hpp"
#include <string>

namespace gf {

// Loads a file into stereo float buffers. Returns false on failure (out is left untouched).
// path is UTF-8. maxSeconds caps very long files.
bool loadAudioFile(const char* path, SampleData& out, double maxSeconds = 600.0);

// Returns just the file name part of a path.
std::string fileNameOf(const char* path);

} // namespace gf
