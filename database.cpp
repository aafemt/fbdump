#define __USE_MINGW_ANSI_STDIO 1
#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <inttypes.h>

#include <memory>
#include <map>
#include <stdexcept>

#include "database.h"
#include "format.h"
#include "static_exception.h"

namespace Database
{
	static const char* decodePageType(uint8_t pageType)
	{
		static constexpr const char* const typeNames[] =
		{
			"unused",
			"Header Page",
			"PIP",
			"TIP",
			"Pointer Page",
			"Data Page",
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

	static const char* decodeHardware(uint8_t value)
	{
		static constexpr const char* const names[] =
		{
			"Intel/i386",
			"AMD/Intel/x64",
			"UltraSparc",
			"PowerPC",
			"PowerPC64",
			"MIPSEL",
			"MIPS",
			"ARM",
			"IA64",
			"s390",
			"s390x",
			"SH",
			"SHEB",
			"HPPA",
			"Alpha",
			"ARM64",
			"PowerPC64el",
			"M68k",
			"RiscV64",
			"MIPS64EL",
			"LOONGARCH"
		};

		if (value < std::size(names))
		{
			return names[value];
		}

		static char buffer[20];
		sprintf(buffer, "Unknown (%u)", value);
		return buffer;
	}

	static const char* decodeOS(uint8_t value)
	{
		static constexpr const char* const names[] =
		{
			"Windows",
			"Linux",
			"Darwin",
			"Solaris",
			"HPUX",
			"AIX",
			"MVS",
			"FreeBSD",
			"NetBSD"
		};

		if (value < std::size(names))
		{
			return names[value];
		}

		static char buffer[20];
		sprintf(buffer, "Unknown (%u)", value);
		return buffer;
	}

	static const char* decodeCompiler(uint8_t value)
	{
		static constexpr const char* const names[] =
		{
			"MSVC",
			"gcc",
			"xlC",
			"aCC",
			"SunStudio",
			"icc"
		};

		if (value < std::size(names))
		{
			return names[value];
		}

		static char buffer[20];
		sprintf(buffer, "Unknown (%u)", value);
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

	static const std::string decodeHeaderFlags(uint16_t flags)
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

		if (flags & Ods::hdr_active_shadow)
		{
			addName("active shadow");
		}
		if (flags & Ods::hdr_force_write)
		{
			addName("force writes");
		}
		if (flags & Ods::hdr_crypt_process)
		{
			addName("crypt in process");
		}
		if (flags & Ods::hdr_no_reserve)
		{
			addName("no reserve");
		}
		if (flags & Ods::hdr_SQL_dialect_3)
		{
			addName("dialect 3");
		}
		if (flags & Ods::hdr_read_only)
		{
			addName("read only");
		}
		if (flags & Ods::hdr_encrypted)
		{
			addName("encrypted");
		}
		const uint16_t backupMode = flags & 0xC00;
		switch (backupMode)
		{
		case 0:
			addName("not backuping");
			break;
		case 0x400:
			addName("backup lock");
			break;
		case 0x800:
			addName("backup merge");
			break;
		default:
			addName("wrong backup mode");
			break;
		}
		const uint16_t shutdownMode = flags & 0x1080;
		switch (shutdownMode)
		{
		case 0:
			addName("online");
			break;
		case 0x80:
			addName("multi-user maintenance");
			break;
		case 0x1000:
			addName("full shutdown");
			break;
		case 0x1080:
			addName("single-user maintenance");
			break;
		default:
			addName("wrong shutdown mode");
			break;
		}
		const uint16_t replicaMode = flags & 0x6000;
		switch (replicaMode)
		{
		case 0:
			addName("master");
			break;
		case 0x2000:
			addName("read-only replica");
			break;
		case 0x4000:
			addName("read-write replica");
			break;
		default:
			addName("wrong replica mode");
			break;
		}
		if (flags & 0x8300)
		{
			addName("unknown attribute");
		}
		return result;
	}

	// For common pieces among versions
	struct OdsDumper
	{
		Reader& f;
		unsigned pageSize;

		OdsDumper(Reader& file) : f(file)
		{
			// file is supposed to have first kilobyte already read and endianness set to a right value
			pageSize = f.gatherInt16(file.buffer + offsetof(Ods::header_page13, hdr_page_size));
		}
		// Just in case if some further ODS have different offset of page size
		unsigned getPageSize()
		{
			return pageSize;
		}
		void dumpHeader(const unsigned char* buffer)
		{
			printf("\tPage size\t\t%u\n", pageSize);
			uint16_t odsMajor = f.gatherInt16(buffer + offsetof(Ods::header_page13, hdr_ods_version));
			printf("\tODS version\t\t%u.%u\n", odsMajor & ~0x8000, f.gatherInt16(buffer + offsetof(Ods::header_page13, hdr_ods_minor)));
			printf("\tPAGES page\t\t%u\n", f.gatherInt32(buffer + offsetof(Ods::header_page13, hdr_PAGES)));
			printf("\tOldest transaction\t%llu\n", (unsigned long long)f.gatherInt16(buffer + offsetof(Ods::header_page13, hdr_tra_high[1])) << 32 |
																	   f.gatherInt32(buffer + offsetof(Ods::header_page13, hdr_oldest_transaction)));
			printf("\tOldest active\t\t%llu\n", (unsigned long long)f.gatherInt16(buffer + offsetof(Ods::header_page13, hdr_tra_high[2])) << 32 |
																  f.gatherInt32(buffer + offsetof(Ods::header_page13, hdr_oldest_active)));
			printf("\tOldest snapshot\t\t%llu\n", (unsigned long long)f.gatherInt16(buffer + offsetof(Ods::header_page13, hdr_tra_high[3])) << 32 |
																	f.gatherInt32(buffer + offsetof(Ods::header_page13, hdr_oldest_snapshot)));
			printf("\tNext transaction\t%llu\n", (unsigned long long)f.gatherInt16(buffer + offsetof(Ods::header_page13, hdr_tra_high[0])) << 32 |
																	 f.gatherInt32(buffer + offsetof(Ods::header_page13, hdr_next_transaction)));
			printf("\tFile sequence number\t%u\n", f.gatherInt16(buffer + offsetof(Ods::header_page13, hdr_sequence)));
			printf("\tNext attachment ID\t%llu\n", (unsigned long long)f.gatherInt16(buffer + offsetof(Ods::header_page13, hdr_att_high)) << 32 |
																  f.gatherInt32(buffer + offsetof(Ods::header_page13, hdr_attachment_id)));
			printf("\tImplementation\t\tHW=%s %s-endian OS=%s CC=%s\n", decodeHardware(buffer[offsetof(Ods::header_page13, hdr_cpu)]),
																	  (buffer[offsetof(Ods::header_page13, hdr_compatibility_flags)] & 1) ? "big" : "little",
																	  decodeOS(buffer[offsetof(Ods::header_page13, hdr_os)]),
																	  decodeCompiler(buffer[offsetof(Ods::header_page13, hdr_cc)]));
			printf("\tShadow count\t\t%u\n", f.gatherInt32(buffer + offsetof(Ods::header_page13, hdr_shadow_count)));
			printf("\tPage buffers\t\t%u\n", f.gatherInt32(buffer + offsetof(Ods::header_page13, hdr_page_buffers)));
			printf("\tNext header page\t%u\n", f.gatherInt32(buffer + offsetof(Ods::header_page13, hdr_next_page)));
			uint32_t clumpletEnd = f.gatherInt16(buffer + offsetof(Ods::header_page13, hdr_end));
			printf("\tClumplets end\t\t%u\n", clumpletEnd);
			if (clumpletEnd > pageSize)
			{
				fprintf(stderr, "Structure error: header clumplet end is beyond end of page\n");
				clumpletEnd = pageSize;
			}
			printf("\tDatabase created\t%s %s\n", formatISCDate(f.gatherInt32(buffer + offsetof(Ods::header_page13, hdr_creation_date[0]))).c_str(),
												  formatISCTime(f.gatherInt32(buffer + offsetof(Ods::header_page13, hdr_creation_date[1]))).c_str());
			uint16_t flags = f.gatherInt16(buffer + offsetof(Ods::header_page13, hdr_flags));
			printf("\tAttributes\t\t%x (%s)\n", flags, decodeHeaderFlags(flags).c_str());
			printf("\tCrypt page\t\t%u\n", f.gatherInt32(buffer + offsetof(Ods::header_page13, hdr_crypt_page)));
			printf("\tCrypt plugin\t\t\"%.32s\"\n", buffer + offsetof(Ods::header_page13, hdr_crypt_plugin));
			printf("\tVariable header data:\n");
			for (ptrdiff_t i = offsetof(Ods::header_page13, hdr_data); i < clumpletEnd; )
			{
				uint8_t clumpletType = buffer[i++];

				static constexpr uint8_t HDR_end				= 0;
				static constexpr uint8_t HDR_root_file_name		= 1;	// Original name of root file
				static constexpr uint8_t HDR_file				= 2;	// Secondary file
				static constexpr uint8_t HDR_last_page			= 3;	// Last logical page number of file
				static constexpr uint8_t HDR_sweep_interval		= 4;	// Transactions between sweeps
				static constexpr uint8_t HDR_crypt_checksum		= 5;	// Checksum of critical crypt parameters
				static constexpr uint8_t HDR_difference_file	= 6;	// Delta file that is used during backup lock
				static constexpr uint8_t HDR_backup_guid		= 7;	// GUID generated on each switch into backup mode
				static constexpr uint8_t HDR_crypt_key			= 8;	// Name of a key used to crypt database
				static constexpr uint8_t HDR_crypt_hash			= 9;	// Validator of key correctness
				static constexpr uint8_t HDR_db_guid			= 10;	// Database GUID
				static constexpr uint8_t HDR_repl_seq			= 11;	// Replication changelog sequence
				static constexpr uint8_t HDR_max				= 11;	// Maximum HDR_clump value

				static constexpr const char* const clumpletNames[] =
				{
					"End of clumplets",
					"Root file name",
					"Continuation file",
					"Last logical page",
					"Sweep interval",
					"Crypt checksum",
					"Backup differrence file",
					"Database backup GUID",
					"Encryption key name",
					"Key validator",
					"Database GUID",
					"Replication sequence"
				};
				static_assert(std::size(clumpletNames) == HDR_max + 1);

				if (clumpletType <= HDR_max)
				{
					printf("\t\t%s\t", clumpletNames[clumpletType]);
				}
				else
				{
					printf("Unknown clumplet %u", clumpletType);
				}
				if (clumpletType != HDR_end)
				{
					if (i >= clumpletEnd)
					{
						fprintf(stderr, "Incomplete header clumplet\n");
					}
					else
					{
						uint8_t length = buffer[i++];
						if (clumpletEnd - i < length)
						{
							fprintf(stderr, "Structure error: length of clumplet value %u is bigger that data in buffer %tu\n", length, clumpletEnd - i);
						}
						else
						{
							// Print the value
							switch (clumpletType)
							{
							case HDR_root_file_name:
							case HDR_file:
							case HDR_crypt_checksum:
							case HDR_difference_file:
							case HDR_crypt_key:
							case HDR_crypt_hash:
								{
									printf("\"%.*s\"", length, buffer + i);
									break;
								}
							case HDR_last_page:
							case HDR_sweep_interval:
							case HDR_repl_seq:
								{
									printf("%llu", f.gatherInt(buffer + i, length));
									break;
								}
							case HDR_backup_guid:
							case HDR_db_guid:
								{
									if (length != sizeof(UUID))
									{
										fprintf(stderr, "Size of backup UUID is %u, not 16\n", length);
									}
									else
									{
										UUID uuid;
										f.gatherUUID(buffer + i, uuid);
										printf("{%08X-%04hX-%04hX-%02X%02X-%02X%02X%02X%02X%02X%02X}",
												(unsigned)uuid.Data1,
												uuid.Data2,
												uuid.Data3,
												uuid.Data4[0], uuid.Data4[1], uuid.Data4[2],
												uuid.Data4[3], uuid.Data4[4], uuid.Data4[5],
												uuid.Data4[6], uuid.Data4[7]);
									}

								}
							}
							i += length;
						}
					}
				}
				printf("\n");
				if (clumpletType == HDR_end)
				{
					break;
				}
			}
		}
	};

	// Specific to Firebird 5 ODS 13
	struct Ods13: public OdsDumper
	{
		using OdsDumper::OdsDumper;
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
			dumper.reset(new Ods13(file));
			break;
		default:
			fprintf(stderr, "Warning: unsupported ODS %u found, output may be incorrect\n", ods);
			dumper.reset(new OdsDumper(file));
			break;
		}

		unsigned pageSize = dumper->getPageSize();
		// Another sanity check
		if ((pageSize & (pageSize - 1)) != 0)
		{
			throw std::runtime_error(std::string("Page size ") + std::to_string(pageSize) + " is invalid (must be a power of two)");
		}

		// Now we have enough info to go through whole database dumping every single page
		std::unique_ptr<unsigned char[]> buffer(new unsigned char[pageSize]);
		unsigned realPageNumber = 0;
		// This map is ordered to produce ordered stats without further sort.
		// Though perhaps unordered_map + sort could be faster.
		std::map<uint8_t, unsigned> stats;

		while (!file.eof())
		{
			file.readBuffer(buffer.get(), pageSize);

			// Dump header
			const unsigned char* p = buffer.get(); // Just for convenience
			const uint8_t pageType = p[offsetof(Ods::pag, pag_type)];
			const uint8_t flags = p[offsetof(Ods::pag, pag_flags)];
			printf("Page %u (%u), type %s, flags %x (%s), written %u times, SCN %u\n",
					realPageNumber, file.gatherInt32(p + offsetof(Ods::pag, pag_pageno)),
					decodePageType(pageType),
					flags, decodeFlags(pageType, flags).c_str(),
					file.gatherInt32(p + offsetof(Ods::pag, pag_generation)),
					file.gatherInt32(p + offsetof(Ods::pag, pag_scn)));

			stats[pageType]++;

			if ((flags & Ods::hdr_encrypted) == 0)
			{
				// Therre is no way to dump content of encrypted pages so far
				switch (pageType)
				{
				case Ods::pag_header:
					{
						dumper->dumpHeader(buffer.get());
						break;
					}
				default:
					break;
				}
			}

			++realPageNumber;
		}

		printf("\nSummary:\n");
		for (const auto& itr : stats)
		{
			printf("\t%s: %u (%0.2f%%, %0.2fmb)\n", decodePageType(itr.first), itr.second, itr.second * 100.0 / realPageNumber, itr.second / (1024.0 * 1024.0) * pageSize);
		}
	}
}
