#pragma once

#include <string>

bool convertToUtf8(const unsigned char* from, size_t length, std::string& to, unsigned charset);
