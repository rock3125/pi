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

#include "lineedit.h"

#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <sys/ioctl.h>

#include <cctype>
#include <cerrno>
#include <iostream>

//////////////////////////////////////////////////////////////////

namespace
{
	//! control characters we care about
	enum
	{
		KEY_CTRL_A	= 1,
		KEY_CTRL_B	= 2,
		KEY_CTRL_C	= 3,
		KEY_CTRL_D	= 4,
		KEY_CTRL_E	= 5,
		KEY_CTRL_F	= 6,
		KEY_CTRL_H	= 8,
		KEY_TAB		= 9,
		KEY_ENTER	= 13,
		KEY_CTRL_K	= 11,
		KEY_CTRL_L	= 12,
		KEY_CTRL_N	= 14,
		KEY_CTRL_P	= 16,
		KEY_CTRL_T	= 20,
		KEY_CTRL_U	= 21,
		KEY_CTRL_W	= 23,
		KEY_ESC		= 27,
		KEY_BACKSPACE	= 127
	};

	//! restores the terminal however we leave the editor
	class RawMode
	{
	public:
		RawMode(void)
			: ok(false)
		{
			if (tcgetattr(STDIN_FILENO,&saved)!=0)
				return;

			struct termios raw = saved;

			//! no echo, no line buffering, and read one byte at a time.
			//! ISIG stays off so ctrl-c arrives as a character and can
			//! abandon the line the way bash does
			raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
			raw.c_iflag &= ~(IXON | ICRNL);
			raw.c_cc[VMIN] = 1;
			raw.c_cc[VTIME] = 0;

			ok = (tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw)==0);
		}

		~RawMode(void)
		{
			if (ok)
				tcsetattr(STDIN_FILENO,TCSAFLUSH,&saved);
		}

		bool Ok(void) const { return ok; }

	private:
		RawMode(const RawMode&);
		const RawMode& operator=(const RawMode&);

		struct termios	saved;
		bool			ok;
	};

	void WriteOut(const std::string& str)
	{
		if (str.empty())
			return;

		size_t written=0;
		while (written<str.size())
		{
			ssize_t n=write(STDOUT_FILENO,str.data()+written,str.size()-written);
			if (n<=0)
				return;
			written += size_t(n);
		}
	}

	//! read one byte, blocking
	bool ReadByte(char& ch)
	{
		ssize_t n;
		do
		{
			n = read(STDIN_FILENO,&ch,1);
		}
		while (n<0 && errno==EINTR);

		return (n==1);
	}

	//! read one byte, but give up after timeoutMs - used for the bytes
	//! following an escape so that a bare ESC key does not hang
	bool ReadByteTimeout(char& ch,int timeoutMs)
	{
		struct pollfd pfd;
		pfd.fd = STDIN_FILENO;
		pfd.events = POLLIN;

		int r;
		do
		{
			r = poll(&pfd,1,timeoutMs);
		}
		while (r<0 && errno==EINTR);

		if (r<=0)
			return false;

		return ReadByte(ch);
	}

	bool IsWordChar(char ch)
	{
		unsigned char c=(unsigned char)ch;
		return (isalnum(c)!=0) || ch=='_';
	}
}

//////////////////////////////////////////////////////////////////

LineEditor::LineEditor(void)
	: pos(0)
	, historyPos(0)
{
};

LineEditor::~LineEditor(void)
{
};

bool LineEditor::IsTerminal(void)
{
	return isatty(STDIN_FILENO)==1 && isatty(STDOUT_FILENO)==1;
};

size_t LineEditor::TerminalWidth(void)
{
	struct winsize ws;
	if (ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==0 && ws.ws_col>0)
		return ws.ws_col;

	return 80;
};

std::string LineEditor::DefaultHistoryFile(void)
{
	const char* home=getenv("HOME");
	if (home==NULL || home[0]==0)
		return "";

	return std::string(home)+"/.pi_history";
};

//////////////////////////////////////////////////////////////////

void LineEditor::AddHistory(const std::string& line)
{
	if (line.empty())
		return;

	//! do not store the same command twice in a row
	if (!history.empty() && history.back()==line)
		return;

	history.push_back(line);

	if (history.size()>MAX_HISTORY)
	{
		history.erase(history.begin(),history.begin()+(history.size()-MAX_HISTORY));
	}
};

void LineEditor::LoadHistory(const std::string& filename)
{
	if (filename.empty())
		return;

	FILE* fh=fopen(filename.c_str(),"r");
	if (fh==NULL)
		return;

	char buf[4096];
	while (fgets(buf,sizeof(buf),fh)!=NULL)
	{
		std::string line(buf);
		while (!line.empty() && (line[line.size()-1]=='\n' || line[line.size()-1]=='\r'))
			line.erase(line.size()-1);

		AddHistory(line);
	}
	fclose(fh);
};

void LineEditor::SaveHistory(const std::string& filename) const
{
	if (filename.empty())
		return;

	FILE* fh=fopen(filename.c_str(),"w");
	if (fh==NULL)
		return;

	for (size_t i=0; i<history.size(); i++)
	{
		fprintf(fh,"%s\n",history[i].c_str());
	}
	fclose(fh);
};

//////////////////////////////////////////////////////////////////

void LineEditor::Refresh(const std::string& prompt)
{
	size_t cols=TerminalWidth();
	size_t plen=prompt.size();

	//! scroll the window horizontally rather than let the terminal wrap -
	//! that keeps everything on one row and the redraw simple
	size_t start=0;
	while (plen+(pos-start)>=cols && start<buffer.size())
		start++;

	size_t len=buffer.size()-start;
	while (plen+len>cols && len>0)
		len--;

	std::string out;
	out += "\r";
	out += prompt;
	out += buffer.substr(start,len);

	//! erase whatever used to be further along the line
	out += "\x1b[0K";

	//! and put the cursor back
	size_t col=plen+(pos-start);
	if (col>0)
	{
		char seq[32];
		snprintf(seq,sizeof(seq),"\r\x1b[%dC",int(col));
		out += seq;
	}
	else
	{
		out += "\r";
	}

	WriteOut(out);
};

//////////////////////////////////////////////////////////////////

void LineEditor::Insert(char ch)
{
	buffer.insert(buffer.begin()+pos,ch);
	pos++;
};

void LineEditor::DeleteCharBefore(void)
{
	if (pos==0)
		return;

	buffer.erase(pos-1,1);
	pos--;
};

void LineEditor::DeleteCharAt(void)
{
	if (pos>=buffer.size())
		return;

	buffer.erase(pos,1);
};

void LineEditor::DeleteWordBefore(bool wordCharsOnly)
{
	size_t end=pos;

	//! step back over whitespace first, then over the word itself
	while (pos>0 && isspace((unsigned char)buffer[pos-1]))
		pos--;

	if (wordCharsOnly)
	{
		//! alt-backspace stops at punctuation, the way bash does
		while (pos>0 && IsWordChar(buffer[pos-1]))
			pos--;
	}
	else
	{
		//! ctrl-w takes everything back to the previous whitespace
		while (pos>0 && !isspace((unsigned char)buffer[pos-1]))
			pos--;
	}

	buffer.erase(pos,end-pos);
};

void LineEditor::DeleteToStart(void)
{
	buffer.erase(0,pos);
	pos=0;
};

void LineEditor::DeleteToEnd(void)
{
	buffer.erase(pos);
};

void LineEditor::Transpose(void)
{
	//! ctrl-t swaps the two characters before the cursor and steps over them
	if (buffer.size()<2 || pos==0)
		return;

	if (pos==buffer.size())
		pos--;

	char tmp=buffer[pos-1];
	buffer[pos-1]=buffer[pos];
	buffer[pos]=tmp;
	pos++;
};

void LineEditor::MoveWordLeft(void)
{
	while (pos>0 && !IsWordChar(buffer[pos-1]))
		pos--;
	while (pos>0 && IsWordChar(buffer[pos-1]))
		pos--;
};

void LineEditor::MoveWordRight(void)
{
	while (pos<buffer.size() && !IsWordChar(buffer[pos]))
		pos++;
	while (pos<buffer.size() && IsWordChar(buffer[pos]))
		pos++;
};

//////////////////////////////////////////////////////////////////

void LineEditor::HistoryPrevious(void)
{
	if (history.empty() || historyPos==0)
		return;

	//! keep the line being typed so it can come back
	if (historyPos==history.size())
		savedLine=buffer;

	historyPos--;
	buffer=history[historyPos];
	pos=buffer.size();
};

void LineEditor::HistoryNext(void)
{
	if (historyPos>=history.size())
		return;

	historyPos++;
	if (historyPos==history.size())
		buffer=savedLine;
	else
		buffer=history[historyPos];

	pos=buffer.size();
};

//////////////////////////////////////////////////////////////////

bool LineEditor::ReadLinePlain(const std::string& prompt,std::string& line)
{
	//! not a terminal - no editing, just hand back whole lines
	fputs(prompt.c_str(),stdout);
	fflush(stdout);

	if (!std::getline(std::cin,line))
		return false;

	//! tolerate files/pipes that still carry dos line endings
	if (!line.empty() && line[line.size()-1]=='\r')
		line.erase(line.size()-1);

	return true;
};

bool LineEditor::ReadLineRaw(const std::string& prompt,std::string& line)
{
	RawMode raw;
	if (!raw.Ok())
		return ReadLinePlain(prompt,line);

	buffer.clear();
	pos=0;
	historyPos=history.size();
	savedLine.clear();

	Refresh(prompt);

	do
	{
		char ch;
		if (!ReadByte(ch))
		{
			//! end of input
			return false;
		}

		switch (ch)
		{
			case KEY_ENTER:
			case 10:
			{
				WriteOut("\r\n");
				line=buffer;
				return true;
			}

			case KEY_CTRL_C:
			{
				//! abandon this line and start a fresh one
				WriteOut("^C\r\n");
				line.clear();
				return true;
			}

			case KEY_CTRL_D:
			{
				if (buffer.empty())
				{
					//! end of input, as in bash
					WriteOut("\r\n");
					return false;
				}
				DeleteCharAt();
				break;
			}

			case KEY_BACKSPACE:
			case KEY_CTRL_H:
			{
				DeleteCharBefore();
				break;
			}

			case KEY_CTRL_A:	pos=0; break;
			case KEY_CTRL_E:	pos=buffer.size(); break;
			case KEY_CTRL_B:	if (pos>0) pos--; break;
			case KEY_CTRL_F:	if (pos<buffer.size()) pos++; break;
			case KEY_CTRL_P:	HistoryPrevious(); break;
			case KEY_CTRL_N:	HistoryNext(); break;
			case KEY_CTRL_U:	DeleteToStart(); break;
			case KEY_CTRL_K:	DeleteToEnd(); break;
			case KEY_CTRL_W:	DeleteWordBefore(false); break;
			case KEY_CTRL_T:	Transpose(); break;

			case KEY_CTRL_L:
			{
				//! clear the screen and redraw at the top
				WriteOut("\x1b[H\x1b[2J");
				break;
			}

			case KEY_TAB:
			{
				//! no completion - swallow it rather than corrupt the line
				break;
			}

			case KEY_ESC:
			{
				char a,b;
				if (!ReadByteTimeout(a,50))
				{
					//! a bare escape - ignore it
					break;
				}

				if (a=='[')
				{
					if (!ReadByteTimeout(b,50))
						break;

					if (b>='0' && b<='9')
					{
						char c;
						if (!ReadByteTimeout(c,50))
							break;

						if (c=='~')
						{
							switch (b)
							{
								case '1':	pos=0; break;				// home
								case '7':	pos=0; break;				// home
								case '3':	DeleteCharAt(); break;		// delete
								case '4':	pos=buffer.size(); break;	// end
								case '8':	pos=buffer.size(); break;	// end
								default:	break;						// pgup/pgdn/F-keys
							}
						}
						else if (c==';')
						{
							//! a modified key such as ctrl-left ("1;5D")
							char mod,key;
							if (!ReadByteTimeout(mod,50)) break;
							if (!ReadByteTimeout(key,50)) break;

							switch (key)
							{
								case 'D':	MoveWordLeft(); break;
								case 'C':	MoveWordRight(); break;
								case 'H':	pos=0; break;
								case 'F':	pos=buffer.size(); break;
								default:	break;
							}
						}
						else
						{
							//! an F-key or similar - drain the rest of it
							while (c!='~' && !((c>='A' && c<='Z') || (c>='a' && c<='z')))
							{
								if (!ReadByteTimeout(c,50))
									break;
							}
						}
					}
					else
					{
						switch (b)
						{
							case 'A':	HistoryPrevious(); break;
							case 'B':	HistoryNext(); break;
							case 'C':	if (pos<buffer.size()) pos++; break;
							case 'D':	if (pos>0) pos--; break;
							case 'H':	pos=0; break;
							case 'F':	pos=buffer.size(); break;
							default:	break;
						}
					}
				}
				else if (a=='O')
				{
					//! application cursor mode
					if (!ReadByteTimeout(b,50))
						break;

					switch (b)
					{
						case 'A':	HistoryPrevious(); break;
						case 'B':	HistoryNext(); break;
						case 'C':	if (pos<buffer.size()) pos++; break;
						case 'D':	if (pos>0) pos--; break;
						case 'H':	pos=0; break;
						case 'F':	pos=buffer.size(); break;
						default:	break;
					}
				}
				else
				{
					//! alt-<key>
					switch (a)
					{
						case 'b':	MoveWordLeft(); break;
						case 'f':	MoveWordRight(); break;
						case KEY_BACKSPACE:
						case KEY_CTRL_H:
							DeleteWordBefore(true);
							break;
						default:	break;
					}
				}
				break;
			}

			default:
			{
				//! anything printable goes into the line
				if ((unsigned char)ch>=32)
					Insert(ch);
				break;
			}
		}

		Refresh(prompt);
	}
	while (true);
};

bool LineEditor::ReadLine(const std::string& prompt,std::string& line)
{
	if (!IsTerminal())
		return ReadLinePlain(prompt,line);

	return ReadLineRaw(prompt,line);
};
