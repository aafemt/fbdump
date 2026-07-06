#define __USE_MINGW_ANSI_STDIO 1
#include <string.h>
#include <stdio.h>

#include "dumper.h"

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
	static constexpr char alphabet[] = "0123456789ABCDEF";

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
