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
skip_matrix = [
    #i  f  l  d   ui X  ul X
    [1, 0, 0, 0,  1, 1, 0, 1],  # i
    [0, 1, 0, 0,  0, 1, 0, 1],  # f
    [0, 0, 1, 0,  0, 1, 1, 1],  # l
    [0, 0, 0, 1,  0, 1, 0, 1],  # d

    [1, 0, 0, 0,  1, 1, 0, 1],  # ui
    [1, 1, 1, 1,  1, 1, 1, 1],  # X
    [0, 0, 1, 0,  0, 1, 1, 1],  # ul
    [1, 1, 1, 1,  1, 1, 1, 1],  # X
]
for width in range(4):
    for src_type in range(8):
        for dst_type in range(8):
            if skip_matrix[src_type][dst_type]:
                continue
            case = (width << 6) | (src_type << 3) | (dst_type)
            if width == 0:
                print(f"case {case:04o}: OPC({types[dst_type]}) = (pr_{types[dst_type]}_t) OPA({types[src_type]}); break;")
            elif width == 2:
                print(f"case {case:04o}: VectorCompUop(&OPC({types[dst_type]}), (pr_{types[dst_type]}_t), &OPA({types[src_type]})); break;")
            else:
                if (src_type & 2) == (dst_type & 2):
                    print(f"case {case:04o}: OPC({vec_types[dst_type]}{width+1}) = (pr_{vec_types[dst_type]}{width+1}_t) OPA({vec_types[src_type]}{width+1}); break;")
                else:
                    print(f"case {case:04o}:")
                    for i in range(width + 1):
                        print(f"\t(&OPC({types[dst_type]}))[{i}] = (pr_{types[dst_type]}_t) (&OPA({types[src_type]}))[{i}];")
                    print(f"\tbreak;")
