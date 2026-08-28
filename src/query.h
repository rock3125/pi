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

	//! unify two terms already on the stack, the way "=" does.  any
	//! variable it instantiates is also published as a binding, which is
	//! what the enclosing AND uses to substitute later goals
	bool Unify(int a,int b,BindingList& bindings);

	//! unify two list terms - either side may be a [H|T] pattern
	bool UnifyLists(int a,int b,BindingList& bindings);

	//! are two terms structurally identical, the way "==" asks?  resolves
	//! variables but binds nothing - two distinct free variables are not
	//! identical, however they are named
	bool Identical(int a,int b);

	//! helper for Identical: compare a [H|T] pattern against the elements
	//! of a plain list from element "from" onward
	bool IdenticalToListTail(int headTail,int list,size_t from);

	//! copy a term to the top of the stack and return its size.  free
	//! variables are copied as aliases of the original, so binding the
	//! copy still reaches the variable the caller can see
	int CopyTerm(int index);

	//! build the tail of a list - [b,c] out of [a,b,c] - on the stack,
	//! so that a [H|T] pattern has a real list to give to T
	int ListTail(int listIndex);

	//! copy the term at index to the top of the stack with every bound
	//! variable replaced by its value, so the copy stays meaningful after
	//! the bindings that produced it are undone.  a variable that is
	//! still free is looked up by name in the body solution bs - that is
	//! how the R of append's [H|R] head gets what the recursion published
	int Materialize(int index,BindingList& bs);

	//! recursive worker for Materialize - appends the copy and returns
	//! how many stack slots it wrote
	int MaterializeNode(int index,BindingList& bs,int depth);

	//! find what a body solution says a variable of this name is
	int LookupByName(size_t name,BindingList& bs);

	int queryStart;

	std::vector<Node*>	query;
	DataBase&			database;
};
