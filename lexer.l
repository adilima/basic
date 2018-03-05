%{
#include "basic.h"
#include "parser.hpp"
#include <limits>
extern basic::interpreter* interp;
%}

ALPHA [A-Za-z]
DIGIT [0-9]
NAME  ({ALPHA}|{DIGIT}|\$)({ALPHA}|{DIGIT}|[_.\-/$])*
NUM1  [-+]?{DIGIT}+\.?([eE][-+]?{DIGIT}+)?
NUM2  [-+]?{DIGIT}*\.{DIGIT}+([eE][-+]?{DIGIT}+)?
FLOAT {NUM1}|{NUM2}

COMMON_OPERATOR  [\+\-\*\/\=\^\<\>]
QUOTED_TEXT      \"[^\n"]+\"

%option outfile="lexer.cpp"
%option noyywrap bison-bridge

%%

{QUOTED_TEXT}  {
/*
In the version, a STRING must already be an expression,
so we just create one from here.
*/
yytext[strlen(yytext)-1] = '\0';
yylval->llvmConstant = llvm::ConstantDataArray::getString(*interp, &yytext[1]);
return STRING;
}

{DIGIT}+ {
long nValue = atol(yytext);
yylval->llvmConstant = interp->get_constant_long(nValue);
return LONG;
}

{FLOAT} {
double dValue = atof(yytext);
yylval->llvmConstant = interp->get_constant_double(dValue);
return DOUBLE;
}

{COMMON_OPERATOR} { return yytext[0]; }

false|False|true|True {
if (!strcasecmp(yytext, "false"))
{
    yylval->llvmConstant = llvm::ConstantInt::getFalse(*interp);
	return BOOLEAN;
}
yylval->llvmConstant = llvm::ConstantInt::getTrue(*interp);
return BOOLEAN;
}

{NAME} {
if (!strcasecmp(yytext, "dim"))
{
	yylval->typeID = DIM;
	return DIM;
}
else if (!strcasecmp(yytext, "sub"))
{
    yylval->typeID = SUB;
	return SUB;
}
else if (!strcasecmp(yytext, "function"))
{
    yylval->typeID = FUNCTION;
	return FUNCTION;
}
else if (!strcasecmp(yytext, "as"))
{
    yylval->typeID = AS;
	return AS;
}
else if (!strcasecmp(yytext, "if"))
{
    yylval->typeID = IF;
	return IF;
}
else if (!strcasecmp(yytext, "else"))
{
    yylval->typeID = ELSE;
	return ELSE;
}
else if (!strcasecmp(yytext, "elseif"))
{
    yylval->typeID = ELSEIF;
	return ELSEIF;
}
else if (!strcasecmp(yytext, "endif"))
{
    yylval->typeID = ENDIF;
	return ENDIF;
}
else if (!strcasecmp(yytext, "then"))
{
    yylval->typeID = THEN;
	return THEN;
}
else if (!strcasecmp(yytext, "for"))
{
    yylval->typeID = FOR;
	return FOR;
}
else if (!strcasecmp(yytext, "each"))
{
    yylval->typeID = EACH;
	return EACH;
}
else if (!strcasecmp(yytext, "to"))
{
    yylval->typeID = TO;
	return TO;
}
else if (!strcasecmp(yytext, "step"))
{
    yylval->typeID = STEP;
	return STEP;
}
else if (!strcasecmp(yytext, "next"))
{
    yylval->typeID = NEXT;
	return NEXT;
}
else if (!strcasecmp(yytext, "byte"))
{
    yylval->typeID = BYTE;
	return TYPEID;
}
else if (!strcasecmp(yytext, "boolean"))
{
    yylval->typeID = BOOLEAN;
	return TYPEID;
}
else if (!strcasecmp(yytext, "integer"))
{
   yylval->typeID = INTEGER;
   return TYPEID;
}
else if (!strcasecmp(yytext, "long"))
{
   yylval->typeID = LONG;
   return TYPEID;
}
else if (!strcasecmp(yytext, "single"))
{
   yylval->typeID = SINGLE;
   return TYPEID;
}
else if (!strcasecmp(yytext, "double"))
{
   yylval->typeID = DOUBLE;
   return TYPEID;
}
else if (!strcasecmp(yytext, "string"))
{
   yylval->typeID = STRING;
   return TYPEID;
}
else if (!strcasecmp(yytext, "end"))
{
    yylval->typeID = END;
	return END;
}
else
{
    yylval->identifier = strdup(yytext);
    return ID;
}
}

\n  return yytext[0];

[ \t]+  /* eat all whitespace */
.   return yytext[0];

%%

