def iter(func):
    for i in range(4):
        for j in range(4):
            for k in range(4):
                for l in range(4):
                    func(i, j, k, l)

coord=['x', 'y', 'z', 'w']
def label(i, j, k, l):
    return f"swizzle_{coord[l]}{coord[k]}{coord[j]}{coord[i]}"

def print_ref(i, j, k, l):
    print(f"\t\t&&{label(i, j, k, l)},")

def print_op(i, j, k, l):
    print(f"\t{label(i, j, k, l)}: vec = swizzle (vec, (pr_ivec4_t) {{ {l}, {k}, {j}, {i} }}); goto negate;")

iter(print_op)
print("\tstatic void *swizzle_table[256] = {")
iter(print_ref)
print("\t};")
