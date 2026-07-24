// Static message buffer definition. Much simplier that Firebird's one.

#pragma once

#include <memory>
#include <string>
#include <string.h>

#include "metadata.h"

struct Record
{
	Metadata* metadata = new Metadata;
	// A little hack relying on the fact that in C++ empty array has size 0.
	// Alignment requirement is the biggest from all fields alignment to make sure
	// that there will be no fillers after that in derived classes.
	alignas(8) char data[0];

	~Record()
	{
		metadata->release();
	}
};

namespace Fields
{
	// Not really charsets
	enum struct Charset : size_t { ASCII = 1, UTF8 = 4 };

	// Alignment requirements are copied from Firebird's jrd/align.h:type_alignment
	// It is not forced unless necessary because of bug in C::B code completion plugin
	template <size_t size, Charset bpc = Charset::ASCII>
	struct /* alignas(sizeof(short)) */ Varchar
	{
		unsigned short length = 0;
		char value[size * static_cast<size_t>(bpc)] = {};

		Varchar(Record* record, short* nullPtr)
		{
			record->metadata->addField(SQL_VARYING, 0, sizeof(value), 0, bpc == Charset::UTF8? 4 : 2,
									reinterpret_cast<char*>(this) - record->data,
									reinterpret_cast<char*>(nullPtr) - record->data,
									alignof(*this));
		}
		void operator=(const char* v)
		{
			length = std::min(sizeof(value), strlen(v));
			memcpy(value, v, length);
		}
		void operator=(const std::string& v)
		{
			length = std::min(sizeof(value), v.size());
			memcpy(value, v.c_str(), length);
		}
		void trim()
		{
			while (length > 0 && value[length - 1] == ' ')
				--length;
		}
	};
	static_assert(alignof(Varchar<1>) == sizeof(short), "Wrong Varchar alignment");
	static_assert(offsetof(Varchar<1>, value) == 2);

	template <size_t size>
	struct Binary
	{
		unsigned char value[size] = {};

		Binary(Record* record, short* nullPtr)
		{
			// Set subtype to 1 though the rest of Firebird code isn't prepared for that.
			record->metadata->addField(SQL_TEXT, 1, sizeof(value), 0, 1,
									reinterpret_cast<char*>(this) - record->data,
									reinterpret_cast<char*>(nullPtr) - record->data,
									alignof(*this));
		}
	};

	template <size_t size>
	struct Varbinary
	{
		unsigned short length = 0;
		unsigned char value[size] = {};

		Varbinary(Record* record, short* nullPtr)
		{
			// Set subtype to 1 though the rest of Firebird code isn't prepared for that.
			record->metadata->addField(SQL_VARYING, 1, sizeof(value), 0, 1,
									reinterpret_cast<char*>(this) - record->data,
									reinterpret_cast<char*>(nullPtr) - record->data,
									alignof(*this));
		}
	};
	static_assert(alignof(Varbinary<1>) == sizeof(short), "Wrong Varbinary alignment");

	struct Short
	{
		short value = 0;

		Short(Record* record, short* nullPtr)
		{
			record->metadata->addField(SQL_SHORT, 0, sizeof(value), 0, 0,
									reinterpret_cast<char*>(this) - record->data,
									reinterpret_cast<char*>(nullPtr) - record->data,
									alignof(*this));
		}
	};
	static_assert(alignof(Short) == sizeof(ISC_SHORT), "Wrong Integer alignment");

	struct Integer
	{
		int value = 0;

		Integer(Record* record, short* nullPtr)
		{
			record->metadata->addField(SQL_LONG, 0, sizeof(value), 0, 0,
									reinterpret_cast<char*>(this) - record->data,
									reinterpret_cast<char*>(nullPtr) - record->data,
									alignof(*this));
		}
	};
	static_assert(alignof(Integer) == sizeof(int32_t), "Wrong Integer alignment");

	struct alignas(sizeof(ISC_INT64)) Int64
	{
		long long value = 0;

		Int64(Record* record, short* nullPtr)
		{
			record->metadata->addField(SQL_INT64, 0, sizeof(value), 0, 0,
									reinterpret_cast<char*>(this) - record->data,
									reinterpret_cast<char*>(nullPtr) - record->data,
									alignof(*this));
		}
	};
	static_assert(alignof(Int64) == sizeof(ISC_INT64), "Wrong Int64 alignment");

	struct Timestamp
	{
		ISC_TIMESTAMP value = {};

		Timestamp(Record* record, short* nullPtr)
		{
			record->metadata->addField(SQL_TIMESTAMP, 0, sizeof(value), 0, 0,
									reinterpret_cast<char*>(this) - record->data,
									reinterpret_cast<char*>(nullPtr) - record->data,
									alignof(*this));
		}
	};
	static_assert(alignof(Timestamp) == sizeof(ISC_DATE), "Wrong Timestamp alignment");

	struct Blob
	{
		ISC_QUAD value = {};

		Blob(Record* record, short* nullPtr)
		{
			record->metadata->addField(SQL_BLOB, 0, sizeof(value), 0, 0,
									reinterpret_cast<char*>(this) - record->data,
									reinterpret_cast<char*>(nullPtr) - record->data,
									alignof(*this));
		}
	};
	static_assert(alignof(Blob) == sizeof(ISC_QUAD::gds_quad_low), "Wrong Blob alignment");

	struct Boolean
	{
		unsigned char value = 0;

		Boolean(Record* record, short* nullPtr)
		{
			record->metadata->addField(SQL_BOOLEAN, 0, 1, 0, 0,
									reinterpret_cast<char*>(this) - record->data,
									reinterpret_cast<char*>(nullPtr) - record->data,
									alignof(*this));
		}
		void operator=(const bool v)
		{
			value = v ? FB_TRUE : FB_FALSE;
		}
	};
	static_assert(alignof(Boolean) == 1, "Wrong Boolean alignment");

	struct Double
	{
		double value = 0.0;

		Double(Record* record, short* nullPtr)
		{
			record->metadata->addField(SQL_DOUBLE, 0, sizeof(double), 0, 0,
									reinterpret_cast<char*>(this) - record->data,
									reinterpret_cast<char*>(nullPtr) - record->data,
									alignof(*this));
		}
	};
	static_assert(alignof(Double) == 8, "Wrong Double alignment");
}

// Some macros to simplify declaration
#define DECLARE struct : public Record
#define BIGINT(name) Fields::Int64 name {this, &name##_null}; short name##_null = 1
#define FLD_SHORT(name) Fields::Short name {this, &name##_null}; short name##_null = 1
#define INTEGER(name) Fields::Integer name {this, &name##_null}; short name##_null = 1
#define BINARY(name, length) Fields::Binary<length> name {this, &name##_null}; short name##_null = 1
#define VARBINARY(name, length) Fields::Varbinary<length> name {this, &name##_null}; short name##_null = 1
#define VARCHAR(name, length) Fields::Varchar<length> name {this, &name##_null}; short name##_null = 1
#define VARCHAR_UTF8(name, length) Fields::Varchar<length, Fields::Charset::UTF8> name {this, &name##_null}; short name##_null = 1
#define TIMESTAMP(name) Fields::Timestamp name{this, &name##_null}; short name##_null = 1
#define BLOB(name) Fields::Blob name{this, &name##_null}; short name##_null = 1
// Damned Windows already defines BOOLEAN and BOOL
#define FLD_BOOLEAN(name) Fields::Boolean name{this, &name##_null}; short name##_null = 1
#define FLD_DOUBLE(name) Fields::Double name{this, &name##_null}; short name##_null = 1

/*
Example of usage:
DECLARE
{
	VARCHAR(name, 9);
	VARCHAR_UTF8(name2, 9);
	VARBINARY(salt, 9);
	INTEGER(i);
	BIGINT(b);
} record;
*/

// Test right alignments and offsets
struct _RecordTest: public Record
{
	BINARY(a, 1);
	// Big aligned field arter small
	BIGINT(b);
	// Small after big
	FLD_BOOLEAN(c);
	// And big again
	TIMESTAMP(d);
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
static_assert(offsetof(_RecordTest, a.value) - offsetof(_RecordTest, data) == 0, "Wrong offset");
static_assert(offsetof(_RecordTest, a_null) - offsetof(_RecordTest, data) == 2, "Wrong offset");
static_assert(offsetof(_RecordTest, b.value) - offsetof(_RecordTest, data) == 8, "Wrong offset");
static_assert(offsetof(_RecordTest, b_null) - offsetof(_RecordTest, data) == 16, "Wrong offset");
static_assert(offsetof(_RecordTest, c.value) - offsetof(_RecordTest, data) == 18, "Wrong offset");
static_assert(offsetof(_RecordTest, c_null) - offsetof(_RecordTest, data) == 20, "Wrong offset");
static_assert(offsetof(_RecordTest, d.value) - offsetof(_RecordTest, data) == 24, "Wrong offset");
static_assert(offsetof(_RecordTest, d_null) - offsetof(_RecordTest, data) == 32, "Wrong offset");
#pragma GCC diagnostic pop
