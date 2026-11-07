%option noyywrap nounput noinput yylineno
%x COMMENT

%{
#include <cstdlib>
#include <string>
#include "ast.hpp"
#include "parser.tab.hpp"

int yycolumn = 1;
static int commentLine = 0;
static int commentCol = 0;

#define YY_USER_ACTION                                   \
    yylval = nullptr;                                    \
    yylloc.first_line = yylloc.last_line = yylineno;     \
    yylloc.first_column = yycolumn;                      \
    yylloc.last_column = yycolumn + (int)yyleng;         \
    yycolumn += (int)yyleng;

static Node* leaf(NodeKind kind) {
    return makeNode(kind, yylloc.first_line, yylloc.first_column, yytext);
}
%}

D       [0-9]
L       [a-zA-Z_]
H       [0-9a-fA-F]
E       ([eE][+-]?{D}+)

%%

"/*"                    { commentLine = yylineno; commentCol = yylloc.first_column; BEGIN(COMMENT); }
<COMMENT>"*/"           { BEGIN(INITIAL); }
<COMMENT>\n             { yycolumn = 1; }
<COMMENT>[^*\n]+        { }
<COMMENT>"*"            { }
<COMMENT><<EOF>>        {
                            addDiagnostic("lexical", commentLine, commentCol, "unterminated comment");
                            BEGIN(INITIAL);
                            yyterminate();
                        }

"//"[^\n]*              { }
^[ \t]*"#"[^\n]*        { }
[ \t\r\f\v]+            { }
\n                      { yycolumn = 1; }

"int"                   { return INT; }
"long"                  { return LONG; }
"short"                 { return SHORT; }
"char"                  { return CHAR; }
"float"                 { return FLOAT; }
"double"                { return DOUBLE; }
"bool"                  { return BOOL; }
"void"                  { return VOID; }
"auto"                  { return AUTO; }
"const"                 { return CONST; }
"if"                    { return IF; }
"else"                  { return ELSE; }
"while"                 { return WHILE; }
"do"                    { return DO; }
"for"                   { return FOR; }
"return"                { return RETURN; }
"break"                 { return BREAK; }
"continue"              { return CONTINUE; }
"using"                 { return USING; }
"namespace"             { return NAMESPACE; }
"static_cast"           { return STATIC_CAST; }

"true"|"false"          {
                            yylval = leaf(NodeKind::BoolLit);
                            yylval->ival = yytext[0] == 't';
                            return BOOL_LIT;
                        }

{L}({L}|{D})*           { yylval = leaf(NodeKind::Ident); return IDENT; }

(0[xX]{H}+|{D}+)[uUlL]* {
                            yylval = leaf(NodeKind::IntLit);
                            yylval->ival = std::strtoll(yytext, nullptr, 0);
                            return INT_LIT;
                        }

{D}+{E}[fFlL]?          |
{D}*"."{D}+{E}?[fFlL]?  |
{D}+"."{D}*{E}?[fFlL]?  {
                            yylval = leaf(NodeKind::FloatLit);
                            yylval->dval = std::strtod(yytext, nullptr);
                            return FLOAT_LIT;
                        }

'(\\.|[^\\'\n])+'       {
                            yylval = leaf(NodeKind::CharLit);
                            std::string value = unescapeC(std::string(yytext + 1, yyleng - 2));
                            if (value.size() != 1) {
                                addDiagnostic("lexical", yylloc.first_line, yylloc.first_column,
                                              "multi-character constant " + std::string(yytext));
                            }
                            yylval->ival = value.empty() ? 0 : static_cast<signed char>(value[0]);
                            return CHAR_LIT;
                        }

\"(\\.|[^\\"\n])*\"     {
                            yylval = leaf(NodeKind::StringLit);
                            yylval->sval = unescapeC(std::string(yytext + 1, yyleng - 2));
                            return STRING_LIT;
                        }

"::"                    { return SCOPE; }
"++"                    { return INC; }
"--"                    { return DEC; }
"&&"                    { return AND; }
"||"                    { return OR; }
"=="                    { return EQ; }
"!="                    { return NE; }
"<="                    { return LE; }
">="                    { return GE; }
"<<="                   { return SHL_ASSIGN; }
">>="                   { return SHR_ASSIGN; }
"<<"                    { return SHL; }
">>"                    { return SHR; }
"+="                    { return ADD_ASSIGN; }
"-="                    { return SUB_ASSIGN; }
"*="                    { return MUL_ASSIGN; }
"/="                    { return DIV_ASSIGN; }
"%="                    { return MOD_ASSIGN; }
"&="                    { return AND_ASSIGN; }
"|="                    { return OR_ASSIGN; }
"^="                    { return XOR_ASSIGN; }

[-+*/%<>=!~&|^?:;,(){}\[\]] { return yytext[0]; }

.                       {
                            addDiagnostic("lexical", yylloc.first_line, yylloc.first_column,
                                          "unexpected character '" + std::string(yytext) + "'");
                        }

%%
