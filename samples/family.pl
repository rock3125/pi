father(fred,peter).
father(fred,mark).
father(fred,micheal).
father(fred,jj).
mother(anne,peter).
mother(anne,mark).
mother(frieda,micheal).
mother(frieda,jj).

different(X,Y) :- X \= Y.

%brother(X,Y) :- father(Z,X), father (Z,Y), mother(K,X), mother(K,Y), different(X,Y).

half(X,Y) :- father(Z,X), father (Z,Y), mother(K,X), mother(L,Y), different(K,L) ; father(A,X), father (B,Y), mother(K,X), mother(K,Y), different(A,B).

