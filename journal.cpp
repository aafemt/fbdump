#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>
#include <stdexcept>
#include <vector>
#include <errno.h>
#include <sys/stat.h>

#include "journal.h"
#include "static_exception.h"

namespace Journal
{
	struct Atoms : public std::vector<std::string>
	{
		std::string get(uint32_t n)
		{
			if (n >= size())
			{
				return std::string("Undefined atom number ") + std::to_string(n);
			}
			return at(n);
		}
	};

	void dumpIt(Reader& file)
	{
		struct stat64 st{};
		if (fstat64(file.f, &st) == -1)
		{
			fprintf(stderr, "fstat error %u\n", errno);
		}
		printf("Replication journal %s, length %llu\n", to_string(file.fileName).c_str(), st.st_size);
		char signature[SignatureSize];
		// Here we read separate pieces of header instead of whole one to handle different endianness
		file.readBuffer(signature, SignatureSize);
		printf("\tSignature: %.12s\n", signature);
		if (memcmp(signature, Signature, SignatureSize) != 0)
		{
			throw static_exception("Signature is wrong");
		}
		const unsigned short version = file.getInt16();
		printf("\tVersion: %u\n", version);
		if (version != 1)
		{
			throw static_exception("Unrecognized journal version");
		}
		const unsigned short state = file.getInt16();
		printf("\tState: ");
		switch (state)
		{
		case SEGMENT_STATE_FREE:
			{
				printf("free\n");
				break;
			}
		case SEGMENT_STATE_USED:
			{
				printf("used\n");
				break;
			}
		case SEGMENT_STATE_FULL:
			{
				printf("full\n");
				break;
			}
		case SEGMENT_STATE_ARCH:
			{
				printf("archived\n");
				break;
			}
		default:
			{
				printf("unknown (%x)\n", state);
				break;
			}
		}
		UUID uuid;
		uuid.Data1 = file.getInt32();
		uuid.Data2 = file.getInt16();
		uuid.Data3 = file.getInt16();
		file.readBuffer(uuid.Data4, sizeof(uuid.Data4));
		printf("\tUUID: {%08X-%04hX-%04hX-%02X%02X-%02X%02X%02X%02X%02X%02X}\n",
				(unsigned)uuid.Data1,
				uuid.Data2,
				uuid.Data3,
				uuid.Data4[0], uuid.Data4[1], uuid.Data4[2],
				uuid.Data4[3], uuid.Data4[4], uuid.Data4[5],
				uuid.Data4[6], uuid.Data4[7]);
		const uint64_t sequence = file.getInt64();
		printf("\tSequence: %llu\n", sequence);
		uint64_t length = file.getInt64();
		printf("\tLength: %llu\n", length);
		length -= sizeof(SegmentHeader);

		// Physical size of file can be bigger than size of data inside
		while (length > 0)
		{
			uint64_t transactionNumber = file.getInt64();
			uint16_t protocol = file.getInt16();
			uint16_t flags = file.getInt16();
			uint32_t blockLength = file.getInt32();
			printf("Transaction number %llu, protocol %x, flags %04x (", transactionNumber, protocol, flags);
			bool comma = false;
			if (flags & BLOCK_BEGIN_TRANS)
			{
				printf("begin");
				comma = true;
			}
			if (flags & BLOCK_END_TRANS)
			{
				printf("%send", comma ? "," : "");
			}
			if (flags & ~(BLOCK_BEGIN_TRANS | BLOCK_END_TRANS))
			{
				printf("%sunknown flag", comma ? "," : "");
			}
			printf("), length: %u\n", blockLength);

			if (length < blockLength + sizeof(Block))
			{
				fprintf(stderr, "Structure error: too long transaction block\n");
				return;
			}

			length -= blockLength + sizeof(Block);

			// Atoms are individual for each block
			Atoms atoms;

			while (blockLength > 0)
			{
				const Operation op = static_cast<Operation>(file.getInt8());
				size_t opLength = 1;
				switch (op)
				{
				case opStartTransaction:
					{
						printf("\tStart transaction\n");
						break;
					}
				case opPrepareTransaction:
					{
						printf("\tPrepare transaction\n");
						break;
					}
				case opCommitTransaction:
					{
						printf("\tCommit transaction\n");
						break;
					}
				case opRollbackTransaction:
					{
						printf("\tRollback transaction\n");
						break;
					}
				case opCleanupTransaction:
					{
						printf("\tCleanup transaction\n");
						break;
					}
				case opStartSavepoint:
					{
						printf("\tStart savepoint\n");
						break;
					}
				case opReleaseSavepoint:
					{
						printf("\tRelease savepoint\n");
						break;
					}
				case opRollbackSavepoint:
					{
						printf("\tRollback savepoint\n");
						break;
					}
				case opInsertRecord:
					{
						uint32_t atom = file.getInt32();
						std::string name = atoms.get(atom);
						opLength += 4;
						if (protocol > 1)
						{
							atom = file.getInt32();
							opLength += 4;
							name += '.';
							name += atoms.get(atom);
						}
						uint32_t dataLength = file.getInt32();
						opLength += 4 + dataLength;
						printf("\tInsert record into %s, data length %u\n", name.c_str(), dataLength);
						HexDumper dumper;
						file.dumpTo(dumper, dataLength);
						break;
					}
				case opUpdateRecord:
					{
						uint32_t atom = file.getInt32();
						std::string name = atoms.get(atom);
						opLength += 4;
						if (protocol > 1)
						{
							atom = file.getInt32();
							opLength += 4;
							name += '.';
							name += atoms.get(atom);
						}
						uint32_t oldDataLength = file.getInt32();
						opLength += 4 + oldDataLength;
						printf("\tUpdate record in %s\n\t\tOld data length %u\n", name.c_str(), oldDataLength);
						HexDumper dumper;
						file.dumpTo(dumper, oldDataLength);
						dumper.flush();
						uint32_t newDataLength = file.getInt32();
						opLength += 4 + newDataLength;
						printf("\t\tNew data length %u\n", newDataLength);
						file.dumpTo(dumper, newDataLength);
						break;
					}
				case opDeleteRecord:
					{
						uint32_t atom = file.getInt32();
						std::string name = atoms.get(atom);
						opLength += 4;
						if (protocol > 1)
						{
							atom = file.getInt32();
							opLength += 4;
							name += '.';
							name += atoms.get(atom);
						}
						uint32_t dataLength = file.getInt32();
						opLength += 4 + dataLength;
						printf("\tDelete record from %s, data length %u\n", name.c_str(), dataLength);
						HexDumper dumper;
						file.dumpTo(dumper, dataLength);
						break;
					}
				case opStoreBlob:
					{
						uint32_t bidHigh = file.getInt32();
						uint32_t bidLow = file.getInt32();
						opLength += 8;
						printf("\tBlob with id (%x:%x)\n", bidHigh, bidLow);
						while (uint16_t segmentLength = file.getInt16())
						{
							printf("\t\tSegment of length %hu\n", segmentLength);
							opLength += 2 + segmentLength;
							if (opLength > blockLength)
							{
								fprintf(stderr, "Structure error: blob data length bigger than block length\n");
								return;
							}
							HexDumper dumper;
							file.dumpTo(dumper, segmentLength);
						}
						break;
					}
				case opExecuteSql:
					{
						uint32_t atom = file.getInt32();
						std::string userName = atoms.get(atom);
						uint32_t sqlLength = file.getInt32();
						opLength += 4 + 4 + sqlLength;
						printf("\tExecute SQL as %s, length %u\n", userName.c_str(), sqlLength);
						HexDumper dumper;
						file.dumpTo(dumper, sqlLength);
						break;
					}
				case opSetSequence:
					{
						uint32_t atom = file.getInt32();
						std::string name = atoms.get(atom);
						if (protocol > 1)
						{
							atom = file.getInt32();
							opLength += 4;
							name += '.';
							name += atoms.get(atom);
						}
						uint64_t value = file.getInt64();
						opLength += 4 + 8;
						printf("\tSet sequence %s to %lld\n", name.c_str(), value);
						break;
					}
				case opExecuteSqlIntl:
					{
						uint32_t atom = file.getInt32();
						std::string userName = atoms.get(atom);
						if (protocol > 1)
						{
							atom = file.getInt32();
							opLength += 4;
						}
						uint8_t charset = file.getInt8();
						uint32_t sqlLength = file.getInt32();
						opLength += 4 + 4 + 1 + sqlLength;
						printf("\tExecute SQL as %s, length %u, charset %u\n", userName.c_str(), sqlLength, charset);
						if (protocol > 1)
						{
							printf("\t\tSchema path: %s\n", atoms.get(atom).c_str());
						}
						HexDumper dumper;
						file.dumpTo(dumper, sqlLength);
						break;
					}
				case opDefineAtom:
					{
						uint8_t atomLength = file.getInt8();
						char buffer[atomLength];
						file.readBuffer(buffer, atomLength);
						opLength += 1 + atomLength;
						printf("\tAtom: %.*s\n", atomLength, buffer);
						atoms.emplace_back(static_cast<const char*>(buffer), static_cast<size_t>(atomLength));
						break;
					}
				default:
					{
						throw std::runtime_error(std::string("Unknown operation ") + std::to_string(op));
					}
				}
				if (opLength > blockLength)
				{
					fprintf(stderr, "Structure error: operation length bigger than block length\n");
					return;
				}
				//printf("block length = %u, op length = %zu\n", blockLength, opLength);
				blockLength -= opLength;
			}
		}
		if (!file.eof())
		{
			printf("*** some garbage beyond data ***\n");
		}
	}
}
