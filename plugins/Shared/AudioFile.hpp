// Grainfield — audio file loading (WAV, AIFF, FLAC, MP3)
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
