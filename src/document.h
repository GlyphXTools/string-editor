#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <map>
#include <string>
#include <vector>
#include <stack>
#include <set>
#include "datetime.h"
#include "stringlist.h"
#include "strbuf.h"

static const unsigned int SF_NEW       = 0x01;
static const unsigned int SF_SAVE_MASK = SF_NEW;

class Document
{
public:
	enum Type
	{
		DT_NAME,
		DT_INDEX,
	};

	enum Method
	{
		AM_NONE = -1,

		// For name-based documents
		AM_UNION,
		AM_UNION_OVERWRITE,
		AM_INTERSECT,
		AM_DIFFERENCE,

		// For index-based documents
		AM_OVERWRITE,
		AM_APPEND,
	};

	struct String
	{
		enum
		{
			SF_POSITION = 1,
			SF_NAME     = 2,
			SF_VALUE    = 4,
			SF_COMMENT  = 8
		};

		int		      m_flags;
		unsigned long m_position;
		std::wstring  m_name;
		std::wstring  m_value;
		std::wstring  m_comment;

		String() : m_flags(SF_POSITION | SF_NAME | SF_VALUE | SF_COMMENT), m_position(0) {}
	};

	struct StringInfo
	{
		unsigned long  m_position;
		const wchar_t* m_name;
		const wchar_t* m_comment;
		unsigned char  m_flags;
		uint64_t       m_modified;

		StringInfo() : m_name(NULL) {}
	};

	struct VersionInfo
	{
		std::wstring  m_author;
		std::wstring  m_notes;
		uint64_t	  m_saved;
		unsigned long m_numLanguages;
		unsigned long m_numStrings;
		unsigned long m_numDifferences;
	};

	const std::map<LANGID,std::wstring>& getPostfixes() const { return m_newPostfixes; }
	void setPostfix(LANGID language, const std::wstring& postfix );

	const std::vector<StringInfo>&     getStrings() const { return m_curVersion->m_strings; }
	const std::vector<const wchar_t*>& getValues()  const { return m_curValues->m_virt; }
	const std::vector<const wchar_t*>& getValues(LANGID language) const;

	std::pair<int, int> getStringLifetime(unsigned int id) const;
	const StringInfo&   getString(unsigned int id, int version = -1) const;
	const wchar_t*      getValue (unsigned int id, int version = -1) const;

	bool hasStringChanged(unsigned int id, int version = -1) const;
	bool hasValueChanged(unsigned int id,  int version = -1) const;

	void   getLanguages(std::set<LANGID>& languages, int version = -1) const;
	void   addLanguage(LANGID language);
	void   changeLanguage(LANGID from, LANGID to);
	void   deleteLanguage(LANGID language);
	bool   setActiveLanguage(LANGID language);
	LANGID getActiveLanguage() const { return m_curLanguage; }

	// These functions are the only way to manually change strings
	unsigned int addString();
	void         setString(unsigned int id, const String& str);
	void         deleteString(unsigned int id);
	void         setPosition(unsigned int id, unsigned int position);

	void getVersions( std::vector<VersionInfo>& versions ) const;
	int  getActiveVersion() const { return (int)(m_curVersion - &m_versions[0]); }
	bool setActiveVersion(int version = -1);
	void saveVersion(const std::wstring& author, const std::wstring& notes);
	void increaseVersion();

	Type getType()    const { return m_type; }
	bool isModified() const;
	bool isValidName(unsigned int id) const;

	// Export current language to this filename
	void exportFile(LANGID language, IFile& output) const;

	void write(IFile& output) const;
	void addStrings(const StringList& strings, Method method);

	Document(Type type, LANGID language);
	Document(IFile& input);

private:
	void checkChanged(unsigned int id);
	void checkChangedAll(unsigned int id);

	struct StringValues
	{
		std::vector<std::wstring>   m_phys;
		std::vector<const wchar_t*> m_virt;
		std::set<size_t>            m_changed;		// List of values that are different from the prevous version
	};

	struct Version : VersionInfo
	{
		std::vector<StringInfo>        m_strings;
		std::map<LANGID, StringValues> m_values;
		std::set<size_t>               diff_strings; // List of m_strings that are different from the previous version
	};

	struct CurrentString
	{
		// Current version cached version of strings
		std::wstring  m_name;
		std::wstring  m_comment;
	};

	// Main data structures
	std::vector<Version>                      m_versions;
	std::vector<CurrentString>                m_strings;
	std::stack<unsigned int>                  m_freelist;
	std::multimap<std::wstring, unsigned int> m_names;
	std::map<LANGID, std::wstring>            m_oldPostfixes;
	std::map<LANGID, std::wstring>            m_newPostfixes;
	StringBuffer                              m_buffer;
	Type                                      m_type;

	// Current language/version
	Version*      m_curVersion;
	LANGID        m_curLanguage;
	StringValues* m_curValues;
};

#endif