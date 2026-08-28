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

#include "parser.h"

//////////////////////////////////////////////////////////////////

PrologParser::PrologParser(void)
	: buffer(NULL)
	, quoteChar('\'')
	, endOfStatementToken(tFullStop)
	, ANDToken(tComma)
	, ORToken(tSemiColon)
{
	InitParser();
}

PrologParser::~PrologParser(void)
{
	InitParser();
}

void PrologParser::InitParser(void)
{
	safe_delete_array(buffer);
	buffer=NULL;
	error=false;
	index=0;
	prevIndex=0;
	yyInt=0;
	yyString.clear();
	token=tEOF;
};

void PrologParser::UngetToken(void)
{
	index=prevIndex;
};

bool PrologParser::CanStartIdentifier(char ch)
{
	return ((ch>='a' && ch<='z') ||
			(ch>='A' && ch<='Z'));
};

bool PrologParser::IsValidForIdentifier(char ch)
{
	return (CanStartIdentifier(ch) || (ch>='0' && ch<='9') || ch=='_');
};

//! a keyword only counts when the text that follows it cannot be part of
//! an identifier - otherwise "is" would be found at the front of "island"
//! and "list" at the front of "listen"
bool PrologParser::KeywordAt(const char* kw)
{
	size_t len=strlen(kw);
	if (index+len>bufferSize)
		return false;
	if (strncmp(&buffer[index],kw,len)!=0)
		return false;
	return !IsValidForIdentifier(buffer[index+len]);
};

PrologParser::PrologParserTokens PrologParser::GetNextToken(void)
{
	//! ready to rewind
	prevIndex=index;

	//! passed end?
	if (index>=bufferSize)
		return tEOF;

	bool whiteSpace=false;
	do
	{
		whiteSpace=false;

		switch (buffer[index])
		{
			case ' ':
			case 10:
			case 13:
			case '\t':
			{
				whiteSpace=true;
				index++;
				break;
			}
			case '?':
			{
				index++;

				//! "?-" is the standard prolog query marker - take the '-'
				//! as part of it rather than as a minus sign
				if (index<bufferSize && buffer[index]=='-')
				{
					index++;
					return tQuestionMark;
				}
				SetError("queries start with '?-'");
				return tEOF;
			}
			case '_':
			{
				index++;
				return tUnusedVariable;
			};
			case '/':
			{
				// comment?
				int oldindex=index;
				if (buffer[index+1]=='*')
				{
					index+=2;
					bool found = false;
					while (index<bufferSize && !found)
					{
						found = (buffer[index]=='*' && buffer[index+1]=='/');
						if (!found)
							index++;
					}
					if (!found)
					{
						index=oldindex;
						SetError("unterminated comment /* ... found");
						return tEOF;
					}
					whiteSpace = true;
				}
				else
				{
					index++;
					return tDivide;
				}
				break;
			}
			case '%':
			{
				// skip comments
				while (index<bufferSize)
				{
					if (buffer[index]==0x0d || buffer[index]==0x0a)
						break;
					index++;
				}
				whiteSpace = true;
				break;
			}
			case '!':
			{
				index++;

				//! catch the pre-standard spelling of \= with a pointer at
				//! the right one rather than a puzzling parse error later
				if (buffer[index]=='=')
				{
					SetError("'!=' is not prolog - use '\\=' (or '=\\=' for arithmetic)");
					return tEOF;
				}
				return tCut;
			}
			case ':':
			{
				index++;
				if (buffer[index]=='-')
				{
					index++;
					return tImplies;
				}
				else
				{
					return tColon;
				}
				break;
			}
			case '(':
			{
				index++;
				return tLeft;
			}
			case ')':
			{
				index++;
				return tRight;
			}
			case '|':
			{
				index++;
				return tListSeperator;
			}
			case '[':
			{
				index++;
				return tSquareLeft;
			}
			case ']':
			{
				index++;
				return tSquareRight;
			}
			case '{':
			{
				index++;
				return tCurlyLeft;
			}
			case '}':
			{
				index++;
				return tCurlyRight;
			}
			case '*':
			{
				index++;
				return tTimes;
			}
			case '+':
			{
				index++;
				return tPlus;
			}
			case '-':
			{
				index++;
				if (buffer[index]>='0' && buffer[index]<='9')
				{
					index--;
					break;
				}
				else
					return tMinus;
			}
			case '.':
			{
				index++;
				return tFullStop;
			}
			case ',':
			{
				index++;
				return tComma;
			}
			case ';':
			{
				index++;
				return tSemiColon;
			}
			case '\\':
			{
				index++;

				//! "\==" not identical, "\=" not unifiable
				if (buffer[index]=='=')
				{
					index++;
					if (buffer[index]=='=')
					{
						index++;
						return tNotIdentical;
					}
					return tNotUnifiable;
				}

				//! "\+" - negation as failure
				if (buffer[index]=='+')
				{
					index++;
					return tUnaryNot;
				}

				SetError("expected '\\=', '\\==' or '\\+' after '\\'");
				return tEOF;
			}
			case '=':
			{
				index++;

				//! "=<" - less or equal
				if (buffer[index]=='<')
				{
					index++;
					return tLessThan;
				}

				//! "=:=" - arithmetic equality
				if (buffer[index]==':' && buffer[index+1]=='=')
				{
					index+=2;
					return tEqual;
				}

				//! "=\=" - arithmetic inequality
				if (buffer[index]=='\\' && buffer[index+1]=='=')
				{
					index+=2;
					return tNotEqual;
				}

				//! "==" - term identity
				if (buffer[index]=='=')
				{
					index++;
					return tIdentical;
				}

				//! a lone "=" unifies its two sides
				return tAssign;
			}
			case '>':
			{
				index++;
				if (buffer[index]!='=')
				{
					return tGreater;
				}
				else
				{
					index++;
					return tGreaterThan;
				}
				break;
			}
			case '<':
			{
				index++;

				//! same courtesy for the pre-standard less-or-equal
				if (buffer[index]=='=')
				{
					SetError("'<=' is not prolog - use '=<'");
					return tEOF;
				}
				return tLess;
			}
		};
	}
	while (whiteSpace);

	if (index>=bufferSize)
		return tEOF;

	//! "is" evaluates its right hand side; "=" (tAssign) unifies.
	//! there is no "not" keyword - negation is spelled \+
	if (KeywordAt("is"))
	{
		index+=2;
		return tIs;
	}

	if (KeywordAt("true"))
	{
		index+=4;
		return tTrue;
	}
	if (KeywordAt("fail"))
	{
		index+=4;
		return tFail;
	}
	if (KeywordAt("list"))
	{
		index+=4;
		return tCommandList;
	}
	if (KeywordAt("delete"))
	{
		index+=6;
		return tCommandDelete;
	}
	if (KeywordAt("false"))
	{
		index+=5;
		return tFalse;
	}

	// number?
	if ((buffer[index]=='-' && buffer[index+1]>='0' && buffer[index+1]<='9') || (buffer[index]>='0' && buffer[index]<='9'))
	{
		bool bMinus=false;
		if (buffer[index]=='-')
		{
			index++;
			bMinus=true;
		}

		int num=0;
		do
		{
			num=num*10 + int(buffer[index++]-'0');
		}
		while (buffer[index]>='0' && buffer[index]<='9');

		if (bMinus)
			yyInt=-num;
		else
			yyInt=num;

		if (buffer[index]=='.' && buffer[index+1]>='0' && buffer[index+1]<='9')
		{
			index++;
			float mantissa=0.0f;
			float dividor=0.1f;
			do
			{
				mantissa=mantissa + float(buffer[index++]-'0') * dividor;
				dividor=dividor*0.1f;
			}
			while (buffer[index]>='0' && buffer[index]<='9');
			yyFloat=float(num)+mantissa;
			yyInt=0;

			if (bMinus)
				yyFloat=-yyFloat;

			return tFloat;
		}
		return tInt;
	}

	//! string?
	if (buffer[index]==quoteChar)
	{
		index++;

		char buf[MAX_STRING_LENGTH+1];
		buf[0]=0;
		size_t textIndex=0;
		do
		{
			if (buffer[index]!=quoteChar)
			{
				buf[textIndex++]=buffer[index++];
				buf[textIndex]=0;
			}
		}
		while (textIndex<MAX_STRING_LENGTH && buffer[index]!=quoteChar);

		//! proper string?
		if (buffer[index]!=quoteChar)
		{
			SetError("String too long");
			return tEOF;
		}

		index++;

		//! valid string
		yyString=buf;
		return tString;
	}

	//! identifier ( [a..z|A..Z]
	if (CanStartIdentifier(buffer[index]))
	{
		char buf[256];
		buf[0]=buffer[index++];
		buf[1]=0;
		size_t bufferIndex=1;
		do
		{
			if (IsValidForIdentifier(buffer[index]))
			{
				buf[bufferIndex++]=buffer[index++];
				buf[bufferIndex]=0;
			}
		}
		while (IsValidForIdentifier(buffer[index]) && bufferIndex<255);

		if (bufferIndex>255)
		{
			SetError("Identifier too long");
			return tEOF;
		}
		yyString=buf;
		if ((buf[0]>='A' && buf[0]<='Z'))
		{
			return tVariable;
		}
		return tIdentifier;
	}

	if (index>=bufferSize)
		return tEOF;

	UngetToken();
	SetError("unknown token");
	return tEOF;
};

void PrologParser::SetError(const std::string& _errStr)
{
	// count the number of lines up to now
	int line=1;
	int charCntr=1;
	for (size_t i=0; i<index; i++)
	{
		if (buffer[i]=='\n') 
		{
			charCntr=1;
			line++;
		}
		else
		{
			charCntr++;
		}
	}
	errStr=_errStr+" (line "+System::Int2Str(line)+", character "+System::Int2Str(charCntr)+")";
	error=true;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<Structure*> PrologParser::ParseStatements(const std::string& code)
{
	std::vector<Structure*> list;

	InitParser();

	bufferSize=code.size();
	if (bufferSize==0)
		return list;

	buffer=new char[bufferSize+1];
	PostCond(buffer!=NULL);
	strcpy(buffer,code.c_str());

	list=ParseStatements();

	safe_delete_array(buffer);
	bufferSize=0;
	buffer=NULL;

	return list;
};

Structure* PrologParser::ParseQuery(const std::string& code)
{
	InitParser();

	Structure::MarkVariableFrame();

	bufferSize=code.size();
	if (bufferSize==0)
		return NULL;

	buffer=new char[bufferSize+1];
	PostCond(buffer!=NULL);
	strcpy(buffer,code.c_str());

	Structure* ret=ParseQuery();

	safe_delete_array(buffer);
	bufferSize=0;
	buffer=NULL;

	return ret;
};

Structure* PrologParser::ParseStructure(const std::string& code)
{
	InitParser();

	bufferSize=code.size();
	if (bufferSize==0)
		return NULL;

	buffer=new char[bufferSize+1];
	PostCond(buffer!=NULL);
	strcpy(buffer,code.c_str());

	Structure* ret=ParseStructure();

	safe_delete_array(buffer);
	bufferSize=0;
	buffer=NULL;

	return ret;
};

Structure* PrologParser::ParseCommand(const std::string& command)
{
	InitParser();

	bufferSize=command.size();
	if (bufferSize==0)
		return NULL;

	buffer=new char[bufferSize+1];
	PostCond(buffer!=NULL);
	strcpy(buffer,command.c_str());

	Structure* ret=ParseCommand();

	safe_delete_array(buffer);
	bufferSize=0;
	buffer=NULL;

	return ret;
};

//! get error
bool PrologParser::GetError(std::string& _errStr)
{
	_errStr=errStr;
	return error;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<Structure*> PrologParser::ParseStatements(void)
{
	std::vector<Structure*> list;
	do
	{
		Structure::MarkVariableFrame();

		token=GetNextToken();
		if (token==tIdentifier)
		{
			UngetToken();
			Structure* s=ParseStatement();
			if (error) break;

			list.push_back(s);
		}
		else if (token!=tEOF)
		{
			SetError("unexpected token");
			break;
		}
	}
	while (token!=tEOF);

	if (error)
	{
		size_t size=list.size();
		for (size_t i=0; i<size; i++)
		{
			safe_delete(list[i]);
		}
		list.clear();
	}
	return list;
};

// query				->	'?' subExpr '.'
Structure* PrologParser::ParseQuery(void)
{
	token=GetNextToken();
	if (token!=tQuestionMark)
	{
		SetError("query must start with '?-'");
		return NULL;
	};

	Structure* s1=SubExpr();
	if (error) { return NULL; }

	token=GetNextToken();
	if (token!=endOfStatementToken)
	{
		SetError("query must end with '.'");
		safe_delete(s1);
		return NULL;
	};

	return s1;
};

// statement			->	structure ':-' subExpr '.' |
//							structure ':-' subExpr '{' variableExpression '}' '.' |
//							structure '.'
Structure* PrologParser::ParseStatement(void)
{
	Structure* s1=ParseStructure();
	if (error) { return NULL; }

	token=GetNextToken();
	if (token!=endOfStatementToken && token!=tImplies)
	{
		SetError("statement must end in either '.' or ':-'");
		safe_delete(s1);
		return NULL;
	}
	if (token==endOfStatementToken)
	{
		return s1;
	}

	Structure* e=SubExpr();

	token=GetNextToken();
	if (token!=endOfStatementToken && token!=tCurlyLeft)
	{
		SetError(":- expression must be followed by '.'");
		safe_delete(s1);
		safe_delete(e);
		return NULL;
	}

	Structure* s=new Structure();
	s->tag=Structure::ST_IMPLIES;
	s->left=s1;
	s->right=e;

	//! does it have a constraint section?
	if (token!=tCurlyLeft)
	{
		return s;
	}
	else
	{
		Structure* variableExpression=VariableExpression();
		if (error)
		{
			return NULL;
		}

		token=GetNextToken();
		if (token!=tCurlyRight)
		{
			SetError("expected '}'");
			safe_delete(s);
			return NULL;
		}

		s->constraintExpression=variableExpression;

		token=GetNextToken();
		if (token!=endOfStatementToken)
		{
			SetError(":- expression must be followed by '.'");
			safe_delete(s1);
			return NULL;
		}

		return s;
	}
};

// subExpr				->	subExpr2 ';' subExpr |
//							subExpr2
//
Structure* PrologParser::SubExpr(void)
{
	Structure* expr = SubExpr2();
	if (error) return NULL;
	token = GetNextToken();
	if (token==ORToken)
	{
		Structure* expr2 = SubExpr();
		if (error) { safe_delete(expr); return NULL; }
		Structure* subExpr = new Structure();
		subExpr->tag = TokenToOperator(ORToken);
		subExpr->left = expr;
		subExpr->right = expr2;
		return subExpr;
	}
	else
	{
		UngetToken();
		return expr;
	}
};

// subExpr2				->	expression ',' subExpr2 |
//							expression
//
Structure* PrologParser::SubExpr2(void)
{
	Structure* expr = Expression();
	if (error) return NULL;
	token = GetNextToken();
	if (token==ANDToken)
	{
		Structure* expr2 = SubExpr2();
		if (error) { safe_delete(expr); return NULL; }
		Structure* subExpr = new Structure();
		subExpr->tag = TokenToOperator(ANDToken);
		subExpr->left = expr;
		subExpr->right = expr2;
		return subExpr;
	}
	else
	{
		UngetToken();
		return expr;
	}
};

int PrologParser::OperatorPrecedence(size_t tag)
{
	switch (tag)
	{
		case Structure::ST_TIMES:
		case Structure::ST_DIVIDE:
		{
			return 1;
		}
		case Structure::ST_PLUS:
		case Structure::ST_MINUS:
		{
			return 2;
		}
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
		{
			return 3;
		}
	}
	return 0;
};

Structure* PrologParser::Combine(size_t op,Structure* lhs,Structure* rhs)
{
	int mine=OperatorPrecedence(op);
	int theirs=OperatorPrecedence(rhs->tag);

	//! rotate while the rhs root binds looser than this operator, or
	//! equally loose among the arithmetic operators, which associate to
	//! the left: "2 - 3 - 4" is (2 - 3) - 4.  recursion carries the lhs
	//! down to its proper place: "1 - 2 - 3 - 4" needs it at the bottom
	if (!rhs->bracketed && theirs>0 &&
		(theirs>mine || (theirs==mine && mine<3)))
	{
		rhs->left=Combine(op,lhs,rhs->left);
		return rhs;
	}

	Structure* e=new Structure();
	e->tag=op;
	e->left=lhs;
	e->right=rhs;
	return e;
};

bool PrologParser::IsVariableBinaryOperator(PrologParser::PrologParserTokens token)
{
	return (token==tEqual || token==tNotEqual || token==tIdentical ||
			token==tNotIdentical || token==tNotUnifiable || token==tLess ||
			token==tLessThan || token==tGreater || token==tGreaterThan);
};

bool PrologParser::IsBinaryOperator(PrologParser::PrologParserTokens token)
{
	return (token==tEqual || token==tNotEqual || token==tIdentical ||
			token==tNotIdentical || token==tNotUnifiable || token==tLess ||
			token==tLessThan || token==tGreater || token==tGreaterThan ||
			token==tAssign || token==tIs || token==tPlus || token==tMinus ||
			token==tDivide || token==tTimes);
};

size_t PrologParser::TokenToOperator(PrologParser::PrologParserTokens t)
{
	switch (t)
	{
		case tPlus:
		{
			return Structure::ST_PLUS;
		}
		case tMinus:
		{
			return Structure::ST_MINUS;
		}
		case tDivide:
		{
			return Structure::ST_DIVIDE;
		}
		case tTimes:
		{
			return Structure::ST_TIMES;
		}
		case tAssign:
		{
			return Structure::ST_ASSIGN;
		}
		case tIs:
		{
			return Structure::ST_IS;
		}
		case tIdentical:
		{
			return Structure::ST_IDENTICAL;
		}
		case tNotIdentical:
		{
			return Structure::ST_NOTIDENTICAL;
		}
		case tNotUnifiable:
		{
			return Structure::ST_NOTUNIFIABLE;
		}
		case tEqual:
		{
			return Structure::ST_EQUAL;
		}
		case tNotEqual:
		{
			return Structure::ST_NOTEQUAL;
		}
		case tLess:
		{
			return Structure::ST_LESS;
		}
		case tLessThan:
		{
			return Structure::ST_LESSTHAN;
		}
		case tGreater:
		{
			return Structure::ST_GREATER;
		}
		case tGreaterThan:
		{
			return Structure::ST_GREATERTHAN;
		}
		default:
		{
			break;
		}
	}
	if (t==ORToken)
	{
		return Structure::ST_OR;
	}
	else if (t==ANDToken)
	{
		return Structure::ST_AND;
	}
	else
	{
		PreCond("unknown operator token"==NULL);
		return Structure::ST_AND;
	}
};

// expression			->	expression '!=' expression |
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
Structure* PrologParser::Expression(void)
{
	token=GetNextToken();
	if (token==tLeft)
	{
		Structure* s1=Expression();
		token=GetNextToken();
		if (token!=tRight)
		{
			if (IsBinaryOperator(token))
			{
				PrologParserTokens op=token;
				Structure* s2=Expression();
				if (error) { safe_delete(s1); return NULL; }

				Structure* e=Combine(TokenToOperator(op),s1,s2);

				token=GetNextToken();
				if (token!=tRight)
				{
					SetError("expected ')'");
					safe_delete(e);
					return NULL;
				}

				return e;
			}
			else
			{
				SetError("expected ')'");
				safe_delete(s1);
				return NULL;
			}
		}

		token=GetNextToken();

		//! whatever the brackets held is one operand from here on
		s1->bracketed=true;

		if (IsBinaryOperator(token))
		{
			PrologParserTokens op=token;
			Structure* s2=Expression();
			if (error) { safe_delete(s1); return NULL; }

			return Combine(TokenToOperator(op),s1,s2);
		}
		else
		{
			UngetToken();
		}
		return s1;
	}
	else if (token==tCut)
	{
		Structure* s=new Structure();
		s->tag=Structure::ST_CUT;
		return s;
	}
	else if (token==tFail)
	{
		Structure* s=new Structure();
		s->tag=Structure::ST_FAIL;
		return s;
	}
	else if (token==tUnaryNot)
	{
		token=GetNextToken();

		//! the negated goal may be bracketed - \+(goal) - or bare - \+ goal
		Structure* e;
		if (token==tLeft)
		{
			e=Expression();
			if (error) { return NULL; }

			//! Expression() stops before the closing bracket - it has to be
			//! fetched before it can be checked (as the '(' expression ')'
			//! case above does)
			token=GetNextToken();
			if (token!=tRight)
			{
				SetError("')' missing on \\+()");
				safe_delete(e);
				return NULL;
			}
		}
		else
		{
			UngetToken();
			e=Expression();
			if (error) { return NULL; }
		}

		Structure* s=new Structure();
		s->tag=Structure::ST_NOT;
		s->left=e;
		return s;
	}
	else
	{
		PrologParserTokens temp=token;
		UngetToken();

		Structure* s1;
		if (temp==tIdentifier)
		{
			s1=ParseStructure();
		}
		else
		{
			s1=ParseSimpleStructure();
		}

		if (error) return NULL;

		bool negated=false;
		token=GetNextToken();
		if ((token==tInt && yyInt<0) || (token==tFloat && yyFloat<0))
		{
			negated=true;
			UngetToken();
			token=tMinus;
		};

		if (IsBinaryOperator(token))
		{
			PrologParserTokens op=token;
			Structure* s2=Expression();
			if (error) { safe_delete(s1); return NULL; }

			if (negated)
			{
				//! the lexer read "- 5" as the literal -5 and the minus was
				//! re-synthesised above, so the sign is carried twice.  the
				//! literal is the leftmost leaf of whatever s2 parsed into -
				//! in "3 - 5 + 2" it sits at the bottom left of (-5) + 2
				Structure* leaf=s2;
				while (OperatorPrecedence(leaf->tag)>0 && !leaf->bracketed)
					leaf=leaf->left;
				if (leaf->tag==Structure::ST_INT)
					leaf->i=-leaf->i;
				else if (leaf->tag==Structure::ST_FLOAT)
					leaf->f=-leaf->f;
			}

			return Combine(TokenToOperator(op),s1,s2);
		}
		else
		{
			UngetToken();
			return s1;
		}
	}
};

// structure			->	ident '(' simpleStructureList ')' |
Structure* PrologParser::ParseStructure(void)
{
	token=GetNextToken();
	if (token!=tIdentifier)
	{
		UngetToken();
		return NULL;
	}
	std::string id=yyString;

	token=GetNextToken();
	if (token!=tLeft)
	{
		//! no left bracket?  -> arity==0
		UngetToken();
		Structure* s=new Structure();
		s->name=Structure::AddString(id);
		return s;
	}

	std::vector<Structure*> list=ParseSimpleStructureList();
	if (error) { return NULL; }

	token=GetNextToken();
	if (token!=tRight)
	{
		SetError("expected ')'");
		return NULL;
	}

	Structure* s=new Structure();
	s->name=Structure::AddString(id);
	s->structures=list;

	return s;
};

// variableList			->	variable ',' variableList |
//							variable
std::vector<Structure*> PrologParser::VariableList(void)
{
	std::vector<Structure*> list;
	do
	{
		token=GetNextToken();
		if (token!=tVariable && token!=tUnusedVariable)
		{
			SetError("expected variable");
			UngetToken();
			break;
		}

		if (token==tVariable)
		{
			Structure* s=new Structure(Structure::AddString(yyString));
			s->tag=Structure::ST_VAR;
			list.push_back(s);
		}
		else
		{
			Structure* s=new Structure();
			s->tag=Structure::ST_UNUSEDVAR;
			list.push_back(s);
		}

		token=GetNextToken();
		if (token!=tComma)
		{
			UngetToken();
			break;
		}
	}
	while (!error);

	if (error)
	{
		size_t size=list.size();
		for (size_t i=0; i<size; i++)
		{
			safe_delete(list[i]);
		};
		list.clear();
	}
	return list;
};

// simpleStructureList	->	simpleStructure ',' simpleStructureList |
//							simpleStructure
std::vector<Structure*> PrologParser::ParseSimpleStructureList(void)
{
	std::vector<Structure*> list;
	do
	{
		Structure* s=ParseSimpleStructure();
		if (error) break;

		if (s!=NULL)
			list.push_back(s);

		token=GetNextToken();
		if (token!=tComma)
		{
			UngetToken();
			break;
		}
	}
	while (!error);

	if (error)
	{
		size_t size=list.size();
		for (size_t i=0; i<size; i++)
		{
			safe_delete(list[i]);
		};
		list.clear();
	}
	return list;
};


// simpleStructure		->	structure |
//							simpleVariable
Structure* PrologParser::ParseSimpleStructure(void)
{
	token=GetNextToken();
	if (token==tIdentifier)
	{
		UngetToken();
		return ParseStructure();
	}
	UngetToken();
	return SimpleVariable();
};

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
Structure* PrologParser::VariableExpression(void)
{
	token=GetNextToken();
	if (token==tLeft)
	{
		Structure* s1=VariableExpression();
		token=GetNextToken();
		if (token!=tRight)
		{
			if (IsVariableBinaryOperator(token))
			{
				PrologParserTokens op=token;
				Structure* s2=VariableExpression();
				if (error) { safe_delete(s1); return NULL; }

				Structure* e=new Structure();
				e->tag=TokenToOperator(op);
				e->left=s1;
				e->right=s2;

				token=GetNextToken();
				if (token!=tRight)
				{
					SetError("expected ')'");
					safe_delete(e);
					return NULL;
				}

				return e;
			}
			else
			{
				SetError("expected ')'");
				safe_delete(s1);
				return NULL;
			}
		}
		return s1;
	}
	else
	{
		UngetToken();

		Structure* s1=SimpleVariable();
		if (error) { return NULL; }

		token=GetNextToken();
		if (IsVariableBinaryOperator(token))
		{
			PrologParserTokens op=token;
			Structure* s2=VariableExpression();
			if (error) { safe_delete(s1); return NULL; }

			Structure* e=new Structure();
			e->tag=TokenToOperator(op);
			e->left=s1;
			e->right=s2;

			return e;
		}
		else
		{
			UngetToken();
			return s1;
		}
	}
};

// simpleVariable		->	variable |
//							int |
//							float |
//							string |
//							bool
//							'[' ']' |
//							'[' list ']'
Structure* PrologParser::SimpleVariable(void)
{
	token=GetNextToken();
	if (token!=tVariable && token!=tInt && token!=tFloat && token!=tString && token!=tTrue && token!=tFalse &&
		token!=tUnusedVariable && token!=tSquareLeft)
	{
		SetError("Expected simple variable type (var,int,float,string, list or boolean type)");
		return NULL;
	}
	switch (token)
	{
		case tVariable:
		{
			Structure* s=new Structure(Structure::AddString(yyString));
			s->tag=Structure::ST_VAR;
			return s;
		}
		case tUnusedVariable:
		{
			Structure* s=new Structure();
			s->tag=Structure::ST_UNUSEDVAR;
			return s;
		}
		case tString:
		{
			Structure* s=new Structure(Structure::AddString(yyString));
			s->tag=Structure::ST_STRING;
			return s;
		}
		case tInt:
		{
			return new Structure(yyInt);
		}
		case tFloat:
		{
			return new Structure(yyFloat);
		}
		case tTrue:
		case tFalse:
		{
			return new Structure(token==tTrue);
		}
		case tSquareLeft:
		{
			token=GetNextToken();
			if (token==tSquareRight)
			{
				Structure* s=new Structure();
				s->tag=Structure::ST_LIST;
				return s;
			}
			else
			{
				UngetToken();
				bool specialList = false;
				std::vector<Structure*> list=ParseList(specialList);
				if (error) return NULL;

				Structure* s = new Structure(list);
				if (specialList)
				{
					s->tag = Structure::ST_HEADTAIL;
				}
				
				for (size_t i=0; i<list.size(); i++)
				{
					safe_delete(list[i]);
				}

				token=GetNextToken();
				if (token!=tSquareRight)
				{
					SetError("list must close with ']'");
					safe_delete(s);
					return NULL;
				}

				return s;
			}
		}
		default:
		{
			PostCond("unknown simple type"==NULL);
		}
	};
	return NULL;
};

// list					->	expression ',' list |
//							variable '|' variable |
//							expression
std::vector<Structure*> PrologParser::ParseList(bool& specialType)
{
	std::vector<Structure*> list;
	specialType = false;
	do
	{
		Structure* sv=Expression();
		if (error) break;

		list.push_back(sv);

		// is this a special list [var|var]
		token=GetNextToken();
		if (token==tListSeperator)
		{
			if (list.size()!=1)
			{
				SetError("[var|var] list type can only contain two items");
				break;
			}

			sv=Expression();
			if (error) break;

			if (list[0]->tag!=Structure::ST_VAR || sv->tag!=Structure::ST_VAR)
			{
				SetError("[var|var] list type can only contain two variables");
				break;
			}

			list.push_back(sv);
			specialType=true;
			return list;
		}
		else if (token!=tComma)
		{
			UngetToken();
			break;
		}
	}
	while (true);

	if (error)
	{
		for (size_t i=0; i<list.size(); i++)
		{
			safe_delete(list[i]);
		};
		list.clear();
	};

	return list;
};

//! interpreter commands
//! list [int[[-][int]]]
//! delete [int[[-][int]]]
Structure* PrologParser::ParseCommand(void)
{
	PrologParserTokens commandToken;

	token=GetNextToken();
	switch (token)
	{
		case tCommandDelete:
		{
			commandToken = token;
			break;
		}
		case tCommandList:
		{
			commandToken = token;
			break;
		}
		default:
		{
			SetError("Unknown command");
			return NULL;
		}
	}

	int start=0;
	int end=-1;

	token=GetNextToken();
	if (token==tInt)
	{
		start=yyInt;
		token=GetNextToken();
		if (token==tMinus || (token==tInt && yyInt<0))
		{
			if (token==tInt)
			{
				end=-yyInt;
			}
			else
			{
				token=GetNextToken();
				if (token==tInt)
				{
					end=yyInt;
				}
			}
		}
		else
		{
			end = start;
		}
	}

	Structure* s=new Structure();
	s->tag=commandToken;
	s->i=start;
	s->f=float(end);

	return s;
};

