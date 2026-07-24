#pragma once

#include <string>

struct Dumper
{
	// This routine is called for each piece of data
	virtual void dumpIt(const unsigned char* buffer, size_t length) = 0;
};

struct HexDumper : public Dumper
{
	char string[2 + 16 * 3 + 3 + 16 + 2] = "*                                                 *                  *";
	unsigned pos = 0;

	HexDumper();
	~HexDumper();

	void clearString();
	void dumpIt(const unsigned char* buffer, size_t length) override;
	void flush();
};

struct CompactHexDumper : public Dumper
{
	void dumpIt(const unsigned char* buffer, size_t length) override;
};

struct Attachment;
struct Reader;
// Dump data from record of given table
void dumpData(Reader& file, const std::string& schema, const std::string& name, const unsigned char* data, size_t dataLength, Attachment& att);
