from struct import *
from pprint import *
import sys

sprite_types = [
    "vp parallel upright",
    "facing upright",
    "vp parallel",
    "oriented",
    "vp parallel oriented",
]

def read_frame(frame, sprite, pref=""):
    x,y,w,h = unpack("4i", sprite[:4*4])
    sprite = sprite[4*4:]
    print(f"{pref}[{x},{y},{w},{h}]");
    return sprite[w*h:]

def read_group(frame, sprite):
    numFrames = unpack("<i", sprite[:4])[0]
    sprite = sprite[4:]
    intervals = unpack(f"<{numFrames}f", sprite[:4*numFrames])
    sprite = sprite[4*numFrames:]
    for i in range(numFrames):
        sprite = read_frame (i, sprite, f"{i} {intervals[i]} ")
    return sprite

for arg in sys.argv[1:]:
    print(arg)
    sprite = open(arg, "rb").read()
    s = unpack("<4s i i f 2i i f i", sprite[:9*4])
    sprite = sprite[9*4:]

    print(f"id     : {s[0]}")
    print(f"version: {s[1]}")
    print(f"type   : {sprite_types[s[2]]}")
    print(f"radius : {s[3]}")
    print(f"extent : {(s[4],s[5])}")
    print(f"frames : {s[6]}")
    print(f"beam l : {s[7]}")
    print(f"sync   : {s[8]}")
    numFrames = s[6]

    for i in range(numFrames):
        type = unpack("<i", sprite[:4])[0]
        sprite = sprite[4:]
        if type == 0:
            sprite = read_frame (i, sprite)
        else:
            sprite = read_group (i, sprite)
    print()
