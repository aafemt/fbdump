// Custom IStatus implementation

#pragma once

//#include <atomic>
#include <exception>
#include <new>
#include <string>

#include "firebird/Interface.h"

class StatusTypeCatchIgnore
{
public:
	static void catchException(void*) {}
};

struct StatusTypeCatchStuff
{
	Firebird::IStatus* real_status;

	static void catchException(StatusTypeCatchStuff* target);

	StatusTypeCatchStuff(Firebird::IStatus* status): real_status(status) {}
};

class Status: public Firebird::IStatusImpl<Status, StatusTypeCatchIgnore>, public std::exception
{
	struct DataHolder
	{
		// Is atomic overhead really needed here? I hope no.
		//std::atomic_int refCnt;
		int refCnt;
		// Buffer containing TLV clumplets with one byte tag and two bytes length.
		// I don't believe that a sane error line can contain a parameter longer than 64k.
		// Firebird code check that it is 1024 bytes or less.
/*
		Replaced with fixed length data for all tags except string and interpreted because it is a little more compact
		Data for tags:
		isc_arg_gds: 4 bytes gds code
		isc_arg_string, isc_arg_interpreted: 2 bytes length + XXXX bytes the string
		isc_arg_number and the rest: 4 bytes data
		isc_arg_end - final tag (is not stored, network only).
*/

		unsigned char* data = nullptr;
		size_t data_len = 0;
		// Buffer for backward compatible status array format returned by getErrors()
		intptr_t* legacyVector = nullptr;

		DataHolder(size_t length);
		DataHolder(const DataHolder&) = delete; // Non-copyable
		DataHolder(DataHolder&&) = delete; // Non-moveable
		~DataHolder();

		void addRef()
		{
			refCnt++;
		}
		void release()
		{
			if (--refCnt == 0)
				delete this;
		}

		// Create data holder and fill it with status from a buffer in legacy format.
		// Can return nullptr if the buffer is empty
		static DataHolder* create(const intptr_t* legacy_value, unsigned value_length = 1000);
		const intptr_t* getLegacy();

		struct deleter
		{
			void operator()(DataHolder* holder) const
			{
				if (holder != nullptr)
					holder->release();
			}
		};
	};

private:
	enum class to { Throw, Log, Ignore }; // Error handling modes

	const char* w = nullptr;
	bool dyn = false;
	to behavior = to::Throw;
	const unsigned* ignored = nullptr;
	size_t found = 0; // Position of first found gdscode from ignore list
	DataHolder* errors = nullptr;
	DataHolder* warnings = nullptr;
	mutable std::string whatCache;

	// This constructor is private to force external code using move constructor but
	// let clone() copy self.
	Status(const Status& orig) noexcept: IStatusImpl(orig), w(orig.w), dyn(true)
	{
		errors = orig.errors;
		if (errors != nullptr)
			errors->addRef();
		warnings = orig.warnings;
		if (warnings != nullptr)
			warnings->addRef();
	}
	void clean()
	{
		if (errors != nullptr)
		{
			errors->release();
			errors = nullptr;
		}
		if (warnings != nullptr)
		{
			warnings->release();
			warnings = nullptr;
		}
		whatCache.clear();
	}
public:
	struct value_t
	{
		unsigned char type = isc_arg_end;
		union
		{
			const char* string = nullptr;
			int integer;
		};
	};

	struct iterator
	{
		friend class Status;

		iterator(DataHolder* source, size_t off = 0): holder(source), offset(off)
		{
			// Shouldn't we increment reference counter on holder?..
		}

		iterator& operator++();

		explicit operator bool() const
		{
			return holder != nullptr && offset < holder->data_len;
		}
		Status::value_t operator[](size_t index);
		unsigned gdscode();
		size_t interpret(char* buffer, size_t size);

	private:
		Status::DataHolder* holder;
		size_t offset;
	};

	Status(const char* where, bool dynamic = false): w(where), dyn(dynamic) {}
	Status(const char* where, std::nothrow_t): w(where), dyn(false), behavior(to::Log) {}
	// This move constructor allows usage of Status as an exception.
	// If simple "throw o" doesn't work, use "throw std::move(o)".
	Status(Status&& orig) noexcept // Moveable
		: whatCache(std::move(orig.whatCache))
	{
		w = orig.w;
		dyn = false; // Memory of temporary objects is managed by compiler
		errors = orig.errors;
		orig.errors = nullptr;
		warnings = orig.warnings;
		orig.warnings = nullptr;
	}
	~Status()
	{
		clean();
	}
	// For usage like interface->something(status("something"), .....);
	Status* operator()(const char* newWhere)
	{
		w = newWhere;
		behavior = to::Throw;
		whatCache.clear();
		return this;
	}
	// The same but only logging
	Status* operator()(const char* newWhere, std::nothrow_t)
	{
		w = newWhere;
		behavior = to::Log;
		whatCache.clear();
		return this;
	}
	// Ignoring of some gds codes
	Status* operator()(const char* newWhere, const unsigned* list)
	{
		w = newWhere;
		ignored = list;
		behavior = to::Ignore;
		whatCache.clear();
		return this;
	}
	// Just for consistency of usage
	Status* operator()()
	{
		whatCache.clear();
		return this;
	}

	// IDisposable implementation
	void dispose() override
	{
		if (dyn) // Hardly can anyone call dispose() on automatic object but make it safe.
			delete this;
	}
	// IStatus implementation
	void init() override
	{
		clean();
	}
	unsigned getState() const override;
	void setErrors2(unsigned length, const intptr_t* value) override;
	void setWarnings2(unsigned length, const intptr_t* value) override;
	void setErrors(const intptr_t* value) override;
	void setWarnings(const intptr_t* value) override;
	const intptr_t* getErrors() const override;
	const intptr_t* getWarnings() const override;
	Firebird::IStatus* clone() const override;

	static void clearException(Status* status)
	{
		status->clean();
	}
	static void checkException(Status* status);
	static void setVersionError(Status* status, const char* what, unsigned foundVersion, unsigned requiredVersion);

	// std::exception implementation
	const char* what() const noexcept override;
	const char* where() const noexcept
	{
		return w;
	}

	// Additional handy routines
	iterator beginErrors() const
	{
		return iterator(errors);
	}
	iterator beginWarnings() const
	{
		return iterator(warnings);
	}
	iterator whatHappened() const
	{
		return iterator(errors, found);
	}
};
