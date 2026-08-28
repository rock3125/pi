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

#include "query.h"

//////////////////////////////////////////////////////////////////

Query::Query(std::vector<Node*> _query,DataBase& _database,int level)
	//! listed in declaration order - members are initialised in the order
	//! they are declared, not the order they appear here
	: query(_query)
	, database(_database)
{
	PreCond(query.size()>0);

	queryStart=int(Engine::GetStackSize());
	for (size_t i=0; i<query.size(); i++)
	{
		Engine::AddToStack(new Node(*query[i]));
	};

	Trace2(level,"QUERY: %s",Engine::PrettyPrint(queryStart).c_str());
};

Query::~Query(void)
{
};

bool Query::Execute(int level,int pc,Set& bindings,bool& cut)
{
	Node* n=Engine::GetStack(pc);

	if (pc>MAX_PC)
	{
		System::printf("infinite recursion detected or system too complex (stack too deep)");
		return false;
	}

	Trace2(level,"EXECUTE :%s",Engine::PrettyPrint(pc).c_str());

	switch (n->type)
	{
		case Structure::ST_VAR:
		{
			return false;
		}

		case Structure::ST_CUT:
		{
			cut = true;
			return true;
		}

		case Structure::ST_FAIL:
		{
			return false;
		}

		case Structure::ST_NOT:
		{
			//! negation as failure - the goal is proved, and "not" succeeds
			//! exactly when that proof fails.  a successful "not" binds
			//! nothing, so no bindings are published either way.
			Set s;

			//! a cut inside the negated goal is local to it and must not
			//! prune the clause that contains the not()
			bool innerCut=false;

			bool success=!Execute(level+1,pc+1,s,innerCut);

			Trace1(level,success?"NOT :yes":"NOT :no");
			return success;
		}

		case Structure::ST_EQUAL:
		case Structure::ST_NOTEQUAL:
		case Structure::ST_LESS:
		case Structure::ST_LESSTHAN:
		case Structure::ST_GREATER:
		case Structure::ST_GREATERTHAN:
		{
			Node result;
			if (Engine::EvaluateExpression(pc,result))
			{
				return result.b;
			}
			return false;
		}
		case Structure::ST_IDENTICAL:
		case Structure::ST_NOTIDENTICAL:
		{
			//! "==" walks both terms structurally and binds nothing, which
			//! is what separates it from "=:=" (evaluates) and "=" (binds)
			int lhs=pc+1;
			bool same=Identical(lhs,lhs+SizeOfClause(lhs));
			return (n->type==Structure::ST_IDENTICAL) ? same : !same;
		}
		case Structure::ST_NOTUNIFIABLE:
		{
			//! "\=" - try the unification for real, then take back every
			//! variable it managed to bind.  the attempt may get part way
			//! before failing, so the undo runs whichever way it went
			int lhs=pc+1;
			BindingList b;
			bool unifies=Unify(lhs,lhs+SizeOfClause(lhs),b);

			for (size_t i=0; i<b.size(); i++)
			{
				Engine::GetStack()[b[i]->lhs]->fu=-1;
			}
			return !unifies;
		}
		case Structure::ST_IMPLIES:
		{
			size_t size1=SizeOfClause(pc+1);
			size_t size2=SizeOfClause(pc+1+size1);
			int index=pc+size1+1;

			//! the clause spans the implies node itself plus the head and the
			//! body, so size1+size2 is one short and used to leave the very
			//! last node of every body unlinked - in "r(Z) :- w(M), Z = M."
			//! that is the final M, which stayed unbound no matter what w did
			Engine::ForwardBind(pc,1+size1+size2);
			Set s1;
			bool success=Execute(level+1,index,s1,cut);
			if (success)
			{
				Engine::FilterSet(pc,s1);
				bindings=s1;
			}
			Trace1(level+1,success?"EXEC :yes":"EXEC :no");
			return success;
		}
		case Structure::ST_AND:
		{
			std::vector<Node*> saveStack;
			std::vector<Node*> newStack;
			Set s,newSet;

			int index=pc+1;

			Trace2(level,"AND_a  :%s",Engine::PrettyPrint(index).c_str());

			bool success=false;
			if (Execute(level+1,index,s,cut))
			{
				Trace1(level+1,"EXEC :yes");
				index+=SizeOfClause(index);

				// for each set of successful bindings - execute the second part
				// of the AND equation
				size_t size=s.size();
				if (size>0)
				{
					for (size_t i=0; i<size; i++)
					{
						//! create fake forward bindings for each
						//! set of the solutions to forward a possible
						//! solution onto the stack and unify it correctly
						//! this way we bind the second AND's variables to the
						//! results of the first AND (as we should)
						BindingList& b=s[i];
						size_t size3=b.size();
						size_t size2=SizeOfClause(index);
						for (size_t j=0; j<size2; j++)
						{
							Node* n=Engine::GetStack(index+j);
							saveStack.push_back(n);

							// n is the stack uninstantiated, b is a list of bindings
							// n is guaranteed uninstantiated because it is the matching
							// database rule unmodified till now
							if (n->type==Structure::ST_VAR)
							{
								bool found = false;
								for (size_t k=0; k<size3 && !found; k++)
								{
									int lhs = b[k]->lhs;
									if (n->name==Engine::GetStack(lhs)->name)
									{
										// this type of binding (ST_REFERENCE)
										// should only exist inside the query engine
										// part - it substitutes forward / backward
										// for another existing clause on the stack
										found = true;
										Node* newn = new Node();
										newn->type = Structure::ST_REFERENCE;
										newn->fu = b[k]->rhs;
										Engine::SetStack(index+j,newn);
										newStack.push_back(newn);
									}
								}
								if (!found)
								{
									newStack.push_back(n);
								}
							}
							else
							{
								newStack.push_back(n);
							}
						}

						// execute the second parameter of the AND
						Set s2;
						Trace2(level,"AND_b  :%s",Engine::PrettyPrint(index).c_str());
						if (Execute(level+1,index,s2,cut))
						{
							//! merge results from s2 into s1
							success=true;
							size_t size4=s2.size();
							if (size4==0)
							{
								if (!s[i].empty())
									newSet.push_back(s[i]);
							}
							else
							{
								for (size_t j=0; j<size4; j++)
								{
									BindingList b=s[i];
									for (size_t k=0; k<s2[j].size(); k++)
									{
										b.push_back(s2[j][k]);
									}
									if (!b.empty())
										newSet.push_back(b);
								}
							}
						}

						// reset the stack to its previous glory
						// after the AND has executed
						// must use newStack - can't use Engine::GetStack()
						// since that auto substitutes REFERENCES
						PreCond(saveStack.size()==newStack.size());
						for (size_t j=0; j<size2; j++)
						{
							Node* newn = newStack[j];
							if (newn->type==Structure::ST_REFERENCE)
							{
								// if this was a newly inserted item
								// release it and reset the stack
								Engine::SetStack(index+j,saveStack[j]);
								safe_delete(newn);
							}
						}
						saveStack.clear();
						newStack.clear();

						//! a cut on the right hand side commits to the left
						//! hand solution we are currently working on - the
						//! left side's remaining alternatives are discarded.
						//! this engine solves a goal to a complete set rather
						//! than backtracking, so a cut has to truncate that
						//! set here instead of unwinding a choice point
						if (cut)
							break;
					}
					bindings=newSet;
				}
				else
				{
					//! no set - but since the first part executed
					//! successfully with no returning results, there
					//! still is a job to be done
					Set s2;
					if (Execute(level+1,index,s2,cut))
					{
						Trace1(level+1,"EXEC :yes");
						bindings=s2;
						return true;
					}
					return false;
				}
				Trace1(level+1,"EXEC :yes");
			}
			else
			{
				Trace1(level+1,"EXEC :no");
			}
			Trace1(level,success?"AND :yes":"AND :no");
			return success;
		}
		case Structure::ST_OR:
		{
			Set s;
			int index=pc+1;

			bool success=false;
			Set newSet,s1,s2;

			Trace2(level,"OR_a   :%s",Engine::PrettyPrint(index).c_str());
			if (Execute(level+1,index,s1,cut))
			{
				success=true;
				size_t size=s1.size();
				for (size_t i=0; i<size; i++)
				{
					newSet.push_back(s1[i]);
				}
			}

			index+=SizeOfClause(index);

			//! a cut taken in the left branch commits to it - the right
			//! branch is no longer an alternative
			if (!cut)
			{
				Trace2(level,"OR_b   :%s",Engine::PrettyPrint(index).c_str());
				if (Execute(level+1,index,s2,cut))
				{
					success=true;
					size_t size=s2.size();
					for (size_t i=0; i<size; i++)
					{
						newSet.push_back(s2[i]);
					}
				}
			}

			if (success)
				bindings=newSet;

			Trace1(level+1,success?"EXEC :yes":"EXEC :no");
			Trace1(level,success?"OR :yes":"OR :no");
			return success;
		}

		case Structure::ST_STRUCTURE:
		{
			//! this first set is / are the system defined functions
			//! that the interpreter implements - the else statement
			//! deals with database predicates
			if (n->name<=Structure::SYSTEM_DEFINED)
			{
				// write(one parameter)
				if (n->name==Structure::WRITE_PREDICATE && n->arity==1)
				{
					int stackIndex = pc+1;
					Node* str=Engine::GetStack(pc+1);
					if (str->type==Structure::ST_VAR && Valid(str->fu))
					{
						stackIndex = str->fu;
						str=Engine::GetStack(str->fu);
					}

					switch (str->type)
					{
						case Structure::ST_VAR:
						case Structure::ST_STRING:
						{
							Engine::AddToOutputString(Structure::GetString(str->name));
							break;
						}
						case Structure::ST_STRUCTURE:
						{
							Engine::AddToOutputString(Node::ToString(stackIndex));
							break;
						}
						case Structure::ST_FLOAT:
						{
							Engine::AddToOutputString(System::Float2Str(str->f));
							break;
						}
						case Structure::ST_INT:
						{
							Engine::AddToOutputString(System::Int2Str(str->i));
							break;
						}
						case Structure::ST_BOOL:
						{
							if (str->b)
								Engine::AddToOutputString("true");
							else
								Engine::AddToOutputString("false");
							break;
						}
						case Structure::ST_LIST:
						{
							Engine::AddToOutputString(Node::ToString(stackIndex));
							break;
						}
						default:
						{
							PostCond("unknown tag"==NULL);
						}
					}
					return true;
				}
				// nl predicate
				else if (n->name==Structure::NL_PREDICATE && n->arity==0)
				{
					Engine::AddToOutputString(System::nl);
					return true;
				}
			}
			else
			{
				//! custom database predicate:
				//! we now do a recursive call to create a new query
				//! we get all clauses that can match the current one
				//! (from the pc)
				std::vector<int> list;
				list=GetMatchingClauses(pc);
				size_t listSize=list.size();

				//! fail if you can't find any matches
				if (listSize==0)
					return false;

				//! for each matching clause
				bool cut = false;
				bool success = false;
				for (size_t i=0; i<listSize && !cut; i++)
				{
					//! push this clause onto the stack forward unified
					//! and execute it - if it is successful, take its
					//! backwards unified list and put it into mine

					//! stack stuff
					//! push each of these matching clauses (a copy of)
					//! on the stack to work with - one at a time of course
					int stackIndex=Engine::GetStackSize();
					std::vector<Node*> n=database.Get(list[i]);
					size_t size=n.size();
					for (size_t j=0; j<size;j++)
					{
						Node* nn=new Node(*n[j]);
						Engine::AddToStack(nn);
					}

					//! Forward unify new clause
					//! stackIndex is the index of the new clause on the stack
					size_t numUnifications=0;

					if (Engine::ForwardUnify(level,stackIndex,pc,numUnifications))
					{
						//! if the matching clause is a rule then its body has to
						//! be proved before the head may be accepted.  this holds
						//! however few variables the head unification bound - a
						//! ground head such as "r :- fail." binds nothing at all,
						//! and used to be accepted without ever running its body
						bool bodyProved=true;
						Set bodyBindings;

						if (Engine::GetStack(stackIndex)->type==Structure::ST_IMPLIES)
						{
							//! build the new query to be executed
							std::vector<Node*> newQuery;
							size_t size=n.size();

							//! go over each of the new query's items on the stack
							//! and resolve them accordingly
							for (size_t j=0; j<size;j++)
							{
								Node* n=Engine::GetStack(stackIndex+j);

								// if there are any variables left in the new query
								// that are the same name as the already resolved
								// variables - then substitute them too
								if (n->type==Structure::ST_VAR)
								{
									// see if this stack member is equivalently named
									// to another variable and if this var has a substitute
									// already - make it point to the same thing
									for (size_t k=0; k<j; k++)
									{
										Node* n2=Engine::GetStack(stackIndex+k);
										if (n2->type==Structure::ST_VAR && n2->name==n->name)
											n->fu=n2->fu;
									}
								}
								newQuery.push_back(new Node(*n));
							}

							{
								Query q(newQuery,database,level);
								bodyProved=q.ExecuteQueryRecursive(level+1,bodyBindings,cut);
							}

							//! Query copied these onto the engine stack
							for (size_t j=0; j<newQuery.size(); j++)
							{
								safe_delete(newQuery[j]);
							}
							newQuery.clear();

							// check nothing in the bindings is non-sensical
							// i.e. a var bound to an unbound variable
							for (size_t j=0; j<bodyBindings.size() && bodyProved; j++)
							{
								for (size_t k=0; k<bodyBindings[j].size(); k++)
								{
									Node* r = Engine::GetStack(bodyBindings[j][k]->rhs);
									if (r->type==Structure::ST_VAR && !Valid(r->fu))
									{
										bodyProved=false;
										break;
									}
								}
							}
						}

						if (bodyProved)
						{
							//! could we resolve anything forward?
							//! i.e. we can't substitute any variables
							//!      in the new query with old ones
							if (numUnifications>0)
							{
								success=true;

								//! this result set becomes relevant for the parent
								for (size_t j=0; j<bodyBindings.size(); j++)
								{
									bindings.push_back(bodyBindings[j]);
								}
							}
							else
							{
								//! backward unify - i.e. take the variables
								//! of the original query and see if it can
								//! be unified with the forward query's non variables
								BindingList b;
								if (Engine::CreateBindings(level,stackIndex,pc,b))
								{
									success=true;
									if (!b.empty())
										bindings.push_back(b);
								}
							}
						}

						//! cut the rest of the questions early?
						if (cut)
							break;
					}
				}
				return success;
			}
		}
		case Structure::ST_ASSIGN:
		{
			//! "=" unifies its two sides and evaluates neither of them, so
			//! "X = 3 - 1" binds X to the term 3-1.  "is" below is the one
			//! that does arithmetic
			int lhs=pc+1;
			int rhs=lhs+SizeOfClause(lhs);

			BindingList b;
			if (!Unify(lhs,rhs,b))
				return false;

			//! a unification that instantiated nothing - "a = a" - still
			//! succeeds, it just has no solution to publish
			if (!b.empty())
				bindings.push_back(b);

			Trace2(level,"UNIFY :%s",Engine::PrettyPrint(pc).c_str());
			return true;
		}
		case Structure::ST_IS:
		{
			// resolve the target down its forward chain before touching it.
			// ForwardBind links every later occurrence of a variable to the
			// first, and ForwardUnify links a rule's head variable to the
			// caller's - so the node at the end of the chain is the one that
			// has to receive the value.  writing to this occurrence instead
			// would break that chain, and "t(N) :- p(M), N is M + 1." would
			// compute the right answer and then throw it away.
			int lhsIndex=Engine::GetForwardValue(pc+1);
			Node* lhs=Engine::GetStack(lhsIndex);

			// setting lhs->fu only instantiates that one node.  every other
			// occurrence of the same variable further along the clause is a
			// separate stack node, so the assignment is also handed back as a
			// binding - that is what the enclosing AND uses to substitute the
			// remaining occurrences (see the ST_REFERENCE substitution above).
			// without it "M is N - 1, move(M,..)" would call move/4 with M
			// still unbound.
			int valueIndex;

			// if rhs is a list - there is nothing
			// to evaluate - just assign it
			Node* rhs=Engine::GetStack(pc+2);
			if (rhs->type==Structure::ST_LIST)
			{
				valueIndex = pc+2;
			}
			else
			{
				// evaluate expression and push it onto the stack
				Node e;
				if (!Engine::EvaluateExpression(pc+2,e))
					break;

				// EvaluateExpression leaves size at 0 - a computed value
				// occupies exactly one stack slot, and callers step over
				// arguments with SizeOfClause(), so a 0 here would make them
				// read the same slot forever
				Node* value = new Node(e);
				value->size = 1;

				valueIndex = int(Engine::GetStackSize());
				Engine::AddToStack(value);
			}

			// the chain may already end on a value rather than a free
			// variable - "is" is then a comparison, not an assignment.
			// a literal left hand side, as in "3 is 1 + 2", lands here too
			if (lhs->type!=Structure::ST_VAR)
			{
				return Engine::Equivalent(lhsIndex,valueIndex);
			}

			lhs->fu = valueIndex;

			BindingList b;
			b.push_back(Engine::NewBinding(lhsIndex,valueIndex));
			bindings.push_back(b);

			Trace2(level,"IS :%s",Engine::PrettyPrint(pc).c_str());
			return true;
		}
		default:
		{
			PostCond("illegal execution type"==NULL);
		}
	};
	return false;
};

bool Query::ExecuteQuery(void)
{
	Set s;
	bool cut = false;
	if (ExecuteQueryRecursive(1,s,cut))
	{
		Engine::FilterSet(0,s);
		std::string str=Engine::GatherResults(s);
		Engine::AddToOutputString(str);
	if (str.empty())
			Engine::AddToOutputString("yes");

		return true;
	}
	return false;
};

//! the number of stack slots a term occupies where it sits.  a slot the
//! enclosing AND replaced with an ST_REFERENCE stands for the one variable
//! that was there, whatever the size of the value it points at - so its
//! size has to be read off the slot itself and not off its target
static int RawSize(int index)
{
	Node* n=Engine::GetStack()[index];
	if (n->type==Structure::ST_REFERENCE)
		return 1;
	return n->size;
};

//! resolve an index to the slot that really holds the value: through the
//! reference substitutions the AND puts on the stack, and then along the
//! forward chain of variables that have been instantiated.
//!
//! Engine::GetForwardValue() follows the chain but stops on the reference
//! slot itself, and those slots are put back when the AND is done - a
//! binding pointing at one would go stale the moment it was published
static int ResolveIndex(int index)
{
	for (;;)
	{
		Node* n=Engine::GetStack()[index];
		if (n->type==Structure::ST_REFERENCE)
		{
			index=n->fu;
			continue;
		}
		if (n->type==Structure::ST_VAR && Valid(n->fu))
		{
			index=n->fu;
			continue;
		}
		return index;
	}
};

int Query::CopyTerm(int index)
{
	Node* src=Engine::GetStack()[index];

	//! a reference slot is a variable standing in for a value elsewhere on
	//! the stack.  copy it as a variable pointing at the same place - the
	//! reference itself is put back when the AND finishes
	if (src->type==Structure::ST_REFERENCE)
	{
		Node* n=new Node();
		n->type=Structure::ST_VAR;
		n->size=1;
		n->fu=src->fu;
		Engine::AddToStack(n);
		return 1;
	}

	Node* n=new Node(*src);

	//! parent and next are absolute stack references belonging to the
	//! original - a copied term is a value, it is not being solved
	n->parent=-1;
	n->next=-1;

	//! a free variable is copied as a forward link to the original.
	//! resolving the copy therefore arrives at the original variable, and
	//! unifying against the copy binds the one the caller can still see
	if (src->type==Structure::ST_VAR && !Valid(src->fu))
		n->fu=index;

	Engine::AddToStack(n);

	//! then the rest of the subtree, slot by slot, so that a reference
	//! sitting inside it is caught by the case above
	int size=src->size;
	int copied=1;
	int at=index+1;
	while (copied<size)
	{
		copied+=CopyTerm(at);
		at+=RawSize(at);
	}
	return size;
};

int Query::ListTail(int listIndex)
{
	Node* list=Engine::GetStack(listIndex);
	PreCond(list->type==Structure::ST_LIST);

	//! walk past the head to the first element of the tail
	int first=listIndex+1;
	int index=first+RawSize(first);

	Node* n=new Node();
	n->type=Structure::ST_LIST;
	n->arity=list->arity-1;
	n->size=1;

	int at=int(Engine::GetStackSize());
	Engine::AddToStack(n);

	for (size_t i=1; i<list->arity; i++)
	{
		int size=CopyTerm(index);
		n->size+=size;
		index+=RawSize(index);
	}
	return at;
};

bool Query::UnifyLists(int a,int b,BindingList& bindings)
{
	Node* na=Engine::GetStack(a);
	Node* nb=Engine::GetStack(b);

	//! [H|T] on the right, a plain list on the left - do it the other way
	//! round so there is only one destructuring case to write
	if (na->type==Structure::ST_LIST && nb->type==Structure::ST_HEADTAIL)
	{
		return UnifyLists(b,a,bindings);
	}

	//! [H1|T1] = [H2|T2]
	if (na->type==Structure::ST_HEADTAIL && nb->type==Structure::ST_HEADTAIL)
	{
		int head1=a+1;
		int head2=b+1;
		if (!Unify(head1,head2,bindings))
			return false;
		return Unify(head1+RawSize(head1),head2+RawSize(head2),bindings);
	}

	//! [H|T] = [a,b,c] - H takes the first element, T the rest.  the
	//! empty list has no head, so it never matches a [H|T] pattern
	if (na->type==Structure::ST_HEADTAIL && nb->type==Structure::ST_LIST)
	{
		if (nb->arity==0)
			return false;

		int head=a+1;
		if (!Unify(head,b+1,bindings))
			return false;

		return Unify(head+RawSize(head),ListTail(b),bindings);
	}

	//! [a,b,c] = [a,b,c] - same length, element by element
	if (na->type==Structure::ST_LIST && nb->type==Structure::ST_LIST)
	{
		if (na->arity!=nb->arity)
			return false;

		int index1=a+1;
		int index2=b+1;
		for (size_t i=0; i<na->arity; i++)
		{
			if (!Unify(index1,index2,bindings))
				return false;
			index1+=RawSize(index1);
			index2+=RawSize(index2);
		}
		return true;
	}

	return false;
};

bool Query::IdenticalToListTail(int headTail,int list,size_t from)
{
	//! compare [H|T] against the elements of list from position "from":
	//! H against element "from", then T against the rest
	Node* ln=Engine::GetStack(list);
	if (from>=ln->arity)
		return false;

	//! step to element "from"
	int elem=list+1;
	for (size_t i=0; i<from; i++)
		elem+=RawSize(elem);

	int head=headTail+1;
	if (!Identical(head,elem))
		return false;

	int tail=ResolveIndex(head+RawSize(head));
	Node* tn=Engine::GetStack()[tail];

	//! a free tail is not identical to anything concrete
	if (tn->type==Structure::ST_VAR)
		return false;

	if (tn->type==Structure::ST_HEADTAIL)
		return IdenticalToListTail(tail,list,from+1);

	if (tn->type==Structure::ST_LIST)
	{
		//! the tail must hold exactly the remaining elements
		if (tn->arity!=ln->arity-(from+1))
			return false;
		int index1=tail+1;
		int index2=elem+RawSize(elem);
		for (size_t i=0; i<tn->arity; i++)
		{
			if (!Identical(index1,index2))
				return false;
			index1+=RawSize(index1);
			index2+=RawSize(index2);
		}
		return true;
	}
	return false;
};

bool Query::Identical(int a,int b)
{
	a=ResolveIndex(a);
	b=ResolveIndex(b);

	//! the same slot is the same term - and it is the only way two free
	//! variables are ever identical, since occurrences of one variable in
	//! a clause all resolve to its first occurrence
	if (a==b)
		return true;

	Node* na=Engine::GetStack()[a];
	Node* nb=Engine::GetStack()[b];

	//! a bound [H|T] pattern and a plain list can spell the same term
	if (na->type==Structure::ST_HEADTAIL && nb->type==Structure::ST_LIST)
		return IdenticalToListTail(a,b,0);
	if (na->type==Structure::ST_LIST && nb->type==Structure::ST_HEADTAIL)
		return IdenticalToListTail(b,a,0);

	if (na->type!=nb->type)
		return false;

	switch (na->type)
	{
		case Structure::ST_VAR:
		{
			//! a variable's scope is its clause, so two free occurrences
			//! with the same name are the same variable - occurrences are
			//! separate stack nodes and not always chained together.  the
			//! anonymous variable (name 0) is a fresh variable every time
			return na->name==nb->name && na->name!=0;
		}
		case Structure::ST_INT:
		{
			return na->i==nb->i;
		}
		case Structure::ST_FLOAT:
		{
			return na->f==nb->f;
		}
		case Structure::ST_BOOL:
		{
			return na->b==nb->b;
		}
		case Structure::ST_STRING:
		{
			return na->name==nb->name;
		}
		case Structure::ST_STRUCTURE:
		{
			if (na->name!=nb->name || na->arity!=nb->arity)
				return false;

			int index1=a+1;
			int index2=b+1;
			for (size_t i=0; i<na->arity; i++)
			{
				if (!Identical(index1,index2))
					return false;
				index1+=RawSize(index1);
				index2+=RawSize(index2);
			}
			return true;
		}
		case Structure::ST_LIST:
		{
			if (na->arity!=nb->arity)
				return false;

			int index1=a+1;
			int index2=b+1;
			for (size_t i=0; i<na->arity; i++)
			{
				if (!Identical(index1,index2))
					return false;
				index1+=RawSize(index1);
				index2+=RawSize(index2);
			}
			return true;
		}
		case Structure::ST_HEADTAIL:
		{
			int head1=a+1;
			int head2=b+1;
			if (!Identical(head1,head2))
				return false;
			return Identical(head1+RawSize(head1),head2+RawSize(head2));
		}
	}
	return false;
};

bool Query::Unify(int a,int b,BindingList& bindings)
{
	//! both sides may be variables that already have a value, or slots
	//! the enclosing AND is standing in for
	a=ResolveIndex(a);
	b=ResolveIndex(b);

	if (a==b)
		return true;

	Node* na=Engine::GetStack(a);
	Node* nb=Engine::GetStack(b);

	//! ResolveIndex() stops on a value or on a free variable, so a
	//! variable here is one that has not been instantiated yet.  it takes
	//! the shape of the other side, and that is published as a binding -
	//! the enclosing AND needs it to substitute the goals that follow
	if (na->type==Structure::ST_VAR)
	{
		na->fu=b;
		bindings.push_back(Engine::NewBinding(a,b));
		return true;
	}
	if (nb->type==Structure::ST_VAR)
	{
		nb->fu=a;
		bindings.push_back(Engine::NewBinding(b,a));
		return true;
	}

	if (na->type==Structure::ST_LIST || na->type==Structure::ST_HEADTAIL ||
		nb->type==Structure::ST_LIST || nb->type==Structure::ST_HEADTAIL)
	{
		return UnifyLists(a,b,bindings);
	}

	if (na->type!=nb->type)
		return false;

	switch (na->type)
	{
		case Structure::ST_INT:
		{
			return na->i==nb->i;
		}
		case Structure::ST_FLOAT:
		{
			return na->f==nb->f;
		}
		case Structure::ST_BOOL:
		{
			return na->b==nb->b;
		}
		case Structure::ST_STRING:
		{
			return na->name==nb->name;
		}
		case Structure::ST_STRUCTURE:
		{
			//! an atom is a structure of arity zero, so this covers
			//! "fred = fred" as well as "f(X,b) = f(a,Y)"
			if (na->name!=nb->name || na->arity!=nb->arity)
				return false;

			int index1=a+1;
			int index2=b+1;
			for (size_t i=0; i<na->arity; i++)
			{
				if (!Unify(index1,index2,bindings))
					return false;
				index1+=RawSize(index1);
				index2+=RawSize(index2);
			}
			return true;
		}
	}
	return false;
};

bool Query::CanBind(Node* n1,Node* n2)
{
	// anything can bind with an unbound variable
	// since n2 is from the database
	if (n2->type==Structure::ST_VAR)
		return true;

	// if n1 is a variable try and resolve it - if it can't
	// be resolved it can bind to anything - so return true
	if (n1->type==Structure::ST_VAR && Valid(n1->fu))
	{
		n1=Engine::GetStack(Engine::GetForwardValue(n1->fu));
	}
	if (n1->type==Structure::ST_VAR)
	{
		return true;
	}

	// otherwise types must be identical to be unify-able
	if (n1->type==n2->type)
	{
		switch (n1->type)
		{
			case Structure::ST_INT:
			{
				return (n1->i==n2->i);
			}
			case Structure::ST_FLOAT:
			{
				return (n1->f==n2->f);
			}
			case Structure::ST_BOOL:
			{
				return (n1->b==n2->b);
			}
			case Structure::ST_STRUCTURE:
			case Structure::ST_STRING:
			{
				return (n1->name==n2->name);
			}
		}
	}
	return false;
}

bool Query::ListsCanBind(int stackIndex,int dbIndex,int dbLineIndex)
{
	Node* n1=Engine::GetStack(stackIndex);
	Node* n2=database.GetNode(dbIndex,dbLineIndex);

	// anything can bind with an unbound variable
	// since n2 is from the database
	if (n2->type==Structure::ST_VAR)
		return true;

	// if n1 is a variable try and resolve it - if it can't
	// be resolved it can bind to anything - so return true
	if (n1->type==Structure::ST_VAR && Valid(n1->fu))
	{
		stackIndex=n1->fu;
		n1=Engine::GetStack(Engine::GetForwardValue(n1->fu));
	}
	if (n1->type==Structure::ST_VAR)
	{
		return true;
	}

	switch (n1->type)
	{
		case Structure::ST_LIST:
		{
			if (n2->type==Structure::ST_LIST)
			{
				// empty list with non-empty list
				if ((n1->arity==0 && n2->arity!=0) ||
					(n1->arity!=0 && n2->arity==0))
					return false;

				// empty list with empty list
				if (n1->arity==0 && n2->arity==n1->arity)
					return true;

				// unify their head and tails
				int sindex1 = stackIndex+1;
				int sindex2 = sindex1 + SizeOfClause(sindex1);

				int dbindex1 = dbLineIndex + 1;
				int dbindex2 = dbindex1 + database.GetNode(dbIndex,dbindex1)->size;

				if (ListsCanBind(sindex1,dbIndex,dbindex1))
				{
					Node* n3 = database.GetNode(dbIndex,dbindex2);
					if (n3->type==Structure::ST_VAR)
					{
						return true;
					}
					else
					{
						if (n1->arity==n2->arity)
						{
							int arity = ((int)n1->arity) - 1;
							for (int i=0; i<arity; i++)
							{
								n1=Engine::GetStack(sindex2);
								n2=database.GetNode(dbIndex,dbindex2);

								if (n1->type==Structure::ST_LIST || n2->type==Structure::ST_LIST)
								{
									if (!ListsCanBind(sindex2,dbIndex,dbindex2))
										return false;
								}
								else
								{
									if (!CanBind(n1,n2))
										return false;
								}
								
								sindex2 += SizeOfClause(sindex2);
								dbindex2 += database.GetNode(dbIndex,dbindex2)->size;
							}

							return true;
						}
					}
				}
			}
			break;
		}

		case Structure::ST_INT:
		{
			if (n2->type==n1->type)
				return (n1->i==n2->i);
			break;
		}
		case Structure::ST_FLOAT:
		{
			if (n2->type==n1->type)
				return (n1->f==n2->f);
			break;
		}
		case Structure::ST_BOOL:
		{
			if (n2->type==n1->type)
				return (n1->b==n2->b);
			break;
		}
		case Structure::ST_STRUCTURE:
		case Structure::ST_STRING:
		{
			if (n2->type==n1->type)
				return (n1->name==n2->name) && (n1->arity==n2->arity);
			break;
		}
	}
	return false;
};

std::vector<int> Query::GetMatchingClauses(int pc)
{
	Node* stack = Engine::GetStack(pc);
	std::vector<int> list;

	// optimised finding of matching clauses with same name and arity
	std::vector<int> matchList = database.Get(stack->name,stack->arity);

	// find matching structures and enter them into the stack
	// if they match according to the criteria below
	std::vector<Node*> node;
	size_t matchListSize = matchList.size();
	for (size_t i=0; i<matchListSize; i++)
	{
		size_t listIndex = matchList[i];
		node = database.Get(listIndex);
		size_t size = node.size();
		PreCond(size>0);
		Node* n = node[0];

		int index=0;
		if (n->type==Structure::ST_IMPLIES)
		{
			index=1;
		}
		else
		{
			PreCond(n->type==Structure::ST_STRUCTURE);
		}

		n=node[index];

		//! check parameters before allowing it
		bool canBind=true;
		int index1=pc+1;
		int index2=index+1;
		for (size_t j=0; j<n->arity && canBind; j++)
		{
			Node* n1=Engine::GetStack(index1);
			Node* n2=node[index2];

			if (n1->type==Structure::ST_VAR && Valid(n1->fu))
			{
				n1=Engine::GetStack(Engine::GetForwardValue(n1->fu));
			}

			if (n1->type!=Structure::ST_VAR && n2->type!=Structure::ST_VAR)
			{
				if (n1->type==Structure::ST_LIST && n2->type==Structure::ST_HEADTAIL)
				{
					canBind = true;
				}
				else if (n2->type==Structure::ST_LIST && n1->type==Structure::ST_HEADTAIL)
				{
					canBind = true;
				}
				else if (n1->type==n2->type)
				{
					switch (n1->type)
					{
						case Structure::ST_INT:
						{
							canBind=(n1->i==n2->i);
							break;
						}
						case Structure::ST_FLOAT:
						{
							canBind=(n1->f==n2->f);
							break;
						}
						case Structure::ST_BOOL:
						{
							canBind=(n1->b==n2->b);
							break;
						}
						case Structure::ST_STRUCTURE:
						case Structure::ST_STRING:
						{
							canBind=(n1->name==n2->name);
							break;
						}
						case Structure::ST_LIST:
						{
							//! listIndex, not i: ListsCanBind indexes the
							//! database by clause, and i is only this loop's
							//! position in the candidate list.  The two agree
							//! only when the candidates happen to start at
							//! clause 0, so a list argument stopped matching as
							//! soon as any clause was defined ahead of it.
							canBind=ListsCanBind(index1,int(listIndex),index2);
							break;
						}
					}
				}
				else
				{
					canBind=false;
				}
			}

			index1+=SizeOfClause(index1);
			index2+=node[index2]->size;
		}
		if (canBind)
			list.push_back(listIndex);
	}
	return list;
};

bool Query::ExecuteQueryRecursive(int level,Set& s,bool& cut)
{
	//! a query is always a structure
	PreCond(query.size()>0);

	bool success=Execute(level,queryStart,s,cut);
	Trace1(level,success?"EXEC :succeeded":"EXEC :failed");
	return success;
};
