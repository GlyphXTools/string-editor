#include "files.h"
#include "exceptions.h"
using namespace std;

unsigned long PhysicalFile::read(void* buffer, unsigned long size)
{
	SetFilePointer(hFile, m_position, NULL, FILE_BEGIN);
	if (!ReadFile(hFile, buffer, size, &size, NULL))
	{
		throw ReadException();
	}
	m_position = min(m_position + size, m_size);
	return size;
}

unsigned long PhysicalFile::write(const void* buffer, unsigned long size)
{
	SetFilePointer(hFile, m_position, NULL, FILE_BEGIN);
	if (!WriteFile(hFile, buffer, size, &size, NULL))
	{
		throw WriteException();
	}
	m_size     = GetFileSize(hFile, NULL);
	m_position = min(m_position + size, m_size);
	return size;
}

PhysicalFile::PhysicalFile(const wstring& filename, Mode mode)
{
	DWORD dwDesiredAccess       = (mode == WRITE) ? GENERIC_WRITE : GENERIC_READ;
	DWORD dwCreationDisposition = (mode == WRITE) ? CREATE_ALWAYS : OPEN_EXISTING;
	hFile = CreateFile(filename.c_str(), dwDesiredAccess, FILE_SHARE_READ, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		DWORD error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
		{
			throw FileNotFoundException(filename);
		}
		if (mode == WRITE)
		{
			throw IOException(LoadString(IDS_ERROR_FILE_CREATE));
		}
		throw IOException(LoadString(IDS_ERROR_FILE_OPEN));
	}
	m_size     = GetFileSize(hFile, NULL);
	m_position = 0;
}

PhysicalFile::~PhysicalFile()
{
	CloseHandle(hFile);
}
