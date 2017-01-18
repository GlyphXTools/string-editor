#ifndef STRINGLIST_H
#define STRINGLIST_H

#include <iostream>
#include <string>
#include <vector>
#include "files.h"

class StringList
{
public:
	struct StringInfo
	{
		std::wstring  m_name;
		std::wstring  m_value;
		std::wstring  m_comment;
	};

private:
	struct String : StringInfo
	{
		unsigned long m_crc;
		bool operator < (const String& s) {
			return m_crc < s.m_crc;
		}
	};

public:
	class const_iterator
	{
		friend class StringList;

	public:
		const StringInfo& operator *()  const { return m_strings[m_index]; }
		const StringInfo* operator ->() const { return (&**this); }
		const_iterator& operator ++()    { m_index++; return *this; }
		const_iterator& operator --()	 { m_index--; return *this; }
		const_iterator& operator ++(int) { m_index++; return *this; }
		const_iterator& operator --(int) { m_index--; return *this; }

		bool operator ==(const const_iterator& it) const {
			return &m_strings == &it.m_strings && m_index == it.m_index;
		}
		bool operator !=(const const_iterator& it) const {
			return !(*this == it);
		}

	private:
		const_iterator(const std::vector<String>& strings, std::vector<String>::size_type index)
			: m_strings(strings), m_index(index) {}

		const std::vector<String>&     m_strings;
		std::vector<String>::size_type m_index;
	};

	size_t size() const { return m_strings.size(); }
	void   reserve(size_t newSize);
	void   add(const std::wstring& name, const std::wstring& value, const std::wstring& comment);

	void sort();

	const StringInfo& operator[](size_t i) const { return m_strings[i]; }
	      StringInfo& operator[](size_t i)       { return m_strings[i]; }

	const_iterator find( const std::wstring& name ) const;
	const_iterator begin() const { return const_iterator(m_strings, 0); }
	const_iterator end()   const { return const_iterator(m_strings, m_strings.size()); }

	void write(IFile& file, bool sort = false);
	StringList(IFile& file);
	StringList();

private:
	std::vector<String> m_strings;
	bool                m_sorted;
};

#endif