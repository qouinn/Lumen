// Grainfield — render export helpers: WAV writing, renders folder, OS drag-and-drop of files
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gf {

// Writes a 24-bit stereo WAV. Returns false on failure.
bool writeWav24(const std::string& utf8Path, const std::vector<float>& L, const std::vector<float>& R, unsigned sampleRate);

// Documents/Grainfield Renders (created if needed). UTF-8.
std::string rendersFolder();

// Documents/<name> (created if needed). UTF-8.
std::string documentsFolder(const char* name);

// Whole-file text read / write with UTF-8 paths (write goes through a temporary file, so a crash never leaves half a file).
bool readTextFile(const std::string& utf8Path, std::string& out);
bool writeTextFile(const std::string& utf8Path, const std::string& text);

// Starts an OS drag of the file (blocking until dropped or cancelled).
// Must be called while the left mouse button is held. Returns false if unsupported / failed.
bool dragFileOut(const std::string& utf8Path);

// Opens the system file browser with the file selected.
void revealFile(const std::string& utf8Path);

// Whether dragFileOut is implemented on this platform.
bool canDragFiles();

// Gives keyboard focus back to the host window that embeds the plugin (after typing in a text field).
void returnKeyboardFocus(uintptr_t pluginWindow);

} // namespace gf
