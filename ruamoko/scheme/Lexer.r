#include "Lexer.h"
#include "Number.h"
#include "legacy_string.h"
#include "string.h"
#include "Boolean.h"
#include "Error.h"
#include "defs.h"

BOOL isdigit (string x)
{
    return (x == "0" || x == "1" || x == "2" || x == "3" ||
            x == "4" || x == "5" || x == "6" || x == "7" ||
            x == "8" || x == "9");
}

BOOL isspace (string x)
{
    return (x == " " || x == "\t" || x == "\n" || x == "\r");
}

BOOL isjunk (string x)
{
    return isspace (x) || x == ";";
}

BOOL issymbol (string x)
{
    return (x != "" && x != "(" && x !=")" && !isjunk (x));
}

@implementation Lexer

+ (id) newFromSource: (string) s file: (string) f
{
    return [[Lexer alloc] initWithSource: s file: f];
}

- (id) initWithSource: (string) s file: (string) f
{
    self = [super init];
    source = str_new();
    str_copy(source, s);
    filename = f;
    linenum = 1;
    return self;
}

- (SchemeObject*) nextToken
{
    local int len;
    local Number *num;
    local Symbol *sym;
    local String *str;
    local Boolean *bl;

    for (len = 0; isjunk(str_mid(source, len, len+1)); len++) {
            if (str_mid(source, len, len+1) == ";") {
                    while (str_mid(source, len, len+1) != "\n") {
                            len++;
                    }
            }
            if (str_mid(source, len, len+1) == "\n") {
                    linenum++;
            }
    }

    str_copy (source, str_mid(source, len));

    switch (str_mid (source, 0, 1)) {
        case "(":
            str_copy (source, str_mid (source, 1));
            return [Symbol leftParen];
        case ")":
            str_copy (source, str_mid (source, 1));
            return [Symbol rightParen];
        case "0": case "1": case "2":
        case "3": case "4": case "5":
        case "6": case "7": case "8":
        case "9":
            for (len = 1; isdigit(str_mid(source, len, len+1)); len++);
            num = [Number newFromInt: stoi (str_mid(source, 0, len))];
            [num source: filename];
            [num line: linenum];
            str_copy (source, str_mid(source, len));
            return num;
        case "\"":
            for (len = 1; str_mid(source, len, len+1) != "\""; len++);
            str = [String newFromString: str_mid(source, 1, len)];
            [str source: filename];
            [str line: linenum];
            str_copy (source, str_mid(source, len+1));
            return str;
        case "'":
            str_copy (source, str_mid (source, 1));
            return [Symbol quote];
        case ".":
            str_copy (source, str_mid (source, 1));
            return [Symbol dot];
        case "#":
            str_copy (source, str_mid (source, 1));
            switch (str_mid (source, 0, 1)) {
                case "t":
                    bl = [Boolean trueConstant];
                    str_copy (source, str_mid (source, 1));
                    return bl;
                case "f":
                    bl = [Boolean falseConstant];
                    str_copy (source, str_mid (source, 1));
                    return bl;
                default:
                    return [Error type: "parse"
                                  message: "Invalid # constant"
                                  by: self];
            }
        case "":
            return nil;
        default:
            for (len = 1; issymbol(str_mid(source, len, len+1)); len++);
            sym = [Symbol forString: str_mid (source, 0, len)];
            str_copy (source, str_mid(source, len));
            return sym;
    }
}

- (int) lineNumber
{
    return linenum;
}

- (int) line
{
    return linenum;
}

- (string) source
{
    return filename;
}
 
@end
