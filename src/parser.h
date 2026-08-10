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

#include "structure.h"

//
// { } notation below is used as a "constrained expression" which I introduced for testing and optimisation
//     only - don't use it unless you have to
//

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// comment				->	'%' text till eol
//
// query				->	'?-' subExpr '.' |
//							'?' subExpr '.'
//
// statementList		->	statement statementList |
//							{e}
//
// statement			->	structure ':-' subExpr '.' |
//							structure ':-' subExpr '{' variableExpression '}' '.' |
//							structure '.'
//
// subExpr				->	subExpr2 ';' subExpr |
//							subExpr2
//
// subExpr2				->	expression ',' subExpr2 |
//							expression
//
// expression			->	expression '!=' expression |
//							expression ',' expression |
//							expression '==' expression |
//							expression '<' expression |
//							expression '<=' expression |
//							expression '>' expression |
//							expression '>=' expression |
//							expression '=' expression |
//							expression '+' expression |
//							expression '-' expression |
//							expression '/' expression |
//							expression '*' expression |
//							'not' '(' expression ')' |
//							'(' expression ')' |
//							structure |
//							simpleStructure |
//							'!' |
//							'fail'
//
// structure			->	ident '(' simpleStructureList ')'
//
// variableList			->	variable ',' variableList |
//							variable
//
// variableExpression	->	variableExpression '==' variableExpression |
//							variableExpression '!=' variableExpression |
//							variableExpression '<' variableExpression |
//							variableExpression '<=' variableExpression |
//							variableExpression '>' variableExpression |
//							variableExpression '>=' variableExpression |
//							variableExpression '&&' variableExpression |
//							variableExpression '||' variableExpression |
//							'(' variableExpression ')' |
//							simpleVariable
//
// simpleVariable		->	variable |
//							int |
//							float |
//							string |
//							bool |
//							'[' ']' |
//							'[' list ']' |

// simpleStructureList	->	simpleStructure ',' simpleStructureList |
//							simpleStructure
//
// simpleStructure		->	structure |
//							simpleVariable
//
// list					->	expression ',' list |
//							variable '|' variable |
//							expression
//

////////////////////////////////////////////////////////////////////////////////////////////////////////////

class PrologParser
{
	const PrologParser& operator=(const PrologParser&);
	PrologParser(const PrologParser&);
public:
	PrologParser(void);
	~PrologParser(void);

	//! parse a query
	Structure* ParseQuery(const std::string& code);

	//! parse a list of statements
	std::vector<Structure*> ParseStatements(const std::string& code);

	//! parse an interpreter command
	Structure* ParseCommand(const std::string& command);

	//! get error
	bool GetError(std::string& errStr);

	enum
	{
		MAX_STRING_LENGTH	=	1024
	};

	enum PrologParserTokens
	{
		tEOF=0,

		//! terminals
		tTimes=1000,
		tPlus,
		tMinus,
		tDivide,
		tGreater,
		tGreaterThan,
		tLess,
		tLessThan,
		tEqual,
		tAssign,
		tComma,
		tAnd,
		tOr,
		tInt,
		tFloat,
		tString,
		tFullStop,
		tImplies,
		tSemiColon,
		tTrue,
		tFalse,
		tIdentifier,
		tVariable,
		tUnusedVariable,
		tLeft,
		tRight,
		tUnaryNot,
		tNotEqual,
		tQuestionMark,
		tSquareLeft,
		tSquareRight,
		tColon,
		tCurlyLeft,
		tCurlyRight,
		tListSeperator,
		tCut,
		tComment,
		tFail,

		tCommandList,
		tCommandDelete
	};

protected:
	//! setup parser for a new run
	void InitParser(void);

	//! used for testing only
	//! parse a block of code
	Structure* ParseStructure(const std::string& code);

	//! can only unget last token
	void UngetToken(void);

	//! set specific error
	void SetError(const std::string& errStr);

	//! get next token out of parser
	PrologParserTokens GetNextToken(void);

	//! identifier name checking
	bool CanStartIdentifier(char ch);

	//! identifier name checking
	bool IsValidForIdentifier(char ch);

	//! turn text into parse tree
	Structure* ParseStructure(void);

	//! parse a simple structure list
	std::vector<Structure*> ParseSimpleStructureList(void);

	//! parse a simple structure
	Structure* ParseSimpleStructure(void);

	//! parse a series of expressions seperated by &&
	Structure* SubExpr(void);

	//! parse a series of expressions seperated by &&
	Structure* SubExpr2(void);

	//! parse an expression and return an expression tree
	Structure* Expression(void);

	//! parse a statement
	Structure* ParseStatement(void);

	//! parse query part of grammar
	Structure* ParseQuery(void);

	//! parse statements
	std::vector<Structure*> ParseStatements(void);

	//! parse variable list
	std::vector<Structure*> VariableList(void);

	//! parse a variable expression
	Structure* VariableExpression(void);

	//! parse a simple variable structure
	Structure* SimpleVariable(void);

	//! parse a list
	std::vector<Structure*> ParseList(bool& specialType);

	//! parse an interpreter command
	Structure* ParseCommand(void);

private:
	bool IsVariableBinaryOperator(PrologParserTokens token);
	bool IsBinaryOperator(PrologParserTokens token);
	size_t TokenToOperator(PrologParserTokens t);

private:
	//! text to parse
	char*				buffer;
	size_t				bufferSize;

	//! index into text right now
	size_t				index;

	//! for undoing last character get
	size_t				prevIndex;

	//! current token
	PrologParserTokens	token;

	//! yy values
	std::string			yyString;
	int					yyInt;
	float				yyFloat;

	//! has there been a parser error
	bool				error;

	//! error description
	std::string			errStr;

	const char					quoteChar;
	const PrologParserTokens	endOfStatementToken;
	const PrologParserTokens	ANDToken;
	const PrologParserTokens	ORToken;
};

