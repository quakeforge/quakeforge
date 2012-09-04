# vim:ts=4:et

from struct import unpack, pack

def read_byte(file, count=1):
    size = 1 * count
    data = file.read(size)
    data = unpack("<%dB" % count, data)
    if count == 1:
        return data[0]
    return data

def read_int(file, count=1):
    size = 4 * count
    data = file.read(size)
    data = unpack("<%di" % count, data)
    if count == 1:
        return data[0]
    return data

def read_string(file, size):
    data = file.read(size)
    s = ""
    for c in data:
        s = s + chr(c)
    return s

class WadFile:
    class LumpInfo:
        def __init__(self, filepos, disksize, size, type, compression):
            self.filepos = filepos
            self.disksize = disksize
            self.size = size
            self.type = type
            self.compression = compression
        pass
    class MipTex:
        def __init__(self, data):
            width, height = unpack("<ii", data[16:24])
            self.width, self.height = width, height
            offsets = unpack("<4i", data[24:40])
            self.texels = [None] * 4
            self.texels[0] = data[offsets[0]:offsets[0] + width * height]
            self.texels[1] = data[offsets[1]:offsets[1] + width * height >> 2]
            self.texels[2] = data[offsets[2]:offsets[2] + width * height >> 4]
            self.texels[3] = data[offsets[3]:offsets[3] + width * height >> 6]

    def __init__(self):
        self.lumps = {}
    @classmethod
    def load(cls, path):
        f = open(path,"rb")
        id = read_string(f, 4)
        numlumps = read_int(f)
        infotableofs = read_int(f)
        if id == 'WAD2':
            wf = WadFile()
            wf.path = path
            f.seek(infotableofs, 0)
            for i in range(numlumps):
                filepos, disksize, size = read_int (f, 3)
                type, compression, pad1, pad2 = read_byte(f, 4)
                name = read_string(f, 16)
                name = name.split("\0")[0]
                name = name.lower()
                wf.lumps[name] = WadFile.LumpInfo(filepos, disksize, size,
                                                  type, compression)
            return wf
        return None
    def getLump(self, name):
        return self.lumps[name.lower()]
    def getData(self, name):
        lump = self.getLump(name)
        f = open(self.path, "rb")
        f.seek(lump.filepos, 0)
        data = f.read(lump.disksize)
        if lump.type == 68:
            return WadFile.MipTex(data)
        else:
            return data
        return data
