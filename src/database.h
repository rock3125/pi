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

// fwd.
class Node;

typedef std::vector<int> IntList;

//////////////////////////////////////////////////////////////////

class DataBase
{
public:
	DataBase(void);
	~DataBase(void);

	void Clear(void);

	std::vector<int> Get(size_t name,size_t arity);
	std::vector<Node*> Get(size_t index);
	Node* GetNode(size_t index1,size_t index2);

	void Add(std::vector<Node*> node);

	size_t Size(void) const;

	bool Empty(void) const;

	std::vector<Node*>& operator[](size_t index);

	// delete a range of rules from the database
	void Delete(int start,int end);

private:
	void AddToLookup(std::vector<Node*>& node);
	void AddToLookup(size_t index,std::vector<Node*>& node);

private:
	enum
	{
		NAME_MULTIPLIER = 1000
	};

	std::vector<std::vector<Node*> >	nodes;
	std::map<size_t,IntList> lookup;
};

