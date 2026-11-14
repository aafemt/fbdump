#define __USE_MINGW_ANSI_STDIO 1
#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

#include <memory>
#include <stdexcept>

#include "database.h"
#include "static_exception.h"

namespace Database
{
	static const char* decodePageType(uint8_t pageType)
	{
		static constexpr const char* const typeNames[] =
		{
			"unused",
			"header",
			"PIP",
			"TIP",
			"Pointer Page",
			"Data",
			"Index Root",
			"Index Page",
			"BLOB Page",
			"Generator Page",
			"SCN Page"
		};
		static_assert(std::size(typeNames) == Ods::pag_max + 1);

		if (pageType <= Ods::pag_max)
		{
			return typeNames[pageType];
		}
		static char buffer[20];
		sprintf(buffer, "Unknown (%u)", pageType);
		return buffer;
	}

	static const std::string decodeFlags(uint8_t pageType, uint8_t flags)
	{
		std::string result;
		bool first = true;
		auto addName = [&](const char* name)
		{
			if (!first)
			{
				result += ',';
			}
			else
			{
				first = false;
			}
			result += name;
		};

		for (int i = 0; i < 8; ++i)
		{
			if (flags & (1 << i))
			{
				switch (pageType)
				{
				case Ods::pag_blob:
					{
						static constexpr const char* const flagNames[8] =
						{
							"pointers",
							"unknown",
							"unknown",
							"unknown",
							"unknown",
							"unknown",
							"unknown",
							"encrypted"
						};
						addName(flagNames[i]);
						break;
					}
				case Ods::pag_index:
					{
						static constexpr const char* const flagNames[8] =
						{
							"don't gc",
							"descending",
							"unknown",
							"unknown",
							"jump info",
							"released",
							"unknown",
							"encrypted"
						};
						addName(flagNames[i]);
						break;
					}
				case Ods::pag_data:
					{
						static constexpr const char* const flagNames[8] =
						{
							"orphan",
							"full",
							"large",
							"swept",
							"secondary",
							"unknown",
							"unknown",
							"encrypted"
						};
						addName(flagNames[i]);
						break;
					}
				case Ods::pag_pointer:
					{
						static constexpr const char* const flagNames[8] =
						{
							"eof",
							"unknown",
							"unknown",
							"unknown",
							"unknown",
							"unknown",
							"unknown",
							"encrypted"
						};
						addName(flagNames[i]);
						break;
					}
				default:
					{
						static constexpr const char* const flagNames[8] =
						{
							"unknown",
							"unknown",
							"unknown",
							"unknown",
							"unknown",
							"unknown",
							"unknown",
							"encrypted"
						};
						addName(flagNames[i]);
						break;
					}
				}
			}
		}

		return result;
	}

	// For common pieces among versions
	struct OdsDumper
	{
		// Just in case if some further ODS have different offset of page size
		unsigned getPageSize(Reader& file)
		{
			// file is supposed to have first kilobyte already read and endianness set to a right value
			return file.gatherInt16(file.buffer + offsetof(Ods::header_page13, hdr_page_size));
		}
	};

	// Specific to Firebird 5 ODS 13
	struct Ods13: public OdsDumper
	{

	};

	void dumpIt(Reader& file)
	{
		struct stat64 st{};
		if (fstat64(file.f, &st) == -1)
		{
			fprintf(stderr, "fstat error %u\n", errno);
		}
		printf("Firebird database %s, length %llu\n", to_string(file.fileName).c_str(), (unsigned long long)st.st_size);

		// Sanity checks
		if (file.buffer[offsetof(Ods::pag, pag_type)] != Ods::pag_header)
		{
			throw static_exception("First page is not a header page");
		}
		// These calls to gatherInt are safe even if endianness is not detected yet because zero value has a symmetric layout
		if (file.gatherInt32(file.buffer + offsetof(Ods::pag, pag_pageno)) != 0)
		{
			throw static_exception("Wrong page number of a header page");
		}
		if (file.gatherInt32(file.buffer + offsetof(Ods::pag, pag_generation)) == 0)
		{
			throw static_exception("First page was not ever written");
		}

		// Get endianness from compatibility flag. It must be more reliable than to derive it from major ODS number value
		file.endianness = (file.buffer[offsetof(Ods::header_page13, hdr_compatibility_flags)] & 1) ? __ORDER_BIG_ENDIAN__ : __ORDER_LITTLE_ENDIAN__;

		uint16_t ods = file.gatherInt16(file.buffer + offsetof(Ods::header_page13, hdr_ods_version));
		if ((ods & 0xFF00) != 0x8000) // Check whole high byte, just in case
		{
			throw static_exception("Not a Firebird database");
		}
		ods &= ~0x8000;

		std::unique_ptr<OdsDumper> dumper;
		switch (ods)
		{
		case 13:
			dumper.reset(new Ods13);
			break;
		default:
			throw std::runtime_error(std::string("Unsupported ODS ") + std::to_string(ods) + " found");
		}

		unsigned pageSize = dumper->getPageSize(file);
		// Another sanity check
		if ((pageSize & (pageSize - 1)) != 0)
		{
			throw std::runtime_error(std::string("Page size ") + std::to_string(pageSize) + " is invalid (must be a power of two)");
		}

		// Now we have enough info to go through whole database dumping every single page
		std::unique_ptr<unsigned char[]> buffer(new unsigned char[pageSize]);
		unsigned realPageNumber = 0;
		while (!file.eof())
		{
			file.readBuffer(buffer.get(), pageSize);

			// Dump header
			const unsigned char* p = buffer.get(); // Just for convenience
			printf("Page %u (%u), type %s, flags %x (%s), written %u times, SCN %u\n",
					realPageNumber, file.gatherInt32(p + offsetof(Ods::pag, pag_pageno)),
					decodePageType(p[offsetof(Ods::pag, pag_type)]),
					p[offsetof(Ods::pag, pag_flags)], decodeFlags(p[offsetof(Ods::pag, pag_type)], p[offsetof(Ods::pag, pag_flags)]).c_str(),
					file.gatherInt32(p + offsetof(Ods::pag, pag_generation)),
					file.gatherInt32(p + offsetof(Ods::pag, pag_scn)));

			++realPageNumber;
		}
	}
}
