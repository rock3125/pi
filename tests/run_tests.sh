#!/bin/sh
#
# Regression tests for the PI prolog engine.
#
#   ./tests/run_tests.sh          run every test
#
# Each test feeds a script to the interpreter and compares the answers
# against what is expected.  Run from the directory holding the binary.

PROLOG=./prolog
PASS=0
FAIL=0

# check <name> <program-file-or-"-"> <input> <expected...>
check()
{
	name="$1"; prog="$2"; input="$3"; expected="$4"

	if [ "$prog" = "-" ]; then
		actual=$(printf "%s\nexit\n" "$input" | $PROLOG 2>&1)
	else
		actual=$(printf "%s\nexit\n" "$input" | $PROLOG "$prog" 2>&1)
	fi

	# keep only the answer lines - drop banner, prompts, timings, blanks
	actual=$(printf "%s" "$actual" | \
		grep -v "Prolog Interpreter Engine" | \
		grep -v "type 'help'" | \
		grep -v "compiled database" | \
		grep -v "execution time" | \
		sed 's/^>//' | \
		grep -v "^$" | \
		tr '\n' '|' | sed 's/|$//')

	if [ "$actual" = "$expected" ]; then
		PASS=$((PASS+1))
	else
		FAIL=$((FAIL+1))
		echo "FAIL: $name"
		echo "      expected: $expected"
		echo "      actual:   $actual"
	fi
}

# count <name> <program> <input> <pattern> <expected-count>
count()
{
	name="$1"; prog="$2"; input="$3"; pattern="$4"; expected="$5"
	actual=$(printf "%s\nexit\n" "$input" | $PROLOG "$prog" 2>&1 | grep -c "$pattern")
	if [ "$actual" = "$expected" ]; then
		PASS=$((PASS+1))
	else
		FAIL=$((FAIL+1))
		echo "FAIL: $name (expected $expected matches, got $actual)"
	fi
}

######################################################################
# query marker
######################################################################

check "query marker ?- with space" samples/family.pl \
	'?- father(fred,peter).' 'yes'
check "query marker ?- no space" samples/family.pl \
	'?-father(fred,peter).' 'yes'
check "bare ? rejected" samples/family.pl \
	'?father(fred,peter).' 'error parsing query: query must start with '"'"'?-'"'"' (line 1, character 2)'
check "minus still parses after ?-" - '?- X is 3 - 1.' 'X=2'
check "negative literal after ?-" - '?- X is 0 - 5.' 'X=-5'
check "query must end with a full stop" samples/family.pl \
	'?- father(fred,X)' 'error parsing query: query must end with '"'"'.'"'"' (line 1, character 18)'

######################################################################
# facts and simple unification
######################################################################

check "fact lookup, one var" samples/family.pl \
	'?- father(fred,X).' 'X=peter|X=mark|X=micheal|X=jj'

check "fact lookup, other direction" samples/family.pl \
	'?- mother(X,jj).' 'X=frieda'

check "ground fact true" samples/family.pl '?- father(fred,peter).' 'yes'
check "ground fact false" samples/family.pl '?- father(fred,zoe).' 'no'

check "rule with \=" samples/family.pl '?- different(a,b).' 'yes'
check "rule with \= fails" samples/family.pl '?- different(a,a).' 'no'

check "two-var rule" samples/family.pl '?- half(peter,X).' 'X=micheal|X=jj'
check "two-var rule, other" samples/family.pl '?- half(micheal,X).' 'X=peter|X=mark'

check "disjunction dedups" samples/family.pl '?- mother(anne,X) ; mother(anne,X).' \
	'X=peter|X=mark'

######################################################################
# lists
######################################################################

check "empty list" samples/test.pl '?- test([]).' 'yes'
check "single element list" samples/test.pl '?- test([1]).' '1|yes'
check "atom list" samples/test.pl '?- test([a,b,c]).' 'a|b|c|yes'
check "longer list" samples/test.pl '?- test([1,2,3,4,5]).' '1|2|3|4|5|yes'

######################################################################
# a list argument matches wherever its clause sits in the database
#
# GetMatchingClauses used to hand ListsCanBind its own loop counter in
# place of the database index of the candidate clause.  The two agree
# only while the candidates start at clause 0, so a clause with a list
# argument stopped matching the moment anything was defined ahead of it -
# and every test here happened to define it first.
######################################################################

check "empty list arg, clause first" - 'p([],0).
?- p([],N).' 'N=0'
check "empty list arg, one clause ahead" - 'x(a).
p([],0).
?- p([],N).' 'N=0'
check "empty list arg, several clauses ahead" - 'x(a).
y(b).
z(c).
p([],0).
?- p([],N).' 'N=0'
check "list arg in second position" - 'x(a).
p(0,[]).
?- p(N,[]).' 'N=0'
check "non-empty list arg, clauses ahead" - 'x(a).
p([a,b],two).
?- p([a,b],N).' 'N=two'
check "nested list arg, clauses ahead" - 'x(a).
p([[1,2],[3]],ok).
?- p([[1,2],[3]],N).' 'N=ok'
check "a list argument still has to match" - 'x(a).
p([a],one).
?- p([b],N).' 'no'
check "recursion over lists, clauses ahead" - 'x(a).
len([],0).
len([H|T],N) :- len(T,M), N is M + 1.
?- len([a,b,c],N).' 'N=3'
check "recursion over lists, after a whole program" - 'father(fred,peter).
mother(anne,peter).
different(X,Y) :- X \= Y.
len([],0).
len([H|T],N) :- len(T,M), N is M + 1.
?- len([a,b,c,d],N).' 'N=4'

######################################################################
# arithmetic - "is" evaluates, "=" does not
######################################################################

check "is minus" - '?- X is 3 - 1.' 'X=2'
check "is times" - '?- A is 2 * 3.' 'A=6'
check "is plus" - '?- A is 2 + 3.' 'A=5'
check "is divide" - '?- A is 10 / 2.' 'A=5'
check "chained is" - '?- B is 7 - 2, C is B + 1.' 'B=5|C=6'
check "ground is checks" - '?- 3 is 1 + 2.' 'yes'
check "ground is fails" - '?- 4 is 1 + 2.' 'no'
check "is then match" - 'p(1).
p(2).
?- Y is 2, p(Y).' 'Y=2'
check "is then fail to match" - 'p(1).
p(2).
?- Z is 5 - 1, p(Z).' 'no'
check "is from fact" - 'p(5).
?- p(M), N is M + 1.' 'M=5|N=6'

######################################################################
# unification - "=" binds without evaluating
######################################################################

check "= does not evaluate" - '?- X = 3 - 1.' 'X=3-1'
check "= binds an atom" - '?- X = fred.' 'X=fred'
check "= on equal atoms" - '?- fred = fred.' 'yes'
check "= on different atoms" - '?- fred = mary.' 'no'
check "= unifies structures" - '?- f(X,b) = f(a,Y).' 'X=a|Y=b'
check "= destructures a list" - '?- [H|T] = [1,2,3].' 'H=1|T=[2,3]'
check "= on empty lists" - '?- [] = [].' 'yes'
check "= empty list has no head" - '?- [H|T] = [].' 'no'

######################################################################
# comparisons
######################################################################

check "greater true"  - '?- 3 > 2.' 'yes'
check "greater false" - '?- 2 > 3.' 'no'
check "lessthan true" - '?- 2 =< 2.' 'yes'
check "arith equal" - '?- 2 =:= 2.' 'yes'
check "arith equal evaluates" - '?- 1 + 1 =:= 2.' 'yes'
check "arith notequal" - '?- 2 =\= 3.' 'yes'
check "arith notequal evaluates" - '?- 2 * 3 =\= 5.' 'yes'

######################################################################
# operator precedence - * and / bind before + and -, all before =:=
######################################################################

check "times before plus" - '?- X is 2 + 3 * 4.' 'X=14'
check "times before plus, other side" - '?- X is 2 * 3 + 4.' 'X=10'
check "minus associates left" - '?- X is 10 - 3 - 4.' 'X=3'
check "brackets override" - '?- X is (2 + 3) * 4.' 'X=20'

######################################################################
# term identity - == compares structure and binds nothing
######################################################################

check "identical numbers" - '?- 2 == 2.' 'yes'
check "identity does not evaluate" - '?- 1 + 1 == 2.' 'no'
check "int and float differ" - '?- 2 == 2.0.' 'no'
check "identical after binding" - '?- X = a, X == a.' 'X=a'
check "distinct free vars differ" - '?- X == Y.' 'no'
check "same var is identical" - '?- f(X) == f(X).' 'yes'
check "not identical" - '?- f(a) \== f(b).' 'yes'

######################################################################
# \= is not-unifiable - it tries the unification and takes it back
######################################################################

check "notequal true" - '?- 2 \= 3.' 'yes'
check "free var unifies with anything" - '?- X \= a.' 'no'
check "different functors" - '?- f(X) \= g(a).' 'yes'
check "failed attempt is undone" - '?- f(X,g(Y)) \= f(a,b), X = c.' 'X=c'

######################################################################
# the pre-standard spellings are gone, with a pointer at the right one
######################################################################

check "!= rejected" - '?- 2 != 3.' \
	'error parsing query: '"'"'!='"'"' is not prolog - use '"'"'\='"'"' (or '"'"'=\='"'"' for arithmetic) (line 1, character 7)'
check "<= rejected" - '?- 2 <= 3.' \
	'error parsing query: '"'"'<='"'"' is not prolog - use '"'"'=<'"'"' (line 1, character 7)'

######################################################################
# towers of hanoi - move counts must be 2^n - 1
######################################################################

count "hanoi 1 disk"  samples/towers_of_hanoi.pl '?- move(1,left,right,centre).' 'Move top disk' 1
count "hanoi 2 disks" samples/towers_of_hanoi.pl '?- move(2,left,right,centre).' 'Move top disk' 3
count "hanoi 3 disks" samples/towers_of_hanoi.pl '?- move(3,left,right,centre).' 'Move top disk' 7
count "hanoi 4 disks" samples/towers_of_hanoi.pl '?- move(4,left,right,centre).' 'Move top disk' 15
count "hanoi 5 disks" samples/towers_of_hanoi.pl '?- move(5,left,right,centre).' 'Move top disk' 31
count "hanoi 6 disks" samples/towers_of_hanoi.pl '?- move(6,left,right,centre).' 'Move top disk' 63

######################################################################
# head variable propagation - values computed in a body reach the caller
######################################################################

check "head var from arithmetic" - 'p(5).
t(N) :- p(M), N is M + 1.
?- t(X).' 'X=6'

check "head var passthrough" - 'p(5).
t(N) :- p(N).
?- t(X).' 'X=5'

check "head var two levels" - 'p(2).
t(N) :- p(M), N is M + 1.
u(N) :- t(M), N is M + 1.
?- u(X).' 'X=4'

check "list length" - 'len([],0).
len([H|T],N) :- len(T,M), N is M + 1.
?- len([a,b,c],N).' 'N=3'

######################################################################
# not/1
######################################################################

check "negation of false goal" - 'q(a).
?- \+ q(z).' 'yes'
check "negation of true goal" - 'q(a).
?- \+ q(a).' 'no'
check "bracketed negation" - 'q(a).
?- \+(q(a)).' 'no'
check "negation in rule body" - 'q(a).
q(b).
s(X) :- \+ q(X).
?- s(z).' 'yes'
check "negation in rule body, fails" - 'q(a).
s(X) :- \+ q(X).
?- s(a).' 'no'

######################################################################
# relational list predicates - heads destructure, route repeated
# variables, and build results as the recursion returns
######################################################################

LISTS='member(X,[X|_]).
member(X,[_|T]) :- member(X,T).
append([],L,L).
append([H|T],L,[H|R]) :- append(T,L,R).'

check "member checks" - "$LISTS
?- member(2,[1,2,3])." 'yes'
check "member rejects" - "$LISTS
?- member(z,[a,b,c])." 'no'
check "member enumerates" - "$LISTS
?- member(X,[a,b,c])." 'X=a|X=b|X=c'
check "member intersection" - "$LISTS
?- member(X,[a,b]), member(X,[b,c])." 'X=b'
check "append builds forward" - "$LISTS
?- append([1,2],[3,4],Z)." 'Z=[1,2,3,4]'
check "append finds a prefix" - "$LISTS
?- append(X,[3,4],[1,2,3,4])." 'X=[1,2]'
check "append finds a suffix" - "$LISTS
?- append([1,2],Y,[1,2,3,4])." 'Y=[3,4]'
check "append enumerates splits" - "$LISTS
?- append(X,Y,[1,2])." 'X=[]|Y=[1,2]|X=[1]|Y=[2]|X=[1,2]|Y=[]'
check "append chains" - "$LISTS
?- append([1],[2],Z), append(Z,[3],W)." 'Z=[1,2]|W=[1,2,3]'
check "reverse" - "$LISTS
reverse([],[]).
reverse([H|T],R) :- reverse(T,RT), append(RT,[H],R).
?- reverse([1,2,3],R)." 'R=[3,2,1]'
check "last element" - 'last(X,[X]).
last(X,[_|T]) :- last(X,T).
?- last(X,[a,b,c]).' 'X=c'
check "repeated head var routes" - 'max(X,Y,X) :- X >= Y, !.
max(X,Y,Y).
?- max(7,3,M).' 'M=7'
check "repeated head var, other side" - 'max(X,Y,X) :- X >= Y, !.
max(X,Y,Y).
?- max(3,7,M).' 'M=7'
check "literal head in pattern" - '?- [a|T] = [a,b,c].' 'T=[b,c]'
check "literal head must match" - '?- [a|T] = [z,b].' 'no'
check "two elements before the bar" - '?- [A,B|T] = [1,2,3,4].' 'A=1|B=2|T=[3,4]'

######################################################################
# cut
######################################################################

check "cut prunes alternatives" - 'q(a).
q(b).
q(c).
r(X) :- q(X), !.
?- r(X).' 'X=a'

check "cut with no alternatives" - 'q(a).
r(X) :- q(X), !.
?- r(X).' 'X=a'

check "cut then more goals" - 'q(a).
q(b).
p(1).
r(X) :- q(X), !, p(Y).
?- r(X).' 'X=a'

check "bare cut succeeds" - 'r :- !.
?- r.' 'yes'

check "fail predicate" - 'r :- fail.
?- r.' 'no'

######################################################################

echo
echo "passed: $PASS   failed: $FAIL"

######################################################################
# the line editor only switches on for a terminal, so those tests drive
# the interpreter under a pty - they need python3 for its pty module
######################################################################

EXTRA=0
if [ "$FAIL" -eq 0 ]; then
	if command -v python3 >/dev/null 2>&1; then
		echo
		python3 "$(dirname "$0")/test_interactive.py" || EXTRA=1
		echo
		python3 "$(dirname "$0")/test_server.py" || EXTRA=1
	else
		echo
		echo "(skipping interactive and server tests - python3 not available)"
	fi
fi

[ "$FAIL" -eq 0 ] && [ "$EXTRA" -eq 0 ]
