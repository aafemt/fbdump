#include <stdexcept>
#include <winnls.h>

#include "tstring.h"

std::string to_string(const std::wstring& origin)
{
	std::string result;
	result.reserve(origin.size()); // Most probably case for ASCII

	for (std::wstring::const_iterator itr = origin.begin(); itr < origin.end(); ++itr)
	{
		char32_t c = *itr;
		if constexpr (sizeof(wchar_t) == sizeof(char16_t))
		{
			// Convert UTF-16 into UCS-32
			if (c >= HIGH_SURROGATE_START && c <= HIGH_SURROGATE_END)
			{
				// Lead surrogate
				char32_t d;
				constexpr char32_t offset = (HIGH_SURROGATE_START << 10) + LOW_SURROGATE_START - 0x10000;
				if (++itr == origin.end() || ((d = *itr) & 0xfc00) != LOW_SURROGATE_START)
				{
					throw std::range_error("Missing trail surrogate");
				}
				c = (c << 10) + d - offset;
			}
			else if (c >= LOW_SURROGATE_START && c <= LOW_SURROGATE_END)
			{
				throw std::range_error("Missing lead surrogate");
			}
		}
		if (c > 0x10FFFF)
		{
			throw std::out_of_range("Unicode character out of range");
		}
		// Convert UCS-32 to UTF-8
		char32_t res = (c & 0x3f)
				| ((c << 2) & 0x3f00)
				| ((c << 4) & 0x3f0000)
				| ((c << 6) & 0x3f000000);
		if ((c & ~0x7f) == 0)
		{
			result.push_back(c);
		}
		else if ((c & ~0x7ff) == 0)
		{
			res |= 0xc080;
			result.push_back(res >> 8);
			result.push_back(res & 0xff);
		}
		else if ((c & ~0xffff) == 0)
		{
			res |= 0xe08080;
			result.push_back((res >> 16) & 0xff);
			result.push_back((res >> 8) & 0xff);
			result.push_back(res & 0xff);
		}
		else
		{
			res |= 0xf0808080;
			result.push_back((res >> 24) & 0xff);
			result.push_back((res >> 16) & 0xff);
			result.push_back((res >> 8) & 0xff);
			result.push_back(res & 0xff);
		}
	}
	return result;
}

const std::string& to_string(const std::string& origin)
{
	return origin;
}

