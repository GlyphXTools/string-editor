#ifndef COMMANDS_H
#define COMMANDS_H

#include "document.h"

class ICommand
{
public:
	virtual void execute(Document* &document) = 0;
	virtual ~ICommand() {}
};

extern ICommand* ParseCommand(std::vector<std::string>::const_iterator& args, const std::vector<std::string>::const_iterator& end);

#endif