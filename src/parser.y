%code requires {
#include <string>
#include "ast.hpp"
}

%code {
#include <cstdio>
#include <vector>

int yylex();
void yyerror(const char* message);

Node* g_program = nullptr;

static Node* mk(NodeKind kind, const YYLTYPE& loc, const std::string& text = "", std::vector<Node*> kids = {}) {
    return makeNode(kind, loc.first_line, loc.first_column, text, std::move(kids));
}

static Node* binary(Node* left, const char* op, Node* right, const YYLTYPE& loc) {
    return mk(NodeKind::Binary, loc, op, {left, right});
}

static Node* assign(Node* left, const char* op, Node* right, const YYLTYPE& loc) {
    return mk(NodeKind::Assign, loc, op, {left, right});
}

static Node* typed(Node* node, const Node* type) {
    node->dtype = type->text;
    node->isConst = type->isConst;
    node->isAuto = type->text == "auto";
    return node;
}
}

%define api.value.type {Node*}
%define parse.error verbose
%define parse.trace
%locations
%token-table
%expect 0

%token IDENT        "identifier"
%token INT_LIT      "integer constant"
%token FLOAT_LIT    "floating constant"
%token CHAR_LIT     "character constant"
%token BOOL_LIT     "boolean constant"
%token STRING_LIT   "string literal"

%token INT "int" LONG "long" SHORT "short" CHAR "char" FLOAT "float" DOUBLE "double" BOOL "bool"
%token VOID "void" AUTO "auto" CONST "const"
%token IF "if" ELSE "else" WHILE "while" DO "do" FOR "for"
%token RETURN "return" BREAK "break" CONTINUE "continue"
%token USING "using" NAMESPACE "namespace" STATIC_CAST "static_cast" SCOPE "::"

%token INC "++" DEC "--" AND "&&" OR "||" EQ "==" NE "!=" LE "<=" GE ">=" SHL "<<" SHR ">>"
%token ADD_ASSIGN "+=" SUB_ASSIGN "-=" MUL_ASSIGN "*=" DIV_ASSIGN "/=" MOD_ASSIGN "%="
%token AND_ASSIGN "&=" OR_ASSIGN "|=" XOR_ASSIGN "^=" SHL_ASSIGN "<<=" SHR_ASSIGN ">>="

%precedence THEN
%precedence ELSE

%right '=' ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN AND_ASSIGN OR_ASSIGN XOR_ASSIGN SHL_ASSIGN SHR_ASSIGN
%right '?' ':'
%left OR
%left AND
%left '|'
%left '^'
%left '&'
%left EQ NE
%left '<' '>' LE GE
%left SHL SHR
%left '+' '-'
%left '*' '/' '%'
%precedence UNARY
%precedence INC DEC

%printer { if ($$) fprintf(yyo, "%s", nodeLabel($$).c_str()); } <>

%start program

%%

program
    : externals                                 { g_program = makeNode(NodeKind::Program, 1, 1, "", $1->kids); $$ = g_program; }
    ;

externals
    : %empty                                    { $$ = makeNode(NodeKind::List, 0, 0); }
    | externals external                        { $$ = $1; if ($2) $$->kids.push_back($2); }
    ;

external
    : function_def                              { $$ = $1; }
    | function_decl                             { $$ = $1; }
    | declaration                               { $$ = $1; }
    | USING NAMESPACE IDENT ';'                 { $$ = mk(NodeKind::Using, @1, $3->text); }
    | error ';'                                 { $$ = nullptr; yyerrok; }
    | error '}'                                 { $$ = nullptr; yyerrok; }
    ;

type_spec
    : base_type                                 { $$ = $1; }
    | CONST base_type                           { $$ = $2; $$->isConst = true; }
    ;

base_type
    : INT                                       { $$ = mk(NodeKind::TypeSpec, @1, "int"); }
    | LONG                                      { $$ = mk(NodeKind::TypeSpec, @1, "int"); }
    | LONG INT                                  { $$ = mk(NodeKind::TypeSpec, @1, "int"); }
    | LONG LONG                                 { $$ = mk(NodeKind::TypeSpec, @1, "int"); }
    | LONG LONG INT                             { $$ = mk(NodeKind::TypeSpec, @1, "int"); }
    | SHORT                                     { $$ = mk(NodeKind::TypeSpec, @1, "int"); }
    | SHORT INT                                 { $$ = mk(NodeKind::TypeSpec, @1, "int"); }
    | CHAR                                      { $$ = mk(NodeKind::TypeSpec, @1, "char"); }
    | FLOAT                                     { $$ = mk(NodeKind::TypeSpec, @1, "double"); }
    | DOUBLE                                    { $$ = mk(NodeKind::TypeSpec, @1, "double"); }
    | BOOL                                      { $$ = mk(NodeKind::TypeSpec, @1, "bool"); }
    | VOID                                      { $$ = mk(NodeKind::TypeSpec, @1, "void"); }
    | AUTO                                      { $$ = mk(NodeKind::TypeSpec, @1, "auto"); }
    ;

function_def
    : type_spec IDENT '(' params ')' block      { $$ = mk(NodeKind::FuncDef, @2, $2->text, {$4, $6}); $$->dtype = $1->text; }
    ;

function_decl
    : type_spec IDENT '(' params ')' ';'        { $$ = mk(NodeKind::FuncDecl, @2, $2->text, {$4}); $$->dtype = $1->text; }
    ;

params
    : %empty                                    { $$ = mk(NodeKind::Params, @$); }
    | VOID                                      { $$ = mk(NodeKind::Params, @1); }
    | param_list                                { $$ = $1; }
    ;

param_list
    : param                                     { $$ = mk(NodeKind::Params, @1, "", {$1}); }
    | param_list ',' param                      { $$ = $1; $$->kids.push_back($3); }
    ;

param
    : type_spec IDENT                           { $$ = typed(mk(NodeKind::Param, @2, $2->text), $1); }
    | type_spec '&' IDENT                       { $$ = typed(mk(NodeKind::Param, @3, $3->text), $1); $$->isRef = true; }
    | type_spec IDENT '[' ']'                   { $$ = typed(mk(NodeKind::Param, @2, $2->text), $1); $$->isArray = true; }
    | type_spec IDENT '[' expr ']'              { $$ = typed(mk(NodeKind::Param, @2, $2->text), $1); $$->isArray = true; }
    ;

declaration
    : type_spec init_declarators ';'            {
                                                    $$ = mk(NodeKind::DeclStmt, @1, typeText(typed($1, $1)), $2->kids);
                                                    for (Node* d : $$->kids) typed(d, $1);
                                                }
    ;

init_declarators
    : init_declarator                           { $$ = mk(NodeKind::List, @1, "", {$1}); }
    | init_declarators ',' init_declarator      { $$ = $1; $$->kids.push_back($3); }
    ;

init_declarator
    : declarator                                { $$ = $1; }
    | declarator '=' initializer                { $$ = $1; $$->init = $3; $$->kids.push_back($3); }
    ;

declarator
    : IDENT                                     { $$ = mk(NodeKind::VarDecl, @1, $1->text); }
    | IDENT '[' expr ']'                        { $$ = mk(NodeKind::VarDecl, @1, $1->text, {$3}); $$->isArray = true; $$->sizeExpr = $3; }
    | IDENT '[' ']'                             { $$ = mk(NodeKind::VarDecl, @1, $1->text); $$->isArray = true; }
    ;

initializer
    : expr                                      { $$ = $1; }
    | '{' init_list '}'                         { $$ = $2; }
    | '{' init_list ',' '}'                     { $$ = $2; }
    ;

init_list
    : expr                                      { $$ = mk(NodeKind::InitList, @1, "", {$1}); }
    | init_list ',' expr                        { $$ = $1; $$->kids.push_back($3); }
    ;

block
    : '{' block_items '}'                       { $$ = mk(NodeKind::Block, @1, "", $2->kids); }
    ;

block_items
    : %empty                                    { $$ = makeNode(NodeKind::List, 0, 0); }
    | block_items block_item                    { $$ = $1; $$->kids.push_back($2); }
    ;

block_item
    : declaration                               { $$ = $1; }
    | statement                                 { $$ = $1; }
    ;

statement
    : block                                                     { $$ = $1; }
    | expression ';'                                            { $$ = mk(NodeKind::ExprStmt, @1, "", {$1}); }
    | ';'                                                       { $$ = mk(NodeKind::Empty, @1); }
    | IF '(' expression ')' statement %prec THEN                { $$ = mk(NodeKind::If, @1, "", {$3, $5}); }
    | IF '(' expression ')' statement ELSE statement            { $$ = mk(NodeKind::If, @1, "", {$3, $5, $7}); }
    | WHILE '(' expression ')' statement                        { $$ = mk(NodeKind::While, @1, "", {$3, $5}); }
    | DO statement WHILE '(' expression ')' ';'                 { $$ = mk(NodeKind::DoWhile, @1, "", {$2, $5}); }
    | FOR '(' opt_expr ';' opt_expr ';' opt_expr ')' statement  { $$ = mk(NodeKind::For, @1, "", {$3, $5, $7, $9}); }
    | FOR '(' declaration opt_expr ';' opt_expr ')' statement   { $$ = mk(NodeKind::For, @1, "", {$3, $4, $6, $8}); }
    | FOR '(' type_spec IDENT ':' expr ')' statement            {
                                                                    Node* var = typed(mk(NodeKind::VarDecl, @4, $4->text), $3);
                                                                    $$ = mk(NodeKind::RangeFor, @1, "", {var, $6, $8});
                                                                }
    | FOR '(' type_spec '&' IDENT ':' expr ')' statement        {
                                                                    Node* var = typed(mk(NodeKind::VarDecl, @5, $5->text), $3);
                                                                    var->isRef = true;
                                                                    $$ = mk(NodeKind::RangeFor, @1, "", {var, $7, $9});
                                                                }
    | RETURN ';'                                                { $$ = mk(NodeKind::Return, @1); }
    | RETURN expression ';'                                     { $$ = mk(NodeKind::Return, @1, "", {$2}); }
    | BREAK ';'                                                 { $$ = mk(NodeKind::Break, @1); }
    | CONTINUE ';'                                              { $$ = mk(NodeKind::Continue, @1); }
    | error ';'                                                 { $$ = mk(NodeKind::Empty, @1); yyerrok; }
    ;

opt_expr
    : %empty                                    { $$ = mk(NodeKind::Empty, @$); }
    | expression                                { $$ = $1; }
    ;

expression
    : expr                                      { $$ = $1; }
    | expression ',' expr                       { $$ = mk(NodeKind::Comma, @2, ",", {$1, $3}); }
    ;

expr
    : expr '=' expr                             { $$ = assign($1, "=", $3, @2); }
    | expr ADD_ASSIGN expr                      { $$ = assign($1, "+=", $3, @2); }
    | expr SUB_ASSIGN expr                      { $$ = assign($1, "-=", $3, @2); }
    | expr MUL_ASSIGN expr                      { $$ = assign($1, "*=", $3, @2); }
    | expr DIV_ASSIGN expr                      { $$ = assign($1, "/=", $3, @2); }
    | expr MOD_ASSIGN expr                      { $$ = assign($1, "%=", $3, @2); }
    | expr AND_ASSIGN expr                      { $$ = assign($1, "&=", $3, @2); }
    | expr OR_ASSIGN expr                       { $$ = assign($1, "|=", $3, @2); }
    | expr XOR_ASSIGN expr                      { $$ = assign($1, "^=", $3, @2); }
    | expr SHL_ASSIGN expr                      { $$ = assign($1, "<<=", $3, @2); }
    | expr SHR_ASSIGN expr                      { $$ = assign($1, ">>=", $3, @2); }
    | expr '?' expression ':' expr              { $$ = mk(NodeKind::Ternary, @2, "?:", {$1, $3, $5}); }
    | expr OR expr                              { $$ = binary($1, "||", $3, @2); }
    | expr AND expr                             { $$ = binary($1, "&&", $3, @2); }
    | expr '|' expr                             { $$ = binary($1, "|", $3, @2); }
    | expr '^' expr                             { $$ = binary($1, "^", $3, @2); }
    | expr '&' expr                             { $$ = binary($1, "&", $3, @2); }
    | expr EQ expr                              { $$ = binary($1, "==", $3, @2); }
    | expr NE expr                              { $$ = binary($1, "!=", $3, @2); }
    | expr '<' expr                             { $$ = binary($1, "<", $3, @2); }
    | expr '>' expr                             { $$ = binary($1, ">", $3, @2); }
    | expr LE expr                              { $$ = binary($1, "<=", $3, @2); }
    | expr GE expr                              { $$ = binary($1, ">=", $3, @2); }
    | expr SHL expr                             { $$ = binary($1, "<<", $3, @2); }
    | expr SHR expr                             { $$ = binary($1, ">>", $3, @2); }
    | expr '+' expr                             { $$ = binary($1, "+", $3, @2); }
    | expr '-' expr                             { $$ = binary($1, "-", $3, @2); }
    | expr '*' expr                             { $$ = binary($1, "*", $3, @2); }
    | expr '/' expr                             { $$ = binary($1, "/", $3, @2); }
    | expr '%' expr                             { $$ = binary($1, "%", $3, @2); }
    | '-' expr %prec UNARY                      { $$ = mk(NodeKind::Unary, @1, "-", {$2}); }
    | '+' expr %prec UNARY                      { $$ = mk(NodeKind::Unary, @1, "+", {$2}); }
    | '!' expr %prec UNARY                      { $$ = mk(NodeKind::Unary, @1, "!", {$2}); }
    | '~' expr %prec UNARY                      { $$ = mk(NodeKind::Unary, @1, "~", {$2}); }
    | INC expr %prec UNARY                      { $$ = mk(NodeKind::Unary, @1, "++", {$2}); }
    | DEC expr %prec UNARY                      { $$ = mk(NodeKind::Unary, @1, "--", {$2}); }
    | expr INC                                  { $$ = mk(NodeKind::Postfix, @2, "++", {$1}); }
    | expr DEC                                  { $$ = mk(NodeKind::Postfix, @2, "--", {$1}); }
    | '(' type_spec ')' expr %prec UNARY        { $$ = mk(NodeKind::Cast, @1, "", {$4}); $$->dtype = $2->text; }
    | STATIC_CAST '<' type_spec '>' '(' expression ')'
                                                { $$ = mk(NodeKind::Cast, @1, "static_cast", {$6}); $$->dtype = $3->text; }
    | name '(' args ')'                         {
                                                    $$ = mk(NodeKind::Call, @1, $1->text, $3->kids);
                                                    $$->qualifier = $1->qualifier;
                                                }
    | name '[' expression ']'                   {
                                                    $$ = mk(NodeKind::Index, @1, $1->text, {$3});
                                                    $$->qualifier = $1->qualifier;
                                                }
    | name                                      { $$ = $1; }
    | INT_LIT                                   { $$ = $1; }
    | FLOAT_LIT                                 { $$ = $1; }
    | CHAR_LIT                                  { $$ = $1; }
    | BOOL_LIT                                  { $$ = $1; }
    | strings                                   { $$ = $1; }
    | '(' expression ')'                        { $$ = $2; }
    ;

name
    : IDENT                                     { $$ = $1; }
    | IDENT SCOPE IDENT                         {
                                                    $$ = $3;
                                                    $$->qualifier = $1->text;
                                                    $$->line = $1->line;
                                                    $$->col = $1->col;
                                                }
    ;

strings
    : STRING_LIT                                { $$ = $1; }
    | strings STRING_LIT                        {
                                                    $$ = $1;
                                                    $$->sval += $2->sval;
                                                    $$->text = $$->text.substr(0, $$->text.size() - 1) + $2->text.substr(1);
                                                }
    ;

args
    : %empty                                    { $$ = makeNode(NodeKind::List, 0, 0); }
    | arg_list                                  { $$ = $1; }
    ;

arg_list
    : expr                                      { $$ = mk(NodeKind::List, @1, "", {$1}); }
    | arg_list ',' expr                         { $$ = $1; $$->kids.push_back($3); }
    ;

%%

void yyerror(const char* message) {
    addDiagnostic("syntax", yylloc.first_line, yylloc.first_column, message);
}

std::string tokenName(int token) {
    std::string name = yytname[YYTRANSLATE(token)];
    if (name.size() >= 2 && name.front() == '"' && name.back() == '"') {
        name = name.substr(1, name.size() - 2);
    }
    return name;
}
