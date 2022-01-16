print("""// encoding is tssooo
// t = 0: 32-bit, t = 1: 64-bit
// ss = 00: reserved
// ss = 01: 2 components
// ss = 10: 3 components
// ss = 11: 4 components
// ooo = 000: and
// ooo = 001: or
// ooo = 010: xor
// ooo = 011: add.i
// ooo = 100: nand
// ooo = 101: nor
// ooo = 110: xnor
// ooo = 111: add.f
""")
#for vec3
types = [
    ["int", "int", "int", "int", "int", "int", "int", "float"],
    ["long", "long", "long", "long", "long", "long", "long", "double"]
]
#does not include size (2 or 4, 3 is special)
vec_types = [
    ["ivec", "ivec", "ivec", "ivec", "ivec", "ivec", "ivec", "vec"],
    ["lvec", "lvec", "lvec", "lvec", "lvec", "lvec", "lvec", "dvec"]
]
operators = ["&", "|", "^", "+"]

def case_str(type, width, op):
    case = (type << 5) | (width << 3) | (op)
    return f"case {case:03o}:"

def src_str(type, width, op):
    if width & 1:
        return f"OPA({vec_types[type][op]}{width+1})"
    else:
        return f"OPA({types[type][op]})"

def dst_str(type, width, op):
    return f"OPC({types[type][op]})"

def hop_str(type, width, op):
    return f"OP_hop{width+1}"

for type in range(2):
    for width in range(1, 4): # 0 is reserved
        for opcode in range(8):
            case = case_str(type, width, opcode)
            src = src_str(type, width, opcode)
            dst = dst_str(type, width, opcode)
            hop = hop_str(type, width, opcode)
            op = operators[opcode & 3]
            if width == 2:
                print(f"{case} {dst} = {hop} (&{src}, {op}); break;")
            else:
                print(f"{case} {dst} = {hop} ({src}, {op}); break;")
