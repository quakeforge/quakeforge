#include <AutoreleasePool.h>
#include <hash.h>
#include <qfile.h>
#include <script.h>
#include <string.h>

#include "algebra.h"
#include "basisblade.h"
#include "basisgroup.h"
#include "metric.h"
#include "multivector.h"
#include "util.h"

@static AutoreleasePool *autorelease_pool;
@static void
arp_start (void)
{
	autorelease_pool = [[AutoreleasePool alloc] init];
}

@static void
arp_end (void)
{
	[autorelease_pool release];
	autorelease_pool = nil;
}

static void
basic_test (void)
{
	arp_start ();

	BasisBlade *a = [[BasisBlade basis:1] retain];
	BasisBlade *b = [[BasisBlade basis:2] retain];
	BasisBlade *c = [[BasisBlade basis:4] retain];
	BasisBlade *d = [[BasisBlade basis:8] retain];
	BasisBlade *blades[] = {a, b, c, d};
	static string names[] = {"a", "b", "c", "d"};

//	printf ("a: %@\n", a);
//	printf ("b: %@\n", b);
//	printf ("c: %@\n", c);
//	printf ("d: %@\n", d);

	arp_end ();
#if 0
	arp_start ();
	for (int i = 0; i < 4; i++) {
		arp_end ();
		arp_start ();
		BasisBlade *vec = blades[i];
		printf ("%s: %@\n", names[i], vec);
		for (int j = 0; j < 4; j++) {
			BasisBlade *bvec = [vec outerProduct:blades[j]];
			if (![bvec scale]) {
				continue;
			}
			printf ("%s^%s: %@\n", names[i], names[j], bvec);
			for (int k = 0; k < 4; k++) {
				BasisBlade *tvec = [bvec outerProduct:blades[k]];
				if (![tvec scale]) {
					continue;
				}
				printf ("%s^%s^%s: %@\n", names[i], names[j], names[k],
						tvec);
				for (int l = 0; l < 4; l++) {
					BasisBlade *qvec = [tvec outerProduct:blades[l]];
					if (![qvec scale]) {
						continue;
					}
					printf ("%s^%s^%s^%s: %@\n",
							names[i], names[j], names[k], names[l],
							qvec);
				}
			}
		}
	}
	arp_end ();
#endif
	arp_start ();

	Metric *m = [Metric R:3,0,1];
	BasisBlade *ad = [a geometricProduct:d metric:m];
	BasisBlade *prod = [ad geometricProduct:ad metric:m];
	printf ("%s%s %s%s: %@\n",
			names[0], names[3], names[0], names[3], prod);

	Algebra *alg = [Algebra R:3, 0, 1];
	double plane1_vals[4] = {1, 0, 0, 8};
	double plane2_vals[4] = {0, 1, 0, 8};
	double origin_vals[4] = {0, 0, 0, 1};
	MultiVector *plane1 = [alg group:0 values:plane1_vals];
	MultiVector *plane2 = [alg group:0 values:plane2_vals];
	MultiVector *origin = [alg group:3 values:origin_vals];

	MultiVector *line = [plane1 wedge:plane2];
	MultiVector *point = [[line dot:origin] product:[line reverse]];
	printf ("plane1:%@\nplane2:%@\nline:%@\norigin:%@\n", plane1, plane2, line, origin);
	printf ("point:%@\n", point);

	arp_end ();
}

typedef struct var_s {
	string name;
	MultiVector *value;
} var_t;

static hashtab_t *symtab;

typedef enum {
	EOF, SEMI,
	VAR, EQUAL,
	ID, VALUE,
	OPENP, CLOSEP,
	OPENB, CLOSEB,
	OPENS, CLOSES,
	MUL, DIV, PLUS, MINUS,
	WEDGE, ANTIWEDGE, DOT,
	REVERSE, DUAL, UNDUAL,
} token_e;

script_t script;
string token_str;
token_e lookahead = -1;

typedef struct token_s {
	token_e     id;
	union {
		MultiVector *value;
		string      name;
	};
} token_t;

static token_t
get_token ()
{
	if (!Script_TokenAvailable (script, 1)) {
		return {EOF, nil};
	}
	Script_GetToken (script, 1);
	switch (token_str) {
		case "var": return {VAR, nil};
		case "=":   return {EQUAL, nil};
		case ";":   return {SEMI, nil};
		case "(":   return {OPENP, nil};
		case ")":   return {CLOSEP, nil};
		case "[":   return {OPENB, nil};
		case "]":   return {CLOSEB, nil};
		case "{":   return {OPENS, nil};
		case "}":   return {CLOSES, nil};
		case "*":   return {MUL, nil};
		case "/":   return {DIV, nil};
		case "+":   return {PLUS, nil};
		case "-":   return {MINUS, nil};
		case "^":   return {WEDGE, nil};
		case "&":   return {ANTIWEDGE, nil};
		case ".":   return {DOT, nil};
		case "~":   return {REVERSE, nil};
		case "!":   return {DUAL, nil};
		case "?":   return {UNDUAL, nil};
	}
	return {ID, .name = token_str };
}

static void
syntax_error ()
{
	obj_error (nil, 0, "syntax error before `%s': %d\n", token_str,
			   Script_GetLine (script));
}

static int
match (token_e token)
{
	if (lookahead == -1) {
		lookahead = get_token ().id;
	}
	return token == lookahead;
}

static void
advance ()
{
	lookahead = get_token ().id;
}

static Algebra *algebra;
static MultiVector *minus_one;
static MultiVector *expression ();

static int
is_digit (string x)
{
	return (x == "0" || x == "1" || x == "2" || x == "3" ||
			x == "4" || x == "5" || x == "6" || x == "7" ||
			x == "8" || x == "9");
}

static int
is_number (string x)
{
	return x == "." || is_digit (x);
}

static MultiVector *
factor ()
{
	MultiVector *vec;
	if (match (REVERSE)) {
		advance ();
		vec = [factor () reverse];
	} else if (match (DUAL)) {
		advance ();
		vec = [factor () dual];
	} else if (match (UNDUAL)) {
		advance ();
		vec = [factor () undual];
	} else if (match (MINUS)) {
		advance ();
		vec = [minus_one product:factor ()];
	} else if (match (OPENP)) {
		advance ();
		vec = expression ();
		if (!match (CLOSEP)) {
			syntax_error ();
		}
		advance ();
	} else if (match (ID)) {
		if (is_digit (str_mid (token_str, 0, 1))) {
			string num_str = nil;
			string blade_str = nil;
			int pos = 0;
			while (is_number (str_mid (token_str, pos, pos + 1))) {
				pos++;
			}
			num_str = str_mid (token_str, 0, pos);
			if (str_mid (token_str, pos, pos + 1) == "e") {
				blade_str = str_mid (token_str, ++pos);
			}
			BasisBlade *blade = [BasisBlade basis:0];
			pos = 0;
			while (is_digit (str_mid (blade_str, pos, pos + 1))) {
				int x = str_char (blade_str, pos++) - '0';
				BasisBlade *new = [BasisBlade basis:1 << x];
				blade = [blade outerProduct:new];
			}

			double num = strtod (num_str, nil) * [blade scale];
			vec = [algebra ofGrade:[blade grade]];
			*[vec componentFor:blade] = num;
		} else {
			var_t      *var = Hash_Find (symtab, token_str);
			if (!var) {
				syntax_error ();
			}
			vec = var.value;
		}
		advance ();
	} else {
		syntax_error ();
		vec = nil;
	}
	return vec;
}

static MultiVector *
high_term ()
{
	MultiVector *vec = nil;
	SEL op = nil;
	while (1) {
		if (vec) {
			vec = [vec performSelector:op withObject: factor()];
		} else {
			vec = factor ();
		}
		if (match (WEDGE)) {
			op = @selector(wedge:);
			advance ();
		} else if (match (ANTIWEDGE)) {
			op = @selector(antiwedge:);
			advance ();
		} else if (match (DOT)) {
			op = @selector(dot:);
			advance ();
		} else {
			return vec;
		}
	}
}

static MultiVector *
term ()
{
	MultiVector *vec = nil;
	SEL op = nil;
	while (1) {
		if (vec) {
			vec = [vec performSelector:op withObject: high_term()];
		} else {
			vec = high_term ();
		}
		if (match (REVERSE) || match (DUAL) || match (OPENP) || match (ID)) {
			op = @selector(product:);
		} else if (match (DIV)) {
			op = @selector(divide:);
			advance ();
		} else {
			return vec;
		}
	}
}

static MultiVector *
expression ()
{
	MultiVector *vec = nil;
	SEL op = nil;
	while (1) {
		if (vec) {
			vec = [vec performSelector:op withObject: term()];
		} else {
			vec = term ();
		}
		if (match (PLUS)) {
			op = @selector(plus:);
			advance ();
		} else if (match (MINUS)) {
			op = @selector(minus:);
			advance ();
		} else {
			return vec;
		}
	}
}

static var_t *
assignment ()
{
	if (!match (ID) || is_digit (str_mid (token_str, 0, 1))) {
		printf ("%s\n", token_str);
		syntax_error ();
	}
	var_t      *var = Hash_Find (symtab, token_str);
	if (!var) {
		syntax_error ();
	}
	advance ();
	if (!match (EQUAL)) {
		syntax_error ();
	}
	advance ();
	var.value = [expression () retain];
	return var;
}

static var_t *
declaration ()
{
	if (!match (ID) || is_digit (str_mid (token_str, 0, 1))) {
		syntax_error ();
	}
	if (Hash_Find (symtab, token_str)) {
		syntax_error ();
	}
	var_t *var = obj_malloc (sizeof (var_t));
	var.name = str_hold (str_unmutable (token_str));
	Hash_Add (symtab, var);
	assignment ();
	return var;
}

static int
parse_script (string name, QFile file)
{
	script = Script_New ();
	Script_SetSingle (script, "()[]{}/+-^&~=;!");
	token_str = Script_FromFile (script, name, file);

	while (!match (EOF)) {
		arp_end ();
		arp_start ();
		var_t *var;
		if (match (VAR)) {
			advance ();
			var = declaration ();
			printf ("var %s = %@\n", var.name, var.value);
		} else {
			var = assignment ();
			printf ("%s = %@\n", var.name, var.value);
		}
		advance ();
	}

	Script_Delete (script);
	return 1;
}

static string
get_symtab_key (void *var, void *unused)
{
	return ((var_t *) var).name;
}

static Algebra *
parse_algebra (string spec)
{
	ivec3       R = {};
	string      s = spec;
	if (is_digit (str_mid (spec, 0, 1))) {
		for (int i = 0; i < 3; i++) {
			int         end = 0;
			R[i] = strtol (s, &end, 0);
			string      e = str_mid (s, end, end + 1);
			if (!e) {
				break;
			}
			if (e != ",") {
				goto bad_spec;
			}
			s = str_mid (s, end + 1);
		}
		if (!R[0] && !R[1] && !R[2]) {
			goto bad_spec;
		}
		return [Algebra R:R[0], R[1], R[2]];
	} else {
	}
bad_spec:
	printf ("bad algebra spec: %s\n", spec);
	return nil;
}

int
main (int argc, string *argv)
{
	symtab = Hash_NewTable (127, get_symtab_key, nil, nil);
	if (argc < 2) {
		basic_test ();
	} else {
		arp_start ();
		algebra = [[Algebra PGA:3] retain];
		arp_end ();
		arp_start ();
		double m1 = -1;
		minus_one = [[algebra ofGrade:0 values:&m1] retain];
		arp_end ();
		arp_start ();
		for (int i = 1; i < argc; i++) {
			if (argv[i] == "-a") {
				Algebra *a = [parse_algebra (argv[++i]) retain];
				if (!a) {
					return 1;
				}
				[algebra release];
				algebra = a;
				[minus_one release];
				minus_one = [[algebra ofGrade:0 values:&m1] retain];
				continue;
			}
			QFile       file = Qopen (argv[i], "rt");
			if (file) {
				arp_end ();
				arp_start ();
				printf ("Using algebra %@\n", algebra);
				int         res = parse_script (argv[i], file);
				Qclose (file);
				if (!res) {
					return 1;
				}
			} else {
				printf ("%s: failed to open '%s'\n", argv[0], argv[i]);
				return 1;
			}
		}
	}
	return 0;
}
