// Custom IMessageMetadata implementation

#pragma once

#include <memory>
#include <vector>

#include "status.h"

class Metadata: public Firebird::IMessageMetadataImpl<Metadata, StatusTypeCatchStuff>
{
	struct FieldMetadata
	{
		unsigned fType;
		unsigned fSubtype;
		unsigned fLength;
		int fScale;
		unsigned fCharSet;
		unsigned fOffset;
		unsigned fNullOffset;
		//const char* field;
		//const char* relation;
		//const char* owner;
		//const char* alias;
		//std::unique_ptr<char[]> nameStorage;

		FieldMetadata(unsigned type, unsigned subtype, unsigned length, int scale, unsigned charset, unsigned offset, unsigned nullOffset):
			fType(type), fSubtype(subtype), fLength(length), fScale(scale), fCharSet(charset), fOffset(offset), fNullOffset(nullOffset) /*,
			field(""), relation(""), owner(""), alias("")*/
		{}
		//FieldMetadata(unsigned type, unsigned subtype, unsigned length, int scale, unsigned charset, unsigned offset, unsigned nullOffset,
		//            unsigned fieldLength, const char* fieldValue, unsigned relationLength, const char* relationValue,
		//            unsigned ownerLength, const char* ownerValue, unsigned aliasLength, const char* aliasValue);
	};

	std::vector<FieldMetadata> fields;
public:
	int refCnt;
	unsigned alignment = 2;
	Metadata(size_t size = 8): refCnt(1)
	{
		fields.reserve(size);
	}

	// IReferenceCounted implementation
	void addRef() override
	{
		refCnt++;
	}
	int release() override
	{
		if (--refCnt <= 0)
		{
			delete this;
			return 0;
		}
		return 1;
	}

	bool checkIndex(StatusTypeCatchStuff* status, unsigned index, const char* func)
	{
		if (index < fields.size())
			return true;

		const ISC_STATUS err[] = { isc_arg_gds, isc_invalid_index_val, isc_arg_number, (ISC_STATUS)index, isc_arg_string, (ISC_STATUS)func, isc_arg_end };
		status->real_status->setErrors(err);
		return false;
	}

	// IMessageMetadata implementation
	unsigned getCount(StatusTypeCatchStuff*) override
	{
		return fields.size();
	}
	const char* getField([[maybe_unused]]StatusTypeCatchStuff* status, [[maybe_unused]]unsigned index) override
	{
		//if (!checkIndex(status, index, __func__)) return "";
		//return fields[index].field;
		return "";
	}
	const char* getSchema([[maybe_unused]]StatusTypeCatchStuff* status, [[maybe_unused]]unsigned index) override
	{
		return "";
	}
	const char* getRelation([[maybe_unused]]StatusTypeCatchStuff* status, [[maybe_unused]]unsigned index) override
	{
		//if (!checkIndex(status, index, __func__)) return "";
		//return fields[index].relation;
		return "";
	}
	const char* getOwner([[maybe_unused]]StatusTypeCatchStuff* status, [[maybe_unused]]unsigned index) override
	{
		//if (!checkIndex(status, index, __func__)) return "";
		//return fields[index].owner;
		return "";
	}
	const char* getAlias([[maybe_unused]]StatusTypeCatchStuff* status, [[maybe_unused]]unsigned index) override
	{
		//if (!checkIndex(status, index, __func__)) return "";
		//return fields[index].alias;
		return "";
	}
	unsigned getType(StatusTypeCatchStuff* status, unsigned index) override
	{
		if (!checkIndex(status, index, __func__)) return 0;
		return fields[index].fType & ~1u;
	}
	FB_BOOLEAN isNullable(StatusTypeCatchStuff* status, unsigned index) override
	{
		if (!checkIndex(status, index, __func__)) return FB_TRUE;
		return (fields[index].fType & 1u) ? FB_TRUE : FB_FALSE;
	}
	int getSubType(StatusTypeCatchStuff* status, unsigned index) override
	{
		if (!checkIndex(status, index, __func__)) return 0;
		return fields[index].fSubtype;
	}
	unsigned getLength(StatusTypeCatchStuff* status, unsigned index) override
	{
		if (!checkIndex(status, index, __func__)) return 0;
		return fields[index].fLength;
	}
	int getScale(StatusTypeCatchStuff* status, unsigned index) override
	{
		if (!checkIndex(status, index, __func__)) return 0;
		return fields[index].fScale;
	}
	unsigned getCharSet(StatusTypeCatchStuff* status, unsigned index) override
	{
		if (!checkIndex(status, index, __func__)) return 0;
		return fields[index].fCharSet;
	}
	unsigned getOffset(StatusTypeCatchStuff* status, unsigned index) override
	{
		if (!checkIndex(status, index, __func__)) return 0;
		return fields[index].fOffset;
	}
	unsigned getNullOffset(StatusTypeCatchStuff* status, unsigned index) override
	{
		if (!checkIndex(status, index, __func__)) return 0;
		return fields[index].fNullOffset;
	}
	Firebird::IMetadataBuilder* getBuilder([[maybe_unused]]StatusTypeCatchStuff* status) override
	{
		//notImplemented(status->real_status, "Metadata::getBuilder");
		return nullptr;
	}
	unsigned getMessageLength(StatusTypeCatchStuff*) override
	{
		return getMessageLength();
	}
	unsigned getAlignment(StatusTypeCatchStuff*) override
	{
		return alignment;
	}
	unsigned getAlignedLength(StatusTypeCatchStuff*) override
	{
		return getAlignedLength();
	}

	// Additional methods
	void addField(unsigned type, unsigned subtype, unsigned length, int scale, unsigned charset, unsigned offset, unsigned nullOffset, unsigned align)
	{
		fields.emplace_back(type, subtype, length, scale, charset, offset, nullOffset);
		if (align > alignment)
			alignment = align;
	}

/*
	void addField(unsigned type, unsigned subtype, unsigned length, int scale, unsigned charset, unsigned offset, unsigned nullOffset, unsigned align,
					  unsigned fieldLength, const char* fieldValue, unsigned relationLength, const char* relationValue,
					  unsigned ownerLength, const char* ownerValue, unsigned aliasLength, const char* aliasValue)
	{
		fields.emplace_back(type, subtype, length, scale, charset, offset, nullOffset,
							fieldLength, fieldValue, relationLength, relationValue, ownerLength, ownerValue, aliasLength, aliasValue);
		if (align > alignment)
			alignment = align;
	}
*/

	unsigned getMessageLength()
	{
		if (fields.size() == 0)
			return 0;

		return fields.back().fNullOffset + 2;
	}
	unsigned getAlignment()
	{
		return alignment;
	}
	unsigned getAlignedLength()
	{
		if (fields.size() == 0)
			return 0;

		return (fields.back().fNullOffset + 2 + alignment - 1) & ~(alignment - 1);
	}
};
