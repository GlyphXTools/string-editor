#include <algorithm>
#include <vector>

#include "stringlist.h"
#include "utils.h"
#include "crc32.h"
#include "exceptions.h"
using namespace std;

#pragma pack(1)
struct DESC
{
	uint32_t crc;
	uint32_t lenValue;	// In characters
	uint32_t lenName;	// In characters
};
#pragma pack()

struct INDEX
{
	unsigned long crc;
	size_t        index;

	bool operator < (const INDEX& right)
	{
		return crc < right.crc;
	}
};

StringList::const_iterator StringList::find(const wstring& name) const
{
	if (m_sorted)
	{
		// Do a binary search
		string ascii = WideToAnsi(name);
		unsigned long crc = crc32(ascii.c_str(), ascii.length());
		int low = 0, high = (int)m_strings.size() - 1;
		while (high >= low)
		{
			int i = (low + high) / 2;
			if (crc == m_strings[i].m_crc)
			{
				// Found the correct CRC, search all equal CRCs
				while (i > 0 && crc == m_strings[i-1].m_crc) i--;
				for (; i < (int)m_strings.size() && crc == m_strings[i].m_crc; i++)
				{
					if (m_strings[i].m_name == name)
					{
						return const_iterator(m_strings, i);
					}
				}
				break;
			}
			if (crc < m_strings[i].m_crc) high = i - 1;
			else low = i + 1;
		}
	}
	else
	{
		// Do a linear search
		for (size_t i = 0; i < m_strings.size(); i++)
		{
			if (m_strings[i].m_name == name)
			{
				return const_iterator(m_strings, i);
			}
		}
	}
	return end();
}

void StringList::reserve(size_t newSize)
{
	m_strings.reserve(newSize);
}

void StringList::add(const std::wstring& name, const std::wstring& value, const std::wstring& comment)
{
	unsigned long crc = m_strings.empty() ? 0 : m_strings.back().m_crc;

	m_strings.push_back(String());
	String& str = m_strings.back();

	string ascii = WideToAnsi(name);
	str.m_crc     = crc32(ascii.c_str(), ascii.length());
	str.m_name    = name;
	str.m_value   = value;
	str.m_comment = comment;

	if (m_sorted && str.m_crc < crc)
	{
		m_sorted = false;
	}
}

void StringList::sort()
{
	std::sort(m_strings.begin(), m_strings.end());
	m_sorted = true;
}

void StringList::write(IFile& output, bool doSort)
{
	// Write number of strings
	uint32_t leNumStrings = htolel((unsigned long)m_strings.size());
	if (output.write((char*)&leNumStrings, sizeof(uint32_t)) != sizeof(uint32_t))
	{
		throw WriteException();
	}

	// Create strings info
	unsigned long sizeValues = 0;
	unsigned long sizeNames  = 0;

	vector<string> names(m_strings.size());
	vector<DESC>   desc(m_strings.size());
	vector<INDEX>  indices(m_strings.size());
	for (size_t i = 0; i < m_strings.size(); i++)
	{
		names[i] = WideToAnsi(m_strings[i].m_name);
		indices[i].crc   = (unsigned long)crc32(names[i].c_str(), names[i].length());
		indices[i].index = i;
		desc[i].lenValue = letohl((unsigned long)m_strings[i].m_value.length());
		desc[i].lenName  = letohl((unsigned long)names[i].length());
		desc[i].crc      = letohl(indices[i].crc);
		sizeValues += (unsigned long)(m_strings[i].m_value.length() * sizeof(wchar_t));
		sizeNames  += (unsigned long)(names[i].length() * sizeof(char));
	}

	if (doSort)
	{
		// Sort strings info
		std::sort(indices.begin(), indices.end());
	}

	char* data = new char[sizeValues + sizeNames];
	unsigned long offsetValues = 0;
	unsigned long offsetNames  = sizeValues;
	for (size_t i = 0; i < m_strings.size(); i++)
	{
		size_t idx = indices[i].index;
		memcpy(data + offsetValues, m_strings[idx].m_value.c_str(), m_strings[idx].m_value.length() * sizeof(wchar_t));
		memcpy(data + offsetNames, names[idx].c_str(), names[idx].length() * sizeof(char));
		offsetValues += (unsigned long)(m_strings[idx].m_value.length() * sizeof(wchar_t));
		offsetNames  += (unsigned long)(names[idx].length() * sizeof(char));

		// Write strings info
		if (output.write(&desc[idx], sizeof(DESC)) != sizeof(DESC))
		{
			delete[] data;
			throw WriteException();
		}
	}

	// Write raw data
	if (output.write(data, sizeValues + sizeNames) != sizeValues + sizeNames)
	{
		delete[] data;
		throw WriteException();
	}
	delete[] data;
}

StringList::StringList(IFile& input)
{
	// Read number of strings
	uint32_t leNumStrings;
	if (input.read((char*)&leNumStrings, sizeof(uint32_t)) != sizeof(uint32_t))
	{
		throw ReadException();
	}
	unsigned long nStrings = letohl(leNumStrings);

	// Read strings info
	vector<DESC> desc(nStrings);
	if (input.read((char*)&desc[0], nStrings * sizeof(DESC)) != nStrings * sizeof(DESC))
	{
		throw ReadException();
	}

	// Calculate total strings size
	unsigned long sizeValues = 0;
	unsigned long sizeNames  = 0;
	m_sorted = true;
	for (size_t i = 0; i < desc.size(); i++)
	{
		sizeValues += 2 * letohl(desc[i].lenValue);
		sizeNames  += 1 * letohl(desc[i].lenName);
		if (m_sorted && i > 0 && desc[i].crc < desc[i-1].crc)
		{
			m_sorted = false;
		}
	}

	// Read raw data
	char* data = new char[sizeValues + sizeNames];
	if (input.read(data, sizeValues + sizeNames) != sizeValues + sizeNames)
	{
		delete[] data;
		throw ReadException();
	}

	// Convert strings info
	unsigned long offsetValues = 0;
	unsigned long offsetNames  = sizeValues;

	m_strings.resize(nStrings);
	for (unsigned long i = 0; i < nStrings; i++)
	{
		m_strings[i].m_crc   = letohl(desc[i].crc);
		m_strings[i].m_name  = AnsiToWide(string((char*)(data + offsetNames), letohl(desc[i].lenName)));
		m_strings[i].m_value = wstring((wchar_t*)(data + offsetValues), letohl(desc[i].lenValue));
		offsetValues += 2 * letohl(desc[i].lenValue);
		offsetNames  += 1 * letohl(desc[i].lenName);
	}
	delete[] data;
}

StringList::StringList()
{
	m_sorted = true;
}