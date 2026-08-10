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

//////////////////////////////////////////////////////////////////

#include "node.h"
#include "engine.h"
#include "database.h"

//////////////////////////////////////////////////////////////////

class Query
{
public:
	Query(std::vector<Node*> query,DataBase& database,int level=0);
	~Query(void);

	//! execute the query
	bool ExecuteQuery(void);

	//! some arbitrary value to stop overflows from occuring
	enum
	{
		MAX_PC = 1500000
	};

private:

	//! get clauses that match the current stack structure
	std::vector<int> GetMatchingClauses(int pc);

	//! execute a sub-query on the stack
	bool Execute(int level,int pc,Set& bindings,bool& cut);

	//! execute a query with bindings
	bool ExecuteQueryRecursive(int level,Set& s,bool& cut);

	//! can two lists bind with each other?
	bool ListsCanBind(int stackIndex,int dbIndex,int dbLineIndex);

	//! can two simple nodes bind (not for lists)
	bool CanBind(Node* n1,Node* n2);

	int queryStart;

	std::vector<Node*>	query;
	DataBase&			database;
};
