def iter(func):
    for i in range(4):
        for j in range(4):
            for k in range(4):
                for l in range(4):
                    func(i, j, k, l)

def iter16(func):
    for i in range(16):
        func(i)

import sys

coord=['x', 'y', 'z', 'w']
def label(i, j, k, l):
    return f"swizzle_{coord[l]}{coord[k]}{coord[j]}{coord[i]}"

def print_ref(i, j, k, l):
    print(f"\t\t&&{label(i, j, k, l)},")

def print_op(i, j, k, l):
    print(f"\t{label(i, j, k, l)}: vec = swizzle (vec, (pr_ivec4_t) {{ {l}, {k}, {j}, {i} }}); goto negate;")

def print_data(i, j, k, l):
    print(f"\t{{ {l+1:2}, {k+1:2}, {j+1:2}, {i+1:2} }},")

def print_swizzle_f(i, j, k, l):
    swiz = i * 64 + j * 16 + k * 4 + l
    addr = (swiz + 1) * 4
    print(f"\t{{ OP(0, 0, 0, OP_SWIZZLE_F), 0, 0x{swiz:04x}, {addr} }},")

def print_neg_f(i):
    swiz = i * 0x100 + 0xe4
    addr = (i + 1) * 4
    print(f"\t{{ OP(0, 0, 0, OP_SWIZZLE_F), 0, 0x{swiz:04x}, {addr} }},")

def print_zero_f(i):
    swiz = i * 0x1000 + 0xe4
    addr = (i + 1) * 4
    print(f"\t{{ OP(0, 0, 0, OP_SWIZZLE_F), 0, 0x{swiz:04x}, {addr} }},")

def print_swizzle_d(i, j, k, l):
    swiz = i * 64 + j * 16 + k * 4 + l
    addr = (swiz + 1) * 8
    print(f"\t{{ OP(0, 0, 0, OP_SWIZZLE_D), 0, 0x{swiz:04x}, {addr} }},")

def print_neg_d(i):
    swiz = i * 0x100 + 0xe4
    addr = (i + 1) * 8
    print(f"\t{{ OP(0, 0, 0, OP_SWIZZLE_D), 0, 0x{swiz:04x}, {addr} }},")

def print_zero_d(i):
    swiz = i * 0x1000 + 0xe4
    addr = (i + 1) * 8
    print(f"\t{{ OP(0, 0, 0, OP_SWIZZLE_D), 0, 0x{swiz:04x}, {addr} }},")

def print_eights(i, j, k, l):
    print(f"\t{{ {8:2}, {8:2}, {8:2}, {8:2} }},")

def print_nines(i):
    print(f"\t{{ {9:2}, {9:2}, {9:2}, {9:2} }},")

def print_neg(n):
    x = [1, 2, 3, 4]
    for i in range(4):
        if n & (1<< i):
            x[i] = -x[i]
    print(f"\t{{ {x[0]:2}, {x[1]:2}, {x[2]:2}, {x[3]:2} }},")

def print_zero(z):
    x = [1, 2, 3, 4]
    for i in range(4):
        if z & (1<< i):
            x[i] = 0
    print(f"\t{{ {x[0]:2}, {x[1]:2}, {x[2]:2}, {x[3]:2} }},")

types = ["f", "d"]
tests = ["swizzle", "neg", "zero"]

if sys.argv[1] == "case":
    iter(print_op)
    print("\tstatic void *swizzle_table[256] = {")
    iter(print_ref)
    print("\t};")
elif sys.argv[1] == "test":
    print('#include "head.c"')
    print()
    print("static pr_vec4_t swizzle_f_init[] = {")
    print_data(3, 2, 1, 0)
    iter(print_eights)
    print("};")
    print("static pr_vec4_t swizzle_f_expect[] = {")
    print_data(3, 2, 1, 0)
    iter(print_data)
    print("};")
    print()
    print("static dstatement_t swizzle_f_statements[] = {")
    iter(print_swizzle_f)
    print("};")
    print()
    print("static pr_vec4_t neg_f_init[] = {")
    print_neg(0)
    iter16(print_nines)
    print("};")
    print()
    print("static pr_vec4_t neg_f_expect[] = {")
    print_neg(0)
    iter16(print_neg)
    print("};")
    print()
    print("static dstatement_t neg_f_statements[] = {")
    iter16(print_neg_f)
    print("};")
    print()
    print("static pr_vec4_t zero_f_init[] = {")
    print_zero(0)
    iter16(print_nines)
    print("};")
    print()
    print("static pr_vec4_t zero_f_expect[] = {")
    print_zero(0)
    iter16(print_zero)
    print("};")
    print()
    print("static dstatement_t zero_f_statements[] = {")
    iter16(print_zero_f)
    print("};")
    print()
    print("static pr_dvec4_t swizzle_d_init[] = {")
    print_data(3, 2, 1, 0)
    iter(print_eights)
    print("};")
    print("static pr_dvec4_t swizzle_d_expect[] = {")
    print_data(3, 2, 1, 0)
    iter(print_data)
    print("};")
    print()
    print("static dstatement_t swizzle_d_statements[] = {")
    iter(print_swizzle_d)
    print("};")
    print()
    print("static pr_dvec4_t neg_d_init[] = {")
    print_neg(0)
    iter16(print_nines)
    print("};")
    print()
    print("static pr_dvec4_t neg_d_expect[] = {")
    print_neg(0)
    iter16(print_neg)
    print("};")
    print()
    print("static dstatement_t neg_d_statements[] = {")
    iter16(print_neg_d)
    print("};")
    print()
    print("static pr_dvec4_t zero_d_init[] = {")
    print_zero(0)
    iter16(print_nines)
    print("};")
    print()
    print("static pr_dvec4_t zero_d_expect[] = {")
    print_zero(0)
    iter16(print_zero)
    print("};")
    print()
    print("static dstatement_t zero_d_statements[] = {")
    iter16(print_zero_d)
    print("};")
    print()
    print("test_t tests[] = {")
    for t in types:
        for o in tests:
            print("\t{")
            print(f'\t\t.desc = "{o} {t}",')
            print(f"\t\t.num_globals = num_globals({o}_{t}_init,{o}_{t}_expect),")
            print(f"\t\t.num_statements = num_statements({o}_{t}_statements),")
            print(f"\t\t.statements = {o}_{t}_statements,")
            print(f"\t\t.init_globals = (pr_int_t *) {o}_{t}_init,")
            print(f"\t\t.expect_globals = (pr_int_t *) {o}_{t}_expect,")
            print("\t},")
    print("};")
    print()
    print('#include "main.c"')
