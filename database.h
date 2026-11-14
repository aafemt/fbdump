#pragma once

#include <stdint.h>
#include <rpc.h>

#include "reader.h"

namespace Ods
{
	// From firebird/src/jrd/ods.h
	struct pag
	{
		uint8_t pag_type;
		uint8_t pag_flags;
		uint16_t pag_reserved;		// not used but anyway present because of alignment rules
		uint32_t pag_generation;
		uint32_t pag_scn;
		uint32_t pag_pageno;		// for validation
	};

	inline constexpr uint8_t pag_undefined		= 0;
	inline constexpr uint8_t pag_header			= 1;		// Database header page
	inline constexpr uint8_t pag_pages			= 2;		// Page inventory page
	inline constexpr uint8_t pag_transactions	= 3;		// Transaction inventory page
	inline constexpr uint8_t pag_pointer		= 4;		// Pointer page
	inline constexpr uint8_t pag_data			= 5;		// Data page
	inline constexpr uint8_t pag_root			= 6;		// Index root page
	inline constexpr uint8_t pag_index			= 7;		// Index (B-tree) page
	inline constexpr uint8_t pag_blob			= 8;		// Blob data page
	inline constexpr uint8_t pag_ids			= 9;		// Gen-ids
	inline constexpr uint8_t pag_scns			= 10;		// SCN's inventory page
	inline constexpr uint8_t pag_max			= 10;		// Max page type

	struct header_page13
	{
		pag hdr_header;
		uint16_t hdr_page_size;			// Page size of database
		uint16_t hdr_ods_version;			// Version of on-disk structure
		uint32_t hdr_PAGES;				// Page number of PAGES relation
		uint32_t hdr_next_page;			// Page number of next hdr page
		uint32_t hdr_oldest_transaction;	// Oldest interesting transaction
		uint32_t hdr_oldest_active;		// Oldest transaction thought active
		uint32_t hdr_next_transaction;		// Next transaction id
		uint16_t hdr_sequence;			// sequence number of file
		uint16_t hdr_flags;				// Flag settings, see below
		int32_t hdr_creation_date[2];		// Date/time of creation
		uint32_t hdr_attachment_id;		// Next attachment id
		int32_t hdr_shadow_count;			// Event count for shadow synchronization
		uint8_t hdr_cpu;					// CPU database was created on
		uint8_t hdr_os;					// OS database was created under
		uint8_t hdr_cc;					// Compiler of engine on which database was created
		uint8_t hdr_compatibility_flags;	// Cross-platform database transfer compatibility flags
		uint16_t hdr_ods_minor;			// Update version of ODS
		uint16_t hdr_end;					// offset of HDR_end in page
		uint32_t hdr_page_buffers;			// Page buffers for database cache
		uint32_t hdr_oldest_snapshot;		// Oldest snapshot of active transactions
		int32_t hdr_backup_pages; 		// The amount of pages in files locked for backup
		uint32_t hdr_crypt_page;			// Page at which processing is in progress
		char hdr_crypt_plugin[32];		// Name of plugin used to crypt this DB
		int32_t hdr_att_high;				// High word of the next attachment counter
		uint16_t hdr_tra_high[4];			// High words of the transaction counters
		uint8_t hdr_data[];				// Misc data
	};
	static_assert(std::has_unique_object_representations_v<header_page13>);

	struct header_page14
	{
		pag hdr_header;
		uint16_t hdr_page_size;				// Page size of database
		uint16_t hdr_ods_version;			// Version of on-disk structure
		uint16_t hdr_ods_minor;				// Update version of ODS
		uint16_t hdr_flags;					// Flag settings, see below
		uint8_t hdr_backup_mode;
		uint8_t hdr_shutdown_mode;
		uint8_t hdr_replica_mode;
		uint32_t hdr_PAGES;					// Page number of PAGES relation
		uint32_t hdr_page_buffers;			// Page buffers for database cache
		uint16_t hdr_end;					// Offset of HDR_end in page
		uint64_t hdr_next_transaction;		// Next transaction id
		uint64_t hdr_oldest_transaction;	// Oldest interesting transaction
		uint64_t hdr_oldest_active;			// Oldest transaction thought active
		uint64_t hdr_oldest_snapshot;		// Oldest snapshot of active transactions
		uint64_t hdr_attachment_id;			// Next attachment id
		struct
		{
			uint8_t hdr_cpu;				// CPU database was created on
			uint8_t hdr_os;					// OS database was created under
			uint8_t hdr_cc;					// Compiler of engine on which database was created
			uint8_t hdr_compat;				// Cross-platform database transfer compatibility flags
		} hdr_db_impl;
		UUID hdr_guid;						// Database GUID
		int32_t hdr_creation_date[2];		// Date/time of creation
		int32_t hdr_shadow_count;			// Event count for shadow synchronization
		uint32_t hdr_crypt_page;			// Page at which processing is in progress
		char hdr_crypt_plugin[32];			// Name of plugin used to crypt this DB
		uint8_t hdr_data[];					// Misc data
	};
	//static_assert(std::has_unique_object_representations_v<header_page6>);

}

namespace Database
{
	void dumpIt(Reader& file);
}
