#include <iostream>
#include <fstream>
#include "application.h"
#include "commands.h"
#include "exceptions.h"

using namespace std;

class CommandList
{
	Document*			m_document;
	vector<ICommand*>	m_commands;

	static void showHelp()
	{
		cout <<
            "Empire at War String Editor, Copyright (C) 2008 Mike Lankamp\n\n"
			"Usage: StringEditor <command> [... <command>]\n"
			"Usage: StringEditor @<script-file>\n\n"
			"The commands list contains commands that are executed in sequence.\n"
			"Alternatively, you can use the second usage to specify a script file to read\n"
			"and execute. Script files can contain the same commands as the command-line.\n\n"
			"Note that unless you use the 'save' command, all changes will be lost.\n\n"
			"A typical command list might look like:\n"
			"open MyText.vdf import union MasterTextFile.dat export Final.dat\n\n"
			"Commands:\n"
			"---------\n\n"
			"new <type> <lang>             Creates a file. Type can be 'index' or 'name'\n"
			"                              Uses the specified language as initial language.\n"
			"open <file>                   Opens an existing VDF file.\n"
			"import <method> <lang> <file> Imports a DAT file. Method can be 'intersect',\n"
			"                              'union', 'difference' or 'union_overwrite' for.\n"
			"                              Name-Indexed files and 'overwrite' or 'append'.\n"
			"                              for Index-Indexed files.\n"
			"                              Lang is the language code of the language to put\n"
			"                              the imported strings in.\n"
			"export <lang> <file>          Exports DAT file. Lang is the language code of\n"
			"                              the language that will be exported.\n"
			"languages                     If no document is open it prints all supported\n"
			"                              languages, with their language codes. Otherwise,\n"
			"                              it prints the languages of the latest version.\n"
			;
	}

public:
	bool parse(const vector<string>& tokens)
	{
		vector<string>::const_iterator cur = tokens.begin();
		while (cur != tokens.end())
		{
			ICommand* command = ParseCommand(cur, tokens.end());
			if (command == NULL)
			{
				showHelp();
				return false;
			}
			m_commands.push_back(command);
		}
		return true;
	}

	void execute()
	{
		for (vector<ICommand*>::iterator p = m_commands.begin(); p != m_commands.end(); p++)
		{
			(*p)->execute(m_document);
		}
	}

	CommandList()
	{
		m_document = NULL;
	}

	~CommandList()
	{
		for (vector<ICommand*>::iterator p = m_commands.begin(); p != m_commands.end(); p++)
		{
			delete *p;
		}
		delete m_document;
	}
};

static int RunConsole(const vector<string>& args)
{
	vector<string> arguments;

	// Create command line from file or arguments
	if (args[1][0] == '@')
	{
		// Load the script file
		ifstream input(args[1].substr(1).c_str(), ios::binary);
		input.seekg(0, ios::end);
		size_t size = input.tellg();
		input.seekg(ios::beg);

		char* buf = new char[size + 1]; buf[size] = '\0';
		input.read(buf, (streamsize)size);
		const char* delim = " \t\r\n\f\v";
		char* context = NULL;
		char* tok = strtok_s(buf, delim, &context);
		while (tok != NULL)
		{
			arguments.push_back(tok);
			tok = strtok_s(NULL, delim, &context);
		}
		delete[] buf;

		for (vector<string>::iterator i = arguments.begin(); i != arguments.end(); i++)
		{
			string& arg = *i;
			if (arg[0] == '"')
			{
				arg = arg.substr(1);
				while (i + 1 != arguments.end() && arg[arg.size() - 1] != '"')
				{
					arg = arg + " " + *(i+1);
					arguments.erase(i+1);
				}

				if (arg[arg.size() - 1] == '"')
				{
					arg = arg.substr(0, arg.size() - 1);
				}
			}
		}
	}
	else for (size_t i = 1; i < args.size(); i++)
	{
		arguments.push_back(args[i]);
	}

	CommandList commands;
	try
	{
		if (commands.parse(arguments))
		{
			commands.execute();
		}
	}
	catch (ParseException& e)
	{
		cerr << "Error: " << e.what() << endl;
	}

	return 0;
}

static int RunApplication()
{
	#ifdef NDEBUG
	FreeConsole();		// We don't need the console in the Release build
	#endif

	try
	{
		HINSTANCE hInstance = GetModuleHandle(NULL);
		Application application(hInstance);
		return application.run();
	}
	catch (wexception &e)
	{
		MessageBox(NULL, e.what(), NULL, MB_OK);
	}
	catch (exception &e)
	{
		MessageBoxA(NULL, e.what(), NULL, MB_OK);
	}
	return -1;
}

int main(int argc, const char* argv[])
{
	if (argc <= 1)
	{
		return RunApplication();
	}

	try
	{
		vector<string> args;
		for (int i = 0; i < argc; i++)
		{
			args.push_back(argv[i]);
		}
		return RunConsole(args);
	}
	catch (wexception &e)
	{
		wcout << endl << e.what() << endl;
	}
	catch (exception &e)
	{
		cout << endl << e.what() << endl;
	}
	return -1;
}