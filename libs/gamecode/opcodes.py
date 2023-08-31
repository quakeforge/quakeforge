bitmap_txt = """
0 0000 mmss load
0 0001 mmss store
0 0010 mmss push
0 0011 mmss pop
0 1ccc ttss compare
0 0000 00nn
0 0000 0000 noop
0 0000 0001 adjstk
0 0000 0010 constant
0 1011 otss udivops
0 1111 s0mm load64
0 1111 s1mm store64
0 1111 m000 lea2

1 0ooo ttss mathops
1 011r tuss shiftops
1 0110 o1oo string
1 1ccc t0ss compare2
1 1t00 ooss bitops
1 1001 01mm jump
1 1001 11mm call (return specified st->c)
1 1001 1100 return (size in st->c)
1 1010 t1ss scale
1 1010 t100 swizzle
1 1011 tooo vecops
1 1101 01oo move
1 1101 11oo memset
1 1101 c111 statef
1 1110 c1cc branch
1 1110 c111 stated
1 1111 00mm lea
1 1111 01td vecops2
1 1111 10oo fbitops
1 1111 1100 convert (conversion mode in st->b)
1 1111 1101 with (mode in st->a, value in st->b, reg in st->c)
1 1111 1110 extend
1 1111 1111 hops
"""

import copy

address_mode = "ABCD"
address_types = [
    "ev_void, ev_invalid",
    "ev_entity, ev_field",
    "ev_ptr, ev_short",
    "ev_ptr, ev_int",
    "ev_void, ev_short",
    "ev_void, ev_int",
]
address_widths = [
    [ "1, 0", "1, 1", "1, 0", "1, 1", ],
    [ "2, 0", "1, 1", "1, 0", "1, 1", ],
    [ "3, 0", "1, 1", "1, 0", "1, 1", ],
    [ "4, 0", "1, 1", "1, 0", "1, 1", ],
    [ "-1, 0", "1, 1", "1, 0", "1, 1", "-1, 0", "-1, 1"],
]
#store, pop, lea
store_fmt = [
    "%ga",
    "%Ga.%Gb(%Ea)",
    "*(%Ga + %sb)",
    "*(%Ga + %Gb)",
    "%ga + %sb",
    "%ga + %Gb",
]
# load and push
load_fmt =  [
    "*%Ga, %gc",
    "%Ga.%Gb(%Ea)",
    "*(%Ga + %sb)",
    "*(%Ga + %Gb)",
]
branch_fmt = [
    "branch %sa (%Oa)",
    "*%Ga",
    "%Ga[%sb]",
    "%Ga[%Gb]",
]
compare_ccc = [ "eq", "lt", "gt", None, "ne", "ge", "le", None]
type_tt = ['I', 'F', 'L', 'D']
etype_tt = ["ev_int", "ev_float", "ev_long", "ev_double"]
unsigned_t = ["ev_uint", "ev_ulong"]
float_t = ["ev_float", "ev_double"]

adjstk_formats = {
    "opcode": "OP_ADJSTK",
    "mnemonic": "adjstk",
    "opname": "adjstk",
    "format": "%sa, %sb",
    "widths": "0, 0, 0",
    "types": "ev_short, ev_short, ev_invalid",
}
bitops_formats = {
    "opcode": "OP_{op_bit[oo].upper()}_{bit_type[t]}_{ss+1}",
    "mnemonic": "{op_bit[oo]}",
    "opname": "{op_bit[oo]}",
    "format": "{bit_fmt[oo]}",
    "widths": "{ss+1}, { oo < 3 and ss+1 or 0}, {ss+1}",
    "types": "{bit_types[t]}, {oo < 3 and bit_types[t] or 'ev_invalid'}, {bit_types[t]}",
    "args": {
        "op_bit": ["bitand", "bitor", "bitxor", "bitnot"],
        "bit_type": ["I", "L"],
        "bit_types": ["ev_int", "ev_long"],
        "bit_fmt": [
            "%Ga, %Gb, %gc",
            "%Ga, %Gb, %gc",
            "%Ga, %Gb, %gc",
            "%Ga, %gc",
        ],
    },
}
branch_formats = {
    "opcode": "OP_{op_cond[c*4+cc].upper()}",
    "mnemonic": "{op_cond[c*4+cc]}",
    "opname": "{op_cond[c*4+cc]}",
    "format": "{cond_fmt[c*4+cc]}{branch_fmt[0]}",
    "widths": "0, 0, 1",
    "types": "ev_short, ev_invalid, ev_int",
    "args": {
        "op_mode": "ABCD",
        "op_cond": ["ifz",  "ifb",  "ifa",  None,
                    "ifnz", "ifae", "ifbe", None],
        "branch_fmt": branch_fmt,
        "cond_fmt": ["%Gc ", "%Gc ", "%Gc ", "", "%Gc ", "%Gc ", "%Gc ", ""],
    },
}
call_formats = {
    "opcode": "OP_CALL_{op_mode[mm]}",
    "mnemonic": "call",
    "opname": "call",
    "format": "{call_fmt[mm]}",
    "widths": "{call_widths[mm]}, -1",
    "types": "{call_types[mm]}, ev_void",
    "args": {
        "op_mode": ".BCD",
        "call_fmt": [
            None,           # return handled seprately
            "%Ga, %gc",
            "%Ga[%sb], %gc",
            "%Ga[%Gb], %gc",
        ],
        "call_types": [
            None,
            "ev_void, ev_invalid",
            "ev_ptr, ev_short",
            "ev_ptr, ev_int",
        ],
        "call_widths": [ None, "1, 0", "1, 0", "1, 1" ]
    },
}
compare_formats = {
    "opcode": "OP_{op_cmp[ccc].upper()}_{cmp_type[tt]}_{ss+1}",
    "mnemonic": "{op_cmp[ccc]}.{cmp_type[tt]}",
    "opname": "{op_cmp[ccc]}",
    "widths": "{ss+1}, {ss+1}, {ss+1}",
    "types": "{cmp_types[tt]}, {cmp_types[tt]}, {res_types[tt & 2]}",
    "args": {
        "op_cmp": compare_ccc,
        "cmp_type": type_tt,
        "cmp_types": etype_tt,
        "res_types": etype_tt,
    },
}
compare2_formats = {
    "opcode": "OP_{op_cmp[ccc].upper()}_{cmp_type[t]}_{ss+1}",
    "mnemonic": "{op_cmp[ccc]}.{cmp_type[t]}",
    "opname": "{op_cmp[ccc]}",
    "widths": "{ss+1}, {ss+1}, {ss+1}",
    "types": "{cmp_types[t]}, {cmp_types[t]}, ev_int",
    "args": {
        "op_cmp": compare_ccc,
        "cmp_type": ['u', 'U'],
        "cmp_types": unsigned_t,
    },
}
constant_formats = {
    "opcode": "OP_LDCONST",
    "mnemonic": "ldconst",
    "opname": "ldconst",
    "format": "%sa, %sb, %gc",
    "widths": "0, 0, -1",
    "types": "ev_short, ev_short, ev_void",
}
convert_formats = {
    "opcode": "OP_CONV",
    "mnemonic": "conv",
    "opname": "conv",
    "format": "%Ga %Cb %gc",
    "widths": "-1, 0, -1",
    "types": "ev_void, ev_short, ev_void",
}
fbitops_formats = {
    "opcode": "OP_{op_fbit[oo].upper()}_F",
    "mnemonic": "{op_fbit[oo]}.f",
    "opname": "{op_fbit[oo]}",
    "format": "{fbit_fmt[oo]}",
    "widths": "1, 1, 1",
    "types": "{fbit_types[0]}, {fbit_types[oo==3]}, {fbit_types[0]}",
    "args": {
        "op_fbit": ["bitand", "bitor", "bitxor", "bitnot"],
        "fbit_types": ["ev_float", "ev_invalid"],
        "fbit_fmt": [
            "%Ga, %Gb, %gc",
            "%Ga, %Gb, %gc",
            "%Ga, %Gb, %gc",
            "%Ga, %gc",
        ],
    },
}
extend_formats = {
    "opcode": "OP_EXTEND",
    "mnemonic": "extend",
    "opname": "extend",
    "format": "%Ga%Xb, %gc",
    "widths": "-1, 0, -1",
    "types": "ev_void, ev_short, ev_void",
}
hops_formats = {
    "opcode": "OP_HOPS",
    "mnemonic": "hops",
    "opname": "hops",
    "format": "%Hb %Ga, %gc",
    "widths": "-1, 0, 1",
    "types": "ev_void, ev_short, ev_void",
}
jump_formats = {
    "opcode": "OP_JUMP_{op_mode[mm]}",
    "mnemonic": "jump",
    "opname": "jump",
    "format": "{jump_fmt[mm]}",
    "widths": "{jump_widths[mm]}, 0",
    "types": "{jump_types[mm]}",
    "args": {
        "op_mode": "ABCD",
        "jump_fmt": branch_fmt,
        "jump_types": [
            "ev_short, ev_invalid, ev_invalid",
            "ev_void, ev_int, ev_invalid",
            "ev_ptr, ev_short, ev_invalid",
            "ev_ptr, ev_int, ev_invalid",
        ],
        "jump_widths": [ "0, 0", "1, 1", "1, 0", "1, 1" ]
    },
}
load64_formats = {
    "opcode": "OP_LOAD64_{op_mode[mm]}_{s+3}",
    "mnemonic": "load64",
    "opname": "load64",
    "format": "{load_fmt[mm]}, %gc",
    "widths": "{load_widths[s+2][mm]}, {s+3}",
    "types": "{load_types[mm]}, ev_void",
    "args": {
        "op_mode": address_mode,
        "load_fmt": load_fmt,
        "load_types": address_types,
        "load_widths": address_widths,
    },
}
lea_formats = {
    "opcode": "OP_LEA_{op_mode[mm]}",
    "mnemonic": "lea",
    "opname": "lea",
    "format": "{lea_fmt[mm]}, %gc",
    "widths": "{lea_widths[mm]}, 1",
    "types": "{lea_types[mm]}, ev_ptr",
    "args": {
        "op_mode": address_mode,
        "lea_fmt": store_fmt,
        "lea_types": address_types,
        "lea_widths": address_widths[4],
    },
}
lea2_formats = {
    "opcode": "OP_LEA_{op_mode[m]}",
    "mnemonic": "lea",
    "opname": "lea",
    "format": "{lea_fmt[m+4]}, %gc",
    "widths": "{lea_widths[m+4]}, 1",
    "types": "{lea_types[m+4]}, ev_ptr",
    "args": {
        "op_mode": "EF",
        "lea_fmt": store_fmt,
        "lea_types": address_types,
        "lea_widths": address_widths[4],
    },
}
load_formats = {
    "opcode": "OP_LOAD_{op_mode[mm]}_{ss+1}",
    "mnemonic": "load",
    "opname": "load",
    "format": "{load_fmt[mm]}, %gc",
    "widths": "{load_widths[ss][mm]}, {ss+1}",
    "types": "{load_types[mm]}, ev_void",
    "args": {
        "op_mode": address_mode,
        "load_fmt": load_fmt,
        "load_types": address_types,
        "load_widths": address_widths,
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
    "opname": "memset{suff_memset[oo]}",
    "format": "{memset_fmt[oo]}",
    "widths": "{memset_widths[oo]}",
    "types": "{memset_types[oo]}",
    "args": {
        "op_memset": ["i", "p", "pi", None],
        "suff_memset": ["", "p", "p", None],
        "memset_fmt": ["%Ga, %sb, %gc", "%Ga, %Gb, %Gc", "%Ga, %sb, %Gc", None],
        "memset_widths": [
            "1, 0, -1",
            "1, 1, 1",
            "1, 0, 1",
            None,
        ],
        "memset_types": [
            "ev_int, ev_short, ev_void",
            "ev_int, ev_int, ev_ptr",
            "ev_int, ev_short, ev_ptr",
        ],
    },
}
move_formats = {
    "opcode": "OP_MOVE_{op_move[oo].upper()}",
    "mnemonic": "move.{op_move[oo]}",
    "opname": "move{suff_move[oo]}",
    "format": "{move_fmt[oo]}",
    "widths": "{move_widths[oo]}",
    "types": "{move_types[oo]}",
    "args": {
        "op_move": ["i", "p", "pi", None],
        "suff_move": ["", "p", "p", None],
        "move_fmt": ["%Ga, %sb, %gc", "%Ga, %Gb, %Gc", "%Ga, %sb, %Gc", None],
        "move_widths": [
            "-1, 0, -1",
            "1, 1, 1",
            "1, 0, 1",
            None,
        ],
        "move_types": [
            "ev_void, ev_short, ev_void",
            "ev_ptr, ev_int, ev_ptr",
            "ev_ptr, ev_short, ev_ptr",
        ],
    },
}
noop_formats = {
    "opcode": "OP_NOP",
    "mnemonic": "nop",
    "opname": "nop",
    "format": "there were plums...",
    "widths": "0, 0, 0",
    "types": "ev_invalid, ev_invalid, ev_invalid",
}
push_formats = {
    "opcode": "OP_PUSH_{op_mode[mm]}_{ss+1}",
    "mnemonic": "push",
    "opname": "push",
    "format": "{push_fmt[mm]}",
    "widths": "{ss+1}, 0, 0",
    "types": "{push_types[mm]}, ev_invalid",
    "args": {
        "op_mode": address_mode,
        "push_fmt": load_fmt,
        "push_types": address_types,
    },
}
pop_formats = {
    "opcode": "OP_POP_{op_mode[mm]}_{ss+1}",
    "mnemonic": "pop",
    "opname": "pop",
    "format": "{pop_fmt[mm]}",
    "widths": "{ss+1}, 0, 0",
    "types": "{pop_types[mm]}, ev_invalid",
    "args": {
        "op_mode": address_mode,
        "pop_fmt": store_fmt,
        "pop_types": address_types,
    },
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
    "opcode": "OP_{mn_shift[u*2+r].upper()}_{shift_type[u*2+t]}_{ss+1}",
    "mnemonic": "{mn_shift[u*2+r]}.{shift_type[u*2+t]}",
    "opname": "{op_shift[u*2+r]}",
    "widths": "{ss+1}, {ss+1}, {ss+1}",
    "types": "{shift_types[t][u]}, {shift_types[t][0]}, {shift_types[t][u]}",
    "args": {
        "mn_shift": ["shl", "asr", "shl", "shr"],
        "op_shift": ["shl", "shr", "shl", "shr"],
        "shift_type": ['I', 'L', 'u', 'U'],
        "shift_types": [
            ["ev_int", "ev_uint"],
            ["ev_long", "ev_ulong"],
        ],
    },
}
statef_formats = {
    "opcode": "OP_STATE_{state[c]}",
    "mnemonic": "state.{state[c]}",
    "opname": "state",
    "format": "{state_fmt[c]}",
    "widths": "1, 1, {c}",
    "types": "ev_float, ev_func, {state_types[c]}",
    "args": {
        "state": ["ft", "ftt"],
        "state_fmt": ["%Ga, %Gb", "%Ga, %Gb, %Gc"],
        "state_types": ["ev_invalid", "ev_float"],
    },
}
stated_formats = {
    "opcode": "OP_STATE_{state[c]}",
    "mnemonic": "state.{state[c]}",
    "opname": "state",
    "format": "{state_fmt[c]}",
    "widths": "1, 1, {c}",
    "types": "ev_int, ev_func, {state_types[c]}",
    "args": {
        "state": ["dt", "dtt"],
        "state_fmt": ["%Ga, %Gb", "%Ga, %Gb, %Gc"],
        "state_types": ["ev_invalid", "ev_double"],
    },
}
store_formats = {
    "opcode": "OP_STORE_{op_mode[mm]}_{ss+1}",
    "mnemonic": "{store_op[mm]}",
    "opname": "{store_op[mm]}",
    "format": "%Gc, {store_fmt[mm]}",
    "widths": "{store_widths[ss][mm]}, {ss+1}",
    "types": "{store_types[mm]}, ev_void",
    "args": {
        "op_mode": address_mode,
        "store_fmt": store_fmt,
        "store_op": ["assign", "store", "store", "store"],
        "store_types": address_types,
        "store_widths": address_widths,
    },
}
store64_formats = {
    "opcode": "OP_STORE64_{op_mode[mm]}_{s+3}",
    "mnemonic": "{store_op[mm]}64",
    "opname": "{store_op[mm]}64",
    "format": "%Gc, {store_fmt[mm]}",
    "widths": "{store_widths[s+2][mm]}, {s+3}",
    "types": "{store_types[mm]}, ev_void",
    "args": {
        "op_mode": address_mode,
        "store_fmt": store_fmt,
        "store_op": ["assign", "store", "store", "store"],
        "store_types": address_types,
        "store_widths": address_widths,
    },
}
string_formats = {
    "opcode": "OP_{op_str[o*4+oo].upper()}_S",
    "mnemonic": "{op_str[o*4+oo]}.s",
    "opname": "{op_str[o*4+oo]}",
    "format": "{str_fmt[o*4+oo]}",
    "widths": "1, {(o*4+oo)<7 and 1 or 0}, 1",
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
            "ev_string, ev_string, ev_int",
            "ev_string, ev_string, ev_int",
            "ev_string, ev_string, ev_int",
            "ev_string, ev_string, ev_string",
            "ev_string, ev_string, ev_int",
            "ev_string, ev_string, ev_int",
            "ev_string, ev_string, ev_int",
            "ev_string, ev_invalid, ev_int",
        ],
    },
}
swizzle_formats = {
    "opcode": "OP_SWIZZLE_{swiz_type[t]}",
    "mnemonic": "swizzle.{swiz_type[t]}",
    "opname": "swizzle",
    "format": "%Ga.%Sb %gc",
    "widths": "4, 0, 4",
    "types": "{swizzle_types[t]}",
    "args": {
        "swiz_type": ['F', 'D'],
        "swizzle_types": float_t,
    },
}
return_formats = {
    "opcode": "OP_RETURN",
    "mnemonic": "return",
    "opname": "return",
    "widths": "-1, -1, 0",    # width specified by st->c
    "format": "%Mc5",
    "types": "ev_void, ev_void, ev_void",
}
udivops_formats = {
    "opcode": "OP_{op_udiv[o].upper()}_{udiv_type[t]}_{ss+1}",
    "mnemonic": "{op_udiv[o]}.{udiv_type[t]}",
    "opname": "{op_udiv[o]}",
    "widths": "{ss+1}, {ss+1}, {ss+1}",
    "types": "{udiv_types[t]}, {udiv_types[t]}, {udiv_types[t]}",
    "args": {
        "op_udiv": ["div", "rem"],
        "udiv_type": ['u', 'U'],
        "udiv_types": ["ev_uint", "ev_ulong"],
    },
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
            "3, 3, 3",
            "2, 2, 1",
            "3, 3, 1",
            "4, 4, 1",
            "2, 2, 2",
            "4, 3, 3",
            "3, 4, 3",
            "4, 4, 4",
        ],
        "vec_types": float_t,
    },
}
vecops2_formats = {
    "opcode": "OP_{op_vop[d].upper()}_{vop_type[t]}",
    "mnemonic": "{op_vop[d]}.{vop_type[t]}",
    "opname": "{op_vop[d]}",
    "widths": "4, 4, 4",
    "types": "{vec_types[t]}, {vec_types[t]}, {vec_types[t]}",
    "args": {
        "op_vop": ["qv4mul", "v4qmul"],
        "vop_type": ['F', 'D'],
        "vec_types": float_t,
    },
}
with_formats = {
    "opcode": "OP_WITH",
    "mnemonic": "with",
    "opname": "with",
    "format": "%sa, %sb, %sc",
    "widths": "0, -1, 0",
    "types": "ev_short, ev_void, ev_short",
}

group_map = {
    "adjstk":   adjstk_formats,
    "bitops":   bitops_formats,
    "branch":   branch_formats,
    "call":     call_formats,
    "compare":  compare_formats,
    "compare2": compare2_formats,
    "constant": constant_formats,
    "convert":  convert_formats,
    "extend":   extend_formats,
    "fbitops":  fbitops_formats,
    "hops":     hops_formats,
    "jump":     jump_formats,
    "lea":      lea_formats,
    "lea2":     lea2_formats,
    "load":     load_formats,
    "load64":   load64_formats,
    "mathops":  mathops_formats,
    "memset":   memset_formats,
    "move":     move_formats,
    "noop":     noop_formats,
    "push":     push_formats,
    "pop":      pop_formats,
    "scale":    scale_formats,
    "shiftops": shiftops_formats,
    "statef":   statef_formats,
    "stated":   stated_formats,
    "store":    store_formats,
    "store64":  store64_formats,
    "string":   string_formats,
    "swizzle":  swizzle_formats,
    "return":   return_formats,
    "udivops":  udivops_formats,
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
    #print(f"{opcode:03x}", group)
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
    l = l.strip()
    if not l or l[0] == '#':
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
