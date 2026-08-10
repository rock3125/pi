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

/////////////////////////////////////////////////////////////////////////////

class TTime
{
public:
	TTime(void);
	~TTime(void);

	void Init(void);
	void Reset(void);

	//! Call this onces per game frame
	void Update();

	//! Access funcs
	double CurrentTime(void) const;
	double FrameTime(void) const;

private:

	bool	m_bInitialised;

	double	m_fBaseTime;
	double	m_fLastTime;

	double	m_fCurrentTime;
	double	m_fFrameTime;

	//! monotonic clock reading (in nanoseconds) taken at Init()
	long long	m_dTimerStart;

	//! seconds elapsed on the monotonic clock since Init()
	double	GetMonotonicTime();

	//! raw monotonic clock reading in nanoseconds
	static long long GetTicks();
};

