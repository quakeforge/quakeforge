%{
#include <QF/hash.h>
#include "qfcc.h"
#include "qc-parse.h"

#define YY_NO_UNPUT

int lineno;

void error (char*s){}

int type_or_name (char *token);

int do_grab (void);

extern YYSTYPE yylval;

%}

DIGIT	[0-9]
ID		[a-zA-Z_][a-zA-Z_0-9]*
FLOAT	{DIGIT}+"."{DIGIT}*
NUM		({DIGIT}+("."{DIGIT}*)?)
s		[ \t]
m		([\-+]?)

%%

"/*"				{
						int c;
						do {
							while ((c = input ()) != '*' && c != EOF
								   && c != '\n')
								;
							while (c == '*')
								c = input ();
							if (c == EOF)
								error ("EOF in comment");
							if (c == '\n')
								lineno++;
						} while (c != '/' && c != EOF);
					}

"//".*$	/* nothing to do */

{DIGIT}+			{
						yylval.int_val = atoi (yytext);
						return INT_VAL;
					}

{FLOAT}*			{
						yylval.float_val = atof (yytext);
						return FLOAT_VAL;
					}

{ID}	return type_or_name(yytext);

\"(\\.|[^"])*\"		{
						//yylval.string_val = strdup (yytext);
						return STRING_VAL;
					}

'{s}*{m}{NUM}{s}+{m}{NUM}{s}+{m}{NUM}{s}*'	{
						sscanf (yytext, "%f %f %f", &yylval.vector_val[0],
													&yylval.vector_val[1],
													&yylval.vector_val[2]);
						return VECTOR_VAL;
					}

'{s}*{m}{NUM}{s}+{m}{NUM}{s}+{m}{NUM}{s}+{m}{NUM}{s}*'	{
						sscanf (yytext, "%f %f %f %f", &yylval.vector_val[0],
													   &yylval.vector_val[1],
													   &yylval.vector_val[2],
													   &yylval.vector_val[3]);
						return VECTOR_VAL;
					}

"!"|"("|")"|"{"|"}"|"."|"*"|"/"|"&"|"|"|"+"|"-"|"="|"["|"]"|"#"|";"|","	return yytext[0];

"..."	return ELIPSIS;

"&&"	return AND;
"||"	return OR;
"=="	return EQ;
"!="	return NE;
"<="	return LE;
">="	return GE;
"<"		return LT;
">"		return GT;

"$"{ID}				{
						int ret = do_grab();
						if (ret)
							return ret;
					}

^#{s}+{DIGIT}+{s}+\"(\.|[^"])*\".$ {
					}

\n		lineno++;

.		error ("all your typo are belong to us");

%%

int
yywrap (void)
{
	return 1;
}

typedef struct {
	const char	*name;
	int			value;
	type_t		*type;
} keyword_t;

static keyword_t keywords[] = {
	{"float",	TYPE,	&type_float	},
	{"vector",	TYPE,	&type_vector},
	{"entity",	TYPE,	&type_entity},
	{"string",	TYPE,	&type_string},
	{"void",	TYPE,	&type_void	},
	{"local",	LOCAL,	0			},
	{"return",	RETURN,	0			},
	{"while",	WHILE,	0			},
	{"do",		DO,		0			},
	{"if",		IF,		0			},
	{"else",	ELSE,	0			},
	{"for",		FOR,	0			},
};

static const char *
keyword_get_key (void *kw, void *unused)
{
	return ((keyword_t*)kw)->name;
}

int
type_or_name (char *token)
{
	static int initialized = 0;
	static hashtab_t *keyword_tab;
	keyword_t *keyword;

	if (!initialized) {
		int i;
		keyword_tab = Hash_NewTable (1021, keyword_get_key, 0, 0);
		for (i = 0; i < sizeof (keywords) / sizeof (keywords[0]); i++)
			Hash_Add (keyword_tab, &keywords[i]);
		initialized = 1;
	}
	keyword = Hash_Find (keyword_tab, token);
	if (keyword) {
		yylval.type = keyword->type;
		return keyword->value;
	}
	return NAME;
}

int
do_grab (void)
{
	static int initialized;

	if (!initialized) {
		initialized = 1;
	}
	return 0;
}
