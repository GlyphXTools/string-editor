#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <string>
#include <stdexcept>
#include "utils.h"
#include "resource.h"

class wexception : public std::exception
{
	std::wstring message;
public:
	const wchar_t* what() { return message.c_str(); }
	wexception(const wchar_t* _message) : message(_message) {}
	wexception(const std::wstring& _message) : message(_message) {}
};

class wruntime_error : public wexception
{
public:
	wruntime_error(const std::wstring& message) : wexception(message) {}
};

class IOException : public wruntime_error
{
public:
	IOException(const std::wstring& message) : wruntime_error(message) {}
	~IOException() {}
};

class ReadException : public IOException
{
public:
	ReadException() : IOException(LoadString(IDS_ERROR_FILE_READ)) {}
	~ReadException() {}
};

class WriteException : public IOException
{
public:
	WriteException() : IOException(LoadString(IDS_ERROR_FILE_WRITE)) {}
	~WriteException() {}
};

class FileNotFoundException : public IOException
{
public:
	FileNotFoundException(const std::wstring& file)
		: IOException(LoadString(IDS_ERROR_FILE_FIND, file.c_str())) {}
};

class BadFileException : public IOException
{
public:
	BadFileException()
		: IOException(LoadString(IDS_ERROR_FILE_CORRUPT)) {}
};

class UnsupportedVersionException : public wruntime_error
{
public:
	UnsupportedVersionException()
        : wruntime_error(LoadString(IDS_ERROR_FILE_VERSION)) {}
};

class ParseException : public std::runtime_error
{
public:
	ParseException(const std::string& message)
        : runtime_error(message) {}
};

#endif