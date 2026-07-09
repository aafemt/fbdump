#pragma once

#include <stdint.h>

#include "tstring.h"
#include "dumper.h"

struct Reader
{
	int f;
	tstring fileName;
	unsigned char buffer[4 * 1024];
	unsigned char* readPoint = buffer;
	size_t dataSize = 0;
	size_t bufferOffset = 0;
	unsigned endianness = __BYTE_ORDER__;

	Reader(const tstring& name, int flags);
	~Reader();

	void readBuffer();
	size_t dataLeft();

	void readBuffer(void* buffer, size_t size);
	uint8_t getInt8();
	uint16_t getInt16();
	uint32_t getInt32();
	uint64_t getInt64();
	void dumpTo(Dumper& dumper, size_t size);
	void skip(size_t size);
	bool eof();
	size_t offset();
};
