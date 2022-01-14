print("""// types are encoded as ubf where:
//  u = 0: signed, u = 1: unsigned
//  b = 0: 32-bit, b = 1: 64-bit
//  f = 0: int,    f = 1: float/double
//  unsigned float/double is interpreted as bool
// width is ww where:
//  ww = 00: 1 component
//  ww = 01: 2 components
//  ww = 10: 3 components
//  ww = 11: 4 components
// full conversion code is wwsssddd where:
//  ww = width
//  sss = src type
//  ddd = dst type
// case values are in octal
""")
types = [
    "int",
    "float",
    "long",
    "double",
    "uint",
    "int",      # 32-bit bool
    "ulong",
    "long",     # 64-bit bool
]
#does not include size (2 or 4, 3 is special)
vec_types = [
    "ivec",
    "vec",
    "lvec",
    "dvec",
    "uivec",
    "ivec",     # 32-bit bool
    "ulvec",
    "lvec",     # 64-bit bool
]
convert_matrix = [
    #i  f  l  d   ui b  ul B
    [0, 1, 1, 1,  0, 3, 1, 3],  # i
    [1, 0, 1, 1,  1, 3, 1, 3],  # f
    [1, 1, 0, 1,  1, 3, 0, 3],  # l
    [1, 1, 1, 0,  1, 3, 1, 3],  # d

    [0, 1, 1, 1,  0, 3, 1, 3],  # ui
    [2, 2, 2, 2,  2, 0, 2, 3],  # 32-bit bool
    [1, 1, 0, 1,  1, 3, 0, 3],  # ul
    [2, 2, 2, 2,  2, 3, 2, 0],  # 64-bit bool
]

def case_str(width, src_type, dst_type):
    case = (width << 6) | (src_type << 3) | (dst_type)
    return f"case {case:04o}:"

def cast_str(width, src_type, dst_type):
    if width & 1:
        return f"(pr_{vec_types[dst_type]}{width+1}_t)"
    else:
        return f"(pr_{types[dst_type]}_t)"

def src_str(width, src_type, dst_type):
    if width & 1:
        return f"OPA({vec_types[src_type]}{width+1})"
    else:
        return f"OPA({types[src_type]})"

def dst_str(width, src_type, dst_type):
    if width & 1:
        return f"OPC({vec_types[dst_type]}{width+1})"
    else:
        return f"OPC({types[dst_type]})"

def zero_str(width, src_type):
    ones = "{%s}" % (", ".join(["0"] * (width + 1)))
    return f"{cast_str(width, src_type, src_type)} {ones}"

def one_str(width, src_type):
    ones = "{%s}" % (", ".join(["1"] * (width + 1)))
    return f"{cast_str(width, src_type, src_type)} {ones}"

def expand_str(width, src, pref=""):
    src = [f"{pref}{src}[{i}]" for i in range(width + 1)]
    return "{%s}" % (", ".join(src));

for width in range(4):
    for src_type in range(8):
        for dst_type in range(8):
            mode = convert_matrix[src_type][dst_type]
            if not mode:
                continue
            case = case_str(width, src_type, dst_type)
            cast = cast_str(width, src_type, dst_type)
            src = src_str(width, src_type, dst_type)
            dst = dst_str(width, src_type, dst_type)
            if mode == 1:
                if width == 0:
                    print(f"{case} {dst} = {cast} {src}; break;")
                elif width == 2:
                    print(f"{case} VectorCompUop(&{dst},{cast},&{src}); break;")
                else:
                    expand = expand_str(width, src)
                    print(f"{case} {dst} = {cast} {expand}; break;")
            elif mode == 2:
                one = one_str(width, src_type)
                if width == 0:
                    print(f"{case} {dst} = !!{src}; break;")
                elif width == 2:
                    print(f"{case} VectorCompUop(&{dst},!!,&{src}); break;")
                else:
                    expand = expand_str(width, src, "!!")
                    print(f"{case} {dst} = {cast} {expand}; break;")
            elif mode == 3:
                zero = zero_str(width, src_type)
                if width == 0:
                    print(f"{case} {dst} = -!!{src}; break;")
                elif width == 2:
                    print(f"{case} VectorCompUop(&{dst},-!!,&{src}); break;")
                else:
                    expand = expand_str(width, src, "-!!")
                    print(f"{case} {dst} = {cast} {expand}; break;")
