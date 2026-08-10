test([]).
test([H|T]) :- write(H), nl, test(T).

