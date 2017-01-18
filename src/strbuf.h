#ifndef STRBUF_H
#define STRBUF_H

#include <map>
#include <string>
#include <vector>
#include "files.h"
#include "utils.h"

class StringBuffer
{
public:
	uint32_t       getStringOffset(const wchar_t* str) const;
	const wchar_t* getString(uint32_t offset) const;
	void           write(IFile& output) const;

	void           read(IFile& input);
	const wchar_t* addString(const std::wstring& str);
	void           clear();

	~StringBuffer();

private:
	struct Desc
	{
		size_t buffer;
		size_t offset;
	};

	struct Buffer
	{
		wchar_t* data;
		size_t   size;
		size_t   used;
	};

	typedef std::map<const wchar_t*, Desc, wcsless> Index;

	Index			    m_index;
	std::vector<Buffer> m_buffers;
	std::vector<size_t> m_starts;
};

#endif