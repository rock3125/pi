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

#include "structure.h"

//////////////////////////////////////////////////////////////////

std::vector<std::string> Structure::stringArray;
std::vector<std::string> Structure::varArray;
size_t Structure::varFrameMarker = 0;

//////////////////////////////////////////////////////////////////

Structure::Structure(void)
	: left(NULL)
	, right(NULL)
	, constraintExpression(NULL)
{
	tag=Structure::ST_STRUCTURE;
	bracketed=false;
	name=0;
	i=0;
	f=0.0f;
	b=false;
};

Structure::Structure(int _i)
	: left(NULL)
	, right(NULL)
	, constraintExpression(NULL)
{
	tag=Structure::ST_INT;
	bracketed=false;
	name=0;
	i=_i;
	f=0.0f;
	b=false;
};

Structure::Structure(float _f)
	: left(NULL)
	, right(NULL)
	, constraintExpression(NULL)
{
	tag=Structure::ST_FLOAT;
	bracketed=false;
	name=0;
	i=0;
	f=_f;
	b=false;
};

Structure::Structure(bool _b)
	: left(NULL)
	, right(NULL)
	, constraintExpression(NULL)
{
	tag=Structure::ST_BOOL;
	bracketed=false;
	name=0;
	i=0;
	f=0.0f;
	b=_b;
};

Structure::Structure(size_t str)
	: left(NULL)
	, right(NULL)
	, constraintExpression(NULL)
{
	tag=Structure::ST_STRING;
	name=str;
	i=0;
	f=0.0f;
	b=false;
};

Structure::Structure(std::vector<Structure*> _list)
	: left(NULL)
	, right(NULL)
	, constraintExpression(NULL)
{
	tag=Structure::ST_LIST;
	bracketed=false;
	i=0;
	f=0.0f;
	b=false;

	size_t size=_list.size();
	for (size_t j=0; j<size; j++)
	{
		list.push_back(new Structure(*_list[j]));
	};
};

Structure::~Structure(void)
{
	Clear();
};

void Structure::Clear(void)
{
	size_t s=structures.size();
	for (size_t i=0; i<s; i++)
	{
		safe_delete(structures[i]);
	};
	structures.clear();

	s=list.size();
	for (size_t i=0; i<s; i++)
	{
		safe_delete(list[i]);
	};
	list.clear();

	tag=Structure::ST_STRUCTURE;
	bracketed=false;
	name=0;
	safe_delete(left);
	safe_delete(right);
	safe_delete(constraintExpression);
};

Structure::Structure(const Structure& s)
	: left(NULL)
	, right(NULL)
	, constraintExpression(NULL)
{
	operator=(s);
};

const Structure& Structure::operator=(const Structure& s)
{
	Clear();

	size_t size=s.structures.size();
	for (size_t j=0; j<size; j++)
	{
		structures.push_back(new Structure(*s.structures[j]));
	};

	size=s.list.size();
	for (size_t j=0; j<size; j++)
	{
		list.push_back(new Structure(*s.list[j]));
	};

	name=s.name;
	tag=s.tag;
	bracketed=s.bracketed;
	i=s.i;
	f=s.f;
	b=s.b;

	if (s.left!=NULL)
	{
		left=new Structure(*s.left);
	}
	if (s.right!=NULL)
	{
		right=new Structure(*s.right);
	}
	if (s.constraintExpression!=NULL)
	{
		constraintExpression=new Structure(*s.constraintExpression);
	}

	return *this;
};

std::string Structure::PrintRecursive(Structure* structure)
{
	switch (structure->tag)
	{
		case Structure::ST_STRUCTURE:
		{
			std::string str=GetString(structure->name);

			size_t s=structure->structures.size();
			if (s>0)
			{
				str=str+"(";
				for (size_t i=0; i<s; i++)
				{
					str=str+structure->structures[i]->Print();
					if ((i+1)<s)
					{
						str=str+",";
					}
				}
				str=str+")";
			}

			if (structure->constraintExpression!=NULL)
			{
				str=str+" {";
				size_t s=structure->constraintExpression->structures.size();
				if (s>0)
				{
					for (size_t i=0; i<s; i++)
					{
						str=str+structure->constraintExpression->structures[i]->Print();
						if ((i+1)<s)
						{
							str=str+",";
						}
					}
				}
				str=str+":";
				str=str+structure->constraintExpression->Print();
				str=str+"}";
			};

			return str;
		}
		case Structure::ST_VAR:
		{
			return GetString(structure->name);
		}
		case Structure::ST_STRING:
		{
			std::string str = "'" + GetString(structure->name) + "'";
			return str;
		}
		case Structure::ST_CUT:
		{
			return "!";
		}
		case Structure::ST_FAIL:
		{
			return "fail";
		}
		case Structure::ST_UNUSEDVAR:
		{
			return "_";
		}
		case Structure::ST_FLOAT:
		{
			return System::Float2Str(structure->f);
		}
		case Structure::ST_BOOL:
		{
			if (structure->b)
				return "true";
			else
				return "false";
		}
		case Structure::ST_INT:
		{
			return System::Int2Str(structure->i);
		}
		case Structure::ST_LIST:
		{
			std::string str="[";
			size_t s=list.size();
			for (size_t i=0; i<s; i++)
			{
				str=str+PrintRecursive(list[i]);
				if ((i+1)<s)
				{
					str=str+" | ";
				}
			};
			str=str+"]";
			return str;
		}
		default:
		{
			PreCond("unknown tag in structure"==NULL);
		}
	}
	return "";
};

std::string Structure::PrintExpression(Structure* expr)
{
	PreCond(expr!=NULL);

	switch (expr->tag)
	{
		case Structure::ST_ASSIGN:
		{
			return PrintExpression(expr->left) + " = " + PrintExpression(expr->right);
		}
		case Structure::ST_IS:
		{
			return PrintExpression(expr->left) + " is " + PrintExpression(expr->right);
		}
		case Structure::ST_PLUS:
		{
			return PrintExpression(expr->left) + " + " + PrintExpression(expr->right);
		}
		case Structure::ST_TIMES:
		{
			return PrintExpression(expr->left) + " * " + PrintExpression(expr->right);
		}
		case Structure::ST_DIVIDE:
		{
			return PrintExpression(expr->left) + " / " + PrintExpression(expr->right);
		}
		case Structure::ST_MINUS:
		{
			return PrintExpression(expr->left) + " - " + PrintExpression(expr->right);
		}
		case Structure::ST_LESS:
		{
			return PrintExpression(expr->left) + " < " + PrintExpression(expr->right);
		}
		case Structure::ST_LESSTHAN:
		{
			return PrintExpression(expr->left) + " =< " + PrintExpression(expr->right);
		}
		case Structure::ST_GREATER:
		{
			return PrintExpression(expr->left) + " > " + PrintExpression(expr->right);
		}
		case Structure::ST_GREATERTHAN:
		{
			return PrintExpression(expr->left) + " >= " + PrintExpression(expr->right);
		}
		case Structure::ST_EQUAL:
		{
			return PrintExpression(expr->left) + " =:= " + PrintExpression(expr->right);
		}
		case Structure::ST_NOTEQUAL:
		{
			return PrintExpression(expr->left) + " =\\= " + PrintExpression(expr->right);
		}
		case Structure::ST_IDENTICAL:
		{
			return PrintExpression(expr->left) + " == " + PrintExpression(expr->right);
		}
		case Structure::ST_NOTIDENTICAL:
		{
			return PrintExpression(expr->left) + " \\== " + PrintExpression(expr->right);
		}
		case Structure::ST_NOTUNIFIABLE:
		{
			return PrintExpression(expr->left) + " \\= " + PrintExpression(expr->right);
		}
		case Structure::ST_CUT:
		{
			return "!";
		}
		case Structure::ST_FAIL:
		{
			return "fail";
		}
		case Structure::ST_NOT:
		{
			return "\\+(" + PrintExpression(expr->left) + ")";
		}
		case Structure::ST_AND:
		{
			return PrintExpression(expr->left) + ", " + PrintExpression(expr->right);
		}
		case Structure::ST_OR:
		{
			return PrintExpression(expr->left) + " || " + PrintExpression(expr->right);
		}
		case Structure::ST_IMPLIES:
		{
			return PrintExpression(expr->left) + " :- " + PrintExpression(expr->right);
		}
		default:
		{
			return PrintRecursive(expr);
		}
	}
	return "";
};

std::string Structure::Print(void)
{
	return PrintExpression(this);
};

size_t Structure::AddString(const std::string& str)
{
	PreCond(str.size()>0);

	// variable?
	char firstChar = str[0];
	bool isVar = (firstChar=='_' || (firstChar>='A' && firstChar<='Z'));

	if (isVar)
	{
		size_t size=varArray.size();

		// does it exist in the frame?
		for (size_t i=varFrameMarker; i<size; i++)
		{
			if (str==varArray[i])
				return (i + VAR_PREFIX);
		}

		// no - add it
		varArray.push_back(str);
		return (size + VAR_PREFIX);
	}
	else
	{
		size_t size=stringArray.size();

		//! initialise string system with functions
		if (size==0)
		{
			stringArray.push_back("write");
			stringArray.push_back("nl");

			size=stringArray.size();
		};

		for (size_t i=0; i<size; i++)
		{
			if (str==stringArray[i])
				return i;
		}
		stringArray.push_back(str);
		return size;
	}
};

std::string Structure::GetString(size_t stringId)
{
	if ((stringId&VAR_PREFIX) > 0)
	{
		size_t realId = stringId - VAR_PREFIX;
		PreCond(realId<varArray.size());
		return varArray[realId];
	}
	else
	{
		PreCond(stringId<stringArray.size());
		return stringArray[stringId];
	}
};

void Structure::MarkVariableFrame(void)
{
	varFrameMarker = varArray.size();
};

