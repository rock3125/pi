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

#include "lineedit.h"
#include "interpreter.h"
#include "server.h"

//! node.h first - engine.h expects Binding/BindingList/Set to be known
#include "node.h"
#include "engine.h"

#include <iostream>

//////////////////////////////////////////////////////////////////

static LineEditor editor;

//////////////////////////////////////////////////////////////////

//! command line usage - goes to stdout for --help, stderr for a bad option
void Usage(const char* program,FILE* out)
{
	fprintf(out,"PI Prolog Interpreter\n\n");
	fprintf(out,"usage: %s [options] [program.pl]\n\n",program);
	fprintf(out,"  -p, --port <n>       also serve the interpreter over TCP on port <n>\n");
	fprintf(out,"  -b, --bind <addr>    address the server listens on (default 127.0.0.1,\n");
	fprintf(out,"                       '*' for every interface)\n");
	fprintf(out,"  -h, --help           show this help and exit\n");
	fprintf(out,"\n");
	fprintf(out,"With a file argument the program is loaded before the prompt\n");
	fprintf(out,"appears.  Without one the interpreter starts on an empty database.\n");
	fprintf(out,"\n");
	fprintf(out,"At the '>' prompt:\n");
	fprintf(out,"  ?- father(fred,X).    run a query - starts with '?-', ends with '.'\n");
	fprintf(out,"  likes(pete,prolog).   anything else is added to the database\n");
	fprintf(out,"  list [start[-end]]    list the database\n");
	fprintf(out,"  delete <start[-end]>  remove rules from the database\n");
	fprintf(out,"  load <filename>       load a program\n");
	fprintf(out,"  new                   clear the database\n");
	fprintf(out,"  tron / troff          stack tracing on / off (debug build)\n");
	fprintf(out,"  help                  command summary\n");
	fprintf(out,"  exit                  quit (ctrl-d also works)\n");
	fprintf(out,"\n");
	fprintf(out,"The prompt has bash style line editing - up/down for previous\n");
	fprintf(out,"commands, ctrl-w, ctrl-u, ctrl-k, home/end and so on.  The history\n");
	fprintf(out,"is kept in ~/.pi_history between sessions.\n");
	fprintf(out,"\n");
	fprintf(out,"With --port the same commands can be sent over TCP, one per line,\n");
	fprintf(out,"and the answer comes back as text.  The prompt keeps working while\n");
	fprintf(out,"clients are connected, and they all share the one database:\n");
	fprintf(out,"\n");
	fprintf(out,"    %s --port 7071 samples/family.pl\n",program);
	fprintf(out,"    echo '?- father(fred,X).' | nc localhost 7071\n");
	fprintf(out,"\n");
	fprintf(out,"example: %s samples/family.pl\n",program);
};

//! read a whole number argument, returns false if it is not one
static bool ParseInt(const char* text,int& value)
{
	if (text==NULL || text[0]==0)
		return false;

	char* end=NULL;
	long v=strtol(text,&end,10);
	if (end==NULL || *end!=0)
		return false;

	value=int(v);
	return true;
};

int main(int argc,char* argv[])
{
	DataBase database;
	const char* fileToLoad=NULL;

	int port=0;
	std::string bindAddress="127.0.0.1";

	//! command line
	for (int i=1; i<argc; i++)
	{
		std::string arg=argv[i];

		if (arg=="-h" || arg=="--help")
		{
			Usage(argv[0],stdout);
			return 0;
		}

		if (arg=="-p" || arg=="--port")
		{
			if ((i+1)>=argc || !ParseInt(argv[i+1],port) || port<1 || port>65535)
			{
				fprintf(stderr,"%s: --port needs a number between 1 and 65535\n\n",argv[0]);
				Usage(argv[0],stderr);
				return 1;
			}
			i++;
			continue;
		}

		if (arg=="-b" || arg=="--bind")
		{
			if ((i+1)>=argc)
			{
				fprintf(stderr,"%s: --bind needs an address\n\n",argv[0]);
				Usage(argv[0],stderr);
				return 1;
			}
			bindAddress=argv[i+1];
			i++;
			continue;
		}

		if (arg.size()>1 && arg[0]=='-')
		{
			fprintf(stderr,"%s: unrecognised option '%s'\n\n",argv[0],arg.c_str());
			Usage(argv[0],stderr);
			return 1;
		}

		if (fileToLoad!=NULL)
		{
			fprintf(stderr,"%s: only one program can be given on the command line\n\n",argv[0]);
			Usage(argv[0],stderr);
			return 1;
		}
		fileToLoad=argv[i];
	}

	System::GetTimer()->Init();

	#ifdef _DEBUG
	System::printf("Prolog Interpreter Engine (PIE)  [DEBUG]\n");
	#else
	System::printf("Prolog Interpreter Engine (PIE)\n");
	#endif

	if (fileToLoad!=NULL)
	{
		std::lock_guard<std::mutex> guard(EngineLock());
		LoadPrologDataBase(fileToLoad,database);
	}

	//! optional network front end - the prompt below carries on regardless
	PrologServer server;
	if (port!=0)
	{
		if (!server.Start(database,bindAddress,port))
			return 1;
	}

	System::printf("type 'help' for a list of possible commands\n");

	//! previous sessions' commands are available at the prompt
	std::string historyFile=LineEditor::DefaultHistoryFile();
	editor.LoadHistory(historyFile);

	do
	{
		std::string line;

		//! the blank line goes out separately - the prompt itself has to
		//! stay a single line so the editor can redraw it as you type
		System::printf("\n");

		if (!editor.ReadLine(">",line))
		{
			//! end of input (ctrl-d) - treat as exit
			break;
		}

		std::string command=Trim(line);
		if (command.empty())
			continue;

		editor.AddHistory(command);

		//! network clients run through the same lock, so a query from the
		//! prompt and one from a socket never overlap
		bool quit=false;
		{
			std::lock_guard<std::mutex> guard(EngineLock());
			quit = (ExecuteCommand(command,database)==INTERPRETER_QUIT);
		}

		if (quit)
			break;
	}
	while (true);

	editor.SaveHistory(historyFile);

	//! disconnect everybody before the database goes away
	server.Stop();

	//! release the engine stack, the bindings it handed out and the
	//! database - so a clean exit really is clean
	Engine::ResetStack();
	database.Clear();

	return 0;
};
