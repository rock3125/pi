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

//! defined further down - resolve a stack index through references and
//! bound variables to the slot that really holds the value
static int ResolveIndex(int index);

//! defined further down - the slots a term occupies where it sits, which
//! for a substituted reference slot is one, whatever it points at
static int RawSize(int index);

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
				//! hand every binding up unfiltered: the candidate loop
				//! that ran this body prunes to its caller's variables by
				//! slot range, and Materialize needs the clause-variable
				//! bindings to finish values the head only sketched
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
								//! a variable stands for the binding's variable when
								//! it resolves to that variable's slot - matching by
								//! name confuses a clause's X with its caller's X,
								//! and misses a variable reached through a chain
								int r=ResolveIndex(index+j);

								bool found = false;
								for (size_t k=0; k<size3 && !found; k++)
								{
									int lhs = b[k]->lhs;
									if (r==lhs)
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
					//! push a fresh copy of the clause onto the stack
					int stackIndex=Engine::GetStackSize();
					std::vector<Node*> n=database.Get(list[i]);
					size_t size=n.size();
					for (size_t j=0; j<size;j++)
					{
						Node* nn=new Node(*n[j]);
						Engine::AddToStack(nn);
					}

					//! link the copy's repeated variables to their first
					//! occurrence, so a head like max(X,Y,X) routes its
					//! first argument to its third
					Engine::ForwardBind(stackIndex,int(size));

					//! the head is the clause itself for a fact, the left
					//! side of the implies for a rule
					bool isRule=(Engine::GetStack(stackIndex)->type==Structure::ST_IMPLIES);
					int head=isRule?stackIndex+1:stackIndex;

					//! one real unification of head against goal.  it binds
					//! in both directions - the clause's variables take the
					//! goal's values and the goal's variables take the
					//! head's - and records everything it bound, so the
					//! goal can be restored before the next candidate
					BindingList attempt;
					if (Unify(head,pc,attempt))
					{
						//! a rule's body has to be proved before the head
						//! may be accepted - however few variables the head
						//! unification bound, since "r :- fail." binds none
						bool bodyProved=true;
						Set bodyBindings;

						if (isRule)
						{
							//! build the body query from the clause copy -
							//! its variables carry the head bindings through
							//! their forward links into the clause copy
							std::vector<Node*> newQuery;
							for (size_t j=0; j<size;j++)
							{
								Node* qn=Engine::GetStack(stackIndex+j);

								// point later occurrences of a variable at
								// the same substitute as the first
								if (qn->type==Structure::ST_VAR)
								{
									for (size_t k=0; k<j; k++)
									{
										Node* n2=Engine::GetStack(stackIndex+k);
										if (n2->type==Structure::ST_VAR && n2->name==qn->name)
											qn->fu=n2->fu;
									}
								}
								newQuery.push_back(new Node(*qn));
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
						}

						if (bodyProved)
						{
							success=true;

							//! what the caller gets: its own variables bound
							//! by the head unification - materialised, since
							//! the undo below takes the raw bindings back -
							//! plus its variables the body bound directly.
							//! a head value the body completed (the R in
							//! append's [H|R]) is resolved per body solution
							if (bodyBindings.empty())
							{
								BindingList out;
								BindingList none;
								for (size_t j=0; j<attempt.size(); j++)
								{
									if (attempt[j]->lhs<stackIndex)
									{
										out.push_back(Engine::NewBinding(attempt[j]->lhs,
											Materialize(attempt[j]->rhs,none)));
									}
								}
								if (!out.empty())
									bindings.push_back(out);
							}
							else
							{
								for (size_t k=0; k<bodyBindings.size(); k++)
								{
									BindingList& bs=bodyBindings[k];
									BindingList out;
									for (size_t j=0; j<attempt.size(); j++)
									{
										if (attempt[j]->lhs<stackIndex)
										{
											out.push_back(Engine::NewBinding(attempt[j]->lhs,
												Materialize(attempt[j]->rhs,bs)));
										}
									}

									//! the body's own bindings of this goal's
									//! variables - already materialised at the
									//! level that made them
									for (size_t j=0; j<bs.size(); j++)
									{
										if (bs[j]->lhs>=stackIndex)
											continue;
										bool have=false;
										for (size_t m=0; m<out.size() && !have; m++)
											have=(out[m]->lhs==bs[j]->lhs);
										if (!have)
											out.push_back(bs[j]);
									}

									if (!out.empty())
										bindings.push_back(out);
								}
							}
						}
					}

					//! take back whatever the head attempt bound - the next
					//! candidate must see the goal exactly as it was
					for (size_t j=0; j<attempt.size(); j++)
					{
						Engine::GetStack()[attempt[j]->lhs]->fu=-1;
					}

					//! a cut in the body commits to this clause
					if (cut)
						break;
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
	//! occurrences of one variable in the query are separate nodes - link
	//! the later ones to the first, the way a clause's are linked, so that
	//! solving one goal instantiates the variable everywhere it appears
	Engine::ForwardBind(queryStart,int(query.size()));

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

int Query::LookupByName(size_t name,BindingList& bs)
{
	if (name==0)
		return -1;
	for (size_t i=0; i<bs.size(); i++)
	{
		Node* n=Engine::GetStack()[bs[i]->lhs];
		if (n->type==Structure::ST_VAR && n->name==name)
			return bs[i]->rhs;
	}
	return -1;
};

int Query::Materialize(int index,BindingList& bs)
{
	int at=int(Engine::GetStackSize());
	MaterializeNode(index,bs,0);
	return at;
};

int Query::MaterializeNode(int index,BindingList& bs,int depth)
{
	//! a lookup chain that loops (a name bound to itself through the
	//! body solution) would otherwise recurse forever
	if (depth>10000)
		return 0;

	index=ResolveIndex(index);
	Node* n=Engine::GetStack()[index];

	switch (n->type)
	{
		case Structure::ST_VAR:
		{
			//! free after resolving - the body solution may still know it
			int rhs=LookupByName(n->name,bs);
			if (Valid(rhs))
				return MaterializeNode(rhs,bs,depth+1);

			Node* copy=new Node(*n);
			copy->fu=-1;
			copy->parent=-1;
			copy->next=-1;
			copy->size=1;
			Engine::AddToStack(copy);
			return 1;
		}
		case Structure::ST_INT:
		case Structure::ST_FLOAT:
		case Structure::ST_BOOL:
		case Structure::ST_STRING:
		{
			Node* copy=new Node(*n);
			copy->fu=-1;
			copy->parent=-1;
			copy->next=-1;
			copy->size=1;
			Engine::AddToStack(copy);
			return 1;
		}
		case Structure::ST_STRUCTURE:
		case Structure::ST_LIST:
		{
			Node* copy=new Node(*n);
			copy->fu=-1;
			copy->parent=-1;
			copy->next=-1;
			copy->size=1;
			Engine::AddToStack(copy);

			int written=1;
			int child=index+1;
			for (size_t i=0; i<n->arity; i++)
			{
				written+=MaterializeNode(child,bs,depth+1);
				child+=RawSize(child);
			}
			copy->size=written;
			return written;
		}
		case Structure::ST_HEADTAIL:
		{
			//! a bound pattern names a real list - flatten [H|T] into the
			//! list [H, elements of T...] so answers print as [1,2,3]
			Node* out=new Node();
			out->type=Structure::ST_LIST;
			Engine::AddToStack(out);

			int written=1;
			size_t elements=0;
			int cur=index;
			for (;;)
			{
				//! the pattern's head is one element
				int head=cur+1;
				written+=MaterializeNode(head,bs,depth+1);
				elements++;

				//! then whatever the tail turns out to be
				int tail=ResolveIndex(head+RawSize(head));
				Node* tn=Engine::GetStack()[tail];
				if (tn->type==Structure::ST_VAR)
				{
					int rhs=LookupByName(tn->name,bs);
					if (Valid(rhs))
					{
						tail=ResolveIndex(rhs);
						tn=Engine::GetStack()[tail];
					}
				}

				if (tn->type==Structure::ST_HEADTAIL)
				{
					//! [H1|[H2|T]] - keep flattening
					cur=tail;
					depth++;
					if (depth>10000)
						break;
					continue;
				}
				if (tn->type==Structure::ST_LIST)
				{
					int child=tail+1;
					for (size_t i=0; i<tn->arity; i++)
					{
						written+=MaterializeNode(child,bs,depth+1);
						elements++;
						child+=RawSize(child);
					}
					break;
				}

				//! a tail nothing determined - keep the variable visible
				//! rather than inventing elements
				written+=MaterializeNode(tail,bs,depth+1);
				elements++;
				break;
			}
			out->arity=elements;
			out->size=written;
			return written;
		}
		default:
		{
			//! an expression subtree used as a value - copy it verbatim
			int size=n->size;
			for (int i=0; i<size; i++)
			{
				Node* copy=new Node(*Engine::GetStack()[index+i]);
				copy->parent=-1;
				copy->next=-1;
				Engine::AddToStack(copy);
			}
			return size;
		}
	}
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
		stackIndex=Engine::GetForwardValue(n1->fu);
		n1=Engine::GetStack(stackIndex);
	}
	if (n1->type==Structure::ST_VAR)
	{
		return true;
	}

	//! a [H|T] pattern on either side is worth a real attempt - this is
	//! only a filter, and the unification decides properly
	if (n1->type==Structure::ST_HEADTAIL || n2->type==Structure::ST_HEADTAIL)
	{
		return true;
	}

	if (n1->type==Structure::ST_LIST)
	{
		if (n2->type!=Structure::ST_LIST)
			return false;

		//! two plain lists only unify at the same length
		if (n1->arity!=n2->arity)
			return false;

		int sindex=stackIndex+1;
		int dindex=dbLineIndex+1;
		for (size_t i=0; i<n1->arity; i++)
		{
			if (!ListsCanBind(sindex,dbIndex,dindex))
				return false;
			sindex+=RawSize(sindex);
			dindex+=database.GetNode(dbIndex,dindex)->size;
		}
		return true;
	}

	//! plain values compare directly
	return CanBind(n1,n2);
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

			//! a slot the AND substituted is one slot wide however large
			//! the value it references - stepping by the value's size
			//! would land the walk inside a neighbouring argument
			index1+=RawSize(index1);
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
