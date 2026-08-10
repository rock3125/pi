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

#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////
//
// A small readline style line editor - just the standard library and
// termios, so the interpreter keeps its "no dependencies" property.
//
// Supported at the prompt (the bash/emacs bindings):
//
//   up / down, ctrl-p / ctrl-n      walk through the command history
//   left / right, ctrl-b / ctrl-f   move the cursor
//   ctrl-left / ctrl-right          move a word at a time
//   alt-b / alt-f                   move a word at a time
//   home / end, ctrl-a / ctrl-e     start / end of line
//   backspace, delete, ctrl-d       delete a character
//   ctrl-w                          delete the word before the cursor
//   alt-backspace                   delete the word before the cursor
//   ctrl-u                          delete to the start of the line
//   ctrl-k                          delete to the end of the line
//   ctrl-t                          swap the last two characters
//   ctrl-l                          clear the screen
//   ctrl-c                          abandon the line
//   ctrl-d on an empty line         end of input
//
// When stdin is not a terminal (a pipe or a file) all of this is skipped
// and lines are read plainly, so scripting the interpreter still works.
//
//////////////////////////////////////////////////////////////////

class LineEditor
{
public:
	LineEditor(void);
	~LineEditor(void);

	//! read a single line - returns false at end of input
	bool ReadLine(const std::string& prompt,std::string& line);

	//! remember a line for the history
	void AddHistory(const std::string& line);

	//! history persistence - both are quiet about failure, a missing or
	//! unwritable history file is not worth bothering the user with
	void LoadHistory(const std::string& filename);
	void SaveHistory(const std::string& filename) const;

	//! the default history file, "" if there is no home directory
	static std::string DefaultHistoryFile(void);

	enum
	{
		MAX_HISTORY = 500
	};

private:
	//! is stdin a terminal?
	static bool IsTerminal(void);

	//! width of the terminal in columns (80 if it cannot be determined)
	static size_t TerminalWidth(void);

	//! the two ways of reading a line
	bool ReadLineRaw(const std::string& prompt,std::string& line);
	bool ReadLinePlain(const std::string& prompt,std::string& line);

	//! redraw the current line and put the cursor back where it belongs
	void Refresh(const std::string& prompt);

	//! editing primitives operating on buffer/pos
	void Insert(char ch);
	void DeleteCharBefore(void);
	void DeleteCharAt(void);
	void DeleteWordBefore(bool wordCharsOnly);
	void DeleteToStart(void);
	void DeleteToEnd(void);
	void Transpose(void);
	void MoveWordLeft(void);
	void MoveWordRight(void);

	//! replace the line being edited with a history entry
	void HistoryPrevious(void);
	void HistoryNext(void);

private:
	std::vector<std::string>	history;

	//! line currently being edited
	std::string					buffer;

	//! cursor position - an index into buffer, may equal buffer.size()
	size_t						pos;

	//! where we are in the history while walking it.  history.size()
	//! means "the new line the user is typing"
	size_t						historyPos;

	//! the new line, kept safe while the history is being walked
	std::string					savedLine;
};
