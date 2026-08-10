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

#include <assert.h>
#include <stdarg.h>

//////////////////////////////////////////////////////////////////

//! size of the scratch buffers used by printf()/lprintf()
enum { BUFFER_SIZE = 10000 };

//////////////////////////////////////////////////////////////////

//! default writer - used when no other writer (e.g. a gui window) has been
//! installed, sends everything straight to stdout
class ConsoleWriter : public IOWriter
{
public:
	virtual void Write(const std::string& str)
	{
		fwrite(str.c_str(),1,str.size(),stdout);
		fflush(stdout);
	}
};

static ConsoleWriter consoleWriter;

//////////////////////////////////////////////////////////////////

TTime System::timer;
std::string System::nl = "\n";
thread_local IOWriter* System::writer = &consoleWriter;

//////////////////////////////////////////////////////////////////

TTime* System::GetTimer(void)
{
	return &timer;
};

void System::SetWriter(IOWriter* w)
{
	writer = w;
};

IOWriter* System::GetWriter(void)
{
	return writer;
};

void System::printf(const char* fmt,...)
{
    va_list args;
	static thread_local char buffer[BUFFER_SIZE];

	va_start(args,fmt);
	vsnprintf(buffer, BUFFER_SIZE, fmt, args);
	va_end(args);

	if (writer!=NULL)
		writer->Write(buffer);
};

void System::lprintf(int level,const char* format,...)
{
    va_list args;
	static thread_local char buffer[BUFFER_SIZE];

	PreCond(level<2000);

	int index = level * 4;
	if (index>(BUFFER_SIZE-1))
		index = BUFFER_SIZE-1;

	if (index>0)
	{
		memset(buffer,' ',index);
		buffer[index] = 0;
	}

	va_start(args,format);
	vsnprintf(&buffer[index], BUFFER_SIZE-index, format, args);
	va_end(args);

	if (writer!=NULL)
	{
		//! each trace entry is one line - the gui writer used to break lines
		//! by itself, a plain console will not
		std::string str(buffer);
		if (str.empty() || str[str.size()-1]!='\n')
			str = str + "\n";

		writer->Write(str);
	}
};

std::string System::Float2Str(float f)
{
	char buf[256];
	snprintf(buf,sizeof(buf),"%2.4f",f);

	return std::string(buf);
};

std::string System::Int2Str(int i)
{
	//! itoa() is an MSVC extension - snprintf is the portable equivalent
	char buf[256];
	snprintf(buf,sizeof(buf),"%d",i);

	return std::string(buf);
};

void System::PreCond1( bool pc, const char *info, int l, const char *f )
{
	if ( !pc )
	{
		fprintf(stderr,"\nPRECONDITION FAILED\n%s\nfile \"%s\"\nline %d.\n",info,f,l);
		assert(false);
	}
}

bool System::LoadTextFile(const std::string& filename,std::string& text)
{
	FILE* fh=fopen(filename.c_str(),"rb");
	if (fh==NULL)
		return false;

	fseek(fh,0,SEEK_END);
	long fsize=ftell(fh);
	fseek(fh,0,SEEK_SET);

	if (fsize==0)
	{
		fclose(fh);
		return false;
	}

	char* buffer=new char[fsize+1];
	PostCond(buffer!=NULL);
	size_t numRead=fread(buffer,1,fsize,fh);
	fclose(fh);

	buffer[numRead]=0;
	text=buffer;
	//! allocated with new[], so it must be released with delete[]
	delete [] buffer;

	return true;
};


