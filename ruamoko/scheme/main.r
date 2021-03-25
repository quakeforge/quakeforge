#include "Parser.h"
#include "Nil.h"
#include "Cons.h"
#include "Lambda.h"
#include "defs.h"
#include "qfile.h"
#include "string.h"
#include "builtins.h"
#include "Compiler.h"
#include "Machine.h"
#include "CompiledCode.h"

string readfile (string filename)
{
    local string acc = "", res;
    local QFile file = Qopen (filename, "r");
    while (!Qeof (file)) {
            acc += Qgetline (file);
    }
    Qclose (file);
    res = str_new();
    str_copy(res, acc);
    return res;
}

int main (int argc, string *argv)
{
    local Parser *parser;
    local CompiledCode *code;
    local Compiler *comp;
    local Machine *vm;
    local Lambda *lm;
    local SchemeObject *stuff, *res;
	local Error *err;

    if (argc < 1) {
            return -1;
    }

        //traceon();

    parser = [Parser newFromSource: readfile(argv[1]) file: argv[1]];
    vm = [Machine new];
    [vm makeRootCell];
    [parser makeRootCell];
    builtin_addtomachine (vm);
    while ((stuff = [parser read])) {
            if ([stuff isError]) {
					err = (Error *) stuff;
                    printf(">> %s: %i\n", [err source], [err line]);
                    printf(">> Error (%s): %s\n", [err type], [err message]);
                    return -1;
            }
            comp = [Compiler newWithLambda: cons ([Symbol forString: "lambda"],
                                                  cons ([Nil nil],
                                                        cons(stuff, [Nil nil])))
                             scope: nil];
            code = (CompiledCode *) [comp compile];
            if ([code isError]) {
					err = (Error *) code;
                    printf(">> %s: %i\n", [err source], [err line]);
                    printf(">> Error (%s): %s\n", [err type], [err message]);
                    return -1;
            }
            lm = [Lambda newWithCode: code environment: nil];
            [lm invokeOnMachine: vm];
            res = [vm run];
            if ([res isError]) {
					err = (Error *) res;
                    printf(">> %s: %i\n", [err source], [err line]);
                    printf(">> Error (%s): %s\n", [err type], [err message]);
                    return -1;
            }
            [vm reset];
    }
    [SchemeObject finishCollecting];
    return 0;
}
