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

#include <time.h>

#include "timer.h"

/////////////////////////////////////////////////////////////////////////////

TTime::TTime()
	: m_bInitialised(false),
	  m_fBaseTime(0.0f),
	  m_fLastTime(0.0f),
	  m_fCurrentTime(0.0f),
	  m_fFrameTime(0.0f),
	  m_dTimerStart(0)
{
}

TTime::~TTime()
{
	m_bInitialised = false;
}

double TTime::CurrentTime(void) const
{
	return m_fCurrentTime;
}

double TTime::FrameTime(void) const
{
	return m_fFrameTime;
}

long long TTime::GetTicks()
{
	//! CLOCK_MONOTONIC is the POSIX equivalent of QueryPerformanceCounter -
	//! it is unaffected by changes to the wall clock and is nanosecond based
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC,&ts)!=0)
	{
		return 0;
	}
	return (static_cast<long long>(ts.tv_sec) * 1000000000LL) +
	       static_cast<long long>(ts.tv_nsec);
}

void TTime::Init()
{
	m_dTimerStart = GetTicks();
	m_bInitialised = true;
}

void TTime::Update()
{
	if (m_bInitialised)
	{
		m_fCurrentTime = GetMonotonicTime() - m_fBaseTime;
		m_fFrameTime = m_fCurrentTime - m_fLastTime;
		m_fLastTime = m_fCurrentTime;
	}
}

void TTime::Reset()
{
	if (m_bInitialised)
	{
		m_fBaseTime = GetMonotonicTime();
		m_fLastTime = m_fCurrentTime = m_fFrameTime = 0.0;
	}
}

double TTime::GetMonotonicTime()
{
	return static_cast<double>(GetTicks() - m_dTimerStart) / 1000000000.0;
}

/////////////////////////////////////////////////////////////////////////////

