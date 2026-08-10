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

#define SizeOfClause(x) (Engine::GetStack(x)->size)

class Engine
{
public:
	//! reset stack indexes
	static void ResetStack(void);

	//! dump stack to string
	static std::string DumpStack(void);

	//! dump stack to stdout
	static void DumpStackToScreen(void);

	//! get the required result set from top of stack
	static std::string GatherResults(Set& s);

	//! dump set
	static std::string PrintSet(Set& s);

	//! does b exist in s?
	static bool UniqueSet(Set&s,BindingList& b);

	//! filter a set according to the needs of a structure
	//! keep only the variables it is interested in
	static void FilterSet(int pc,Set& s);

	//! pretty print a stack item
	static std::string PrettyPrint(int i);

	//! dump stack to file
	static void DumpStack(const std::string& fname);

	//! evaluate an expression and return a result
	static bool EvaluateExpression(int index,Node& result);

	//! forward unify two clauses
	static bool ForwardUnify(int level,int f1,int f2,size_t& numUnifications);

	//! bind internal variables inside a clause
	static void ForwardBind(int start,int size);

	//! ordinary bindings
	static bool CreateBindings(int level,int target,int parent,BindingList& bindings);

	//! resolve a value all the way down
	static int GetForwardValue(int index);

	//! do two stack entries resolve to the same value?
	static bool Equivalent(int a,int b);

	//! engine stack size
	static size_t GetStackSize(void);

	//! engine stack add
	static void AddToStack(Node* n);

	//! allocate a binding owned by the engine.  bindings are referenced from
	//! result sets that only live for the duration of a query, so they are
	//! all released together by ResetStack()
	static Binding* NewBinding(int lhs,int rhs);

	//! get size of a list
	static size_t ListSize(int index);

	//! different ways of getting the stack
	static Node* GetStack(int index);
	static Node* GetStackDynamic(int& index);
	static std::vector<Node*>& GetStack(void);

	static void SetStack(int index,Node* n);

	//! get/set tron flag
	static bool GetTron(void);
	static void SetTron(bool _tron);

	//! add a string to the output string of the system
	static void AddToOutputString(const std::string& str);
	static const std::string& GetOutputString(void);

private:
	// clear all memory used by stack
	static void ClearStack(void);

	static std::string PrintStackItem(int i);
	static int GetBinding(int var,BindingList& bindings);
	static bool Bind(int var,int rhs,BindingList& bindings);
	static bool CreateBindingsRecursive(int level,int target,int parent,BindingList& bindings);

	//! helper routine for sorting result sets' variables
	static void GatherVars(int index,std::vector<int>& vars);

	static std::vector<Node*>	stack;
	static std::vector<Binding*>	bindingPool;
	static int					dumpCntr;
	static std::string			outstring;
	static bool					tron;
};

