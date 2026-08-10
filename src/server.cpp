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

#include "server.h"
#include "interpreter.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>

//////////////////////////////////////////////////////////////////

namespace
{
	//! collects everything the interpreter prints so it can be handed back
	//! to the client that asked for it
	class StringWriter : public IOWriter
	{
	public:
		virtual void Write(const std::string& str)
		{
			text += str;
		}

		std::string text;
	};

	//! write the whole buffer, coping with short writes
	bool WriteAll(int fd,const std::string& str)
	{
		size_t written=0;
		while (written<str.size())
		{
			ssize_t n=send(fd,str.data()+written,str.size()-written,MSG_NOSIGNAL);
			if (n<=0)
			{
				if (n<0 && errno==EINTR)
					continue;
				return false;
			}
			written += size_t(n);
		}
		return true;
	}
}

//////////////////////////////////////////////////////////////////

PrologServer::PrologServer(void)
	: database(NULL)
	, listenSocket(-1)
	, running(false)
{
	wakeupPipe[0]=-1;
	wakeupPipe[1]=-1;
};

PrologServer::~PrologServer(void)
{
	Stop();
};

bool PrologServer::Running(void) const
{
	return running;
};

//////////////////////////////////////////////////////////////////

bool PrologServer::Start(DataBase& db,const std::string& address,int port)
{
	if (running)
		return false;

	if (port<1 || port>65535)
	{
		System::printf("server: port %d is out of range\n",port);
		return false;
	}

	int fd=socket(AF_INET,SOCK_STREAM,0);
	if (fd<0)
	{
		System::printf("server: could not create a socket (%s)\n",strerror(errno));
		return false;
	}

	//! so a restart does not have to wait out TIME_WAIT
	int on=1;
	setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));

	struct sockaddr_in addr;
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((unsigned short)port);

	if (address.empty() || address=="*")
	{
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else if (inet_pton(AF_INET,address.c_str(),&addr.sin_addr)!=1)
	{
		System::printf("server: '%s' is not an IPv4 address\n",address.c_str());
		close(fd);
		return false;
	}

	if (bind(fd,(struct sockaddr*)&addr,sizeof(addr))!=0)
	{
		System::printf("server: could not bind %s:%d (%s)\n",
					   address.c_str(),port,strerror(errno));
		close(fd);
		return false;
	}

	if (listen(fd,LISTEN_BACKLOG)!=0)
	{
		System::printf("server: could not listen on %s:%d (%s)\n",
					   address.c_str(),port,strerror(errno));
		close(fd);
		return false;
	}

	if (pipe(wakeupPipe)!=0)
	{
		System::printf("server: could not create the shutdown pipe (%s)\n",strerror(errno));
		close(fd);
		return false;
	}

	database = &db;
	listenSocket = fd;
	running = true;

	acceptThread = std::thread(&PrologServer::AcceptLoop,this);

	System::printf("listening on %s:%d\n",
				   (address.empty() || address=="*") ? "0.0.0.0" : address.c_str(),
				   port);
	return true;
};

void PrologServer::Stop(void)
{
	if (!running)
		return;

	running = false;

	//! wake the accept loop.  the listening socket is not closed until that
	//! thread has finished with it
	if (wakeupPipe[1]>=0)
	{
		char byte=0;
		ssize_t ignored=write(wakeupPipe[1],&byte,1);
		(void)ignored;
	}

	if (acceptThread.joinable())
		acceptThread.join();

	if (listenSocket>=0)
	{
		close(listenSocket);
		listenSocket = -1;
	}

	//! wake up every client that is blocked reading.  shutdown() rather than
	//! close() - the owning thread is the one that closes, so the descriptor
	//! cannot be reused underneath us
	{
		std::lock_guard<std::mutex> guard(clientLock);
		for (size_t i=0; i<clientSockets.size(); i++)
		{
			shutdown(clientSockets[i],SHUT_RDWR);
		}
	}

	//! the threads remove themselves from clientSockets as they finish, so
	//! the vector of threads is taken away from under the lock and joined
	//! outside it
	std::vector<std::thread> threads;
	{
		std::lock_guard<std::mutex> guard(clientLock);
		threads.swap(clientThreads);
	}

	for (size_t i=0; i<threads.size(); i++)
	{
		if (threads[i].joinable())
			threads[i].join();
	}

	for (int i=0; i<2; i++)
	{
		if (wakeupPipe[i]>=0)
		{
			close(wakeupPipe[i]);
			wakeupPipe[i]=-1;
		}
	}
};

//////////////////////////////////////////////////////////////////

void PrologServer::AcceptLoop(void)
{
	while (running)
	{
		//! wait for either a connection or the signal to stop
		struct pollfd fds[2];
		fds[0].fd = listenSocket;
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		fds[1].fd = wakeupPipe[0];
		fds[1].events = POLLIN;
		fds[1].revents = 0;

		int r=poll(fds,2,-1);
		if (r<0)
		{
			if (errno==EINTR)
				continue;
			break;
		}

		//! Stop() has spoken
		if (fds[1].revents!=0)
			break;

		if (fds[0].revents==0)
			continue;

		struct sockaddr_in peer;
		socklen_t peerLen=sizeof(peer);

		int fd=accept(listenSocket,(struct sockaddr*)&peer,&peerLen);
		if (fd<0)
		{
			if (errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK)
				continue;

			//! something went wrong with the listening socket
			break;
		}

		if (!running)
		{
			close(fd);
			break;
		}

		//! answers are small and latency matters more than packing
		int on=1;
		setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&on,sizeof(on));

		std::lock_guard<std::mutex> guard(clientLock);
		clientSockets.push_back(fd);
		clientThreads.push_back(std::thread(&PrologServer::ClientLoop,this,fd));
	}
};

bool PrologServer::HandleCommand(int fd,const std::string& command)
{
	std::string answer;

	{
		//! one command at a time - the engine's stack, string table and
		//! output string are all global
		std::lock_guard<std::mutex> guard(EngineLock());

		StringWriter capture;
		IOWriter* previous=System::GetWriter();
		System::SetWriter(&capture);

		InterpreterResult result=ExecuteCommand(command,*database);

		System::SetWriter(previous);

		if (result==INTERPRETER_QUIT)
			return false;

		answer.swap(capture.text);
	}

	//! always send something back so a client waiting on a reply is not
	//! left hanging on a command that printed nothing
	if (answer.empty())
		answer = "\n";
	else if (answer[answer.size()-1]!='\n')
		answer += "\n";

	return WriteAll(fd,answer);
};

void PrologServer::ClientLoop(int fd)
{
	std::string pending;
	char buf[4096];

	bool open=true;
	while (open && running)
	{
		ssize_t n=recv(fd,buf,sizeof(buf),0);
		if (n<0 && errno==EINTR)
			continue;

		if (n<=0)
			break;

		pending.append(buf,size_t(n));

		if (pending.size()>MAX_LINE)
		{
			WriteAll(fd,"line too long\n");
			break;
		}

		//! deal with every complete line that has arrived
		size_t start=0;
		while (open)
		{
			size_t eol=pending.find('\n',start);
			if (eol==std::string::npos)
				break;

			std::string line=pending.substr(start,eol-start);
			start = eol+1;

			//! telnet sends cr lf
			if (!line.empty() && line[line.size()-1]=='\r')
				line.erase(line.size()-1);

			if (!Trim(line).empty())
				open = HandleCommand(fd,line);
		}
		pending.erase(0,start);
	}

	//! unregister before closing, so Stop() can never call shutdown() on a
	//! descriptor number that has already been closed and handed out again
	{
		std::lock_guard<std::mutex> guard(clientLock);
		for (size_t i=0; i<clientSockets.size(); i++)
		{
			if (clientSockets[i]==fd)
			{
				clientSockets.erase(clientSockets.begin()+i);
				break;
			}
		}
		close(fd);
	}
};
