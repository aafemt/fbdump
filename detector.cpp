#include <stdexcept>
#include <string.h>

#include "detector.h"
#include "journal.h"
#include "database.h"

FileType detectFileType(Reader& file)
{
	// Is it a replication journal?
	if (file.dataSize >= sizeof(Journal::SegmentHeader) && memcmp(file.buffer, Journal::Signature, Journal::SignatureSize) == 0)
	{
		const unsigned char ver1 = file.buffer[offsetof(Journal::SegmentHeader, hdr_version)];
		const unsigned char ver2 = file.buffer[offsetof(Journal::SegmentHeader, hdr_version) + 1];
		if (ver1 == 1 && ver2 == 0)
		{
			// Version 1, little endian
			file.endianness = __ORDER_LITTLE_ENDIAN__;
		}
		else if (ver1 == 0 && ver2 == 1)
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
	// Is it a database?
	if (file.dataSize >= sizeof(Ods::header_page13)
		&& file.buffer[offsetof(Ods::pag, pag_type)] == Ods::pag_header
		&& file.gatherInt32(file.buffer + offsetof(Ods::pag, pag_pageno)) == 0
		&& file.gatherInt32(file.buffer + offsetof(Ods::pag, pag_generation)) != 0
		)
	{
		file.endianness = file.buffer[offsetof(Ods::header_page13, hdr_compatibility_flags)] & 1 ? __ORDER_BIG_ENDIAN__ : __ORDER_LITTLE_ENDIAN__;
		uint16_t ods = file.gatherInt16(file.buffer + offsetof(Ods::header_page13, hdr_ods_version));
		if (ods & 0x8000)
		{
			// It looks like a Firebird database
			ods &= ~0x8000;
			if (ods >= 11 && ods <= 14)
			{
				return FileType::Database;
			}
		}
		// else it may look like a database but not quite Firebird one.
	}

	return FileType::Unknown;
}
