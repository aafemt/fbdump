#define __USE_MINGW_ANSI_STDIO 1
#include <string.h>
#include <stdio.h>

#include "dumper.h"
#include "format.h"
#include "reader.h"
#include "record.h"

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

#include <map>
#include <unordered_map>
#include <tuple>
#include <vector>

#include "fbinterface.h"
#include "status.h"

namespace
{
	typedef std::pair<std::string, std::string> TableKey;
	typedef std::vector<Ods::Descriptor13> Format;
	typedef std::map<size_t, Format> FormatList;

	struct TableHash
	{
		std::size_t operator()(const TableKey& value) const
		{
			return std::hash<TableKey::first_type>{}(value.first) + std::hash<TableKey::second_type>{}(value.second);
		}
	};
	std::unordered_map<TableKey, FormatList, TableHash> cache;
}

void dumpData(Reader& file, const std::string& schema, const std::string& name, const unsigned char* data, size_t dataLength, Attachment& att)
{
	if (att)
	{
		TableKey tableKey(schema, name);
		auto itr = cache.find(tableKey);
		if (itr == cache.end())
		{
			Status st(nullptr);
			if (!att.trans)
			{
				att.trans.reset(att->startTransaction(st("Start transaction"), 0, nullptr));
			}
			if (!att.formatQuery)
			{
				if (schema.empty())
				{
					att.formatQuery.reset(att->prepare(st("Prepare format query"), att.trans.get(),
											0, "select FMT.RDB$DESCRIPTOR from RDB$RELATIONS REL left join RDB$FORMATS FMT on REL.RDB$RELATION_ID=FMT.RDB$RELATION_ID"
												" where REL.RDB$RELATION_NAME=?"
												" order by FMT.RDB$FORMAT DESC",
											SQL_DIALECT_V6, Firebird::IStatement::PREPARE_PREFETCH_NONE));
				}
				else
				{
					att.formatQuery.reset(att->prepare(st("Prepare format query"), att.trans.get(),
											0, "select FMT.RDB$DESCRIPTOR from RDB$RELATIONS REL left join RDB$FORMATS FMT on REL.RDB$RELATION_ID=FMT.RDB$RELATION_ID"
												" where REL.RDB$SCHEMA_NAME = ? AND REL.RDB$RELATION_NAME=?"
												" order by FMT.RDB$FORMAT DESC",
											SQL_DIALECT_V6, Firebird::IStatement::PREPARE_PREFETCH_NONE));
				}
			}

			DECLARE
			{
				BLOB(format);
			} formatInfo;

			std::unique_ptr<Firebird::IResultSet, interface_deleter> cursor;
			if (schema.empty())
			{
					DECLARE
					{
						VARCHAR_UTF8(name, 63);
					} tableName;
					tableName.name_null = 0;
					tableName.name = name;

					cursor.reset(att.formatQuery->openCursor(st("Open format list"), att.trans.get(), tableName.metadata, tableName.data, formatInfo.metadata, 0));
			}
			else
			{
					DECLARE
					{
						VARCHAR_UTF8(schema, 63);
						VARCHAR_UTF8(name, 63);
					} tableName;
					tableName.schema_null = 0;
					tableName.schema = schema;
					tableName.name_null = 0;
					tableName.name = name;

					cursor.reset(att.formatQuery->openCursor(st("Open format list"), att.trans.get(), tableName.metadata, tableName.data, formatInfo.metadata, 0));
			}
			// This should create a new empty entry to prevent following attempts to load the same table
			itr = cache.emplace(tableKey, FormatList()).first;
			FormatList& list = itr->second;
			while (cursor->fetchNext(st("Fetch formats list"), formatInfo.data) == Firebird::IStatus::RESULT_OK)
			{
				std::unique_ptr<Firebird::IBlob, interface_deleter> blob(att->openBlob(st("Open format BLOB"), att.trans.get(), &formatInfo.format.value, 0, nullptr));

				short fieldsNumber;
				unsigned r = 0;
				if (blob->getSegment(st("Read relation format"), sizeof(fieldsNumber), &fieldsNumber, &r) == Firebird::IStatus::RESULT_NO_DATA || r != sizeof(fieldsNumber))
				{
					throw static_exception("Record format is badly truncated");
				}

				std::vector<Ods::Descriptor13> format(fieldsNumber);

				unsigned to_read = sizeof(Ods::Descriptor13) * fieldsNumber;
				unsigned char* p = reinterpret_cast<unsigned char*>(format.data());
				r = 0;

				while (to_read > 0 && blob->getSegment(&st, to_read, p, &r) !=  Firebird::IStatus::RESULT_NO_DATA)
				{
					to_read -= r;
					p += r;
					r = 0;
				}

				// Defaults can be ignored
				blob->close(st("Close format BLOB"));
				blob.release();

				// Recalculate required buffer length and offsets
				size_t dataSize = 0;
				for (short i = 0; i < fieldsNumber; ++i)
				{
					const Ods::Descriptor13& desc = format[i];

					if (desc.dsc_offset != 0)
					{
						// This is a real, existing field
						size_t newSize = desc.dsc_offset + desc.dsc_length;
						if (newSize > dataSize)
							dataSize = newSize;
					}
				}
				// If format with this size already exist - don't replace it.
				// This matches applier's logic
				list.try_emplace(dataSize, std::move(format));
			}
		}
		const auto fmtItr = itr->second.find(dataLength);
		if (fmtItr == itr->second.end())
		{
			fprintf(stderr, "No format of length %zu found\n", dataLength);
			// And fall out to binary dump
		}
		else
		{
			const Format& fmt = fmtItr->second;
			for (size_t i = 0; i < fmt.size(); ++i)
			{
				if (data[i / 8] & 1 << (i % 8))
				{
					printf("\t\t%zu> <NULL>\n", i);
				}
				else
				{
					const Ods::Descriptor13& desc = fmt[i];
					if (desc.dsc_offset + desc.dsc_length > dataLength)
					{
						fprintf(stderr, "Data for field %zu run out of buffer: %u of %zu\n", i, desc.dsc_offset + desc.dsc_length, dataLength);
						return;
					}
					switch (desc.dsc_dtype)
					{
					case dtype_varying:
						{
							const unsigned char* v = data + desc.dsc_offset;
							uint16_t len = file.gatherInt16(v);
							if (len > desc.dsc_length - 2)
							{
								fprintf(stderr, "Structure error: actual length of varying string %u is bigger than declared field length %u\n", len, desc.dsc_length - 2);
							}
							else
							{
								printf("\t\t%zu> '%.*s'\n", i, len, v + 2);
							}
							break;
						}
					case dtype_long:
						{
							printf("\t\t%zu> %s\n", i, formatDecimal(desc.dsc_scale, file.gatherInt32(data + desc.dsc_offset)).c_str());
							break;
						}
					default:
						{
							printf("\t\t%zu> unknown type %u\n", i, desc.dsc_dtype);
							HexDumper dumper;
							dumper.dumpIt(data + desc.dsc_offset, desc.dsc_length);
						}
					}
				}
			}
			return;
		}
	}
	CompactHexDumper dmp;
	printf("\t\t");
	dmp.dumpIt(data, dataLength);
	printf("\n");
}
