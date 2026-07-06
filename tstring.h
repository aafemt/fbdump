#pragma once

#include <string>
#include <tchar.h>

typedef std::basic_string<_TCHAR> tstring;

// This function returns string encoded in UTF-8.
std::string to_string(const std::wstring& origin);
const std::string& to_string(const std::string& origin);
