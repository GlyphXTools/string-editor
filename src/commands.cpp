#include "commands.h"
#include "exceptions.h"
#include "utils.h"
using namespace std;

static LANGID ParseLanguage(const string& str)
{
	char* endptr;
	unsigned long value = strtoul(str.c_str(), &endptr, 0);
	if (*endptr == '\0')
	{
		LANGID language = (LANGID)value;

		// If LANGID is smaller than value, xor the cast value to see if we're
		// not discarding bits. If we are, the value is an invalid language.
		if ((value ^ language) == 0)
		{
			set<LANGID> languages;
			GetLanguageList(languages);
			if (languages.find(language) != languages.end())
			{
				return language;
			}
		}
	}
	throw ParseException("Invalid language specifier");
}

//
// Command: new
//
class CommandNew : public ICommand
{
	Document::Type type;
	LANGID         language;

public:
	void execute(Document* &document)
	{
		delete document;
		document = NULL;
		document = new Document(type, language);
	}

	static ICommand* parse(vector<string>::const_iterator& arg, const vector<string>::const_iterator& end)
	{
		Document::Type type;
		LANGID         language;

		if (arg == end) throw ParseException("expected document type");
			 if (_stricmp(arg->c_str(), "index") == 0) type = Document::DT_INDEX;
		else if (_stricmp(arg->c_str(), "name")  == 0) type = Document::DT_NAME;
		else throw ParseException("invalid document type");

		if (++arg == end) throw ParseException("expected document language");
		language = ParseLanguage(*arg++);

		return new CommandNew(type, (LANGID)language);
	}

	CommandNew(Document::Type type, LANGID language)
	{
		this->type     = type;
		this->language = language;
	}
};

//
// Command: open
//
class CommandOpen : public ICommand
{
	wstring filename;

public:
	void execute(Document* &document)
	{
		if (document != NULL)
		{
			delete document;
			document = NULL;
		}
		PhysicalFile input(filename);
		document = new Document(input);
	}

	static ICommand* parse(vector<string>::const_iterator& arg, const vector<string>::const_iterator& end)
	{
		if (arg == end) throw ParseException("expected filename");
		return new CommandOpen(*arg++);
	}

	CommandOpen(const string& filename)
	{
		this->filename = AnsiToWide(filename);
	}
};

//
// Command: import
//
class CommandImport : public ICommand
{
	wstring          filename;
	Document::Method method;
	LANGID           language;

public:
	void execute(Document* &document)
	{
		if (document == NULL)
		{
			throw runtime_error("unable to import; please create or open a document first");
		}

		if (!document->setActiveLanguage(language))
		{
			throw runtime_error("unable to import; specified language does not exist in file");
		}

		if ((document->getType() == Document::DT_INDEX && method != Document::AM_OVERWRITE  && method != Document::AM_APPEND) ||
			(document->getType() == Document::DT_NAME  && method != Document::AM_UNION      && method != Document::AM_UNION_OVERWRITE
													   && method != Document::AM_DIFFERENCE && method != Document::AM_INTERSECT))
		{
			throw runtime_error("unable to import; specified method cannot be used with document type");
		}

		PhysicalFile input(filename);
		StringList strings(input);
		document->addStrings(strings, method);
	}

	static ICommand* parse(vector<string>::const_iterator& arg, const vector<string>::const_iterator& end)
	{
		Document::Method method;
		if (arg == end) throw ParseException("expected import method");
			 if (_stricmp(arg->c_str(), "union")           == 0) method = Document::AM_UNION;
		else if (_stricmp(arg->c_str(), "union_overwrite") == 0) method = Document::AM_UNION_OVERWRITE;
		else if (_stricmp(arg->c_str(), "difference")      == 0) method = Document::AM_DIFFERENCE;
		else if (_stricmp(arg->c_str(), "intersect")       == 0) method = Document::AM_INTERSECT;
		else if (_stricmp(arg->c_str(), "overwrite")       == 0) method = Document::AM_OVERWRITE;
		else if (_stricmp(arg->c_str(), "append")          == 0) method = Document::AM_APPEND;
		else throw ParseException("invalid import method");
		
		if (++arg == end) throw ParseException("expected import language");
		LANGID language = ParseLanguage(*arg);

		if (++arg == end) throw ParseException("expected filename");
		return new CommandImport(method, language, *arg++);
	}

	CommandImport(Document::Method method, LANGID language, const string& filename)
	{
		this->filename = AnsiToWide(filename);
		this->language = language;
		this->method   = method;
	}
};

class CommandExport : public ICommand
{
	wstring filename;
	LANGID  language;

public:
	void execute(Document* &document)
	{
		if (document == NULL)
		{
			throw runtime_error("unable to export; please create or open a document first");
		}

		if (!document->setActiveLanguage(language))
		{
			throw runtime_error("unable to export; specified language does not exist in file");
		}

		// Check if all names are valid
		const vector<Document::StringInfo>& strings = document->getStrings();
		for (size_t i = 0; i < strings.size(); i++)
		{
			if (strings[i].m_name != NULL && !document->isValidName((unsigned int)i))
			{
				throw runtime_error("unable to export; this file contains invalid names");
			}
		}

		PhysicalFile output(filename, PhysicalFile::WRITE);
		document->exportFile(language, output);
	}

	static ICommand* parse(vector<string>::const_iterator& arg, const vector<string>::const_iterator& end)
	{
		if (arg == end) throw ParseException("expected export language");
		LANGID language = ParseLanguage(*arg++);

		if (arg == end) throw ParseException("expected filename");
		return new CommandExport(language, *arg++);
	}

	CommandExport(LANGID language, const string& filename)
	{
		this->filename = AnsiToWide(filename);
		this->language = language;
	}
};

//
// Command: languages
//
class CommandLanguages : public ICommand
{
public:
	void execute(Document* &document)
	{
		set<LANGID> languages;
		if (document == NULL)
			GetLanguageList(languages);
		else
			document->getLanguages(languages);

		// Get language names and sort
		map<wstring,LANGID> names;
		for (set<LANGID>::const_iterator p = languages.begin(); p != languages.end(); p++)
		{
			names.insert(make_pair(GetLanguageName(*p), *p));
		}
		
		// Print language codes and names
		for (map<wstring,LANGID>::const_iterator p = names.begin(); p != names.end(); p++)
		{
			printf("%5d %ls\n", p->second, p->first.c_str());
		}
	}

	static ICommand* parse(vector<string>::const_iterator& arg, const vector<string>::const_iterator& end)
	{
		return new CommandLanguages();
	}
};

//
// Commands list
//

struct COMMAND
{
	char*       name;
	ICommand* (*parse)(vector<string>::const_iterator& arg, const vector<string>::const_iterator& end);
};

//
// IMPORTANT: ALWAYS make sure this array is sorted on the command name (for the binary search)
//
static const int N_COMMANDS = 5;
COMMAND Commands[N_COMMANDS] = {
	{"export",		CommandExport::parse},
	{"import",		CommandImport::parse},
	{"languages",	CommandLanguages::parse},
	{"new",			CommandNew::parse},
	{"open",		CommandOpen::parse},
};

ICommand* ParseCommand(vector<string>::const_iterator& arg, const vector<string>::const_iterator& end)
{
	int low = 0, high = N_COMMANDS - 1;
	while (high >= low)
	{
		int mid = (low + high) / 2;
		int cmp = strcmp(arg->c_str(), Commands[mid].name);
		if (cmp == 0)
		{
			try
			{
				return Commands[mid].parse(++arg, end);
			}
			catch (ParseException& e)
			{
				throw ParseException( "[" + string(Commands[mid].name) + "]: " + e.what() );
			}
		}
		if (cmp < 0) high = mid - 1;
		else		 low  = mid + 1;
	}
	return NULL;
}