% the classic list relations.  each is a relation rather than a function,
% so append runs backwards as well as forwards:
%
%   ?- member(X,[a,b,c]).        % X=a  X=b  X=c
%   ?- append([1,2],[3,4],Z).    % Z=[1,2,3,4]
%   ?- append(X,Y,[1,2]).        % every way to split [1,2]
%   ?- reverse([1,2,3],R).       % R=[3,2,1]
%   ?- last(X,[a,b,c]).          % X=c

member(X,[X|_]).
member(X,[_|T]) :- member(X,T).

append([],L,L).
append([H|T],L,[H|R]) :- append(T,L,R).

reverse([],[]).
reverse([H|T],R) :- reverse(T,RT), append(RT,[H],R).

last(X,[X]).
last(X,[_|T]) :- last(X,T).
