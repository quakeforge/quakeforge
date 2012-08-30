# vim:ts=4:et

from mathutils import Vector

from .script import Script

class Entity:
    def __init__(self):
        self.d = {}
        self.b = []
        pass

class Texinfo:
    def __init__(self, script, plane):
        self.name = script.getToken()
        script.getToken()
        if script.token == "[":
            hldef = True
            self.s_vec = parse_vector(script)
            self.s_offs = float(script.getToken())
            if script.getToken() != "]":
                map_error(script, "Missing ]")
            if script.getToken() != "[":
                map_error(script, "Missing [")
            self.t_vec = parse_vector(script)
            self.t_offs = float(script.getToken())
            if script.getToken() != "]":
                map_error(script, "Missing ]")
        else:
            hldef = False
            self.s_vec, self.t_vec = texture_axis_from_plane(plane)
            self.s_offs = float(script.token)
            self.t_offs = float(script.getToken())
        self.rotate = float(script.getToken())
        self.scale = [0, 0]
        self.scale[0] = float(script.getToken())
        self.scale[1] = float(script.getToken())

baseaxis = (
    (Vector((0,0, 1)), (Vector((1,0,0)), Vector((0,-1,0)))),    #floor
    (Vector((0,0,-1)), (Vector((1,0,0)), Vector((0,-1,0)))),    #ceiling
    (Vector(( 1,0,0)), (Vector((0,1,0)), Vector((0,0,-1)))),    #west wall
    (Vector((-1,0,0)), (Vector((0,1,0)), Vector((0,0,-1)))),    #east wall
    (Vector((0, 1,0)), (Vector((1,0,0)), Vector((0,0,-1)))),    #south wall
    (Vector((0,-1,0)), (Vector((1,0,0)), Vector((0,0,-1))))     #north wall
)

def texture_axis_from_plane(plane):
    best = 0
    bestaxis = 0
    for i in range(6):
        dot = plane[0].dot(baseaxis[i][0])
        if dot > best:
            best = dot
            bestaxis = i
    return baseaxis[bestaxis][1]

def clip_poly(poly, plane, keepon):
    new_poly = []
    last_dist = poly[-1].dot(plane[0]) - plane[1]
    last_point = poly[-1]
    for point in poly:
        dist = point.dot(plane[0]) - plane[1]
        if dist * last_dist < -1e-6:
            #crossed the plane
            frac = last_dist / (last_dist - dist)
            new_poly.append(last_point + frac * (point - last_point))
        if dist < -1e-6 or (dist < 1e-6 and keepon):
            new_poly.append(point)
        last_point = point
        last_dist = dist
    return new_poly

def clip_plane(plane, clip_planes):
    s, t = texture_axis_from_plane(plane)
    t = plane[0].cross(s)
    t.normalize()
    s = t.cross(plane[0])
    s *= 1e4
    t *= 1e4
    o = plane[0] * plane[1]
    poly = [o + s + t, o + s - t, o - s - t, o - s + t]     #CW
    for p in clip_planes:
        poly = clip_poly(poly, p, True)
    return poly

def convert_planes(planes):
    verts = []
    faces = []
    for i in range(len(planes)):
        poly = clip_plane(planes[i], planes[:i] + planes[i + 1:])
        face = []
        for v in poly:
            ind = len(verts)
            for i in range(len(verts)):
                d = verts[i] - v
                if d.dot(d) < 1e-6:
                    ind = i
                    break
            if ind == len(verts):
                verts.append(v)
            face.append(ind)
        faces.append(face)
    return verts, faces

def parse_vector(script):
    v = (float(script.getToken()), float(script.getToken()),
         float(script.getToken()))
    return v

def parse_verts(script):
    if script.token != ":":
        map_error(script, "Missing :")
    script.getToken()
    numverts = int(script.token)
    verts = []
    for i in range(numverts):
        script.tokenAvailable(True)
        verts.append(parse_vector(script))
    return verts

def parse_brush(script, mapent):
    verts = []
    faces = []
    planes = []
    texdefs = []
    planepts = [None] * 3
    if script.getToken(True) != "(":
        verts = parse_verts(script)
    else:
        script.ungetToken()
    while True:
        if script.getToken(True) in [None, "}"]:
            break
        if verts:
            n_v = int(script.token)
            face = [None] * n_v
            if script.getToken() != "(":
                map_error(script, "Missing (")
            for i in range(n_v):
                script.getToken()
                face[i] = int(script.token)
                if i < 3:
                    planepts[i] = Vector(verts[face[i]])
            if script.getToken() != ")":
                map_error(script, "Missing )")
            faces.append(face)
        else:
            for i in range(3):
                if i != 0:
                    script.getToken(True)
                if script.token != "(":
                    map_error(script, "Missing (")
                planepts[i] = Vector(parse_vector(script))
                script.getToken()
                if script.token != ")":
                    map_error(script, "Missing )")
        t1 = planepts[0] - planepts[1]
        t2 = planepts[2] - planepts[1]
        norm = t1.cross(t2)
        norm.normalize()
        plane = (norm, planepts[1].dot(norm))
        planes.append(plane)
        tx = Texinfo(script, plane)
        detail = False
        while script.tokenAvailable():
            script.getToken()
            if script.token == "detail":
                detail = True
            else:
                map_error(script, "invalid flag")
    if not verts:
        verts, faces = convert_planes(planes)
    mapent.b.append((verts,faces))

def parse_epair(script, mapent):
    key = script.token
    script.getToken()
    value = script.token
    mapent.d[key] = value

def parse_entity(script):
    if script.getToken(True) == None:
        return False
    if script.token != "{":
        map_error(script, "Missing {")
    mapent = Entity()
    while True:
        if script.getToken(True) == None:
            map_error(script, "EOF without closing brace")
        if script.token == "}":
            break
        if script.token == "{":
            parse_brush(script, mapent)
        else:
            parse_epair(script, mapent)
    return mapent

class MapError(Exception):
    def __init__(self, fname, line, message):
        Exception.__init__(self, "%s:%d: %s" % (fname, line, message))
        self.line = line

def map_error(self, msg):
    raise MapError(self.filename, self.line, msg)

def parse_map(filename):
    text = open(filename, "rt").read()
    script = Script(filename, text)
    script.error = map_error
    entities = []
    while True:
        ent = parse_entity(script)
        if not ent:
            break
        entities.append(ent)
