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
    

integer main (integer argc, string []argv)
{
    local Parser parser; 
    local CompiledCode code;
    local Compiler comp;
    local Machine vm;
    local Lambda lm;
    local SchemeObject stuff, res;

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
                    printf(">> %s: %i\n", [stuff source], [stuff line]);
                    printf(">> Error (%s): %s\n", [stuff type], [stuff message]);
                    return -1;
            }
            comp = [Compiler newWithLambda: cons ([Symbol forString: "lambda"],
                                                  cons ([Nil nil],
                                                        cons(stuff, [Nil nil])))
                             scope: NIL];
            code = (CompiledCode) [comp compile];
            if ([code isError]) {
                    printf(">> %s: %i\n", [code source], [code line]);
                    printf(">> Error (%s): %s\n", [code type], [code message]);
                    return -1;
            }
            lm = [Lambda newWithCode: code environment: NIL];
            [lm invokeOnMachine: vm];
            res = [vm run];
            if ([res isError]) {
                    printf(">> %s: %i\n", [res source], [res line]);
                    printf(">> Error (%s): %s\n", [res type], [res message]);
                    return -1;
            }
            [vm reset];
    }
    [SchemeObject finishCollecting];
    return 0;
}
