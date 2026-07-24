#include <ios>
#include <stdexcept>
#include <errno.h>
#include <unistd.h>

#include "reader.h"

Reader::Reader(const tstring& name, int flags) : fileName(name)
{
	f = _topen(name.c_str(), flags);
	if (f == -1)
	{
		throw std::ios_base::failure(std::string("Unable to open file \"") + to_string(name) + "\": " + strerror(errno));
	}
}
Reader::~Reader()
{
	close(f);
}

void Reader::readBuffer()
{
	bufferOffset += dataSize;
	ssize_t r = read(f, buffer, sizeof(buffer));
	if (r == -1)
	{
		throw std::ios_base::failure(std::string("Read from \"") + to_string(fileName) + "\" failed: " + strerror(errno));
	}
	readPoint = buffer;
	dataSize = r;
}

size_t Reader::dataLeft()
{
	size_t result = dataSize - (readPoint - buffer);
	if (result == 0)
	{
		readBuffer();
		if (dataSize == 0)
		{
			throw std::ios_base::failure("Unexpected end of file");
		}
		result = dataSize;
	}
	return result;
}

void Reader::readBuffer(void* target, size_t size)
{
	unsigned char* tgt = reinterpret_cast<unsigned char*>(target);
	while (size > 0)
	{
		size_t chunk = std::min(size, dataLeft());
		memcpy(tgt, readPoint, chunk);
		readPoint += chunk;
		tgt += chunk;
		size -= chunk;
	}
}

void Reader::skip(size_t size)
{
	// Just like above but without target
	while (size > 0)
	{
		size_t chunk = std::min(size, dataLeft());
		readPoint += chunk;
		size -= chunk;
	}
}

void Reader::dumpTo(Dumper& dumper, size_t size)
{
	while (size > 0)
	{
		size_t chunk = std::min(size, dataLeft());
		dumper.dumpIt(readPoint, chunk);
		readPoint += chunk;
		size -= chunk;
	}
}

uint8_t Reader::getInt8()
{
	(void)dataLeft(); // Anything that is bigger than zero is fine and zero won't pass here
	return *readPoint++;
}

uint16_t Reader::getInt16()
{
	uint16_t result;
	readBuffer(&result, sizeof(result));
	if (endianness != __BYTE_ORDER__)
	{
		result = __builtin_bswap16(result);
	}
	return result;
}

uint32_t Reader::getInt32()
{
	uint32_t result;
	readBuffer(&result, sizeof(result));
	if (endianness != __BYTE_ORDER__)
	{
		result = __builtin_bswap32(result);
	}
	return result;
}

uint64_t Reader::getInt64()
{
	uint64_t result;
	readBuffer(&result, sizeof(result));
	if (endianness != __BYTE_ORDER__)
	{
		result = __builtin_bswap64(result);
	}
	return result;
}

uint16_t Reader::gatherInt16(const unsigned char* from)
{
	uint16_t result = *reinterpret_cast<const uint16_t*>(from);
	if (endianness != __BYTE_ORDER__)
	{
		result = __builtin_bswap16(result);
	}
	return result;
}

uint32_t Reader::gatherInt32(const unsigned char* from)
{
	uint32_t result = *reinterpret_cast<const uint32_t*>(from);
	if (endianness != __BYTE_ORDER__)
	{
		result = __builtin_bswap32(result);
	}
	return result;
}

uint64_t Reader::gatherInt64(const unsigned char* from)
{
	uint64_t result = *reinterpret_cast<const uint64_t*>(from);
	if (endianness != __BYTE_ORDER__)
	{
		result = __builtin_bswap64(result);
	}
	return result;
}

bool Reader::eof()
{
	if (readPoint < buffer + dataSize)
	{
		// We surely have some data to read
		return false;
	}
	readBuffer();
	// read() returned 0, eof
	return dataSize == 0;
}

size_t Reader::offset()
{
	return bufferOffset + (readPoint - buffer);
}
