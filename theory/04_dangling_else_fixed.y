%token IF ELSE COND OTHER
%precedence THEN
%precedence ELSE
%%
stmt
    : IF '(' COND ')' stmt %prec THEN
    | IF '(' COND ')' stmt ELSE stmt
    | OTHER
    ;
