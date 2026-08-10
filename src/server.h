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

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "database.h"

//////////////////////////////////////////////////////////////////
//
// A plain text TCP front end for the interpreter, on ordinary POSIX
// sockets.  Anything that can write to a socket can drive it:
//
//     echo '?- father(fred,X).' | nc localhost 7071
//     telnet localhost 7071
//
// One line in, the answer out.  A client that sends "exit" (or closes the
// connection) is disconnected; the server itself keeps running.
//
// Each connection gets its own thread, so slow clients cannot block each
// other while reading or writing.  Commands themselves are serialised: the
// engine keeps its state in globals, so only one query runs at a time (see
// EngineLock in interpreter.h).
//
//////////////////////////////////////////////////////////////////

class PrologServer
{
public:
	PrologServer(void);
	~PrologServer(void);

	//! start listening.  returns false and explains why on failure
	bool Start(DataBase& database,const std::string& address,int port);

	//! stop listening, disconnect every client and wait for the threads
	void Stop(void);

	//! is the server running?
	bool Running(void) const;

	enum
	{
		//! how many connections may be waiting to be accepted
		LISTEN_BACKLOG	= 16,

		//! refuse a line longer than this rather than grow without limit
		MAX_LINE		= 1024 * 1024
	};

private:
	PrologServer(const PrologServer&);
	const PrologServer& operator=(const PrologServer&);

	//! accepts connections until the listening socket is shut down
	void AcceptLoop(void);

	//! serves one connection until it closes
	void ClientLoop(int fd);

	//! run one command with output captured and sent back to the client
	bool HandleCommand(int fd,const std::string& command);

private:
	DataBase*					database;

	int							listenSocket;

	//! Stop() writes a byte here to wake the accept loop.  relying on
	//! closing the listening socket to do that is a race - the descriptor
	//! can be reused by another thread before accept() notices
	int							wakeupPipe[2];

	//! read by the accept loop while the main thread writes it in Stop()
	std::atomic<bool>			running;

	std::thread					acceptThread;

	//! live client connections, so Stop() can wake them up
	mutable std::mutex			clientLock;
	std::vector<int>			clientSockets;
	std::vector<std::thread>	clientThreads;
};
