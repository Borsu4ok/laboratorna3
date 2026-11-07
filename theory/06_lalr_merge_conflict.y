%token a b c d e
%%
s
    : a x d
    | b y d
    | a y e
    | b x e
    ;
x
    : c
    ;
y
    : c
    ;
