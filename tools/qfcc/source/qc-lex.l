%{
#include "qfcc.h"
#include "qc-parse.h"

#define YY_NO_UNPUT

%}

DIGIT	[0-9]
ID		[a-zA-Z_][a-zA-Z_0-9]*

%%

{DIGIT}+"."{DIGIT}* return VALUE;

{ID}	return NAME

\"(\.|[^"])*\"

"!"|"("|")"|"{"|"}"|"."|"*"|"/"|"&"|"|"|"+"|"-"|"="|"["|"]"	return yytext[0];

"&&"	return AND;
"||"	return OR;
"=="	return EQ;
"!="	return NE;
"<="	return LE;
">="	return GE;
"<"		return LT;
">"		return GT;

^#

\n

.

%%

int
yywrap (void)
{
	return 1;
}
