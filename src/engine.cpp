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
//! engine state

//! prolog stack roughly according to WAM
std::vector<Node*> Engine::stack;

//! every binding handed out during a query, so they can all be released
//! together when the query is done
std::vector<Binding*> Engine::bindingPool;

int Engine::dumpCntr=0;
std::string Engine::outstring;
bool Engine::tron=false;

//////////////////////////////////////////////////////////////////

size_t Engine::GetStackSize(void)
{
	return stack.size();
};

void Engine::AddToStack(Node* n)
{
	stack.push_back(n);
};

Binding* Engine::NewBinding(int lhs,int rhs)
{
	Binding* b=new Binding(lhs,rhs);
	bindingPool.push_back(b);
	return b;
};

std::vector<Node*>& Engine::GetStack(void)
{
	return stack;
};

size_t Engine::ListSize(int index)
{
	Node* n = GetStack(index);

	if (n->type==Structure::ST_HEADTAIL)
		return 2;

	PreCond(n->type==Structure::ST_LIST);

	size_t count = 1;
	index++;
	for (size_t i=0; i<n->arity; i++)
	{
		Node* n2 = GetStack(index);
		if (n2->type==Structure::ST_LIST)
		{
			size_t s = ListSize(index);
			index += s;
			count += s;
		}
		else
		{
			index += n2->size;
			count += n2->size;
		}
	}
	return count;
};

Node* Engine::GetStack(int index)
{
	int stackSize = stack.size();
	PreCond(index<stackSize);
	Node* n=stack[index];
	if (n->type==Structure::ST_REFERENCE)
	{
		index = n->fu;
		return stack[index];
	}
	return n;
};

Node* Engine::GetStackDynamic(int& index)
{
	PreCond(index<stack.size());
	Node* n=stack[index];
	if (n->type==Structure::ST_REFERENCE)
	{
		index = n->fu;
		return stack[index];
	}
	return n;
};

void Engine::SetStack(int index,Node* n)
{
	stack[index]=n;
};

bool Engine::GetTron(void)
{
	return tron;
};

void Engine::SetTron(bool _tron)
{
	tron=_tron;
};

void Engine::ClearStack(void)
{
	for (size_t i=0; i<stack.size(); i++)
	{
		safe_delete(stack[i]);
	}
	stack.clear();

	//! result sets referencing these are dead once the stack is gone
	for (size_t i=0; i<bindingPool.size(); i++)
	{
		safe_delete(bindingPool[i]);
	}
	bindingPool.clear();
};

void Engine::ResetStack(void)
{
	dumpCntr=0;
	ClearStack();
	outstring.clear();
};


void Engine::AddToOutputString(const std::string& str)
{
	outstring = outstring + str;
};

const std::string& Engine::GetOutputString(void)
{
	return outstring;
};

void Engine::DumpStack(const std::string& fname)
{
	//! to show what is going on
	FILE* fh;
	fh=fopen(fname.c_str(),"w");
	if (fh!=NULL)
	{
		std::string str=Engine::DumpStack();
		fwrite(str.c_str(),str.length(),1,fh);
		fclose(fh);
	}
};

void Engine::DumpStackToScreen(void)
{
	System::printf("%s",DumpStack().c_str());
};

std::string Engine::PrettyPrint(int i)
{
	std::string str;
	Node* n=GetStack(i);

	switch (n->type)
	{
		case Structure::ST_IMPLIES:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+" :- ";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_AND:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+", ";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_OR:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+"; ";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
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
					str=str+Engine::PrettyPrint(index);
					if ((j+1)<n->arity)
						str=str+",";
					Node* n2=Engine::GetStack(index);
					index += n2->size;
//					index+=SizeOfClause(index);
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
			if (GetStack(i)->name==0)
			{
				str=str+"_";
			}
			else
			{
				if (Valid(GetStack(i)->fu))
				{
					str=str+Engine::PrettyPrint(GetForwardValue(GetStack(i)->fu));
				}
				else
				{
					str=str+Structure::GetString(GetStack(i)->name);
				}
			}
			break;
		};
		case Structure::ST_EQUAL:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+"=:=";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_NOTEQUAL:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+"=\\=";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_IDENTICAL:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+"==";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_NOTIDENTICAL:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+"\\==";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_NOTUNIFIABLE:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+"\\=";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_LESS:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+"<";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_NOT:
		{
			str=str+"not("+Engine::PrettyPrint(i+1)+")";
			break;
		};
		case Structure::ST_CUT:
		{
			str=str+"!";
			break;
		};
		case Structure::ST_FAIL:
		{
			str=str+"fail";
			break;
		};
		case Structure::ST_LESSTHAN:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+"=<";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_GREATER:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+">";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_GREATERTHAN:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+">=";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_ASSIGN:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+"=";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_IS:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+" is ";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_PLUS:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+"+";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_TIMES:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+"*";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_DIVIDE:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+"/";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_MINUS:
		{
			int index=i+1;
			str=str+Engine::PrettyPrint(index);
			str=str+"-";
			index+=SizeOfClause(index);
			str=str+Engine::PrettyPrint(index);
			break;
		};
		case Structure::ST_LIST:
		{
			int index=i+1;
			str=str+"[";
			for (size_t j=0; j<n->arity; j++)
			{
				str=str+Engine::PrettyPrint(index);
				if ((j+1)<n->arity)
					str=str+",";
				index+=SizeOfClause(index);
			}
			str=str+"]";
			break;
		}
		case Structure::ST_HEADTAIL:
		{
			int index=i+1;
			str=str+"[";
			for (size_t j=0; j<n->arity; j++)
			{
				str=str+Engine::PrettyPrint(index);
				if ((j+1)<n->arity)
					str=str+"|";
				index+=SizeOfClause(index);
			}
			str=str+"]";
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

std::string Engine::PrintStackItem(int i)
{
	std::string str;
	Node* n = GetStack(i);
	switch (n->type)
	{
		case Structure::ST_STRUCTURE:
		{
			str=str+"structure:";
			str=str+Structure::GetString(GetStack(i)->name)+"/"+System::Int2Str(GetStack(i)->arity);
			if (Valid(GetStack(i)->parent))
			{
				str=str+", parent "+System::Int2Str(GetStack(i)->parent);
			}
			if (Valid(GetStack(i)->next))
			{
				str=str+", next "+System::Int2Str(GetStack(i)->next);
			}
			break;
		};
		case Structure::ST_FLOAT:
		{
			str=str+"float:";
			str=str+System::Float2Str(GetStack(i)->f);
			break;
		};
		case Structure::ST_STRING:
		{
			str=str+"string:";
			str=str+"\""+Structure::GetString(GetStack(i)->name)+"\"";
			break;
		};
		case Structure::ST_BOOL:
		{
			str=str+"bool:";
			if (GetStack(i)->b)
				str=str+"true";
			else
				str=str+"false";
			break;
		};
		case Structure::ST_INT:
		{
			str=str+"int:";
			str=str+System::Int2Str(GetStack(i)->i);
			break;
		};
		case Structure::ST_VAR:
		{
			str=str+"var:";
			str=str+Structure::GetString(GetStack(i)->name);
			if (Valid(GetStack(i)->fu))
			{
				str=str+" (="+System::Int2Str(GetStack(i)->fu)+")";
			}
			break;
		};
		case Structure::ST_EQUAL:
		{
			str=str+"=:=";
			break;
		};
		case Structure::ST_IDENTICAL:
		{
			str=str+"==";
			break;
		};
		case Structure::ST_NOTIDENTICAL:
		{
			str=str+"\\==";
			break;
		};
		case Structure::ST_NOTUNIFIABLE:
		{
			str=str+"\\=";
			break;
		};
		case Structure::ST_CUT:
		{
			str=str+"!";
			break;
		};
		case Structure::ST_FAIL:
		{
			str=str+"!";
			break;
		};
		case Structure::ST_NOTEQUAL:
		{
			str=str+"=\\=";
			break;
		};
		case Structure::ST_LESS:
		{
			str=str+"<";
			break;
		};
		case Structure::ST_LESSTHAN:
		{
			str=str+"=<";
			break;
		};
		case Structure::ST_GREATER:
		{
			str=str+">";
			break;
		};
		case Structure::ST_GREATERTHAN:
		{
			str=str+">=";
			break;
		};
		case Structure::ST_ASSIGN:
		{
			str=str+"=";
			break;
		};
		case Structure::ST_IS:
		{
			str=str+"is";
			break;
		};
		case Structure::ST_NOT:
		{
			str=str+"\\+()";
			break;
		};
		case Structure::ST_PLUS:
		{
			str=str+"+";
			break;
		};
		case Structure::ST_TIMES:
		{
			str=str+"*";
			break;
		};
		case Structure::ST_DIVIDE:
		{
			str=str+"/";
			break;
		};
		case Structure::ST_MINUS:
		{
			str=str+"-";
			break;
		};
		case Structure::ST_IMPLIES:
		{
			str=str+"Implies";
			break;
		}
		case Structure::ST_AND:
		{
			str=str+"AND";
			break;
		}
		case Structure::ST_OR:
		{
			str=str+"OR";
			break;
		}
		case Structure::ST_LIST:
		{
			str=str+"list:";
			str=str+System::Int2Str(GetStack(i)->arity);
			break;
		}
		case Structure::ST_HEADTAIL:
		{
			str=str+"head tail list:";
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

std::string Engine::DumpStack(void)
{
	std::string str;

	str=System::nl+"=================================================== dump "+System::Int2Str(dumpCntr+1)+System::nl+System::nl;
	dumpCntr++;
	for (size_t i=0; i<stack.size(); i++)
	{
		str=str+"---------------------------------- frame "+System::Int2Str(i)+System::nl;

		str=str+PrintStackItem(i)+System::nl;
	}
	return str;
};

int Engine::GetBinding(int var,BindingList& bindings)
{
	size_t size=bindings.size();
	for (size_t i=0; i<size; i++)
	{
		if (bindings[i]->lhs==var)
			return bindings[i]->rhs;
	}
	return -1;
};

bool Engine::EvaluateExpression(int index,Node& result)
{
	Node e1,e2;
	Node* n=GetStack(index);

	switch (n->type)
	{
		case Structure::ST_CUT:
		{
			result.type=Structure::ST_CUT;
			return true;
		}
		case Structure::ST_FAIL:
		{
			result.type=Structure::ST_FAIL;
			return true;
		}
		case Structure::ST_EQUAL:
		{
			if (Engine::EvaluateExpression(index+1,e1) && Engine::EvaluateExpression(index+1+SizeOfClause(index+1),e2))
			{
				result.type=Structure::ST_BOOL;
				if (e1.type==e2.type)
				{
					switch (e1.type)
					{
						case Structure::ST_FLOAT:
						{
							result.b=(e1.f==e2.f);
							return true;
						}
						case Structure::ST_BOOL:
						{
							result.b=(e1.b==e2.b);
							return true;
						}
						case Structure::ST_INT:
						{
							result.b=(e1.i==e2.i);
							return true;
						}
						case Structure::ST_STRUCTURE:
						case Structure::ST_STRING:
						{
							result.b=(e1.name==e2.name);
							return true;
						}
					}
				}
			}
			return false;
		}
		case Structure::ST_NOTEQUAL:
		{
			if (Engine::EvaluateExpression(index+1,e1) && Engine::EvaluateExpression(index+1+SizeOfClause(index+1),e2))
			{
				result.type=Structure::ST_BOOL;
				if (e1.type==e2.type)
				{
					switch (e1.type)
					{
						case Structure::ST_FLOAT:
						{
							result.b=(e1.f!=e2.f);
							return true;
						}
						case Structure::ST_BOOL:
						{
							result.b=(e1.b!=e2.b);
							return true;
						}
						case Structure::ST_INT:
						{
							result.b=(e1.i!=e2.i);
							return true;
						}
						case Structure::ST_STRUCTURE:
						case Structure::ST_STRING:
						{
							result.b=(e1.name!=e2.name);
							return true;
						}
					}
				}
			}
			return false;
		}
		case Structure::ST_LESS:
		{
			if (Engine::EvaluateExpression(index+1,e1) && Engine::EvaluateExpression(index+1+SizeOfClause(index+1),e2))
			{
				result.type=Structure::ST_BOOL;
				if (e1.type==e2.type)
				{
					switch (e1.type)
					{
						case Structure::ST_FLOAT:
						{
							result.b=(e1.f<e2.f);
							return true;
						}
						case Structure::ST_INT:
						{
							result.b=(e1.i<e2.i);
							return true;
						}
					}
				}
			}
			return false;
		}
		case Structure::ST_NOT:
		{
			if (Engine::EvaluateExpression(index+1,e1))
			{
				result.type=Structure::ST_BOOL;
				switch (e1.type)
				{
					case Structure::ST_BOOL:
					{
						result.b=!e1.b;
						return true;
					}
				}
			}
			return false;
		}
		case Structure::ST_LESSTHAN:
		{
			if (Engine::EvaluateExpression(index+1,e1) && Engine::EvaluateExpression(index+1+SizeOfClause(index+1),e2))
			{
				result.type=Structure::ST_BOOL;
				if (e1.type==e2.type)
				{
					switch (e1.type)
					{
						case Structure::ST_FLOAT:
						{
							result.b=(e1.f<=e2.f);
							return true;
						}
						case Structure::ST_INT:
						{
							result.b=(e1.i<=e2.i);
							return true;
						}
					}
				}
			}
			return false;
		}
		case Structure::ST_GREATER:
		{
			if (Engine::EvaluateExpression(index+1,e1) && Engine::EvaluateExpression(index+1+SizeOfClause(index+1),e2))
			{
				result.type=Structure::ST_BOOL;
				if (e1.type==e2.type)
				{
					switch (e1.type)
					{
						case Structure::ST_FLOAT:
						{
							result.b=(e1.f>e2.f);
							return true;
						}
						case Structure::ST_INT:
						{
							result.b=(e1.i>e2.i);
							return true;
						}
					}
				}
			}
			return false;
		}
		case Structure::ST_GREATERTHAN:
		{
			if (Engine::EvaluateExpression(index+1,e1) && Engine::EvaluateExpression(index+1+SizeOfClause(index+1),e2))
			{
				result.type=Structure::ST_BOOL;
				if (e1.type==e2.type)
				{
					switch (e1.type)
					{
						case Structure::ST_FLOAT:
						{
							result.b=(e1.f>=e2.f);
							return true;
						}
						case Structure::ST_INT:
						{
							result.b=(e1.i>=e2.i);
							return true;
						}
					}
				}
				return true;
			}
			return false;
		}
		case Structure::ST_PLUS:
		{
			if (Engine::EvaluateExpression(index+1,e1) && Engine::EvaluateExpression(index+1+SizeOfClause(index+1),e2))
			{
				if (e1.type==e2.type)
				{
					result.type=e1.type;
					switch (e1.type)
					{
						case Structure::ST_FLOAT:
						{
							result.f=(e1.f+e2.f);
							return true;
						}
						case Structure::ST_INT:
						{
							result.i=(e1.i+e2.i);
							return true;
						}
					}
				}
			}
			return false;
		}
		case Structure::ST_TIMES:
		{
			if (Engine::EvaluateExpression(index+1,e1) && Engine::EvaluateExpression(index+1+SizeOfClause(index+1),e2))
			{
				if (e1.type==e2.type)
				{
					result.type=e1.type;
					switch (e1.type)
					{
						case Structure::ST_FLOAT:
						{
							result.f=(e1.f*e2.f);
							return true;
						}
						case Structure::ST_INT:
						{
							result.i=(e1.i*e2.i);
							return true;
						}
					}
				}
			}
			return false;
		}
		case Structure::ST_DIVIDE:
		{
			if (Engine::EvaluateExpression(index+1,e1) && Engine::EvaluateExpression(index+1+SizeOfClause(index+1),e2))
			{
				if (e1.type==e2.type)
				{
					result.type=e1.type;
					switch (e1.type)
					{
						case Structure::ST_FLOAT:
						{
							result.f=(e1.f/e2.f);
							return true;
						}
						case Structure::ST_INT:
						{
							result.i=(e1.i/e2.i);
							return true;
						}
					}
				}
			}
			return false;
		}
		case Structure::ST_MINUS:
		{
			if (Engine::EvaluateExpression(index+1,e1) && Engine::EvaluateExpression(index+1+SizeOfClause(index+1),e2))
			{
				if (e1.type==e2.type)
				{
					result.type=e1.type;
					switch (e1.type)
					{
						case Structure::ST_FLOAT:
						{
							result.f=(e1.f-e2.f);
							return true;
						}
						case Structure::ST_INT:
						{
							result.i=(e1.i-e2.i);
							return true;
						}
					}
				}
			}
			return false;
		}
		case Structure::ST_VAR:
		{
			if (Valid(n->fu))
			{
				return Engine::EvaluateExpression(n->fu,result);
			}
			return false;
		}
		case Structure::ST_BOOL:
		{
			result.type=n->type;
			result.b=n->b;
			return true;
		}
		case Structure::ST_INT:
		{
			result.type=n->type;
			result.i=n->i;
			return true;
		}
		case Structure::ST_LIST:
		{
			result.type=n->type;
			return true;
		}
		case Structure::ST_FLOAT:
		{
			result.type=n->type;
			result.f=n->f;
			return true;
		}
		case Structure::ST_STRUCTURE:
		case Structure::ST_STRING:
		{
			result.type=n->type;
			result.name=n->name;
			return true;
		}
		default:
		{
			PostCond("unknown expression type"==NULL);
		}
	}
	return false;
};

int Engine::GetForwardValue(int index)
{
	while (GetStack(index)->type==Structure::ST_VAR && Valid(GetStack(index)->fu))
	{
		index=GetStack(index)->fu;
	}
	return index;
};

void Engine::ForwardBind(int start,int size)
{
	std::vector<int> listindex;

	//! names must be held as size_t - every variable name carries
	//! Structure::VAR_PREFIX (0x80000000), which turns negative when squeezed
	//! into an int and then never compares equal to the size_t it came from.
	//! this list matched nothing at all while it was a vector<int>
	std::vector<size_t> list;

	for (int i=0; i<size; i++)
	{
		Node* n=GetStack(start+i);
		if (n->type==Structure::ST_VAR)
		{
			if (n->name!=0)
			{
				listindex.push_back(start+i);
				list.push_back(n->name);

				//! try and bind it with an existing same name
				size_t size=list.size()-1;
				for (size_t j=0; j<size; j++)
				{
					if (list[j]==n->name)
					{
						n->fu=Engine::GetForwardValue(listindex[j]);
						break;
					}
				}
			}
		}
	}
}

bool Engine::Equivalent(int a,int b)
{
	if (a==b)
		return true;

	Node* n1=GetStack(a);
	while (Valid(n1->fu))
	{
		a=n1->fu;
		n1=GetStack(a);
	}
	Node* n2=GetStack(b);
	while (Valid(n2->fu))
	{
		b=n2->fu;
		n2=GetStack(b);
	}

	if (n1->type==n2->type)
	{
		switch (n1->type)
		{
			case Structure::ST_FAIL:
			case Structure::ST_CUT:
			{
				return true;
			}
			case Structure::ST_STRING:
			case Structure::ST_VAR:
			{
				return n1->name==n2->name;
			}
			case Structure::ST_INT:
			{
				return n1->i==n2->i;
			}
			case Structure::ST_FLOAT:
			{
				return n1->f==n2->f;
			}
			case Structure::ST_BOOL:
			{
				return n1->b==n2->b;
			}
			case Structure::ST_STRUCTURE:
			{
				if (n1->name==n2->name && n1->arity==n2->arity)
				{
					int index1=a+1;
					int index2=b+1;
					for (size_t i=0; i<n1->arity; i++)
					{
						if (!Equivalent(index1,index2))
							return false;
						index1+=SizeOfClause(index1);
						index2+=SizeOfClause(index2);
					}
					return true;
				}
				return false;
			}
			case Structure::ST_LIST:
			{
				if (n1->arity!=n2->arity)
					return false;

				int index1=a+1;
				int index2=b+1;
				for (size_t i=0; i<n1->arity; i++)
				{
					if (!Equivalent(index1,index2))
						return false;
					index1+=SizeOfClause(index1);
					index2+=SizeOfClause(index2);
				}
				return true;
			}
			case Structure::ST_HEADTAIL:
			{
				int index1=a+1;
				int index2=b+1;
				if (!Equivalent(index1,index2))
					return false;
				return Equivalent(index1+SizeOfClause(index1),index2+SizeOfClause(index2));
			}
			default:
			{
				//! values can carry subtrees this comparison has no view
				//! on - different is the safe answer, since UniqueSet then
				//! keeps both solutions rather than dropping one
				return false;
			}
		}
	}
	return false;
};

// forward unify - i.e. anything in target that can match parent is fine
// if target is a var (already unified or not) then bind / substitute it
// with the parent equivalent - this is pre-running a new query - return
// false if we can't get a match, or true and the number of matches made
bool Engine::ForwardUnify(int level,int target,int parent,size_t& numUnifications)
{
	if (Valid(parent))
	{
		Node* t=GetStack(target);
		Node* p=GetStack(parent);

		//! resolve valid parent references so we don't replace vars
		//! with other vars but whatever they're bound too
		if (p->type==Structure::ST_VAR && Valid(p->fu))
		{
			parent = p->fu; // of course update the stack reference of parent too
			p = GetStack(parent);
		}

		switch (t->type)
		{
			case Structure::ST_HEADTAIL:
			{
				if (p->type==Structure::ST_VAR) return true;

				// such a type of list can only be unified with another
				// list
				if (p->type==Structure::ST_LIST)
				{
					switch (p->arity)
					{
						case 0:
						{
							// [] can not unify with [H|T]
							return false;
						}
						case 1:
						{
							// [x] u [H|T] => H=x , T=[]
							int stackIndex = GetStackSize();

							// create an empty list
							Node* newList = new Node();
							newList->type = Structure::ST_LIST;
							newList->arity = 0;
							AddToStack(newList);

							// head and tail
							numUnifications = numUnifications + 2;

							Node* t1 = GetStack(target+1);
							Node* t2 = GetStack(target+2);

							t1->fu = parent + 1;
							t2->fu = stackIndex;

							return true;
						}
						default:
						{
							// [x,y,z] u [H|T] =>  H=x , T=[y,z]
							int stackIndex = GetStackSize();

							// create an empty list
							Node* newList = new Node();
							newList->type = Structure::ST_LIST;
							newList->arity = p->arity - 1;

							// create a temporary list
							std::vector<Node*> tempList;
							tempList.push_back(newList);

							Node* t1 = GetStack(target+1);
							Node* t2 = GetStack(target+2);

							t1->fu = parent + 1;
							t2->fu = -1;

							bool failed = false;
							int pindex2 = t1->fu + SizeOfClause(t1->fu);
							for (int i=0; i<newList->arity && !failed; i++)
							{
								Node* element = Engine::GetStack(pindex2);
								PreCond(element->size>0);

								// resolve vars
								if (element->type==Structure::ST_VAR && Valid(element->fu))
								{
									pindex2 = Engine::GetForwardValue(element->fu);
									element = Engine::GetStack(pindex2);
								}
								else if (element->type==Structure::ST_VAR)
								{
									// an unresolved var makes no sense in a forward
									// resolve => fail!
									failed = true;
									break;
								}

								int firstElementSize = element->size;
								for (int j=0; j<firstElementSize; j++)
								{
									element = Engine::GetStack(pindex2+j);
									Node* newNode = new Node(*element);
									newNode->fu = -1;
									tempList.push_back(newNode);
								}
								pindex2 += firstElementSize;
							}

							if (!failed)
							{
								for (int i=0; i<tempList.size(); i++)
								{
									Engine::AddToStack(tempList[i]);
								}
								tempList.clear();
								numUnifications += 2;

								Trace3(level,"FORWARD UNIFY %s with %s",Engine::PrettyPrint(target+2).c_str(),
																		Engine::PrettyPrint(stackIndex).c_str());

								t2->fu = stackIndex;
								return true;
							}
							else
							{
								for (int i=0; i<tempList.size(); i++)
								{
									safe_delete(tempList[i]);
								}
								tempList.clear();
								return false;
							}
						}
					}
				}
				return false;
			}
			case Structure::ST_LIST:
			{
				if (p->type==Structure::ST_VAR || p->type==Structure::ST_HEADTAIL) 
					return true;

				if (p->type==Structure::ST_LIST)
				{
					// empty list with non-empty list
					if ((p->arity==0 && t->arity!=0) ||
						(p->arity!=0 && t->arity==0))
						return false;

					//! empty lists always match
					if (p->arity==0 && p->arity==t->arity)
						return true;

					int pindex = parent + 1;
					int tindex = target + 1;
					int arity = ((int)p->arity) - 1;
					for (int i=0; i<arity; i++)
					{
						if (!ForwardUnify(level,tindex,pindex,numUnifications))
							return false;

						pindex += SizeOfClause(pindex);
						tindex += SizeOfClause(tindex);
					}
					return true;
				}
				return false;
			}

			case Structure::ST_VAR:
			{
				if (Valid(t->fu))
				{
					// already has a binding - check it is the same as the proposed
					// binding - or fail if it isn't
					return Engine::Equivalent(Engine::GetForwardValue(t->fu),parent);
				}
				else
				{
					// new forward binding
					Trace3(level,"FORWARD UNIFY %s with %s",Engine::PrettyPrint(target).c_str(),
															Engine::PrettyPrint(Engine::GetForwardValue(parent)).c_str());
					t->fu=Engine::GetForwardValue(parent);
					numUnifications++;
				}
				return true;
			};
			case Structure::ST_STRUCTURE:
			{
				if (p->type==Structure::ST_VAR) return true;
				if (t->type==p->type)
				{
					if (t->arity==p->arity && t->name==p->name)
					{
						int index1=target+1;
						int index2=parent+1;
						for (size_t i=0; i<t->arity; i++)
						{
							if (!Engine::ForwardUnify(level,index1,index2,numUnifications))
								return false;
							
							// if this was a variable - make sure that variables
							// with the same name at the same level get the same values too
							Node* n=Engine::GetStack(index1);
							if (n->type==Structure::ST_VAR)
							{
								int index3 = index1 + SizeOfClause(index1);
								for (size_t j=i+1; j<t->arity; j++)
								{
									Node* n2 = Engine::GetStack(index3);
									if (n2->type==Structure::ST_VAR && n2->name==n->name)
									{
										n2->fu = n->fu;
									}
									index3 += SizeOfClause(index3);
								}
							}

							index1+=SizeOfClause(index1);
							index2+=SizeOfClause(index2);
						}
						return true;
					}
				}
				return false;
			}
			case Structure::ST_INT:
			{
				if (p->type==Structure::ST_VAR) return true;
				return (t->type==p->type && t->i==p->i);
			}
			case Structure::ST_FLOAT:
			{
				if (p->type==Structure::ST_VAR) return true;
				return (t->type==p->type && t->f==p->f);
			}
			case Structure::ST_BOOL:
			{
				if (p->type==Structure::ST_VAR) return true;
				return (t->type==p->type && t->b==p->b);
			}
			case Structure::ST_STRING:
			{
				if (p->type==Structure::ST_VAR) return true;
				return (t->type==p->type && t->name==p->name);
			}
			case Structure::ST_IMPLIES:
			case Structure::ST_AND:
			case Structure::ST_OR:
			{
				return Engine::ForwardUnify(level,target+1,parent,numUnifications);
			};

			case Structure::ST_EQUAL:
			case Structure::ST_NOTEQUAL:
			case Structure::ST_IDENTICAL:
			case Structure::ST_NOTIDENTICAL:
			case Structure::ST_NOTUNIFIABLE:
			case Structure::ST_LESS:
			case Structure::ST_LESSTHAN:
			case Structure::ST_GREATER:
			case Structure::ST_GREATERTHAN:
			case Structure::ST_ASSIGN:
			case Structure::ST_IS:
			case Structure::ST_PLUS:
			case Structure::ST_TIMES:
			case Structure::ST_DIVIDE:
			case Structure::ST_MINUS:
			{
				int index=target+1;
				if (Engine::ForwardUnify(level,index,parent,numUnifications))
				{
					index+=SizeOfClause(index);
					return Engine::ForwardUnify(level,index,parent,numUnifications);
				}
				return false;
			};
		}
		return false;
	}
	return true;
};

//! bind a variable to a rhs; if it already exists in the binding list
//! and it is different, it will fail.  Otherwise it'll be fine
bool Engine::Bind(int var,int rhs,BindingList& bindings)
{
	// bind back to the previous level (fu) if we are dealing
	// with a variable of a different name (doesn't auto do this otherwise)
	Node* n1 = Engine::GetStack(var);
	if (n1->type==Structure::ST_VAR && Valid(n1->fu))
	{
		Node* n2 = Engine::GetStack(n1->fu);
		if (n1->name != n2->name)
			bindings.push_back(Engine::NewBinding(n1->fu,rhs));
	}
	bindings.push_back(Engine::NewBinding(var,rhs));
	return true;
};

bool Engine::CreateBindingsRecursive(int level,int target,int parent,BindingList& bindings)
{
	if (Valid(parent))
	{
		Node* t=GetStack(target);
		while (t->type==Structure::ST_VAR && Valid(t->fu))
		{
			target = t->fu;
			t = Engine::GetStack(t->fu);
		}
		if (t->type==Structure::ST_VAR)
			return false;

		Node* p=GetStack(parent);

		switch (p->type)
		{
			case Structure::ST_LIST:
			{
				if (p->arity!=t->arity)
					return false;

				int arity = p->arity;
				target++;
				parent++;
				for (int i=0; i<arity; i++)
				{
					if (!CreateBindingsRecursive(level,target,parent,bindings))
						return false;
					target += SizeOfClause(target);
					parent += SizeOfClause(parent);
				}
				return true;
			}
			case Structure::ST_HEADTAIL:
			{
				// bind head tail list to ???
				if (t->type==Structure::ST_LIST)
				{
					switch (t->arity)
					{
						case 0:
						{
							// [] can not unify with [H|T]
							return false;
						}
						case 1:
						{
							// [x] u [H|T] => H=x , T=[]
							int stackIndex = GetStackSize();

							// create an empty list
							Node* newList = new Node();
							newList->type = Structure::ST_LIST;
							newList->arity = 0;
							AddToStack(newList);

							return (CreateBindingsRecursive(level,target+1,parent+1,bindings) &&
									CreateBindingsRecursive(level,stackIndex,parent+2,bindings));
						}
						default:
						{
							// [x,y,z] u [H|T] =>  H=x , T=[y,z]
							int stackIndex = GetStackSize();

							// create a new list for the tail section
							Node* newList = new Node();
							newList->type = Structure::ST_LIST;
							newList->arity = t->arity - 1;

							// create a temporary list for tail section
							std::vector<Node*> tempList;
							tempList.push_back(newList);

							// bind head of [H|T] H to head of list
							if (!CreateBindingsRecursive(level,target+1,parent+1,bindings))
								return false;

							bool failed = false;
							int tindex2 = target + 1 + SizeOfClause(target+1);
							for (int i=0; i<newList->arity && !failed; i++)
							{
								Node* element = Engine::GetStack(tindex2);
								PreCond(element->size>0);

								// resolve vars
								if (element->type==Structure::ST_VAR && Valid(element->fu))
								{
									tindex2 = Engine::GetForwardValue(element->fu);
									element = Engine::GetStack(tindex2);
								}
								else if (element->type==Structure::ST_VAR)
								{
									// an unresolved var makes no sense backwards
									// resolve => fail!
									failed = true;
									break;
								}

								int firstElementSize = element->size;
								for (int j=0; j<firstElementSize; j++)
								{
									element = Engine::GetStack(tindex2+j);
									Node* newNode = new Node(*element);
									newNode->fu = -1;
									tempList.push_back(newNode);
								}
								tindex2 += firstElementSize;
							}

							if (!failed)
							{
								for (int i=0; i<tempList.size(); i++)
								{
									Engine::AddToStack(tempList[i]);
								}
								tempList.clear();

								return CreateBindingsRecursive(level,stackIndex,parent+2,bindings);
							}
							else
							{
								for (int i=0; i<tempList.size(); i++)
								{
									safe_delete(tempList[i]);
								}
								tempList.clear();
								return false;
							}
						}
					}
				}
				return false;
			}
			case Structure::ST_VAR:
			{
				//! this bind will always succeed - it is not
				//! restrictive since its is part of a single clause
				//! and not a set - that is higher level
				Trace3(level,"BIND %s with %s",Engine::PrettyPrint(parent).c_str(),
											   Engine::PrettyPrint(target).c_str());
				return Bind(parent,target,bindings);
			};
			case Structure::ST_STRUCTURE:
			{
				if (t->type==p->type)
				{
					if (t->arity==p->arity && t->name==p->name)
					{
						int index1=target+1;
						int index2=parent+1;
						for (size_t i=0; i<t->arity; i++)
						{
							if (!Engine::CreateBindingsRecursive(level,index1,index2,bindings))
								return false;
							index1+=SizeOfClause(index1);
							index2+=SizeOfClause(index2);
						}
					}
				}
				return true;
			}
			case Structure::ST_CUT:
			case Structure::ST_FAIL:
			{
				return true;
			}
			case Structure::ST_NOT:
			{
				return Engine::CreateBindingsRecursive(level,target+1,parent,bindings);
			}
			case Structure::ST_EQUAL:
			case Structure::ST_NOTEQUAL:
			case Structure::ST_IDENTICAL:
			case Structure::ST_NOTIDENTICAL:
			case Structure::ST_NOTUNIFIABLE:
			case Structure::ST_LESS:
			case Structure::ST_LESSTHAN:
			case Structure::ST_GREATER:
			case Structure::ST_GREATERTHAN:
			case Structure::ST_ASSIGN:
			case Structure::ST_IS:
			case Structure::ST_PLUS:
			case Structure::ST_TIMES:
			case Structure::ST_DIVIDE:
			case Structure::ST_MINUS:
			{
				int index=target+1;
				if (Engine::CreateBindingsRecursive(level,index,parent,bindings))
				{
					index+=SizeOfClause(index);
					return Engine::CreateBindingsRecursive(level,index,parent,bindings);
				}
				return false;
			};
		}
	}
	return true;
};

bool Engine::CreateBindings(int level,int target,int parent,BindingList& bindings)
{
	return Engine::CreateBindingsRecursive(level,target,parent,bindings);
};

void Engine::FilterSet(int pc,Set& s)
{
	//! filter results into only necessairy variables
	std::vector<int> vars;
	GatherVars(0,vars);

	size_t varSize=vars.size();
	if (varSize>0)
	{
		Set newSet;
		size_t size1=s.size();
		for (size_t i=0; i<size1; i++)
		{
			BindingList b;
			size_t size2=s[i].size();
			for (size_t j=0; j<size2; j++)
			{
				bool found=false;
				for (size_t k=0; k<varSize && !found; k++)
				{
					int lhs = s[i][j]->lhs;
					Node* n = GetStack(lhs);
					found=(GetStack(vars[k])->name==n->name);
				}
				if (found)
				{
					b.push_back(s[i][j]);
				}
			}
			if (Engine::UniqueSet(newSet,b))
				newSet.push_back(b);
		}
		s=newSet;
	}
};

void Engine::GatherVars(int index,std::vector<int>& vars)
{
	Node* n=GetStack(index);
	switch (n->type)
	{
		case Structure::ST_AND:
		{
			index++;
			GatherVars(index,vars);
			index += SizeOfClause(index);
			GatherVars(index,vars);
			break;
		}
		case Structure::ST_OR:
		{
			index++;
			GatherVars(index,vars);
			index += SizeOfClause(index);
			GatherVars(index,vars);
			break;
		}
		case Structure::ST_STRUCTURE:
		{
			index++;
			for (size_t i=0; i<n->arity; i++)
			{
				Node* n2=GetStack(index);
				if (n2->type==Structure::ST_VAR)
				{
					vars.push_back(index);
				}
				else if (n2->type==Structure::ST_LIST)
				{
					GatherVars(index,vars);
				}
				else if (n2->type==Structure::ST_HEADTAIL)
				{
					GatherVars(index,vars);
				}
				index+=SizeOfClause(index);
			}
			break;
		}
		case Structure::ST_HEADTAIL:
		{
			//! the head may be any term and the tail a variable or a
			//! list, so both sides are walked rather than assumed
			int head=index+1;
			Node* h=GetStack(head);
			if (h->type==Structure::ST_VAR)
				vars.push_back(head);
			else
				GatherVars(head,vars);

			int tail=head+SizeOfClause(head);
			Node* t2=GetStack(tail);
			if (t2->type==Structure::ST_VAR)
				vars.push_back(tail);
			else
				GatherVars(tail,vars);
			break;
		}
		case Structure::ST_LIST:
		{
			index++;
			for (size_t i=0; i<n->arity; i++)
			{
				Node* n2=GetStack(index);
				if (n2->type==Structure::ST_VAR)
					vars.push_back(index);
				index+=SizeOfClause(index);
			}
			break;
		}

		//! binary operators - either operand may be, or may contain, a
		//! variable the caller wants reported.  without these a query like
		//! "?X = 3 - 1." gathers no variables at all and answers a bare "yes"
		case Structure::ST_ASSIGN:
		case Structure::ST_IS:
		case Structure::ST_EQUAL:
		case Structure::ST_NOTEQUAL:
		case Structure::ST_IDENTICAL:
		case Structure::ST_NOTIDENTICAL:
		case Structure::ST_NOTUNIFIABLE:
		case Structure::ST_LESS:
		case Structure::ST_LESSTHAN:
		case Structure::ST_GREATER:
		case Structure::ST_GREATERTHAN:
		case Structure::ST_PLUS:
		case Structure::ST_MINUS:
		case Structure::ST_TIMES:
		case Structure::ST_DIVIDE:
		{
			index++;
			for (int i=0; i<2; i++)
			{
				Node* n2=GetStack(index);
				if (n2->type==Structure::ST_VAR)
					vars.push_back(index);
				else
					GatherVars(index,vars);
				index+=SizeOfClause(index);
			}
			break;
		}

		case Structure::ST_NOT:
		{
			index++;
			Node* n2=GetStack(index);
			if (n2->type==Structure::ST_VAR)
				vars.push_back(index);
			else
				GatherVars(index,vars);
			break;
		}
	}
}

//! gather the results
std::string Engine::GatherResults(Set& s)
{
	//! get an inventory of all variables
	std::vector<int> vars;
	GatherVars(0,vars);

	//! dump those variables of interest
	std::string str;
	size_t size=s.size();
	for (size_t i=0; i<size; i++)
	{
		size_t size2=s[i].size();
		for (size_t j=0; j<size2; j++)
		{
			int var=s[i][j]->lhs;
			bool allowed=false;
			for (size_t k=0; k<vars.size() && !allowed; k++)
			{
				//! bad - needs proper fixing
				if (GetStack(vars[k])->name==GetStack(var)->name)
					allowed=true;
			}

			if (allowed)
			{
				str=str+Structure::GetString(GetStack(var)->name);
				str=str+"=";
				str=str+PrettyPrint(s[i][j]->rhs);
				str=str+System::nl;
			}
		}
	};
	return str;
};

bool Engine::UniqueSet(Set& s,BindingList& b)
{
	bool unique=true;
	size_t size2=s.size();
	size_t size1=b.size();
	for (size_t i=0; i<size2; i++)
	{
		//! solutions binding a different number of variables can never be
		//! the same solution - and comparing them position by position would
		//! read past the end of the shorter one
		if (s[i].size()!=size1)
			continue;

		bool found=false;
		for (size_t j=0; j<size1 && !found; j++)
		{
			int rhs1 = s[i][j]->rhs;
			int rhs2 = b[j]->rhs;
			found=!Engine::Equivalent(rhs1,rhs2);
		}
		if (!found)
			return false;
	}
	return unique;
};

std::string Engine::PrintSet(Set& s)
{
	std::string str;

	//! dump those variables of interest
	size_t size=s.size();
	for (size_t i=0; i<size; i++)
	{
		size_t size2=s[i].size();
		for (size_t j=0; j<size2; j++)
		{
			int var=s[i][j]->lhs;
			str=str+Structure::GetString(GetStack(var)->name);
			str=str+"=";
			str=str+Engine::PrettyPrint(s[i][j]->rhs);
			str=str+System::nl;
		}
		str=str+System::nl;
	};
	return str;
};

