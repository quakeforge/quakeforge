print("""// types are encoded as ubf where:
//  u = 0: signed, u = 1: unsigned
//  b = 0: 32-bit, b = 1: 64-bit
//  f = 0: int,    f = 1: float/double
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
    None,       # no such thing as unsigned float
    "ulong",
    None,       # no such thing as unsigned double
]
#does not include size (2 or 4, 3 is special)
vec_types = [
    "ivec",
    "vec",
    "lvec",
    "dvec",
    "uivec",
    None,       # no such thing as unsigned float
    "ulvec",
    None,       # no such thing as unsigned double
]
convert_matrix = [
    #i  f  l  d   ui X  ul X
    [0, 1, 1, 1,  0, 0, 1, 0],  # i
    [1, 0, 1, 1,  1, 0, 1, 0],  # f
    [1, 1, 0, 1,  1, 0, 0, 0],  # l
    [1, 1, 1, 0,  1, 0, 1, 0],  # d

    [0, 1, 1, 1,  0, 0, 1, 0],  # ui
    [0, 0, 0, 0,  0, 0, 0, 0],  # X
    [1, 1, 0, 1,  1, 0, 0, 0],  # ul
    [0, 0, 0, 0,  0, 0, 0, 0],  # X
]

def case_str(width, src_type, dst_type):
    case = (width << 6) | (src_type << 3) | (dst_type)
    return f"case {case:04o}:"

def cast_str(width, src_type, dst_type):
    if width & 1 and (src_type & 2) == (dst_type & 2):
        return f"(pr_{vec_types[dst_type]}{width+1}_t)"
    else:
        return f"(pr_{types[dst_type]}_t)"

def src_str(width, src_type, dst_type):
    if width & 1 and (src_type & 2) == (dst_type & 2):
        return f"OPA({vec_types[src_type]}{width+1})"
    else:
        return f"OPA({types[src_type]})"

def dst_str(width, src_type, dst_type):
    if width & 1 and (src_type & 2) == (dst_type & 2):
        return f"OPC({vec_types[dst_type]}{width+1})"
    else:
        return f"OPC({types[dst_type]})"

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
            if width == 0:
                print(f"{case} {dst} = {cast} {src}; break;")
            elif width == 2:
                print(f"{case} VectorCompUop(&{dst}, {cast}, &{src}); break;")
            else:
                if (src_type & 2) == (dst_type & 2):
                    print(f"{case} {dst} = {cast} {src}; break;")
                else:
                    print(f"{case}")
                    for i in range(width + 1):
                        print(f"\t(&{dst})[{i}] = {cast} (&{src})[{i}];")
                    print(f"\tbreak;")
