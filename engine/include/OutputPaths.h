#pragma once

// Output path rules + filesystem-based ID uniqueness.
//
// Extracted verbatim from App::GetSTLPath() / App::GetPNGPath() /
// App::IDAlreadyExists() in the original 3DQRGenerator/src/App.cpp.
// The folder names ("output/holder" and "output/tools") are intentionally
// asymmetric — they match the desktop application exactly.

#include "QRObject.h"
#include <string>
#include <filesystem>

namespace outputpaths {

// Canonical relative paths, e.g. "output/holder/H001.stl"
std::string STLRelativePath(const std::string& type, const std::string& id);
std::string PNGRelativePath(const std::string& type, const std::string& id);

// Folder for a type, relative: "output/holder" or "output/tools".
// Returns an empty string for an unknown type.
std::string FolderRelativePath(const std::string& type);

// Absolute versions rooted at `root` (the directory that contains "output/").
std::filesystem::path STLAbsolutePath(const std::filesystem::path& root,
                                      const std::string& type,
                                      const std::string& id);
std::filesystem::path PNGAbsolutePath(const std::filesystem::path& root,
                                      const std::string& type,
                                      const std::string& id);

bool IsValidType(const std::string& type);

// True if EITHER <id>.stl or <id>.png already exists in the type's folder.
// This is the persistent half of the desktop app's duplicate-ID check.
bool IDExistsOnDisk(const std::filesystem::path& root,
                    const std::string& type,
                    const std::string& id);

} // namespace outputpaths
