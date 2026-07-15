// Custom IStatus implemntation for usage simplification

#include <limits.h>
#include <string.h>
#include <utility>
#ifndef __WIN32__
#include <cxxabi.h>
#endif

#include "fbinterface.h"
#include "status.h"

void StatusTypeCatchStuff::catchException(StatusTypeCatchStuff* target)
{
	try
	{
		throw;
	}
	catch(const Status& st)
	{
		if (target->real_status == nullptr)
			return;

		unsigned state = st.getState();
		if (state & Firebird::IStatus::STATE_ERRORS)
		{
			target->real_status->setErrors(st.getErrors());
		}
		if (state & Firebird::IStatus::STATE_WARNINGS)
		{
			target->real_status->setWarnings(st.getWarnings());
		}
	}
	catch(const std::bad_alloc&)
	{
		static const ISC_STATUS st[] = { isc_arg_gds, isc_virmemexh,
										 isc_arg_end
									   };
		target->real_status->setErrors(st);
	}
	catch (const std::exception& ex)
	{
		ISC_STATUS st[] = { isc_arg_gds, isc_random, isc_arg_string, (ISC_STATUS)"std::exception caught",
							isc_arg_gds, isc_random, isc_arg_string, reinterpret_cast<ISC_STATUS>(ex.what()),
							isc_arg_end
						  };
		target->real_status->setErrors(st);
	}
#ifndef __WIN32__ // Workaround internals of pthread_cancel
	catch (const abi::__forced_unwind&)
	{
		throw;
	}
#endif
	catch(...)
	{
		static const ISC_STATUS statusVector[] = {
			isc_arg_gds, isc_random, isc_arg_string, (ISC_STATUS) "Unrecognized C++ exception",
			isc_arg_end};
		target->real_status->setErrors(statusVector);
	}
}

void Status::setVersionError(Status* status, const char* what, unsigned foundVersion, unsigned requiredVersion)
{
	const intptr_t err[] = {
		isc_arg_gds,
		isc_interface_version_too_old,
		isc_arg_number,
		(intptr_t) requiredVersion,
		isc_arg_number,
		(intptr_t) foundVersion,
		isc_arg_string,
		(intptr_t) what,
		isc_arg_end
	};

	status->setErrors(err);
}

namespace
{
	static const intptr_t NoErrorsVector[] = { isc_arg_gds, 0, isc_arg_end };

	size_t calculateLegacySize(const unsigned char* buffer, size_t bufferLength)
	{
		size_t vectorLength = 0;
		if (buffer != nullptr && bufferLength != 0)
		{
			const unsigned char* buffer_end = buffer + bufferLength;

			for (const unsigned char* p = buffer; p < buffer_end;)
			{
				const unsigned char tag = *p++;

				switch(tag)
				{
				case isc_arg_gds:		// generic DSRI status value
				case isc_arg_number:	// numeric argument (long)
				case isc_arg_unix:		// UNIX error code
				case isc_arg_win32:		// Win32 error code
				case isc_arg_warning:	// warning argument
					{
						if ((p += 4) > buffer_end) // Unavoidable sanity check
						{
							// No throw, just cut it
							return vectorLength;
						}
						vectorLength += 2;
						break;
					}
				case isc_arg_string:	// string argument
				case isc_arg_interpreted:	// interpreted status code (string)
					{
						if (p + 2 > buffer_end)
							return vectorLength;

						unsigned len = *reinterpret_cast<const unsigned short*>(p);
						p += len;

						if (p > buffer_end)
							return vectorLength;

						vectorLength += 2;
						break;
					}
				default:	// Every unrecognized code cause abort of parsing. We have no way to indicate errors here
				// These codes are here as well
				//case isc_arg_end:		// end of argument list
				//case isc_arg_sql_state:	// SQLSTATE
				//case isc_arg_cstring:	// count & string argument
				//case isc_arg_vms:		// VAX/VMS status code (long)
				//case isc_arg_domain:	// Apollo/Domain error code
				//case isc_arg_dos:		// MSDOS/OS2 error code
				//case isc_arg_mpexl:		// HP MPE/XL error code
				//case isc_arg_mpexl_ipc:	// HP MPE/XL IPC error code
				//case isc_arg_next_mach:	// NeXT/Mach error code
				//case isc_arg_netware:	// NetWare error code
					return vectorLength;
				}
			}
		}
		return vectorLength;
	}

	// This routine also advance source pointer
	void GatherLegacyVector(const unsigned char* &from, intptr_t* to, size_t toLength)
	{
		for (size_t i = 0; i < toLength; i += 2)
		{
			char tag = *from++;
			to[i] = tag;

			if (tag == isc_arg_string || tag == isc_arg_interpreted)
			{
				unsigned len = *reinterpret_cast<const uint16_t*>(from);
				from += sizeof(uint16_t);

				to[i + 1] = reinterpret_cast<intptr_t>(from);
				from += len;
			}
			else
			{
				to[i + 1] = *reinterpret_cast<const uint32_t*>(from);
				from += sizeof(uint32_t);
			}
		}
	}
}


Status::DataHolder::DataHolder(size_t length): refCnt(1)
{
	// Allocation is here to abort creation on std::bad_alloc
	data_len = length;
	data = new unsigned char[length];
}

Status::DataHolder::~DataHolder()
{
	delete[] data;
	delete[] legacyVector;
}

Status::DataHolder* Status::DataHolder::create(const intptr_t* legacy_value, unsigned value_length)
{
	// Calculate required length
	size_t buffer_size = 0;
	const intptr_t* value = legacy_value;
	for (unsigned length = value_length; length-- > 0;) // Only tags with parameters are interesting
	{
		switch(*value++)
		{
		case isc_arg_gds:		// generic DSRI status value
		case isc_arg_number:	// numeric argument (long)
		case isc_arg_unix:		// UNIX error code
		case isc_arg_win32:		// Win32 error code
		case isc_arg_warning:	// warning argument
			{
				if (length > 0) // Unavoidable sanity check
				{
					length--;
					// So far fix value length to 4 bytes
					buffer_size += 1 + 4;
				}
				break;
			}
		case isc_arg_string:	// string argument
		case isc_arg_interpreted:	// interpreted status code (string)
			{
				if (length > 0)
				{
					length--;
					size_t len = strlen((char*)(*value)) + 1; // Zero terminator is included
					if (len > USHRT_MAX)
					{
						len = USHRT_MAX;
					}
					buffer_size += 1 + 2 + len;
				}
				break;
			}
		case isc_arg_cstring:	// count & string argument
			{
				// Convert CSTRING into STRING
				if (length > 1)
				{
					length -= 2;
					size_t len = *value++;
					if (len >= USHRT_MAX)
					{
						len = USHRT_MAX - 1;
					}
					buffer_size += 1 + 2 + len + 1;
				}
				break;
			}
		case isc_arg_sql_state:	// SQLSTATE
			{
				// Ignore it...?
				if (length > 0)
					length--;
				break;
			}
		default:	// Every unrecognized code cause abort of parsing. We have no way to indicate errors here
		// These codes are here as well
		//case isc_arg_end:		// end of argument list
		//case isc_arg_vms:		// VAX/VMS status code (long)
		//case isc_arg_domain:	// Apollo/Domain error code
		//case isc_arg_dos:		// MSDOS/OS2 error code
		//case isc_arg_mpexl:		// HP MPE/XL error code
		//case isc_arg_mpexl_ipc:	// HP MPE/XL IPC error code
		//case isc_arg_next_mach:	// NeXT/Mach error code
		//case isc_arg_netware:	// NetWare error code
			goto bail;
		}
		value++;
	}
bail:

	if (buffer_size == 0)
		return nullptr;

	DataHolder* result = new DataHolder(buffer_size);

	// Gather values
	value = legacy_value;
	for (unsigned char* buffer = result->data; buffer < result->data + buffer_size;)
	{
		*buffer++ = *value;
		switch(*value++)
		{
		case isc_arg_gds:		// generic DSRI status value
		case isc_arg_number:	// numeric argument (long)
		case isc_arg_unix:		// UNIX error code
		case isc_arg_win32:		// Win32 error code
		case isc_arg_warning:	// warning argument
			{
				// Value of fixed length 4 bytes
				*reinterpret_cast<uint32_t*>(buffer) = *value++;
				buffer += sizeof(uint32_t);
				break;
			}
		case isc_arg_string:	// string argument
		case isc_arg_interpreted:	// interpreted status code (string)
			{
				size_t len = strlen((char*)(*value));
				if (len >= USHRT_MAX)
				{
					len = USHRT_MAX - 1;
				}

				*reinterpret_cast<uint16_t*>(buffer) =  len + 1; // Zero-terminator is included
				buffer += sizeof(uint16_t);

				memcpy(buffer, reinterpret_cast<char*>(*value++), len);
				buffer += len;
				*buffer++ = '\0';

				break;
			}
		case isc_arg_cstring:	// count & string argument
			{
				buffer[-1] = isc_arg_string; // Force override tag
				size_t len = *value++;
				if (len >= USHRT_MAX)
				{
					len = USHRT_MAX - 1;
				}

				*reinterpret_cast<uint16_t*>(buffer) =  len + 1;
				buffer += sizeof(uint16_t);

				memcpy(buffer, reinterpret_cast<char*>(*value++), len);
				buffer += len;
				*buffer++ = '\0';

				break;
			}
		case isc_arg_sql_state:	// SQLSTATE, ignore it
			{
				value++;
				--buffer;   // Undo tag addition
				break;
			}
		default:	// Every unrecognized code cause abort of parsing. We have no way to indicate errors here
		// These codes are here as well
		//case isc_arg_end:		// end of argument list
		//case isc_arg_vms:		// VAX/VMS status code (long)
		//case isc_arg_domain:	// Apollo/Domain error code
		//case isc_arg_dos:		// MSDOS/OS2 error code
		//case isc_arg_mpexl:		// HP MPE/XL error code
		//case isc_arg_mpexl_ipc:	// HP MPE/XL IPC error code
		//case isc_arg_next_mach:	// NeXT/Mach error code
		//case isc_arg_netware:	// NetWare error code
			return result;
		}
	}
	return result;
}

const intptr_t* Status::DataHolder::getLegacy()
{
	if (legacyVector == nullptr)
	{
		if (data != nullptr)
		{
			size_t vectorLength = calculateLegacySize(data, data_len);
			if (vectorLength == 0)
			{
				return NoErrorsVector;
			}
			try
			{
				legacyVector = new intptr_t[vectorLength + 1]; // Include one position for isc_arg_end

				const unsigned char* p = data;
				GatherLegacyVector(p, legacyVector, vectorLength);
				// Add terminator
				legacyVector[vectorLength] = isc_arg_end;
			}
			catch (const std::bad_alloc&)
			{
				static const intptr_t OutOfMemory[] = { isc_arg_gds, isc_virmemexh,
							isc_arg_gds, isc_random, isc_arg_string, (intptr_t)"getErrors()/getWarnings()",
							isc_arg_end };
				return OutOfMemory;
			}
		}
		else
		{
			return NoErrorsVector;
		}
	}
	return legacyVector;
}

unsigned Status::getState() const
{
	unsigned res = (errors != nullptr ? STATE_ERRORS : 0)
		| (warnings != nullptr ? STATE_WARNINGS : 0);

	return res;
}

void Status::setErrors(const intptr_t* value)
{
	// Protection from self-assignment
	if (value != nullptr &&
		(value == NoErrorsVector || (errors != nullptr && value == errors->legacyVector)))
	{
		// Do not touch
		return;
	}

	// Preserve old holder until the new one is formed
	// because self-assignment above doesn't protect from
	// the way Firebird implements appending to the status vector
	std::unique_ptr<DataHolder, DataHolder::deleter> oldErrors(errors);

	errors = nullptr;

	if (value != nullptr && value[0] == isc_arg_gds && value[1] != 0)
		errors = DataHolder::create(value);
}

void Status::setErrors2(unsigned length, const intptr_t* value)
{
	// Protection from self-assignment
	if (value != nullptr &&
		(value == NoErrorsVector || (errors != nullptr && value == errors->legacyVector)))
	{
		// Do not touch
		return;
	}

	std::unique_ptr<DataHolder, DataHolder::deleter> oldErrors(errors);

	errors = nullptr;

	if (value != nullptr && length != 0 && value[0] == isc_arg_gds && value[1] != 0)
		errors = DataHolder::create(value, length);
}

const intptr_t* Status::getErrors() const
{
	if (errors != nullptr)
		return errors->getLegacy();

	return NoErrorsVector;
}

void Status::setWarnings(const intptr_t* value)
{
	// Protection from self-assignment
	if (value != nullptr &&
		(value == NoErrorsVector || (warnings != nullptr && value == warnings->legacyVector)))
	{
		// Do not touch
		return;
	}

	std::unique_ptr<DataHolder, DataHolder::deleter> oldWarnings(warnings);

	warnings = nullptr;

	if (value != nullptr && value[0] == isc_arg_gds && value[1] != 0)
		warnings = DataHolder::create(value);
}

void Status::setWarnings2(unsigned length, const intptr_t* value)
{
	// Protection from self-assignment
	if (value != nullptr &&
		(value == NoErrorsVector || (warnings != nullptr && value == warnings->legacyVector)))
	{
		// Do not touch
		return;
	}

	std::unique_ptr<DataHolder, DataHolder::deleter> oldWarnings(warnings);

	warnings = nullptr;

	if (value != nullptr && length != 0 && value[0] == isc_arg_gds && value[1] != 0)
		warnings = DataHolder::create(value, length);
}

const intptr_t* Status::getWarnings() const
{
	if (warnings != nullptr)
		return warnings->getLegacy();

	return NoErrorsVector;
}

Firebird::IStatus* Status::clone() const
{
	return new Status(*this);
}

void Status::checkException(Status* status)
{
	unsigned state = status->getState();
	if (state & Firebird::IStatus::STATE_ERRORS)
	{
		switch (status->behavior)
		{
		case to::Ignore:
			{
				status->found = 0;
				if (status->ignored != nullptr)
				{
					for (iterator itr = status->beginErrors(); itr; ++itr)
					{
						unsigned code = itr.gdscode();
						for (const unsigned* i = status->ignored; *i != isc_arg_end; i++)
						{
							if (code == *i)
							{
								status->found = itr.offset;
								return;
							}
						}
					}
				}
			}
			// fall through
		case to::Throw:
			throw std::move(*status);
		case to::Log:
			break;
		}
	}
	if (status->errors != nullptr)
	{
		fprintf(stderr, "%s error:\n", status->where());
		for (iterator itr = status->beginErrors(); itr; ++itr)
		{
			char buffer[2048];
			itr.interpret(buffer, sizeof(buffer));
			fprintf(stderr, "%u\t%s\n", itr.gdscode(), buffer);
		}
	}

	if (status->warnings != nullptr)
	{
		fprintf(stderr, "%s warning:\n", status->where());
		std::string message;
		for (iterator itr = status->beginWarnings(); itr; ++itr)
		{
			char buffer[2048];
			itr.interpret(buffer, sizeof(buffer));
			fprintf(stderr, "%u\t%s\n", itr.gdscode(), buffer);
		}
	}
}

const char* Status::what() const noexcept
{
	if (!whatCache.empty())
	{
		// Hardly can be a case, but still
		return whatCache.c_str();
	}

	if (errors != nullptr)
	{
		whatCache += w;
		whatCache += " error:\n";
		for (iterator itr = beginErrors(); itr; ++itr)
		{
			char buffer[2048];
			itr.interpret(buffer, sizeof(buffer));
			whatCache += std::to_string(itr.gdscode());
			whatCache.push_back('\t');
			whatCache += buffer;
			whatCache.push_back('\n');
		}
	}

	if (warnings != nullptr)
	{
		whatCache += w;
		whatCache += " warning:\n";
		for (iterator itr = beginWarnings(); itr; ++itr)
		{
			char buffer[2048];
			itr.interpret(buffer, sizeof(buffer));
			whatCache += std::to_string(itr.gdscode());
			whatCache.push_back('\t');
			whatCache += buffer;
			whatCache.push_back('\n');
		}
	}

	if (whatCache.empty())
	{
		// Strange case of calling what without error happen
		whatCache = w;
		whatCache += " undefined status exception";
	}
	else
	{
		// Remove last EoLN
		whatCache.pop_back();
	}
	return whatCache.c_str();
}

unsigned Status::iterator::gdscode()
{
	if (*this)
	{
		const unsigned char* p = holder->data + offset;
		if (*p == isc_arg_gds) // Extra check
		{
			p += 1;
			return *reinterpret_cast<const uint32_t*>(p);
		}
		else
			return *p; // Provisory solution for primary codes other than isc_arg_gds
	}
	return isc_arg_end;
}

Status::iterator& Status::iterator::operator++()
{
	if (holder != nullptr && holder->data != nullptr)
	{
		const unsigned char* p = holder->data + offset;
		do
		{
			const unsigned char tag = *p++;
			if (tag == isc_arg_string || tag == isc_arg_interpreted)
			{
				unsigned short len = *reinterpret_cast<const uint16_t*>(p);
				p += sizeof(uint16_t) + len;
			}
			else
			{
				p += 4;
			}
			offset = p - holder->data;
		}
		while (offset < holder->data_len && (*p == isc_arg_number || *p == isc_arg_string));
	}
	return *this;
}

Status::value_t Status::iterator::operator[](size_t index)
{
	Status::value_t result;
	if (holder != nullptr && holder->data != nullptr)
	{
		const unsigned char* p = holder->data + offset;
		do
		{
			const unsigned char tag = *p++;
			if (tag == isc_arg_string)
			{
				unsigned short len = *reinterpret_cast<const uint16_t*>(p);
				p += sizeof(uint16_t) + len;
			}
			else
			{
				p += sizeof(uint32_t);
			}
		}
		while (index-- > 0 && p < holder->data + holder->data_len && (*p == isc_arg_number || *p == isc_arg_string));

		if (p < holder->data + holder->data_len)
		{
			// Previous loop exited by reaching needed index. Perhaps.
			if (*p == isc_arg_string)
			{
				result.type = *p;
				p += 1 + sizeof(uint16_t);
				result.string = reinterpret_cast<const char*>(p);
			}
			else if (*p == isc_arg_number)
			{
				result.type = *p;
				p += 1;
				result.integer = *reinterpret_cast<const uint32_t*>(p);
			}
			// else it was a primary code and we run out of parameters
		}
	}
	return result;
}

namespace
{
	class StaticStub: public Firebird::IStatusImpl<StaticStub, StatusTypeCatchIgnore>
	{
		const ISC_STATUS* v;
	public:
		StaticStub() = delete;
		StaticStub(const ISC_STATUS* vector): v(vector)
		{
		}
		void dispose() override {}
		void init() override {}
		unsigned getState() const override
		{
			return STATE_ERRORS;
		}
		void setErrors2(unsigned, const intptr_t* value) override
		{
			v = value;
		}
		void setWarnings2(unsigned, const intptr_t* value) override
		{
			v = value;
		}
		void setErrors(const intptr_t* value) override
		{
			v = value;
		}
		void setWarnings(const intptr_t* value) override
		{
			v = value;
		}
		const intptr_t* getErrors() const override
		{
			return v;
		}
		const intptr_t* getWarnings() const override
		{
			return v;
		}
		Firebird::IStatus* clone() const override
		{
			return nullptr;
		}
	};
}

size_t Status::iterator::interpret(char* buffer, size_t size)
{
	if (*this)
	{
		ISC_STATUS legacyVector[2 + 9 * 2 + 1] = {}; // Two positions for primary code, two positions for each of up to 9 arguments and one position for isc_arg_end
		StaticStub dummy(legacyVector);
		Firebird::IUtil* util = master->getUtilInterface();

		intptr_t* target = legacyVector;
		const unsigned char* p = holder->data + offset;
		const unsigned char* const end = holder->data + holder->data_len;

		for (int i = 0; i < 10; i++)
		{
			unsigned char tag = *p++;
			*target++ = tag;

			if (tag == isc_arg_string || tag == isc_arg_interpreted)
			{
				unsigned short len = *reinterpret_cast<const uint16_t*>(p);
				p += sizeof(uint16_t);

				*target++ = reinterpret_cast<intptr_t>(p);
				p += len;
			}
			else
			{
				*target++ = *reinterpret_cast<const uint32_t*>(p);
				p += sizeof(uint32_t);
			}

			if (p >= end || (*p != isc_arg_number && *p != isc_arg_string))
				break;
		}

		*target = isc_arg_end;

		unsigned res = util->formatStatus(buffer, size, &dummy);
		return res;
	}
	return 0;
}
