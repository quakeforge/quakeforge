#include "Lexer.h"
#include "Number.h"
#include "string.h"


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

BOOL issymbol (string x)
{
    return (x != "" && x != "(" && x !=")" && !isspace (x));
}

@implementation Lexer

+ (id) newFromSource: (string) s
{
    return [[Lexer alloc] initWithSource: s];
}

- (id) initWithSource: (string) s
{
    self = [super init];
    source = s;
    return self;
}

- (SchemeObject) nextToken
{
    local integer len;
    local Number num;
    local Symbol sym;
    local String str;
    
    for (len = 0; isspace(str_mid(source, len, len+1)); len++);
    source = str_mid(source, len);

    switch (str_mid (source, 0, 1)) {
        case "(":
            source = str_mid (source, 1);
            return [Symbol leftParen];
        case ")":
            source = str_mid (source, 1);
            return [Symbol rightParen];
        case "0": case "1": case "2":
        case "3": case "4": case "5":
        case "6": case "7": case "8":
        case "9":
            for (len = 1; isdigit(str_mid(source, len, len+1)); len++);
            num = [Number newFromInt: stoi (str_mid(source, 0, len))];
            source = str_mid(source, len);
            return num;
        case "\"":
            for (len = 1; str_mid(source, len, len+1) != "\""; len++);
            str = [String newFromString: str_mid(source, 1, len)];
            source = str_mid(source, len+1);
            return str;
        case "'":
            source = str_mid (source, 1);
            return [Symbol quote];
        case "":
            return NIL;
        default:
            for (len = 1; issymbol(str_mid(source, len, len+1)); len++);
            sym = [Symbol forString: str_mid (source, 0, len)];
            source = str_mid(source, len);
            return sym;
    }
}

@end
