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

#include <vector>
#include <string>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "timer.h"

//////////////////////////////////////////////////////////////////

class IOWriter
{
public:
	virtual ~IOWriter(void) {}
	virtual void Write(const std::string& str) = 0;
};

//////////////////////////////////////////////////////////////////

class System
{
public:
	//! conversion routines
	static std::string Int2Str(int i);
	static std::string Float2Str(float f);

	//! precond helper routine
	static void PreCond1(bool pc,const char *info,int l,const char *f);

	//! system timer
	static TTime* GetTimer(void);

	//! load a text file
	static bool LoadTextFile(const std::string& filename,std::string& text);

	//! write text to the system output
	static void printf(const char* format,...);

	//! level indent write text to the system output
	static void lprintf(int level,const char* format,...);

	//! setup callback for writer
	static void SetWriter(IOWriter* p);

	//! the writer currently installed - so it can be swapped for the
	//! duration of a call and then put back
	static IOWriter* GetWriter(void);

public:
	static std::string nl;

private:
	//! timer system access
	static TTime timer;

	//! System printer callback - per thread, so that a network client
	//! capturing its own output cannot disturb what the prompt is printing
	//! on another thread
	static thread_local IOWriter* writer;
};

//////////////////////////////////////////////////////////////////

#define safe_delete(x)			{ if ((x)!=NULL) { delete (x); (x)=NULL; } }

//! for anything allocated with new[] - using safe_delete on an array is
//! undefined behaviour
#define safe_delete_array(x)	{ if ((x)!=NULL) { delete [] (x); (x)=NULL; } }

#define PreCond(cond)		{ System::PreCond1((cond),#cond,__LINE__,__FILE__); }
#define PostCond(cond)		{ System::PreCond1((cond),#cond,__LINE__,__FILE__); }

#ifdef _DEBUG

#define Trace3(level,a,b,c)		{ if (Engine::GetTron()) System::lprintf(level,a,b,c); }
#define Trace2(level,a,b)		{ if (Engine::GetTron()) System::lprintf(level,a,b); }
#define Trace1(level,a)			{ if (Engine::GetTron()) System::lprintf(level,a); }

#else

#define Trace3(level,a,b,c)
#define Trace2(level,a,b)
#define Trace1(level,a)

#endif

#ifdef _MSC_VER
#pragma warning(disable:4267)
#pragma warning(disable:4018)
#endif

