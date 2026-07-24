#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>
#include <fcntl.h>
#include <stdexcept>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <locale.h>

#include "fbinterface.h"
#include "tstring.h"
#include "reader.h"
#include "detector.h"
#include "journal.h"
#include "status.h"

void help()
{
	printf("Firebird files dump utility.\n"
			"Usage: fbdump [options] <file name>\n"
			"Options:\n"
			"\t-i\tNext parameter is a file (optional, useful if the file name starts with '-')\n"
			"\t-d\tNext parameter is a database connection string\n"
			"\t-u\tNext parameter is an user name\n"
			"\t-p\tNext parameter is a password\n"
			"\t-?,-h\tThis text\n"
			"\t-v\tDump more data\n"
			"\nExample: fbdump -v db_abc.journal-0000001\n"
			"\nOptions may be combined.\n"
			"Example: fbdump -iv db_abc.journal-0000001\n"
			);
}

bool verbose = false;

int _tmain(int argc, _TCHAR* argv[])
{
	struct CPHolder
	{
		UINT oldCP;
		CPHolder()
		{
			oldCP = GetConsoleOutputCP();
			if (!SetConsoleOutputCP(CP_UTF8))
				printf("Set console output to UTF-8 failed. Error %lu\n", GetLastError());
		}
		~CPHolder()
		{
			SetConsoleOutputCP(oldCP);
		}

	} cpHolder;

	try
	{
		// Recognize command-line options
		tstring fileName;
		tstring database;
		tstring userName;
		tstring password;
		FileType fileType = FileType::Unknown;

		for (int i = 1; i < argc; ++i)
		{
			_TCHAR* arg = argv[i];
			if (*arg == '-')
			{
				while (*(++arg))
				{
					switch (*arg)
					{
					case _TEXT('?'):
					case _TEXT('h'):
						{
							//help(); will be printed later
							break;
						}
					case _TEXT('d'):
						{
							if (!database.empty())
							{
								throw std::invalid_argument("Database option is used twice");
							}
							if (++i >= argc)
							{
								throw std::out_of_range("Database name is missing");
							}
							database = argv[i];
							break;
						}
					case _TEXT('i'):
						{
							if (!fileName.empty())
							{
								throw std::invalid_argument("Input file option is used twice");
							}
							if (++i >= argc)
							{
								throw std::out_of_range("File name is missing");
							}
							fileName = argv[i];
							break;
						}
					case _TEXT('p'):
						{
							if (!password.empty())
							{
								throw std::invalid_argument("Password option is used twice");
							}
							if (++i >= argc)
							{
								throw std::out_of_range("Password is missing");
							}
							password = argv[i];
							break;
						}
					case _TEXT('u'):
						{
							if (!userName.empty())
							{
								throw std::invalid_argument("User name option is used twice");
							}
							if (++i >= argc)
							{
								throw std::out_of_range("User name is missing");
							}
							userName = argv[i];
							break;
						}
					case _TEXT('v'):
						{
							verbose = true;
							break;
						}
					default:
						throw std::invalid_argument(std::string("Unrecognized option ") + char(*arg));
					}
				}
			}
			else
			{
				if (!fileName.empty())
				{
					throw std::invalid_argument("Input file option is used twice");
				}
				fileName = argv[i];
			}
		}

		// If no file name provided - show help
		if (fileName.empty())
		{
			help();
			return 0;
		}

		struct FileHolder
		{
			int f;
			FileHolder(const tstring& name, int flags)
			{
				f = _topen(name.c_str(), flags);
				if (f == -1)
				{
					throw std::runtime_error(std::string("Unable to open file \"") + to_string(name) + "\": " + strerror(errno));
				}
			}
			~FileHolder()
			{
				close(f);
			}
		};

		Reader file(fileName, O_RDONLY | O_BINARY);

		if (fileType == FileType::Unknown)
		{
			file.readBuffer();
			fileType = detectFileType(file);
		}

		Attachment att;
		if (!database.empty())
		{
			HMODULE fbLibrary = LoadLibrary(_TEXT("fbclient"));
			if (fbLibrary == nullptr)
			{
				fprintf(stderr, "Error loading Firebird client %lu\n", GetLastError());
				throw static_exception("Firebird attach failed");
			}
			decltype(Firebird::fb_get_master_interface)* f = reinterpret_cast<decltype(Firebird::fb_get_master_interface)*>((void*)GetProcAddress(fbLibrary, "fb_get_master_interface"));
			if (f == nullptr)
			{
				throw static_exception("fb_get_master_interface function not found in Firebird library");
			}
			master = f();
			ParameterBlock dpb;
			dpb = isc_dpb_version1;
			dpb += isc_dpb_utf8_filename;
			dpb += (unsigned char)0;

			if (!userName.empty())
			{
				dpb.appendTagged(isc_dpb_user_name, userName);
			}
			if (!password.empty())
			{
				dpb.appendTagged(isc_dpb_password, password);
			}
			Status st("Database attach");
			att.reset(master->getDispatcher()->attachDatabase(&st, to_string(database).c_str(), dpb.size(), dpb.c_str()));
		}

		switch (fileType)
		{
		case FileType::Journal:
			Journal::dumpIt(file, att);
			break;
		case FileType::Unknown:
			printf("Unable to determine type of file \"%s\", perhaps it is damaged.\n", to_string(fileName).c_str());
			return 1;
		}

	}
	catch (const std::exception& ex)
	{
		fprintf(stderr, "%s\n", ex.what());
		return 1;
	}

    return 0;
}
