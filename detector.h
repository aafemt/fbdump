#pragma once

#include "reader.h"

enum class FileType
{
	Unknown,
	Journal
};

FileType detectFileType(Reader& file);
