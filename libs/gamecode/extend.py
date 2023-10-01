print("""// encoding is rteemmm
// r = 0: forward, r = 1: reverse (also reverses component order for 2 and 3)
// t = 0: 32-bit, t = 1: 64-bit
// e = 00: 0
// e = 01: 1.0
// e = 10: copy (1-n, 2-4, otherwise 0)
// e = 11: -1.0
// mmm = 000: 1 -> 2
// mmm = 001: 1 -> 3
// mmm = 010: 1 -> 4
// mmm = 011: 2 -> 3
// mmm = 100: 2 -> 4
// mmm = 101: 3 -> 4
// mmm = 110: reserved
// mmm = 111: reserved
""")

types = [
    ["ivec2", "ivec3", "ivec4", "ivec3", "ivec4", "ivec4"],
    ["lvec2", "lvec3", "lvec4", "lvec3", "lvec4", "lvec4"],
]

src_types = [
    ["int", "int", "int", "ivec2", "ivec2", "ivec3"],
    ["long", "long", "long", "lvec2", "lvec2", "lvec3"],
]

extend = [
    ["0", "INT32_C(0x3f800000)", "0", "INT32_C(0xbf800000)"],
    ["0", "INT64_C(0x3ff0000000000000)", "0", "INT64_C(0xbff0000000000000)"],
]

def case_str(reverse, type, ext, mode):
    case = (reverse << 6) | (type << 5) | (ext << 3) | (mode)
    return f"case {case:04o}:"

def dst_str(type, ext, mode):
    return f"OPC({types[type][mode]})"

def cast_str(type, ext, mode):
    return f"(pr_{types[type][mode]}_t)"

def init_str(reverse, type, ext, mode):
    ext_str = extend[type][ext]
    src = f"OPA({src_types[type][mode]})"
    if mode == 0:
        if ext == 2:
            ext_str = src
        if reverse:
            return f"{ext_str}, {src}"
        else:
            return f"{src}, {ext_str}"
    elif mode == 1:
        if ext == 2:
            ext_str = src
        if reverse:
            return f"{ext_str}, {ext_str}, {src}"
        else:
            return f"{src}, {ext_str}, {ext_str}"
    elif mode == 2:
        if ext == 2:
            ext_str = src
        if reverse:
            return f"{ext_str}, {ext_str}, {ext_str}, {src}"
        else:
            return f"{src}, {ext_str}, {ext_str}, {ext_str}"
    elif mode == 3:
        if reverse:
            return f"{ext_str}, {src}[1], {src}[0]"
        else:
            return f"{src}[0], {src}[1], {ext_str}"
    elif mode == 4:
        if reverse:
            if ext == 2:
                return f"{src}[1], {src}[0], {src}[1], {src}[0]"
            else:
                return f"{ext_str}, {ext_str}, {src}[1], {src}[0]"
        else:
            if ext == 2:
                return f"{src}[0], {src}[1], {src}[0], {src}[1]"
            else:
                return f"{src}[0], {src}[1], {ext_str}, {ext_str}"
    elif mode == 5:
        if reverse:
            return f"{ext_str}, {src}[2], {src}[1], {src}[0]"
        else:
            return f"{src}[0], {src}[1], {src}[2], {ext_str}"

for reverse in range(2):
    for type in range(2):
        for ext in range(4):
            for mode in range(6): # 6, 7 are reserved
                case = case_str(reverse, type, ext, mode)
                dst = dst_str(type, ext, mode)
                cast = cast_str(type, ext, mode)
                init = init_str(reverse, type, ext, mode)
                if mode in [1, 3]:
                    print(f"{case} VectorSet({init}, {dst}); break;");
                else:
                    print(f"{case} {dst} = {cast} {{ {init} }}; break;");
