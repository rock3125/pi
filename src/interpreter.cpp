/*

    PI Prolog Interpreter
    Copyright (C) 2004  Rock de Vocht

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

*/

#include "system.h"

#include "interpreter.h"
#include "parser.h"
#include "query.h"

#include <cctype>

//////////////////////////////////////////////////////////////////

//! the parser holds its own buffer and error state between calls, so it is
//! shared state too and lives behind the same lock as everything else
static PrologParser p;

//////////////////////////////////////////////////////////////////

std::mutex& EngineLock(void)
{
	static std::mutex lock;
	return lock;
};

std::string Trim(const std::string& str)
{
	size_t start=0;
	size_t end=str.size();

	while (start<end && isspace((unsigned char)str[start]))
		start++;

	while (end>start && isspace((unsigned char)str[end-1]))
		end--;

	return str.substr(start,end-start);
};

//////////////////////////////////////////////////////////////////

bool LoadPrologDataBase(const std::string& fname,DataBase& database)
{
	double t1,t2;

	std::string databaseStr;
	if (!System::LoadTextFile(fname.c_str(),databaseStr))
	{
		System::printf("could not load database file %s\n",fname.c_str());
		return false;
	}

	//! compile database first
	System::GetTimer()->Update();
	t1=System::GetTimer()->CurrentTime();

	std::string errStr;
	std::vector<Structure*> _database=p.ParseStatements(databaseStr);
	if (p.GetError(errStr))
	{
		System::printf("error parsing statements: %s\n",errStr.c_str());
		return false;
	};

	//! convert complete database to a set of nodes
	for (size_t i=0; i<_database.size(); i++)
	{
		database.Add(Node::StructureToNodeList(_database[i]));
		safe_delete(_database[i]);
	};
	_database.clear();

	System::GetTimer()->Update();
	t2=System::GetTimer()->CurrentTime();

	//! output compilation time
	System::printf("compiled database (%2.8f seconds)\n",(t2-t1));
	return true;
};

//! print the rules currently held in the database
static void List(DataBase& database,int start,int end)
{
	if (database.Empty())
	{
		System::printf("database empty\n");
	}
	else
	{
		for (size_t i=0; i<database.Size(); i++)
		{
			int index = int(i)+1;
			if ((end==-1 && index>=start) || (index>=start && index<=end))
			{
				System::printf("%05d    %s.\n",index,Node::ToString(database[i]).c_str());
			}
		}
	}
};

//! run a query against the database and report the result
static void RunQuery(const std::string& command,DataBase& database)
{
	double t1,t2;

	//! note: an empty database is not an error - plenty of queries
	//! ("?- 3 > 2." and friends) do not consult it at all

	Structure* _query=p.ParseQuery(command);

	std::string errStr;
	if (p.GetError(errStr))
	{
		System::printf("error parsing query: %s\n",errStr.c_str());
		safe_delete(_query);
		return;
	}

	std::vector<Node*> query=Node::StructureToNodeList(_query);
	safe_delete(_query);

	//! start query
	Engine::ResetStack();
	Query q(query,database);

	System::GetTimer()->Update();
	t1=System::GetTimer()->CurrentTime();
	t2=t1;

	if (q.ExecuteQuery())
	{
		System::GetTimer()->Update();
		t2=System::GetTimer()->CurrentTime();

		//! the engine already appends "yes" itself when a successful query
		//! produced no variable bindings
		System::printf("%s\n",Engine::GetOutputString().c_str());
	}
	else
	{
		System::GetTimer()->Update();
		t2=System::GetTimer()->CurrentTime();

		System::printf("no\n");
	}
	System::printf("(execution time %2.8f seconds)\n",(t2-t1));

	for (size_t i=0; i<query.size(); i++)
	{
		safe_delete(query[i]);
	}
	query.clear();
};

//! add one or more statements typed at the prompt to the database
static void AddStatements(const std::string& command,DataBase& database)
{
	std::vector<Structure*> newStats=p.ParseStatements(command);

	std::string errStr;
	if (p.GetError(errStr))
	{
		System::printf("error parsing statement(s): %s\n",errStr.c_str());
	}
	else
	{
		for (size_t i=0; i<newStats.size(); i++)
		{
			database.Add(Node::StructureToNodeList(newStats[i]));
			safe_delete(newStats[i]);
		}
	}
	newStats.clear();
};

void PrintCommandHelp(void)
{
	System::printf("allowed commands: exit, tron, troff, help, new,\n");
	System::printf("                  list [start[-end]], delete <start[-end]>,\n");
	System::printf("                  load <filename>\n");
	System::printf("queries start with '?-' and end with '.'   e.g.  ?- father(fred,X).\n");
	System::printf("anything else is added to the database     e.g.  likes(pete,prolog).\n");
};

//////////////////////////////////////////////////////////////////

InterpreterResult ExecuteCommand(const std::string& line,DataBase& database)
{
	std::string command=Trim(line);
	if (command.empty())
		return INTERPRETER_OK;

	if (command=="quit" ||
		command=="exit" ||
		command=="bye")
	{
		return INTERPRETER_QUIT;
	}

	if (command=="help")
	{
		PrintCommandHelp();
		return INTERPRETER_OK;
	}

	if (command=="tron" || command=="trace")
	{
		Engine::SetTron(true);
		System::printf("stack trace on\n");
		return INTERPRETER_OK;
	}

	if (command=="troff")
	{
		Engine::SetTron(false);
		System::printf("stack trace off\n");
		return INTERPRETER_OK;
	}

	if (command=="new")
	{
		database.Clear();
		System::printf("program cleared\n");
		return INTERPRETER_OK;
	}

	if (command.size()>=4 && command.substr(0,4)=="list")
	{
		std::string errStr;
		Structure* s=p.ParseCommand(command);
		if (p.GetError(errStr))
		{
			System::printf("%s\n",errStr.c_str());
		}
		else if (s!=NULL)
		{
			List(database,s->i,(int)s->f);
		}
		safe_delete(s);
		return INTERPRETER_OK;
	}

	if (command.size()>=6 && command.substr(0,6)=="delete")
	{
		std::string errStr;
		Structure* s=p.ParseCommand(command);
		if (p.GetError(errStr))
		{
			System::printf("%s\n",errStr.c_str());
		}
		else if (s!=NULL)
		{
			database.Delete(s->i-1,(int)s->f>0?(int)s->f-1:(int)s->f);
		}
		safe_delete(s);
		return INTERPRETER_OK;
	}

	if (command.size()>=4 && command.substr(0,4)=="load")
	{
		std::string fname=Trim(command.substr(4));
		if (fname.empty())
		{
			System::printf("usage: load <filename>\n");
		}
		else
		{
			LoadPrologDataBase(fname,database);
		}
		return INTERPRETER_OK;
	}

	if (command[0]=='?')
	{
		RunQuery(command,database);
	}
	else
	{
		AddStatements(command,database);
	}

	return INTERPRETER_OK;
};
