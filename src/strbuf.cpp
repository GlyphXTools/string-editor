#include "strbuf.h"
#include "exceptions.h"
using namespace std;

static const size_t  STRING_BUFFER_SIZE = 512*1024;	// 512 kB

const wchar_t* StringBuffer::addString(const wstring& str)
{
	// First, check the index
	wchar_t* src = (wchar_t*)str.c_str();
	Index::const_iterator p = m_index.find(src);
	if (p != m_index.end())
	{
		return p->first;
	}

	Buffer* buffer = (m_buffers.size() == 0) ? NULL : &m_buffers.back();
	if (buffer == NULL || buffer->size - buffer->used < str.length())
	{
		// Allocate new buffer
		Buffer strbuf;
		strbuf.size = STRING_BUFFER_SIZE;
		strbuf.data = new wchar_t[strbuf.size];
		strbuf.used = 0;
		m_starts.push_back( (buffer != NULL) ? m_starts.back() + buffer->used : 0);
		m_buffers.push_back(strbuf);
		buffer = &m_buffers.back();
	}

	// Copy string
	wchar_t* dest = buffer->data + buffer->used;
	wcscpy_s( dest, buffer->size - buffer->used, str.c_str() );

	// Add to index
	Desc desc;
	desc.buffer  = m_buffers.size() - 1;
	desc.offset  = buffer->used;
	m_index.insert(make_pair(dest, desc));

	buffer->used += str.length() + 1;

	return dest;
}

void StringBuffer::read(IFile& input)
{
	// Read string count
	uint32_t leSize;
	if (input.read(&leSize, sizeof leSize) != sizeof leSize)
	{
		throw ReadException();
	}
	unsigned long nStrings = letohl(leSize);

	// Read string offsets
	vector<uint32_t> leOffsets(nStrings + 1);
	unsigned long size = (nStrings + 1) * sizeof(uint32_t);
	if (input.read(&leOffsets[0], size) != size)
	{
		throw ReadException();
	}

	Buffer buffer;
	buffer.size = letohl( (unsigned long)leOffsets.back() );
	buffer.used = buffer.size;
	buffer.data = NULL;

	try
	{
		// Read strings
		buffer.data = new wchar_t[buffer.size];

		unsigned long size = (unsigned long)(buffer.used * sizeof(wchar_t));
		if (input.read(buffer.data, size) != size)
		{
			throw ReadException();
		}

		// Create index
		m_buffers.push_back(buffer);
		m_starts.push_back(0);
		for (size_t i = 0; i < leOffsets.size(); i++)
		{
			Desc desc;
			desc.buffer = 0;
			desc.offset = letohl( (unsigned long)leOffsets[i] );
			m_index.insert(make_pair(buffer.data + desc.offset, desc));
		}
	}
	catch (...)
	{
		delete[] buffer.data;
		throw;
	}
}

void StringBuffer::write(IFile& output) const
{
	uint32_t leSize = htolel((unsigned long)m_index.size());
	if (output.write(&leSize, sizeof leSize) != sizeof leSize)
	{
		throw WriteException();
	}

	//
	// Write string offsets
	//
	int i = 0;
	vector<uint32_t> offsets( m_index.size() + 1 );
	for (Index::const_iterator p = m_index.begin(); p != m_index.end(); p++, i++)
	{
		offsets[i] = htolel((uint32_t)(m_starts[p->second.buffer] + p->second.offset));
	}
	offsets[i] = htolel((uint32_t)m_starts.back() + m_buffers.back().used);	// Size of written buffer
	
	if (output.write(&offsets[0], (unsigned long)(offsets.size() * sizeof(uint32_t))) != offsets.size() * sizeof(uint32_t))
	{
		throw WriteException();
	}

	// Write string buffers
	for (size_t i = 0; i < m_buffers.size(); i++)
	{
		size_t size = m_buffers[i].used * sizeof(wchar_t);
		if (output.write(m_buffers[i].data, (unsigned long)size) != size)
		{
			throw WriteException();
		}
	}
}

uint32_t StringBuffer::getStringOffset(const wchar_t* str) const
{
	if (str != NULL)
	{
		const Desc& desc = m_index.find(str)->second;
		return (uint32_t)(m_starts[desc.buffer] + desc.offset);
	}
	return UINT32_MAX;
}

const wchar_t* StringBuffer::getString(uint32_t offset) const
{
	if (m_starts.size() > 0 && offset < m_starts.back() + m_buffers.back().used)
	{
		size_t i;
		for (i = 0; (i < m_starts.size() - 1) && (offset > m_starts[i+1]); i++)
		{
			offset -= m_starts[i+1];
		}
		return m_buffers[i].data + offset;
	}
	return NULL;
}

void StringBuffer::clear()
{
	for (size_t i = 0; i < m_buffers.size(); i++)
	{
		delete[] m_buffers[i].data;
	}
	m_buffers.clear();
	m_starts.clear();
	m_index.clear();
}

StringBuffer::~StringBuffer()
{
	clear();
}