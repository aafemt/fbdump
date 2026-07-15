#pragma once

#include <memory>

#include "firebird/Interface.h"
#include "static_exception.h"
#include "tstring.h"

// Deleter for Firebird interfaces for using in std::unique_ptr
struct interface_deleter
{
	void operator()(Firebird::IDisposable* iface) const
	{
		if (iface != nullptr)
			iface->dispose();
	}
	void operator()(Firebird::IReferenceCounted* iface) const
	{
		if (iface != nullptr)
			iface->release();
	}
};

struct ParameterBlock: public std::basic_string<unsigned char>
{
	using std::basic_string<unsigned char>::append;
	using std::basic_string<unsigned char>::operator=;
	typedef std::basic_string<unsigned char> inherited;

	void append(const tstring& value)
	{
		if (value.size() != 0)
		{
			const std::string v = to_string(value);
			inherited::append(reinterpret_cast<const unsigned char*>(v.c_str()), v.size());
		}
	}
	void appendTagged(unsigned char tag, const unsigned char* value, size_t valueLen, int lenLen = 1)
	{
		push_back(tag);
		size_t tmp = valueLen;
		while (--lenLen >= 0)
		{
			push_back((unsigned char)tmp);
			tmp >>= 8;
		}
		if (tmp != 0)
			throw static_exception("Parameter block build failed: too long string");

		if (valueLen > 0)
			append(value, valueLen);
	}
	void appendTagged(unsigned char tag, const tstring& value, int lenLen = 1)
	{
		push_back(tag);

		const std::string v = to_string(value);
		size_t tmp = v.size();
		while (--lenLen >= 0)
		{
			push_back((unsigned char)tmp);
			tmp >>= 8;
		}
		if (tmp != 0)
			throw static_exception("Parameter block build failed: too long string");

		inherited::append(reinterpret_cast<const unsigned char*>(v.c_str()), v.size());
	}

	void appendTagged(unsigned char tag, long long value, int lenLen = 1)
	{
		push_back(tag);
		unsigned long long mask;
		unsigned l = 0;
		size_t lengthOffset = size();
		append(lenLen, '\0');
		do
		{
			push_back(value & 0xFF);
			l++;
			mask = value & 0xFFFFFFFFFFFFFF80;
			value >>= 8;
		}
		while (mask != 0 && mask != 0xFFFFFFFFFFFFFF80);
		// For integer value it is impossible to take more than 8 bytes so length is always one byte
		operator[](lengthOffset) = l;
	}
};

typedef std::unique_ptr<Firebird::IAttachment, interface_deleter> Attachment;

// Smart pointer for transaction. Autostart a transaction with default parameters
class Transaction
{
	Firebird::ITransaction* tr = nullptr;
public:
	Transaction(Firebird::IAttachment* att);
	Transaction(Firebird::IAttachment* att, ParameterBlock& tpb);
	~Transaction();

	void commit();
	void commit(const char* where);
	operator Firebird::ITransaction*()
	{
		return tr;
	}
	explicit operator bool() const
	{
		return tr != nullptr;
	}
};

inline Firebird::IMaster* master = nullptr;
