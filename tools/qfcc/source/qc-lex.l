%{
#include <QF/hash.h>
#include "qfcc.h"
#include "expr.h"
#include "scope.h"
#include "qc-parse.h"

#define YY_NO_UNPUT

void error (char*s){fprintf(stderr,"%d: %s\n",pr_source_line,s);}

int type_or_name (char *token);

int do_grab (char *token);

void add_frame_macro (char *token);

char *make_string (char *token);

extern YYSTYPE yylval;

%}

DIGIT	[0-9]
ID		[a-zA-Z_][a-zA-Z_0-9]*
FLOAT	{DIGIT}+"."{DIGIT}*
NUM		({DIGIT}+("."{DIGIT}*)?)
s		[ \t]
m		([\-+]?)

%x		grab_frame grab_other

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
								pr_source_line++;
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
						yylval.string_val = make_string (yytext);
						return STRING_VAL;
					}

'{s}*{m}{NUM}{s}+{m}{NUM}{s}+{m}{NUM}{s}*'	{
						sscanf (yytext, "' %f %f %f '",
								&yylval.vector_val[0], &yylval.vector_val[1],
								&yylval.vector_val[2]);
						return VECTOR_VAL;
					}

'{s}*{m}{NUM}{s}+{m}{NUM}{s}+{m}{NUM}{s}+{m}{NUM}{s}*'	{
						sscanf (yytext, "' %f %f %f %f '",
								&yylval.vector_val[0], &yylval.vector_val[1],
								&yylval.vector_val[2], &yylval.vector_val[3]);
						return VECTOR_VAL;
					}

^#{s}+{DIGIT}+{s}+\"(\.|[^"])*\".*$ {
						char *p;
						int line;
						
						pr_file_p = yytext + 1;
						line = strtol (pr_file_p, &p, 10);
						pr_file_p = p;
						while (isspace (*pr_file_p))
							pr_file_p++;
						if (!*pr_file_p)
							PR_ParseError ("Unexpected end of file");
						PR_LexString ();		// grab the filename
						while (*pr_file_p && *pr_file_p != '\n')	// ignore flags
							pr_file_p++;
						pr_source_line = line - 1;
						s_file = ReuseString (pr_immediate_string);
					}

"!"|"("|")"|"{"|"}"|"."|"*"|"/"|"&"|"|"|"+"|"-"|"="|"["|"]"|";"|","|"#"	return yytext[0];

"..."	return ELIPSIS;

"&&"	return AND;
"||"	return OR;
"=="	return EQ;
"!="	return NE;
"<="	return LE;
">="	return GE;
"<"		return LT;
">"		return GT;

"$"{s}*{ID}			{
						int ret = do_grab(yytext);
						if (ret > 0)
							return ret;
						else
							BEGIN (-ret);
					}

<grab_frame>{ID}	add_frame_macro (yytext);

<grab_other>[^\n]*	/* skip */

<*>\n				{
						pr_source_line++;
						BEGIN (INITIAL);
					}

<*>{s}*				/* skip */

<*>.				error ("all your typo are belong to us");

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
	yylval.string_val = strdup (token);
	return NAME;
}

static hashtab_t *frame_tab;
static hashtab_t *grab_tab;

typedef struct {
	const char *name;
	int			num;
} frame_t;

static frame_t grab_list[] = {
	{"cd",		0},
	{"origin",	0},
	{"base",	0},
	{"flags",	0},
	{"scale",	0},
	{"skin",	0},
};

static const char *
frame_get_key (void *f, void *unused)
{
	return ((frame_t*)f)->name;
}

static void
frame_free (void *f, void *unused)
{
	free ((char*)((frame_t*)f)->name);
	free (f);
}

int
do_grab (char *token)
{
	static int initialized;
	frame_t *frame;

	if (!initialized) {
		int i;

		initialized = 1;
		frame_tab = Hash_NewTable (1021, frame_get_key, frame_free, 0);
		grab_tab = Hash_NewTable (1021, frame_get_key, 0, 0);
		for (i = 0; i < sizeof (grab_list) / sizeof (grab_list[0]); i++)
			Hash_Add (grab_tab, &grab_list[i]);
	}
	while (isspace (*++token)) // advance over $ and leading space
		;
	if (!strcmp (token, "frame"))
		return -grab_frame;
	if (Hash_Find (grab_tab, token))
		return -grab_other;
	frame = Hash_Find (frame_tab, token);
	if (frame) {
		yylval.int_val = frame->num;
		return INT_VAL;
	}
	return 0;
}

static int frame_number;

void
add_frame_macro (char *token)
{
	frame_t *frame = malloc (sizeof (frame_t));

	frame->name = strdup (token);
	frame->num = frame_number++;
	Hash_Add (frame_tab, frame);
}

void
clear_frame_macros (void)
{
	frame_number = 0;
	if (frame_tab)
		Hash_FlushTable (frame_tab);
}

char *
make_string (char *token)
{
	char *str;

	pr_file_p = token;
	PR_LexString ();
	str = malloc (pr_token_len + 1);
	memcpy (str, pr_token, pr_token_len + 1);
	return str;
}
