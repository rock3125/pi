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
#pragma once

#include <mutex>
#include <string>

#include "database.h"

//////////////////////////////////////////////////////////////////
//
// One place where a line of input is turned into an answer, shared by the
// interactive prompt and by any network client.
//
// Everything these produce is written through System::printf, so the caller
// decides where the text goes by installing an IOWriter first.
//
//////////////////////////////////////////////////////////////////

enum InterpreterResult
{
	//! the command ran (successfully or not - the text says which)
	INTERPRETER_OK = 0,

	//! the caller asked to finish: "exit", "quit" or "bye"
	INTERPRETER_QUIT
};

//! run a single line of input against the database
InterpreterResult ExecuteCommand(const std::string& command,DataBase& database);

//! read a program file into the database
bool LoadPrologDataBase(const std::string& fname,DataBase& database);

//! strip leading and trailing whitespace
std::string Trim(const std::string& str);

//! print the commands that are understood
void PrintCommandHelp(void);

//////////////////////////////////////////////////////////////////

//! The engine keeps its state in globals - the node stack, the string and
//! variable tables, the trace flag, the output writer.  None of that is per
//! query, so only one command may be in flight at a time no matter how many
//! clients are connected.  Take this around every call to ExecuteCommand or
//! LoadPrologDataBase.
std::mutex& EngineLock(void);
