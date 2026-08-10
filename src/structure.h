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

class Structure
{
public:
	Structure(void);
	Structure(int i);
	Structure(float f);
	Structure(bool b);
	Structure(size_t str);
	Structure(std::vector<Structure*> list);

	//! virtual - Print() below is virtual, so structures may be deleted
	//! through a base pointer
	virtual ~Structure(void);
	Structure(const Structure&);
	const Structure& operator=(const Structure&);

	enum Predicate
	{
		WRITE_PREDICATE	=	0,
		NL_PREDICATE	=	1,
		SYSTEM_DEFINED	=	1,

		ST_NONE			=	100,
		ST_STRUCTURE,
		ST_FLOAT,
		ST_BOOL,
		ST_INT,
		ST_STRING,
		ST_VAR,
		ST_UNUSEDVAR,
		ST_AND,
		ST_OR,
		ST_IMPLIES,
		ST_NOT,
		ST_EQUAL,
		ST_NOTEQUAL,
		ST_LESS,
		ST_LESSTHAN,
		ST_GREATER,
		ST_GREATERTHAN,
		ST_ASSIGN,
		ST_PLUS,
		ST_TIMES,
		ST_DIVIDE,
		ST_MINUS,
		ST_LIST,
		ST_CUT,
		ST_REFERENCE,
		ST_HEADTAIL,
		ST_FAIL
	};

	//! clear my child structures
	void Clear(void);

	virtual std::string Print(void);

	//! helper functions for print
	std::string PrintRecursive(Structure* structure);
	//! helper functions for print
	std::string PrintExpression(Structure* expr);

	size_t					tag;

	size_t					name;
	std::vector<Structure*>	structures;
	std::vector<Structure*>	list;
	float					f;
	int						i;
	bool					b;

	Structure*				left;
	Structure*				right;
	Structure*				constraintExpression;


	enum
	{
		VAR_PREFIX = 0x80000000
	};

	// string management routines - we don't use strings
	// inside the engine -  rather substitute them with numbers
	static size_t AddString(const std::string& str);
	static std::string GetString(size_t stringId);

	// to keep variables unique - we reassign them numbers
	// between frames (individual queries and statements)
	// within a frame identical names get the same number assigned
	static void MarkVariableFrame(void);

private:
	static std::vector<std::string> stringArray;
	static std::vector<std::string> varArray;
	static size_t varFrameMarker;
};

