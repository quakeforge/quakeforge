bitmap_txt = """
0 0000 mmss load
0 0001 mmss store
0 0010 mmss push
0 0011 mmss pop
0 010c ccmm branch
0 0110 0nnn rcall 1-8 (0 0100 11mm with 0 params for [r]call0)
0 0110 10ss return
0 0110 1100 returnv
0 0110 1101 with (mode in st->a, value in st->b, reg in st->c)
0 0110 111t state
0 0111 tooo vecops
0 1ccc ttss compare
1 0ooo ttss mathops
1 011r tuss shiftops
1 0110 o1oo string
1 1ccc t0ss compare2
1 1000 ooss bitops
1 1001 t1ss scale
1 1001 t100 swizzle
1 1010 d1xx convert
1 1011 00mm lea
1 1011 01ss any
1 1011 0100 lea_e
1 1011 10ss all
1 1011 1000 pushregs
1 1011 11ss none
1 1011 1100 popregs
1 1101 01oo move
1 1101 11oo memset
1 1110 d1xx convert2
1 11dd t100 vecops2
1 1100 ooss boolops
n 1111 nnnn
0 1011 nnnn
"""

import copy

load_fmt =  [
    "%Ga.%Gb(%Ea), %gc",
    "*%Ga, %gc",
    "*(%Ga + %sb), %gc",
    "*(%Ga + %Gb), %gc",
]
branch_fmt = [
    "branch %sa (%Oa)",
    "*%Ga",
    "%Ga[%sb]",
    "%Ga[%Gb]",
]
compare_ccc = [ "eq", "lt", "gt", None, "ne", "ge", "le", None]
type_tt = ['I', 'F', 'L', 'D']
etype_tt = ["ev_integer", "ev_float", "ev_long", "ev_double"]
unsigned_t = ["ev_uinteger", "ev_ulong"]
float_t = ["ev_float", "ev_double"]

all_formats = {
    "opcode": "OP_ALL_{ss+1}",
    "mnemonic": "all",
    "opname": "all",
    "format": "%Ga, %gc",
    "widths": "{ss+1}, 0, 1",
    "types": "ev_integer, ev_integer, ev_integer",
}
any_formats = {
    "opcode": "OP_ANY_{ss+1}",
    "mnemonic": "any",
    "opname": "any",
    "format": "%Ga, %gc",
    "widths": "{ss+1}, 0, 1",
    "types": "ev_integer, ev_integer, ev_integer",
}
bitops_formats = {
    "opcode": "OP_{op_bit[oo].upper()}_I_{ss+1}",
    "mnemonic": "{op_bit[oo]}",
    "opname": "{op_bit[oo]}",
    "format": "{bit_fmt[oo]}",
    "widths": "{ss+1}, {ss+1}, {ss+1}",
    "types": "ev_integer, ev_integer, ev_integer",
    "args": {
        "op_bit": ["bitand", "bitor", "bitxor", "bitnot"],
        "bit_fmt": [
            "%Ga, %Gb, %gc",
            "%Ga, %Gb, %gc",
            "%Ga, %Gb, %gc",
            "%Ga, %gc",
        ],
    },
}
boolops_formats = {
    "opcode": "OP_{op_bool[oo].upper()}_I_{ss+1}",
    "mnemonic": "{op_bool[oo]}",
    "opname": "{op_bool[oo]}",
    "format": "{bool_fmt[oo]}",
    "widths": "{ss+1}, {ss+1}, {ss+1}",
    "types": "ev_integer, ev_integer, ev_integer",
    "args": {
        "op_bool": ["and", "or", "xor", "not"],
        "bool_fmt": [
            "%Ga, %Gb, %gc",
            "%Ga, %Gb, %gc",
            "%Ga, %Gb, %gc",
            "%Ga, %gc",
        ],
    },
}
branch_formats = {
    "opcode": "OP_{op_cond[ccc].upper()}_{op_mode[mm]}",
    "mnemonic": "{op_cond[ccc]}",
    "opname": "{op_cond[ccc]}",
    "format": "{cond_fmt[ccc]}{branch_fmt[mm]}",
    "widths": "{cond_widths[ccc]}",
    "types": "ev_void, ev_void, ev_integer",
    "args": {
        "op_mode": "ABCD",
        "op_cond": ["ifz",  "ifb",  "ifa",  "jump",
                    "ifnz", "ifae", "ifbe", "call"],
        "branch_fmt": branch_fmt,
        "cond_fmt": ["%Gc ", "%Gc ", "%Gc ", "", "%Gc ", "%Gc ", "%Gc ", ""],
        "cond_widths": [
            "0, 0, 1",
            "0, 0, 1",
            "0, 0, 1",
            "0, 0, 0",
            "0, 0, 1",
            "0, 0, 1",
            "0, 0, 1",
            "0, 0, 0",
        ],
    },
}
compare_formats = {
    "opcode": "OP_{op_cmp[ccc].upper()}_{cmp_type[tt]}_{ss+1}",
    "mnemonic": "{op_cmp[ccc]}.{cmp_type[tt]}",
    "opname": "{op_cmp[ccc]}",
    "widths": "{ss+1}, {ss+1}, {ss+1}",
    "types": "{cmp_types[tt]}, {cmp_types[tt]}, ev_integer",
    "args": {
        "op_cmp": compare_ccc,
        "cmp_type": type_tt,
        "cmp_types": etype_tt,
    },
}
compare2_formats = {
    "opcode": "OP_{op_cmp[ccc].upper()}_{cmp_type[t]}_{ss+1}",
    "mnemonic": "{op_cmp[ccc]}.{cmp_type[t]}",
    "opname": "{op_cmp[ccc]}",
    "widths": "{ss+1}, {ss+1}, {ss+1}",
    "types": "{cmp_types[t]}, {cmp_types[t]}, ev_integer",
    "args": {
        "op_cmp": compare_ccc,
        "cmp_type": ['u', 'U'],
        "cmp_types": unsigned_t,
    },
}
convert_formats = {
    "opcode": "OP_CONV_{op_conv[d*4+xx]}",
    "mnemonic": "conv.{op_conv[d*4+xx]}",
    "opname": "conv",
    "format": "%Ga %gc",
    "widths": "1, 0, 1",
    "types": "{cnv_types[xx][d]}, ev_invalid, {cnv_types[xx][1-d]}",
    "args": {
        "op_conv": ["IF", "LD", "uF", "UD", "FI", "DL", "Fu", "DU"],
        "cnv_types": [
            ["ev_integer", "ev_float"],
            ["ev_long", "ev_double"],
            ["ev_uinteger", "ev_float"],
            ["ev_ulong", "ev_double"],
        ],
    },
}
convert2_formats = copy.deepcopy (convert_formats)
convert2_formats["args"]["op_conv"] = [None, "IL", "uU", "FD",
                                       None, "LI", "Uu", "DF"]
convert2_formats["args"]["cnv_types"] = [
    [None, None],
    ["ev_integer", "ev_long"],
    ["ev_uinteger", "ev_ulong"],
    ["ev_float", "ev_double"],
]
lea_formats = {
    "opcode": "OP_LEA_{op_mode[mm]}",
    "mnemonic": "lea",
    "opname": "lea",
    "format": "{lea_fmt[mm]}",
    "widths": "0, 0, 1",
    "types": "ev_pointer, ev_pointer, ev_pointer",
    "args": {
        "op_mode": "ABCD",
        "lea_fmt": [
            "%ga, %gc",
            "*%Ga, %gc",
            "*(%Ga + %sb), %gc",
            "*(%Ga + %Gb), %gc",
        ],
    },
}
lea_e_formats = {
    "opcode": "OP_LEA_E",
    "mnemonic": "lea",
    "opname": "lea",
    "format": "{load_fmt[0]}",
    "format": "%Ga.%Gb(%Ea), %gc",
    "types": "ev_entity, ev_field, ev_pointer",
    "widths": "0, 0, 1",
}
load_formats = {
    "opcode": "OP_LOAD_{op_mode[mm]}_{ss+1}",
    "mnemonic": "load",
    "opname": "load",
    "format": "{load_fmt[mm]}",
    "widths": "0, 0, {ss+1}",
    "types": "ev_void, ev_void, ev_void",
    "args": {
        "op_mode": "EBCD",
        "load_fmt": load_fmt,
    },
}
mathops_formats = {
    "opcode": "OP_{op_math[ooo].upper()}_{math_type[tt]}_{ss+1}",
    "mnemonic": "{op_math[ooo]}.{math_type[tt]}",
    "opname": "{op_math[ooo]}",
    "widths": "{ss+1}, {ss+1}, {ss+1}",
    "types": "{math_types[tt]}, {math_types[tt]}, {math_types[tt]}",
    "args": {
        "op_math": ["mul", "div", "rem", "mod", "add", "sub", None, None],
        "math_type": type_tt,
        "math_types": etype_tt,
    },
}
memset_formats = {
    "opcode": "OP_MEMSET_{op_memset[oo].upper()}",
    "mnemonic": "memset.{op_memset[oo]}",
    "opname": "memset",
    "format": "{memset_fmt[oo]}",
    "widths": "0, 0, 0",
    "types": "ev_integer, ev_void, ev_void",
    "args": {
        "op_memset": [None, "i", "p", "pi"],
        "memset_fmt": [None, "%Ga, %sb, %gc", "%Ga, %Gb, %Gc", "%Ga, %sb, %Gc"],
    },
}
move_formats = {
    "opcode": "OP_MOVE_{op_move[oo].upper()}",
    "mnemonic": "memset.{op_move[oo]}",
    "opname": "memset",
    "format": "{move_fmt[oo]}",
    "widths": "0, 0, 0",
    "types": "ev_integer, ev_void, ev_void",
    "args": {
        "op_move": [None, "i", "p", "pi"],
        "move_fmt": [None, "%Ga, %sb, %gc", "%Ga, %Gb, %Gc", "%Ga, %sb, %Gc"],
    },
}
none_formats = {
    "opcode": "OP_NONE_{ss+1}",
    "mnemonic": "none",
    "opname": "none",
    "format": "%Ga, %gc",
    "widths": "{ss+1}, 0, 1",
    "types": "ev_integer, ev_invalid, ev_integer",
}
push_formats = {
    "opcode": "OP_PUSH_{op_mode[mm]}_{ss+1}",
    "mnemonic": "push",
    "opname": "push",
    "format": "{push_fmt[mm]}",
    "widths": "{ss+1}, 0, 0",
    "types": "ev_void, ev_void, ev_invalid",
    "args": {
        "op_mode": "ABCD",
        "push_fmt": [
            "%Ga",
            "*%Ga",
            "*(%Ga + %sb)",
            "*(%Ga + %Gb)",
        ],
    },
}
pushregs_formats = {
    "opcode": "OP_PUSHREGS",
    "mnemonic": "pushregs",
    "opname": "pushregs",
    "widths": "0, 0, 0",
    "types": "ev_invalid, ev_invalid, ev_invalid",
    "format": None,
}
pop_formats = {
    "opcode": "OP_POP_{op_mode[mm]}_{ss+1}",
    "mnemonic": "pop",
    "opname": "pop",
    "format": "{pop_fmt[mm]}",
    "widths": "{ss+1}, 0, 0",
    "types": "ev_void, ev_void, ev_invalid",
    "args": {
        "op_mode": "ABCD",
        "pop_fmt": [
            "%ga",
            "*%Ga",
            "*(%Ga + %sb)",
            "*(%Ga + %Gb)",
        ],
    },
}
popregs_formats = {
    "opcode": "OP_POPREGS",
    "mnemonic": "popregs",
    "opname": "popregs",
    "widths": "0, 0, 0",
    "format": None,
    "types": "ev_invalid, ev_invalid, ev_invalid",
}
scale_formats = {
    "opcode": "OP_SCALE_{scale_type[t]}_{ss+1}",
    "mnemonic": "scale.{scale_type[t]}",
    "opname": "scale",
    "widths": "{ss+1}, 1, {ss+1}",
    "types": "{scale_types[t]}, {scale_types[t]}, {scale_types[t]}",
    "args": {
        "scale_type": ['F', 'D'],
        "scale_types": float_t,
    },
}
shiftops_formats = {
    "opcode": "OP_{op_shift[u*2+r].upper()}_{shift_type[u*2+t]}_{ss+1}",
    "mnemonic": "{op_shift[u*2+r]}.{shift_type[u*2+t]}",
    "opname": "{op_shift[u*2+r]}",
    "widths": "{ss+1}, {ss+1}, {ss+1}",
    "types": "{shift_types[t][u]}, {shift_types[t][0]}, {shift_types[t][u]}",
    "args": {
        "op_shift": ["shl", "asr", "shl", "shr"],
        "shift_type": ['I', 'L', 'u', 'U'],
        "shift_types": [
            ["ev_integer", "ev_uinteger"],
            ["ev_long", "ev_ulong"],
        ],
    },
}
state_formats = {
    "opcode": "OP_STATE_{state[t]}",
    "mnemonic": "state.{state[t]}",
    "opname": "state",
    "format": "{state_fmt[t]}",
    "widths": "1, 1, 1",
    "types": "ev_float, ev_func, {state_types[t]}",
    "args": {
        "state": ["ft", "ftt"],
        "state_fmt": ["%Ga, %Gb", "%Ga, %Gb, %Gc"],
        "state_types": ["ev_invalid", "ev_float"],
    },
}
store_formats = {
    "opcode": "OP_STORE_{op_mode[mm]}_{ss+1}",
    "mnemonic": "store",
    "opname": "store",
    "format": "{store_fmt[mm]}",
    "widths": "{ss+1}, 0, {ss+1}",
    "types": "ev_void, ev_void, ev_void",
    "args": {
        "op_mode": "ABCD",
        "store_fmt": [
            "%Gc, %ga",
            "%Gc, *%Ga",
            "%Gc, *(%Ga + %sb)",
            "%Gc, *(%Ga + %Gb)",
        ],
    },
}
string_formats = {
    "opcode": "OP_{op_str[o*4+oo].upper()}_S",
    "mnemonic": "{op_str[o*4+oo]}.s",
    "opname": "{op_str[o*4+oo]}",
    "format": "{str_fmt[o*4+oo]}",
    "widths": "1, 1, 1",
    "types": "{str_types[o*4+oo]}",
    "args": {
        "op_str": ["eq", "lt", "gt", "add", "cmp", "ge", "le", "not"],
        "str_fmt": [
            "%Ga, %Gb, %gc",
            "%Ga, %Gb, %gc",
            "%Ga, %Gb, %gc",
            "%Ga, %Gb, %gc",
            "%Ga, %Gb, %gc",
            "%Ga, %Gb, %gc",
            "%Ga, %Gb, %gc",
            "%Ga, %gc",
        ],
        "str_types": [
            "ev_string, ev_string, ev_integer",
            "ev_string, ev_string, ev_integer",
            "ev_string, ev_string, ev_integer",
            "ev_string, ev_string, ev_string",
            "ev_string, ev_string, ev_integer",
            "ev_string, ev_string, ev_integer",
            "ev_string, ev_string, ev_integer",
            "ev_string, ev_invalid, ev_integer",
        ],
    },
}
swizzle_formats = {
    "opcode": "OP_SWIZZLE_{swiz_type[t]}",
    "mnemonic": "swizzle.{swiz_type[t]}",
    "opname": "swizzle",
    "format": "%Ga %sb %gc",
    "widths": "4, 0, 4",
    "types": "{swizzle_types[t]}",
    "args": {
        "swiz_type": ['F', 'D'],
        "swizzle_types": float_t,
    },
}
rcall_formats = {
    "opcode": "OP_CALL_{nnn+1}",
    "mnemonic": "rcall{nnn+1}",
    "opname": "rcall{nnn+1}",
    "format": "{rcall_fmt[nnn]}",
    "widths": "0, 0, 0",
    "types": "ev_func, ev_void, ev_void",
    "args": {
        "rcall_fmt": [
            "%Fa (%P0b)",
            "%Fa (%P0b, %P1c)",
            "%Fa (%P0b, %P1c, %P2x)",
            "%Fa (%P0b, %P1c, %P2x, %P3x)",
            "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x)",
            "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x, %P5x)",
            "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x, %P5x, %P6x)",
            "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x, %P5x, %P6x, %P7x)",
        ],
    },
}
return_formats = {
    "opcode": "OP_RETURN_{ss+1}",
    "mnemonic": "return{ss+1}",
    "opname": "return",
    "widths": "0, 0, 0",
    "format": "%Ra",
    "types": "ev_void, ev_invalid, ev_invalid",
}
returnv_formats = {
    "opcode": "OP_RETURN_0",
    "mnemonic": "return",
    "opname": "return",
    "widths": "0, 0, 0",
    "format": None,
    "types": "ev_invalid, ev_invalid, ev_invalid",
}
vecops_formats = {
    "opcode": "OP_{op_vop[ooo].upper()}_{vop_type[t]}",
    "mnemonic": "{op_vop[ooo]}.{vop_type[t]}",
    "opname": "{op_vop[ooo]}",
    "widths": "{vec_widths[ooo]}",
    "types": "{vec_types[t]}, {vec_types[t]}, {vec_types[t]}",
    "args": {
        "op_vop": ["cross", "cdot", "vdot", "qdot",
                   "cmul", "qvmul", "vqmul", "qmul"],
        "vop_type": ['F', 'D'],
        "vec_widths": [
            "2, 2, 2",
            "3, 3, 3",
            "4, 4, 4",
            "3, 3, 3",
            "2, 2, 2",
            "4, 3, 3",
            "3, 4, 3",
            "4, 4, 4",
        ],
        "vec_types": float_t,
    },
}
vecops2_formats = {
    "opcode": "OP_{op_vop[dd].upper()}_{vop_type[t]}",
    "mnemonic": "{op_vop[dd]}.{vop_type[t]}",
    "opname": "{op_vop[dd]}",
    "widths": "4, 4, 4",
    "types": "{vec_types[t]}, {vec_types[t]}, {vec_types[t]}",
    "args": {
        "op_vop": [None, "qv4mul", "v4qmul", None],
        "vop_type": ['F', 'D'],
        "vec_types": float_t,
    },
}
with_formats = {
    "opcode": "OP_WITH",
    "mnemonic": "with",
    "opname": "with",
    "format": "%sa, %sb, %sc",
    "widths": "0, 0, 0",
    "types": "ev_void, ev_void, ev_void",
}

group_map = {
    "all":      all_formats,
    "any":      any_formats,
    "bitops":   bitops_formats,
    "boolops":  boolops_formats,
    "branch":   branch_formats,
    "compare":  compare_formats,
    "compare2": compare2_formats,
    "convert":  convert_formats,
    "convert2": convert2_formats,
    "lea":      lea_formats,
    "lea_e":    lea_e_formats,
    "load":     load_formats,
    "mathops":  mathops_formats,
    "memset":   memset_formats,
    "move":     move_formats,
    "none":     none_formats,
    "push":     push_formats,
    "pushregs": pushregs_formats,
    "pop":      pop_formats,
    "popregs":  popregs_formats,
    "scale":    scale_formats,
    "shiftops": shiftops_formats,
    "state":    state_formats,
    "store":    store_formats,
    "string":   string_formats,
    "swizzle":  swizzle_formats,
    "rcall":    rcall_formats,
    "return":   return_formats,
    "returnv":  returnv_formats,
    "vecops":   vecops_formats,
    "vecops2":  vecops2_formats,
    "with":     with_formats,
}

def parse_bits(bit_string):
    bits = [""]
    isbit = bit_string[0] in ['0', '1']
    lastbit = bit_string[0]
    while bit_string:
        bit = bit_string[0]
        bit_string = bit_string[1:]
        if isbit and bit in ['0', '1']:
            bits[-1] = bits[-1] + bit
        elif lastbit == bit:
            bits[-1] = bits[-1] + bit
        else:
            bits.append(bit)
        lastbit = bit
        isbit = bit in ['0', '1']
    return bits

opcodes = [None] * 512

def expand_opcodes(bits, group, num=0):
    if not bits:
        opcodes[num] = group
        return
    block = bits[0]
    bits = bits[1:]
    num <<= len(block)
    if block[0] in ['0', '1']:
        num |= int(block, 2)
        expand_opcodes(bits, group, num)
    else:
        for n in range(1<<len(block)):
            if group is not None:
                expand_opcodes(bits, group + f",{block}={n}", num | n)
            else:
                expand_opcodes(bits, group, num | n)


def process_opcode(opcode, group):
    data = group.split(',')
    params = {}
    for d in data[1:]:
        p = d.split('=')
        params[p[0]] = int(p[1])
    gm = group_map[data[0]]
    if "args" in gm:
        params.update(gm["args"])
    inst = {}
    opcodes[opcode] = inst
    inst["op"] = eval(f'''f"{gm['opcode']}"''', params)
    mn = eval(f'''f"{gm['mnemonic']}"''', params)
    inst["mn"] = f'"{mn}"'
    on = eval(f'''f"{gm['opname']}"''', params)
    inst["on"] = f'"{on}"'
    fmt = "%Ga, %Gb, %gc"
    if "format" in gm:
        if gm["format"] is not None:
            fmt = eval(f'''f"{gm['format']}"''', params)
        else:
            fmt = None
    if fmt is None:
        fmt = "0"
    else:
        fmt = f'"{fmt}"'
    inst["fmt"] = fmt
    inst["wd"] = "{%s}" % eval(f'''f"{gm['widths']}"''', params)
    inst["ty"] = "{%s}" % eval(f'''f"{gm['types']}"''', params)

import sys

if len (sys.argv) < 2 or sys.argv[1] not in ["enum", "table"]:
    sys.stderr.write ("specify output type: enum or table\n")
    sys.exit(1)

lines = bitmap_txt.split('\n')
for l in lines:
    if not l:
        continue
    c = l.split(' ')
    bits = "".join(c[0:3])
    group = c[3:] and c[3] or None
    descr = ' '.join(c[4:])
    #print(bits, group, descr)
    #print(parse_bits(bits))
    expand_opcodes(parse_bits(bits), group)
for i, group in enumerate(opcodes):
    if group is not None:
        process_opcode (i, group)
if sys.argv[1] == "enum":
    N = 4
    W = 15
    for i in range(len(opcodes)//N):
        row = opcodes[i * N:i * N + N]
        #row = map(lambda op: (op and op["op"] or f"OP_spare_{i}"), row)
        row = [(op and op["op"] or f"OP_spare_{i*N+x}")
               for x, op in enumerate(row)]
        row = map(lambda op: f"%-{W}.{W}s" % (op + ','), row)
        if sys.argv[2:3] == ["debug"]:
            print("%03x" % (i << 3), " ".join(row))
        else:
            print(" ".join(row))
elif sys.argv[1] == "table":
    for i, group in enumerate(opcodes):
        if group is not None:
            print(eval('f"[{op}] = {{\\n'
                       '\\t.opname = {on},\\n'
                       '\\t.mnemonic = {mn},\\n'
                       '\\t.widths = {wd},\\n'
                       '\\t.types = {ty},\\n'
                       '\\t.fmt = {fmt},\\n'
                       '}},"', group))
