#define __USE_MINGW_ANSI_STDIO 1
#include <string.h>
#include <stdio.h>
#include <cstdlib>

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

	void dumpData(const Reader& file, const Format& format, const unsigned char* data, size_t index)
	{
		static Firebird::IUtil* util = master->getUtilInterface();

		if (data[index / 8] & 1 << (index % 8))
		{
			printf("<NULL>\n");
		}
		else
		{
			const Ods::Descriptor13& desc = format[index];
/*					This case is impossible because length of format is formed for longest field and is chosen by length
			if (desc.dsc_offset + desc.dsc_length > dataLength)
			{
				fprintf(stderr, "Data for field %zu run out of buffer: %u of %zu\n", i, desc.dsc_offset + desc.dsc_length, dataLength);
				return;
			}
*/
			const unsigned char* v = data + desc.dsc_offset;
			switch (desc.dsc_dtype)
			{
			case dtype_text:
				{
					if (desc.dsc_sub_type == fb_text_subtype_binary)
					{
						HexDumper dumper;
						dumper.dumpIt(v, desc.dsc_length);
					}
					else
					{
						printf("'%.*s'\n", desc.dsc_length, v);
					}
					break;
				}
			case dtype_varying:
				{
					uint16_t len = file.gatherInt16(v);
					if (len > desc.dsc_length - offsetof(paramvary, vary_string))
					{
						fprintf(stderr, "Structure error: actual length of varying string %u is bigger than declared field length %u\n", len, desc.dsc_length - 2);
						len = desc.dsc_length - offsetof(paramvary, vary_string);
					}
					if (desc.dsc_sub_type == fb_text_subtype_binary)
					{
						HexDumper dumper;
						dumper.dumpIt(v + offsetof(paramvary, vary_string), len);
					}
					else
					{
						printf("'%.*s'\n", len, v + offsetof(paramvary, vary_string));
					}
					break;
				}
			case dtype_short:
				{
					printf("%s\n", formatDecimal(desc.dsc_scale, file.gatherInt16(v)).c_str());
					break;
				}
			case dtype_long:
				{
					printf("%s\n", formatDecimal(desc.dsc_scale, file.gatherInt32(v)).c_str());
					break;
				}
			case dtype_sql_date:
				{
					printf("%s\n", formatISCDate(file.gatherInt32(v)).c_str());
					break;
				}
			case dtype_sql_time:
				{
					printf("%s\n", formatISCTime(file.gatherInt32(v)).c_str());
					break;
				}
			case dtype_timestamp:
				{
					printf("%s %s\n", formatISCDate(file.gatherInt32(v)).c_str(), formatISCTime(file.gatherInt32(v + offsetof(ISC_TIMESTAMP, timestamp_time))).c_str());
					break;
				}
			case dtype_blob:
			case dtype_array:
				{
					printf("%x:%x\n", file.gatherInt32(v), file.gatherInt32(v + offsetof(ISC_QUAD, isc_quad_low)));
					break;
				}
			case dtype_int64:
				{
					printf("%s\n", formatDecimal(desc.dsc_scale, file.gatherInt64(v)).c_str());
					break;
				}
			case dtype_boolean:
				{
					printf("%s\n", *v ? "TRUE" : "FALSE");
					break;
				}
			case dtype_dec64:
				{
					// Here I hope that byte layout of DecFloat matches layout of integers of the same size
					// Perhaps it is not true.
					Status st("Get DecFloat16 interface");
					Firebird::IDecFloat16* d = util->getDecFloat16(&st);
					char buf[50];
					static_assert(sizeof(uint64_t) == sizeof(FB_DEC16));
					uint64_t tmp = file.gatherInt64(v);
					d->toString(st("DecFloat to string"), reinterpret_cast<const FB_DEC16*>(&tmp), sizeof(buf), buf);
					printf("%s\n", buf);
					break;
				}
			case dtype_dec128:
				{
					Status st("Get DecFloat34 interface");
					Firebird::IDecFloat34* d = util->getDecFloat34(&st);
					char buf[100];
					static_assert(sizeof(FB_I128) == sizeof(FB_DEC34));
					FB_I128 tmp;
					file.gatherInt128(v, tmp);
					d->toString(st("DecFloat to string"), reinterpret_cast<const FB_DEC34*>(&tmp), sizeof(buf), buf);
					printf("%s\n", buf);
					break;
				}
			case dtype_int128:
				{
					Status st("Get Int128 interface");
					Firebird::IInt128* d = util->getInt128(&st);
					char buf[100];
					FB_I128 tmp;
					file.gatherInt128(v, tmp);
					d->toString(st("Int128 to string"), &tmp, desc.dsc_scale, sizeof(buf), buf);
					printf("%s\n", buf);
					break;
				}
			case dtype_sql_time_tz:
				{
					printf("%s ", formatISCTime(file.gatherInt32(v)).c_str());
					short timeZone = file.gatherInt16(v + offsetof(ISC_TIME_TZ, time_zone));
					if (timeZone < 0)
					{
						printf("TZ id %u\n", timeZone);
					}
					else
					{
						short offset = timeZone - 1439;
						printf("%d:%u\n", offset / 60, std::abs(offset) % 60);
					}
					break;
				}
			case dtype_timestamp_tz:
				{
					printf("%s %s ", formatISCDate(file.gatherInt32(v)).c_str(), formatISCTime(file.gatherInt32(v + offsetof(ISC_TIMESTAMP, timestamp_time))).c_str());
					short timeZone = file.gatherInt16(v + offsetof(ISC_TIMESTAMP_TZ, time_zone));
					if (timeZone < 0)
					{
						printf("TZ id %u\n", timeZone);
					}
					else
					{
						short offset = timeZone - 1439;
						printf("%d:%u\n", offset / 60, std::abs(offset) % 60);
					}
					break;
				}
			// Ex-types are not used in storage, at least for now
			default:
				{
					printf("*** unknown type %u ***\n", desc.dsc_dtype);
					HexDumper dumper;
					dumper.dumpIt(v, desc.dsc_length);
				}
			}
		}
	}
}

void dumpData(Attachment& att, const Reader& file, const std::string& schema, const std::string& name, const unsigned char* oldData, size_t oldDataLength, const unsigned char* newData, size_t newDataLength)
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

		const Format* oldFormat = nullptr;
		size_t oldFormatSize = 0;
		const Format* newFormat = nullptr;
		size_t newFormatSize = 0;
		if (oldData != nullptr && oldDataLength != 0)
		{
			const auto fmtItr = itr->second.find(oldDataLength);
			if (fmtItr == itr->second.end())
			{
				fprintf(stderr, "No format of length %zu found\n", oldDataLength);
				// And fall out to binary dump
			}
			else
			{
				oldFormat = &fmtItr->second;
				oldFormatSize = oldFormat->size();
			}
		}
		if (newData != nullptr && newDataLength != 0)
		{
			const auto fmtItr = itr->second.find(newDataLength);
			if (fmtItr == itr->second.end())
			{
				fprintf(stderr, "No format of length %zu found\n", newDataLength);
				// And fall out to binary dump
			}
			else
			{
				newFormat = &fmtItr->second;
				newFormatSize = newFormat->size();
			}
		}
		size_t maxSize = std::max(oldFormatSize, newFormatSize);
		for (size_t i = 0; i < maxSize; ++i)
		{
			if (oldData != nullptr && oldDataLength != 0 && i < oldFormatSize)
			{
				printf("\t\t%zu< ", i);
				dumpData(file, *oldFormat, oldData, i);
			}
			if (newData != nullptr && newDataLength != 0 && i < newFormatSize)
			{
				printf("\t\t%zu> ", i);
				dumpData(file, *newFormat, newData, i);
			}
		}
		return;
	}
	CompactHexDumper dmp;
	if (oldData != nullptr && oldDataLength != 0)
	{
		printf("\t\t< ");
		dmp.dumpIt(oldData, oldDataLength);
		printf("\n");
	}
	if (newData != nullptr && newDataLength != 0)
	{
		printf("\t\t> ");
		dmp.dumpIt(newData, newDataLength);
		printf("\n");
	}
}
