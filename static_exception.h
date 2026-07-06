#pragma once

#include <exception>

struct static_exception : public std::exception
{
	const char* reason;
	static_exception(const char text[]) : reason(text)
	{
	}
	const char* what() const noexcept override
	{
		return reason;
	}
};
