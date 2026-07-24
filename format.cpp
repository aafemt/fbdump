#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include "format.h"
#include "static_exception.h"

std::string formatDecimal(const short scale, const long long value)
{
	if (scale > 39 || scale < -38)
	{
		fprintf(stderr, "Decimal scale %d is out of range (-38 - 39)\n", scale);
		return "*******";
	}

	char buffer[100];
	char* p = buffer + sizeof(buffer);
	char* end = p;
	short s = scale;
	long long v = value < 0 ? -value : value;

	// Add missing zeros for (unusual) case of positive scale
	while (s > 0)
	{
		*(--p) = '0';
		s--;
	}

	while (v != 0 || s <= 0)
	{
		*(--p) = v % 10 + '0';
		if (++s == 0)
		{
			*(--p) = '.';
		}
		v /= 10;
	}

	if (value < 0)
		*(--p) = '-';

	return std::string(p, end);
}

// this routine is copy-paste from FB sources
void decodeIBDate(int32_t nday, int &out_year, int &out_month, int &out_day)
{
	nday += 678882;
	const int century = (4 * nday - 1) / 146097;
	nday = 4 * nday - 1 - 146097 * century;
	int day = nday / 4;

	nday = (4 * day + 3) / 1461;
	day = 4 * day + 3 - 1461 * nday;
	day = (day + 4) / 4;

	int month = (5 * day - 3) / 153;
	day = 5 * day - 3 - 153 * month;
	day = (day + 5) / 5;

	int year = 100 * century + nday;

	if (month < 10)
		month += 3;
	else {
		month -= 9;
		year += 1;
	}

	out_day = day;
	out_month = month;
	out_year = year;
}

std::string formatISCDate(const int32_t value)
{
	int year, month, day;
	decodeIBDate(value, year, month, day);

	char buffer[40];
	sprintf(buffer, "%04d-%02d-%02d", year, month, day);
	return buffer;
}

std::string formatISCTime(const uint32_t value)
{
	unsigned int t = value / 10000;
	int seconds = t % 60;
	t /= 60;
	int minute = t % 60;
	int hour = t / 60;

	char buffer[40];
	sprintf(buffer, "%02d:%02d:%02d.%04d", hour, minute, seconds, value % 10000);
	return buffer;
}
