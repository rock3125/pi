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

#include "binding.h"

//////////////////////////////////////////////////////////////////

Binding::Binding(void)
{
	rhs=-1;
};

Binding::Binding(int _lhs,int _rhs)
{
	lhs=_lhs;
	rhs=_rhs;
};

Binding::~Binding(void)
{
};

Binding::Binding(const Binding& b)
{
	operator=(b);
};

const Binding& Binding::operator=(const Binding& b)
{
	lhs=b.lhs;
	rhs=b.rhs;
	return *this;
};

