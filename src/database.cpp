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

#include "database.h"
#include "node.h"

//////////////////////////////////////////////////////////////////

DataBase::DataBase(void)
{
};

DataBase::~DataBase(void)
{
	Clear();
};

Node* DataBase::GetNode(size_t index1,size_t index2)
{
	return nodes[index1][index2];
};

std::vector<int> DataBase::Get(size_t name,size_t arity)
{
	std::vector<int> list;
	size_t key = name * NAME_MULTIPLIER + arity;
	std::map<size_t,IntList>::iterator pos=lookup.find(key);
	if (pos!=lookup.end())
	{
		return pos->second;
	}

	return list;
};

std::vector<Node*> DataBase::Get(size_t index)
{
	return nodes[index];
};

void DataBase::AddToLookup(std::vector<Node*>& node)
{
	size_t index = 0;
	if (node[0]->type==Structure::ST_IMPLIES)
	{
		index = 1;
	}

	size_t key = node[index]->name * NAME_MULTIPLIER + node[index]->arity;
	std::map<size_t,IntList>::iterator pos=lookup.find(key);
	if (pos!=lookup.end())
	{
		pos->second.push_back(nodes.size()-1);
	}
	else
	{
		IntList l;
		l.push_back(nodes.size()-1);
		lookup[key]=l;
	}
};

void DataBase::AddToLookup(size_t nodeIndex,std::vector<Node*>& node)
{
	size_t index = 0;
	if (node[0]->type==Structure::ST_IMPLIES)
	{
		index = 1;
	}

	size_t key = node[index]->name * NAME_MULTIPLIER + node[index]->arity;
	std::map<size_t,IntList>::iterator pos=lookup.find(key);
	if (pos!=lookup.end())
	{
		pos->second.push_back(nodeIndex);
	}
	else
	{
		IntList l;
		l.push_back(nodeIndex);
		lookup[key]=l;
	}
};

void DataBase::Add(std::vector<Node*> node)
{
	nodes.push_back(node);
	AddToLookup(node);
};

size_t DataBase::Size(void) const
{
	return nodes.size();
};

bool DataBase::Empty(void) const
{
	return nodes.empty();
};

std::vector<Node*>& DataBase::operator[](size_t index)
{
	PreCond(index<nodes.size());
	return nodes[index];
};

void DataBase::Clear(void)
{
	//! the database owns its nodes - queries always run against copies
	//! pushed onto the engine stack, so freeing them here is safe.
	//! (the original loop tested and stepped i inside the inner j loop,
	//! which is presumably why it ended up commented out)
	for (size_t i=0; i<nodes.size(); i++)
	{
		for (size_t j=0; j<nodes[i].size(); j++)
		{
			safe_delete(nodes[i][j]);
		}
		nodes[i].clear();
	}
	nodes.clear();
	lookup.clear();
};

void DataBase::Delete(int start,int end)
{
	if (end==-1)
	{
		end=nodes.size() - 1;
	}

	if (end>=int(nodes.size()))
	{
		end=int(nodes.size()) - 1;
	}

	if (start>=0 && start<int(nodes.size()) && end>=start)
	{
		//! release the rules being dropped - the database owns its nodes
		for (int i=start; i<=end; i++)
		{
			for (size_t j=0; j<nodes[i].size(); j++)
			{
				safe_delete(nodes[i][j]);
			}
			nodes[i].clear();
		}

		nodes.erase(nodes.begin()+start,nodes.begin()+end+1);
	}

	// rebuild lookup map
	lookup.clear();
	for (size_t i=0; i<nodes.size(); i++)
	{
		AddToLookup(i,nodes[i]);
	}
};

