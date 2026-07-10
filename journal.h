#pragma once

#include <stdint.h>
#include <rpc.h>

#include "reader.h"

namespace Journal
{
	// From Firebird src/jrd/replication
	constexpr size_t SignatureSize = 12;
	constexpr const char* Signature = "FBCHANGELOG\0";

	enum SegmentState : unsigned short
	{
		SEGMENT_STATE_FREE = 0,
		SEGMENT_STATE_USED = 1,
		SEGMENT_STATE_FULL = 2,
		SEGMENT_STATE_ARCH = 3
	};

	enum Operation: unsigned char
	{
		opStartTransaction = 1,
		opPrepareTransaction = 2,
		opCommitTransaction = 3,
		opRollbackTransaction = 4,
		opCleanupTransaction = 5,

		opStartSavepoint = 6,
		opReleaseSavepoint = 7,
		opRollbackSavepoint = 8,

		opInsertRecord = 9,
		opUpdateRecord =  10,
		opDeleteRecord = 11,
		opStoreBlob = 12,
		opExecuteSql = 13,
		opSetSequence = 14,
		opExecuteSqlIntl = 15,

		opDefineAtom = 16
	};


	inline constexpr unsigned short BLOCK_BEGIN_TRANS	= 0x0001;
	inline constexpr unsigned short BLOCK_END_TRANS		= 0x0002;

	struct SegmentHeader
	{
		char hdr_signature[SignatureSize];
		unsigned short hdr_version;
		SegmentState hdr_state;
		UUID hdr_guid;
		uint64_t hdr_sequence;
		uint64_t hdr_length;
	};
	static_assert(std::has_unique_object_representations_v<SegmentHeader>);

	struct Block
	{
		uint64_t traNumber;
		unsigned short protocol;
		unsigned short flags;
		uint32_t length;
	};
	static_assert(std::has_unique_object_representations_v<Block>);


	void dumpIt(Reader& file);
}
