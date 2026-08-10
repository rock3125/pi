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

#include "node.h"
#include "engine.h"

//////////////////////////////////////////////////////////////////

Node::Node(void)
{
	type=Structure::ST_NONE;
	arity=0;
	name=0;
	parent=-1;
	next=-1;
	fu=-1;
	size=0;

	f=0.0f;
	i=0;
	b=false;
};

Node::Node(const Node& n)
{
	operator=(n);
};

const Node& Node::operator=(const Node& n)
{
	type=n.type;
	arity=n.arity;
	name=n.name;
	parent=n.parent;
	next=n.next;
	size=n.size;
	fu=n.fu;
	f=n.f;
	i=n.i;
	b=n.b;
	return *this;
};

Node::~Node(void)
{
};

int Node::GetForwardValue(int index,std::vector<Node*>& nlist)
{
	while (Valid(nlist[index]->fu))
	{
		index=nlist[index]->fu;
	}
	return index;
};

std::string Node::ToString(std::vector<Node*>& n)
{
	return Node::ToString(0,n);
};

std::string Node::ToString(int pc)
{
	return ToString(pc,Engine::GetStack());
};

std::string Node::ToString(int i,std::vector<Node*>& nlist)
{
	std::string str;
	Node* n=nlist[i];

	switch (n->type)
	{
		case Structure::ST_IMPLIES:
		{
			int index=i+1;
			str=str+Node::ToString(index,nlist);
			str=str+" :- ";
			index+=SizeOfNode(index,nlist);
			str=str+Node::ToString(index,nlist);
			break;
		};
		case Structure::ST_AND:
		{
			int index=i+1;
			str=str+Node::ToString(index,nlist);
			str=str+", ";
			index+=SizeOfNode(index,nlist);
			str=str+Node::ToString(index,nlist);
			break;
		};
		case Structure::ST_OR:
		{
			int index=i+1;
			str=str+Node::ToString(index,nlist);
			str=str+" ; ";
			index+=SizeOfNode(index,nlist);
			str=str+Node::ToString(index,nlist);
			break;
		};
		case Structure::ST_STRUCTURE:
		{
			str=str+Structure::GetString(n->name);
			if (n->arity>0)
			{
				str=str+"(";
				int index=i+1;
				for (size_t j=0; j<n->arity; j++)
				{
					str=str+Node::ToString(index,nlist);
					if ((j+1)<n->arity)
						str=str+",";
					index+=SizeOfNode(index,nlist);
				}
				str=str+")";
			}
			break;
		};
		case Structure::ST_FLOAT:
		{
			str=str+System::Float2Str(n->f);
			break;
		};
		case Structure::ST_STRING:
		{
			str=str+"\""+Structure::GetString(n->name)+"\"";
			break;
		};
		case Structure::ST_BOOL:
		{
			if (n->b)
				str=str+"true";
			else
				str=str+"false";
			break;
		};
		case Structure::ST_INT:
		{
			str=str+System::Int2Str(n->i);
			break;
		};
		case Structure::ST_VAR:
		{
			if (nlist[i]->name==0)
			{
				str=str+"_";
			}
			else
			{
				if (Valid(nlist[i]->fu))
				{
					str=str+Node::ToString(GetForwardValue(nlist[i]->fu,nlist),nlist);
				}
				else
				{
					str=str+Structure::GetString(nlist[i]->name);
				}
			}
			break;
		};
		case Structure::ST_EQUAL:
		{
			int index=i+1;
			str=str+Node::ToString(index,nlist);
			str=str+"==";
			index+=SizeOfNode(index,nlist);
			str=str+Node::ToString(index,nlist);
			break;
		};
		case Structure::ST_NOTEQUAL:
		{
			int index=i+1;
			str=str+Node::ToString(index,nlist);
			str=str+"!=";
			index+=SizeOfNode(index,nlist);
			str=str+Node::ToString(index,nlist);
			break;
		};
		case Structure::ST_LESS:
		{
			int index=i+1;
			str=str+Node::ToString(index,nlist);
			str=str+"<";
			index+=SizeOfNode(index,nlist);
			str=str+Node::ToString(index,nlist);
			break;
		};
		case Structure::ST_LESSTHAN:
		{
			int index=i+1;
			str=str+Node::ToString(index,nlist);
			str=str+"<=";
			index+=SizeOfNode(index,nlist);
			str=str+Node::ToString(index,nlist);
			break;
		};
		case Structure::ST_GREATER:
		{
			int index=i+1;
			str=str+Node::ToString(index,nlist);
			str=str+">";
			index+=SizeOfNode(index,nlist);
			str=str+Node::ToString(index,nlist);
			break;
		};
		case Structure::ST_GREATERTHAN:
		{
			int index=i+1;
			str=str+Node::ToString(index,nlist);
			str=str+">=";
			index+=SizeOfNode(index,nlist);
			str=str+Node::ToString(index,nlist);
			break;
		};
		case Structure::ST_ASSIGN:
		{
			int index=i+1;
			str=str+Node::ToString(index,nlist);
			str=str+"=";
			index+=SizeOfNode(index,nlist);
			str=str+Node::ToString(index,nlist);
			break;
		};
		case Structure::ST_PLUS:
		{
			int index=i+1;
			str=str+Node::ToString(index,nlist);
			str=str+"+";
			index+=SizeOfNode(index,nlist);
			str=str+Node::ToString(index,nlist);
			break;
		};
		case Structure::ST_TIMES:
		{
			int index=i+1;
			str=str+Node::ToString(index,nlist);
			str=str+"*";
			index+=SizeOfNode(index,nlist);
			str=str+Node::ToString(index,nlist);
			break;
		};
		case Structure::ST_DIVIDE:
		{
			int index=i+1;
			str=str+Node::ToString(index,nlist);
			str=str+"\\";
			index+=SizeOfNode(index,nlist);
			str=str+Node::ToString(index,nlist);
			break;
		};
		case Structure::ST_MINUS:
		{
			int index=i+1;
			str=str+Node::ToString(index,nlist);
			str=str+"-";
			index+=SizeOfNode(index,nlist);
			str=str+Node::ToString(index,nlist);
			break;
		};
		case Structure::ST_NOT:
		{
			str=str+"not("+Node::ToString(i+1,nlist)+")";
			break;
		}
		case Structure::ST_CUT:
		{
			str=str+"!";
			break;
		}
		case Structure::ST_FAIL:
		{
			str=str+"fail";
			break;
		}
		case Structure::ST_LIST:
		{
			str=str+"[";
			// for each element in the list
			int index=i+1;
			for (size_t j=0; j<n->arity; j++)
			{
				str=str+ToString(index,nlist);
				if ((j+1)<n->arity)
					str=str+" ,";
				index+=SizeOfNode(index,nlist);
			}
			str=str+" ]";
			break;
		}
		case Structure::ST_HEADTAIL:
		{
			str=str+"[";
			// for each element in the list
			int index=i+1;
			for (size_t j=0; j<n->arity; j++)
			{
				str=str+ToString(index,nlist);
				if ((j+1)<n->arity)
					str=str+" |";
				index+=SizeOfNode(index,nlist);
			}
			str=str+" ]";
			break;
		}
		default:
		{
			PostCond("unknown tag"==NULL);
			break;
		}
	}
	return str;
};

std::vector<Node*> Node::StructureToNodeList(Structure* s)
{
	std::vector<Node*> l;

	PreCond(s!=NULL);
	switch (s->tag)
	{
		case Structure::ST_STRUCTURE:
		{
			Node* n=new Node();
			n->type=(Structure::Predicate)s->tag;
			n->name=s->name;
			n->arity=s->structures.size();

			l.push_back(n);

			for (size_t i=0; i<s->structures.size(); i++)
			{
				std::vector<Node*> l2=StructureToNodeList(s->structures[i]);
				for (size_t j=0; j<l2.size(); j++)
				{
					l.push_back(l2[j]);
				}
			}
			n->size=l.size();
			break;
		};
		case Structure::ST_FLOAT:
		{
			Node* n=new Node();
			n->size=1;
			n->type=(Structure::Predicate)s->tag;
			n->f=s->f;
			l.push_back(n);
			break;
		}
		case Structure::ST_LIST:
		case Structure::ST_HEADTAIL:
		{
			Node* n=new Node();
			n->size=1;					// for now!
			n->arity = s->list.size();	// number of elements in list
			n->type=(Structure::Predicate)s->tag;
			l.push_back(n);

			for (size_t i=0; i<s->list.size(); i++)
			{
				Structure* ls = s->list[i];
				std::vector<Node*> ln = StructureToNodeList(ls);

				// add new structure to existing list
				for (size_t j=0; j<ln.size(); j++)
				{
					l.push_back(ln[j]);
				}
			}
			n->size = l.size();
			break;
		}
		case Structure::ST_BOOL:
		{
			Node* n=new Node();
			n->size=1;
			n->type=(Structure::Predicate)s->tag;
			n->b=s->b;
			l.push_back(n);
			break;
		}
		case Structure::ST_CUT:
		{
			Node* n=new Node();
			n->size=1;
			n->type=(Structure::Predicate)s->tag;
			l.push_back(n);
			break;
		}
		case Structure::ST_FAIL:
		{
			Node* n=new Node();
			n->size=1;
			n->type=(Structure::Predicate)s->tag;
			l.push_back(n);
			break;
		}
		case Structure::ST_INT:
		{
			Node* n=new Node();
			n->size=1;
			n->type=(Structure::Predicate)s->tag;
			n->i=s->i;
			l.push_back(n);
			break;
		}
		case Structure::ST_STRING:
		{
			Node* n=new Node();
			n->size=1;
			n->type=Structure::ST_STRING;
			n->name=s->name;
			l.push_back(n);
			break;
		}
		case Structure::ST_VAR:
		{
			Node* n=new Node();
			n->size=1;
			n->type=(Structure::Predicate)s->tag;
			n->name=s->name;
			l.push_back(n);
			break;
		}
		case Structure::ST_UNUSEDVAR:
		{
			Node* n=new Node();
			n->size=1;
			n->type=Structure::ST_VAR;
			l.push_back(n);
			break;
		}

		case Structure::ST_OR:
		case Structure::ST_AND:
		case Structure::ST_IMPLIES:
		case Structure::ST_EQUAL:
		case Structure::ST_NOTEQUAL:
		case Structure::ST_LESS:
		case Structure::ST_LESSTHAN:
		case Structure::ST_GREATER:
		case Structure::ST_GREATERTHAN:
		case Structure::ST_ASSIGN:
		case Structure::ST_PLUS:
		case Structure::ST_TIMES:
		case Structure::ST_DIVIDE:
		case Structure::ST_MINUS:
		{
			Node* n=new Node();
			n->type=(Structure::Predicate)s->tag;
			l.push_back(n);

			std::vector<Node*> l1=StructureToNodeList(s->left);
			std::vector<Node*> l2=StructureToNodeList(s->right);

			for (size_t i=0; i<l1.size(); i++)
			{
				l.push_back(l1[i]);
			}

			for (size_t i=0; i<l2.size(); i++)
			{
				n->size+=l2[i]->size;
				l.push_back(l2[i]);
			}
			n->size=l.size();
			break;
		}

		case Structure::ST_NOT:
		{
			Node* n=new Node();
			n->type=(Structure::Predicate)s->tag;
			l.push_back(n);
			std::vector<Node*> l1=StructureToNodeList(s->left);
			for (size_t i=0; i<l1.size(); i++)
			{
				l.push_back(l1[i]);
			}
			n->size=l.size();
			break;
		}
		case Structure::ST_NONE:
		{
			PreCond("invalid tag"==NULL);
			break;
		}
		default:
		{
			PreCond("unknown tag"==NULL);
			break;
		}
	}
	return l;
};


