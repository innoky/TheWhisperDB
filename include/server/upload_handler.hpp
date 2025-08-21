#pragma once

#include <string>
#include <vector>

// Processes multipart/non-multipart parts vector produced by wServer and saves files that have a filename.
// Returns a short textual summary (e.g., "Saved files: N").
std::string handle_upload(const std::vector<std::string>& parts);
