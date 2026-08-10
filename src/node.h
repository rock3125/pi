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

#include "structure.h"
#include "binding.h"

//////////////////////////////////////////////////////////////////

//! validity of a link
#define Valid(x) ((x)>=0)

#define SizeOfNode(index,node) ((node)[(index)]->size)

//////////////////////////////////////////////////////////////////

class Node
{
public:
	Node(void);
	~Node(void);
	Node(const Node&);
	const Node& operator=(const Node&);

	//! convert a parse structure to a node list
	static std::vector<Node*> StructureToNodeList(Structure* s);

	//! convert a list of nodes to a string (i.e. print)
	static std::string ToString(std::vector<Node*>& n);
	static std::string ToString(int pc);

	// don't use - used by the two above
	static std::string ToString(int i,std::vector<Node*>& n);

	//! static helper routine
	static int GetForwardValue(int index,std::vector<Node*>& nlist);

	//! type of structure
	Structure::Predicate type;

	//! name of variable of structure
	size_t			name;

	//! structures have arity
	size_t			arity;

	//! parent reference
	int				parent;

	//! next solution reference
	int				next;

	//! stack size of this clause
	int				size;

	//! values (depends on tag)
	float					f;
	int						i;
	bool					b;

	//! forward and backward unification
	//! process results (bindings)
	int				fu;
};



