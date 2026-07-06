#include <stdexcept>
#include <string.h>

#include "detector.h"
#include "journal.h"

FileType detectFileType(Reader& file)
{
	if (file.dataSize >= sizeof(Journal::SegmentHeader) && memcmp(file.buffer, Journal::Signature, Journal::SignatureSize) == 0)
	{
		const unsigned char ver1 = file.buffer[offsetof(Journal::SegmentHeader, hdr_version)];
		const unsigned char ver2 = file.buffer[offsetof(Journal::SegmentHeader, hdr_version) + 1];
		if (ver1 == 1 && ver2 == 0)
		{
			// Version 1, little endian
			file.endianness = __ORDER_LITTLE_ENDIAN__;
		}
		else if (ver1 == 1 && ver2 == 0)
		{
			// Version 1, big endian
			file.endianness = __ORDER_BIG_ENDIAN__;
		}
		else
		{
			throw std::runtime_error(std::string("Unrecognized version of journal file \"") + to_string(file.fileName).c_str() + "\" " + std::to_string(ver1) + std::to_string(ver2));
		}
		return FileType::Journal;
	}

	return FileType::Unknown;
}
