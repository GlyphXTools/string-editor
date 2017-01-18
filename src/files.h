#ifndef FILES_H
#define FILES_H

#include <string>
#include "types.h"

class IFile
{
public:
	virtual bool          eof() = 0;
	virtual unsigned long size() = 0;
	virtual void          seek(unsigned long offset) = 0;
	virtual unsigned long tell() = 0;
	virtual unsigned long read(void* buffer, unsigned long size) = 0;
	virtual unsigned long write(const void* buffer, unsigned long size) = 0;
};

class PhysicalFile : public IFile
{
private:
	HANDLE        hFile;
	unsigned long m_position;
	unsigned long m_size;

public:
	enum Mode
	{
		WRITE,
		READ,
	};

	bool          eof()                      { return m_position == m_size; }
	unsigned long size()                     { return m_size; }
	unsigned long tell()                     { return m_position; }
	void          seek(unsigned long offset) { m_position = min(offset, m_size); }
	unsigned long read(void* buffer, unsigned long size);
	unsigned long write(const void* buffer, unsigned long size);

	PhysicalFile(const std::wstring& name, Mode mode = READ);
	~PhysicalFile();
};

#endif