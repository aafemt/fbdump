#pragma once

#include "reader.h"

enum class FileType
{
	Unknown,
	Journal,
	Database
};

FileType detectFileType(Reader& file);
