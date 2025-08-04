from struct import *

def queue(x):
    start = (x >>  0) & ((1 << 16) - 1)
    count = (x >> 16) & ((1 << 16) - 1)
    print(f"start:{start}")
    print(f"end  :{start + count}")

def id_data(x):
    mat_id = (x >>  0) & ((1 << 14) - 1)
    map_id = (x >> 14) & ((1 <<  5) - 1)
    layer  = (x >> 19) & ((1 << 11) - 1)
    nostyle= (x >> 31) & ((1 <<  1) - 1)
    print(f"mat_id :{mat_id}")
    print(f"map_id :{map_id}")
    print(f"layer  :{layer}")
    print(f"nostyle:{nostyle}")

def cone(x):
    adot  = unpack("h", pack("H", (x >>  0) & ((1 << 16) - 1)))[0]
    blend = unpack("h", pack("H", (x >> 16) & ((1 << 16) - 1)))[0]
    print(f"adot :{adot/32767}")
    print(f"blend:{blend/32767}")
