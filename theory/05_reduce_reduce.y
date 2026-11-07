%token ID
%%
stmt
    : variable '=' ID ';'
    | label '=' ID ';'
    ;
variable
    : ID
    ;
label
    : ID
    ;
