#define __USE_MINGW_ANSI_STDIO 1
#include <string.h>
#include <stdio.h>

#include "dumper.h"

namespace
{
	static constexpr char alphabet[] = "0123456789ABCDEF";
}

HexDumper::HexDumper()
{
}

HexDumper::~HexDumper()
{
	if (pos != 0)
	{
		printf("\t\t%s\n", string);
		pos = 0;
	}
}

void HexDumper::clearString()
{
	memset(string + 2, ' ', 16 * 3 - 1);
	memset(string + 2 + 16 * 3 + 2, ' ', 16);
	pos = 0;
}

void HexDumper::dumpIt(const unsigned char* buffer, size_t length)
{
	while (length > 0)
	{
		string[2 + pos * 3] = alphabet[*buffer >> 4];
		string[2 + pos * 3 + 1] = alphabet[*buffer & 0x0F];
		string[2 + 16 * 3 + 2 + pos] = *buffer >= 0x20 && *buffer < 0x7f ? *buffer : '.';
		--length;
		++buffer;
		if (++pos == 16)
		{
			printf("\t\t%s\n", string);
			clearString();
		}
	}
}

void HexDumper::flush()
{
	if (pos != 0)
	{
		printf("\t\t%s\n", string);
		clearString();
		pos = 0;
	}
}

void CompactHexDumper::dumpIt(const unsigned char* buffer, size_t length)
{
	char string[1024];
	char* p = string;
	while (length > 0)
	{
		*p++ = alphabet[*buffer >> 4];
		*p++ = alphabet[*buffer & 0x0F];
		if (p >= string + sizeof(string))
		{
			printf("%.1024s", string);
			p = string;
		}
		++buffer;
		--length;
	}
	if (p > string)
	{
		printf("%.*s", static_cast<int>(p - string), string);
	}
}
