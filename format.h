#pragma once

#include <stdint.h>
#include <string>

// Various formatting routines

void decodeIBDate(int32_t nday, int &out_year, int &out_month, int &out_day);

std::string formatDecimal(const short scale, const long long value);
std::string formatISCDate(const int32_t value);
std::string formatISCTime(const uint32_t value);
