import re
import sys
from pprint import pprint
from io import StringIO
from textwrap import TextWrapper
import os

STRING = r'\s*"((\\.|[^"\\])*)"\s*'
QSTRING = r'\s*("(\\.|[^"\\])*")\s*'
ID = r'([a-zA-Z_][a-zA-Z_0-9]*)'
STORE = r'(VISIBLE\s+|static\s+|extern\s+|)'
FIELD = r'(string|value|int_val|vec)\b'
FLAGS = r'\s*(' + ID + r'\s*(\|\s*' + ID + r')*)\s*'
TYPE = r'(cvar_t|struct cvar_s)\s*\*'
STUFF = r'\s*(.*)\s*'
cvar_decl_re = re.compile(r'^' + STORE + TYPE + ID + r';.*')
cvar_get_re = re.compile(r'.*\bCvar_Get\s*\(\s*"')
cvar_get_join_prev_re = re.compile(r'^\s*Cvar_Get\s*\(\s*".*')
cvar_get_assign_re = re.compile(r'\s*' + ID + r'\s*=\s*$')
cvar_get_incomplete_re = re.compile(r'.*Cvar_Get\s*\(\s*"[^;]*$')
cvar_get_complete_re = re.compile(r'.*Cvar_Get\s*\(\s*".*\);.*$')
cvar_create_re = re.compile(r'\s*(\w+)\s*=\s*Cvar_Get\s*\('
    + STRING
    + ',(' + STRING + '|\s*(.*)\s*),' + FLAGS + r',\s*([^,]+),\s*' + STUFF + r'\);.*')
cvar_use_re = re.compile(ID + r'\s*\->\s*' + FIELD)
cvar_listener_re = re.compile(ID + r'\s*\(\s*cvar_t\s*\*' + ID + '\s*\)\s*')
string_or_id_re = re.compile(f'({ID}|{QSTRING})')
cvar_setrom_re = re.compile(r'\s*Cvar_SetFlags\s*\(\s*' + ID + '.*CVAR_ROM\);.*')
id_re = re.compile(f'\\b{ID}\\b')

#cvar_decl_re = re.compile(r'^cvar_t\s+(\w+)\s*=\s*\{\s*("[^"]*")\s*,\s*("[^"]*")(\s*,\s*(\w+)(\s*,\s*(\w+))?)?\s*\}\s*;(\s*//\s*(.*))?\n')
cvar_set_re = re.compile(r'^(\s+)(Cvar_Set|Cvar_SetValue)\s*\(' + ID + ',\s*(.*)\);(.*)')

wrapper = TextWrapper(width = 69, drop_whitespace = False,
                      break_long_words = False)

class cvar:
    def __init__(self, c_name, name, default, flags, callback, description):
        self.c_name = c_name
        self.name = name
        self.default = default
        self.flags = flags
        if callback in ['NULL', '0']:
            callback = 0
        self.callback = callback
        if description in ['NULL', '0', '']:
            description = 'FIXME no description'
        self.desc = description
        if self.desc==None:
            self.desc = 'None'
        self.data = 0
        self.files = []
        self.type = None
        self.c_type = None
        self.used_field = None
        self.uses = []
    def guess_type(self):
        if self.used_field == "value":
            self.type = "&cexpr_float"
            self.c_type = "float "
        elif self.used_field == "int_val":
            self.type = "&cexpr_int"
            self.c_type = "int "
        elif self.used_field == "vec":
            self.type = "&cexpr_vector"
            self.c_type = "vec4f_t "
        elif self.used_field == "string":
            self.type = 0
            self.c_type = "char *"
        else:
            self.type = "0/* not used */"
            self.c_type = "char *"
    def add_file(self, file, line):
        self.files.append((file, line))
    def add_use(self, field, file, line):
        if self.used_field and self.used_field != field:
            print(f"{file}:{line}: cvar {self.name} used inconsistently")
        self.used_field = field
        self.uses.append((field, file, line))
    def __repr__(self):
        return f"cvar(({self.c_name}, {self.name}, {self.default}, {self.flags}, {self.callback}, {self.desc})"
    def __str__(self):
        return self.__repr__()
    def struct(self):
        A = [
            f'static cvar_t {self.c_name}_cvar = {{\n',
            f'\t.name = "{self.name}",\n',
            f'\t.description =\n',
        ]
        B = [
            f'\t.default_value = "{self.default}",\n',
            f'\t.flags = {self.flags},\n',
            f'\t.value = {{ .type = {self.type}, .value = &{self.c_name} }},\n',
            f'}};\n',
        ]
        desc = []
        dstrings = [m[0].strip() for m in string_or_id_re.findall(self.desc)]
        def wrap(string, comma = ""):
            if string[0] == '"' and string != '""':
                string = string[1:-1]
                dlines = wrapper.wrap(string)
                desc.extend([f'\t\t"{l}"\n' for l in dlines[:-1]])
                desc.append(f'\t\t"{dlines[-1]}"{comma}\n')
            else:
                desc.append(f'\t\t{string}{comma}\n')
        for dstring in dstrings[:-1]:
            wrap (dstring)
        wrap(dstrings[-1], ",")
        return A + desc + B
    def var(self, storage):
        if storage:
            storage += " "
        return [f'{storage}{self.c_type}{self.c_name};\n']
    def set_rom(self):
        if self.flags == 'CVAR_NONE':
            self.flags = 'CVAR_ROM'
        else:
            self.flags = '|'.join([self.flags, 'CVAR_ROM'])
    def register(self):
        return [f'\tCvar_Register (&{self.c_name}_cvar, {self.callback}, {self.data});\n']


cvar_decls = {}
cvar_dict = {}
cvar_sets = []
cvar_rom = set()
cvar_listeners = {}
cvar_listeners_done = set()
cvar_listeners_multi = set()
files = {}
modified = set()

cvar_struct = [
"\tconst char *name;\n",
"\tconst char *description;\n",
"\tconst char *default_value;\n",
"\tunsigned    flags;\n",
"\texprval_t   value;\n",
"\tint       (*validator) (const struct cvar_s *var);\n"
"\tstruct cvar_listener_set_s *listeners;\n"
"\tstruct cvar_s *next;\n"
]
cvar_reg = "void Cvar_Register (cvar_t *var, cvar_listener_t listener, void *data);\n"
cvar_cexpr = '#include "QF/cexpr.h"\n'
cvar_set = 'void Cvar_Set (const char *var, const char *value);\n'
cvar_setvar = 'void Cvar_SetVar (cvar_t *var, const char *value);\n'
cvar_info = 'struct cvar_s;\nvoid Cvar_Info (void *data, const struct cvar_s *cvar);\n'
nq_cl_cvar_info = 'nq/include/client.h'
nq_sv_cvar_info = 'nq/include/server.h'
qw_cl_cvar_info = 'qw/include/client.h'
qw_sv_cvar_info = 'qw/include/server.h'

cvar_file = "include/QF/cvar.h"
line_substitutions = [
    (cvar_file, (40, 60), cvar_struct),
    (cvar_file, (96, 100), cvar_reg),
    (cvar_file, (35, 35), cvar_cexpr),
    (cvar_file, (109, 110), cvar_set),
    (cvar_file, (110, 111), cvar_setvar),
    (nq_cl_cvar_info, (294,295), cvar_info),
    (nq_sv_cvar_info, (300,301), cvar_info),
    (qw_cl_cvar_info, (296,297), cvar_info),
    (qw_sv_cvar_info, (634,635), cvar_info),
]

def get_cvar_defs(fname):
    fi = open(fname,'rt')
    f = files[fname] = fi.readlines()
    i = 0
    while i < len(f):
        cr = cvar_setrom_re.match(f[i])
        if cr:
            cvar_rom.add(cr[1])
            del(f[i])
            continue
        if cvar_get_join_prev_re.match(f[i]):
            if cvar_get_assign_re.match(f[i - 1]):
                f[i - 1] = f"{f[i - 1].rstrip()} {f[i].lstrip()}"
                del(f[i])
                i -= 1
        if cvar_get_incomplete_re.match(f[i]):
            while not cvar_get_complete_re.match(f[i]):
                f[i + 1] = f[i + 1].lstrip()
                f[i] = f[i].rstrip()
                if f[i][-1] == '"' and f[i + 1][0] == '"':
                    #string concatentation
                    f[i] = f[i][:-1] + f[i + 1][1:]
                else:
                    f[i] = f"{f[i]} {f[i + 1]}"
                del(f[i + 1])
        cd = cvar_decl_re.match(f[i])
        if cd:
            if cd[3] not in cvar_decls:
                cvar_decls[cd[3]] = []
            cvar_decls[cd[3]].append((fname, i))
        cl = cvar_listener_re.match(f[i])
        if cl:
            if cl[1] not in cvar_listeners:
                cvar_listeners[cl[1]] = []
            cvar_listeners[cl[1]].append((fname, i, cl[2]))
        cc = cvar_create_re.match(f[i])
        if cc:
            if cc.group(7):
                default = 7
            else:
                default = 5
            cc = cc.group(1, 2, default, 8, 12, 13)
            if cc[0] not in cvar_dict:
                cvar_dict[cc[0]] = cvar(*cc)
            cvar_dict[cc[0]].add_file(fname, i)
        cs = cvar_set_re.match(f[i])
        if cs:
            cvar_sets.append((fname, i))
        i += 1
    fi.close()

for f in sys.argv[1:]:
    get_cvar_defs(f)
#for cv in cvar_decls.keys():
#    if cv not in cvar_dict:
#        print(f"cvar {cv} declared but never created")
for file in files.items():
    fname,f = file
    for i in range(len(f)):
        for use in cvar_use_re.finditer(f[i]):
            if use[1] in cvar_dict and cvar_dict[use[1]]:
                cvar_dict[use[1]].add_use(use[2], fname, i)
keys = list(cvar_dict.keys())
for cv in keys:
    var = cvar_dict[cv]
    if cv not in cvar_decls or var.callback not in cvar_listeners:
        continue
    if var.callback in cvar_listeners:
        if var.callback in cvar_listeners_done:
            if not var.callback in cvar_listeners_multi:
                print(f"WARNING: {var.callback} has multiple uses")
                cvar_listeners_multi.add (var.callback)
        else:
            cvar_listeners_done.add (var.callback);
cvar_listeners_done.clear()
for cv in keys:
    var = cvar_dict[cv]
    var.guess_type()
    if var.c_name in cvar_rom:
        print(var.c_name, 'rom')
        var.set_rom()
    if cv in cvar_decls:
        if var.callback in cvar_listeners_multi:
            var.data = f'&{var.c_name}'
        for d in cvar_decls[cv]:
            decl = cvar_decl_re.match(files[d[0]][d[1]])
            storage = decl[1].strip()
            if storage == "extern":
                subst = var.var(storage)
            else:
                subst = var.var(storage)+var.struct()
            line_substitutions.append((d[0], (d[1], d[1] + 1), subst))
        for c in var.files:
            line_substitutions.append((c[0], (c[1], c[1] + 1), var.register()))
        for u in var.uses:
            # use substitutions are done immediately because they never
            # change the number of lines
            field, fname, line = u
            file = files[fname]
            places = []
            for use in cvar_use_re.finditer(file[line]):
                cv, field = use.group(1, 2)
                if cv == var.c_name:
                    places.append((use.start(), use.end()))
            places.reverse()
            for p in places:
                file[line] = file[line][:p[0]] + var.c_name + file[line][p[1]:]
                modified.add(fname)
    else:
        #for f in cvar_dict[cv].files:
        #    print(f"{f[0]}:{f[1]}: {cv} created but not kept")
        pass
for cv in keys:
    var = cvar_dict[cv]
    if cv not in cvar_decls or var.callback not in cvar_listeners:
        continue
    if var.callback in cvar_listeners_done:
        continue
    for listener in cvar_listeners[var.callback]:
        fname, line, c_name = listener
        file = files[fname]
        file[line] = f"{var.callback} (void *data," " const cvar_t *cvar)\n"
        modified.add(fname)
        cvar_listeners_done.add (var.callback);
        multi = var.callback in cvar_listeners_multi
        line += 1
        while line < len(file) and file[line] != '}\n':
            line += 1
            places = []
            for use in cvar_use_re.finditer(file[line]):
                cv, field = use.group(1, 2)
                if cv == c_name:
                    subst = f'*({var.c_type}*)data' if multi else var.c_name
                    places.append((use.start(), use.end(), subst))
            places.reverse()
            for p in places:
                file[line] = file[line][:p[0]] + p[2] + file[line][p[1]:]
                modified.add(fname)
            places = []
            for v in id_re.finditer(file[line]):
                if v[1] == c_name:
                    places.append((v.start(), v.end(), "cvar"))
            for p in places:
                file[line] = file[line][:p[0]] + p[2] + file[line][p[1]:]
                modified.add(fname)
for s in cvar_sets:
    cs = cvar_set_re.match(files[s[0]][s[1]])
    subst = None
    val = cs[4]
    if cs[2] == "Cvar_SetValue" and cs[3] in cvar_dict:
        if val[0] == '(':
            #strip off a cast
            val = val[val.index(')') + 1:].strip()
        subst = f'{cs[1]}{cs[3]} = {val};{cs[5]}\n'
    elif cs[2] == "Cvar_Set" and cs[3] in cvar_dict:
        subst = f'{cs[1]}{cs[2]} ("{cvar_dict[cs[3]].name}", {val});{cs[5]}\n'
    elif cs[2] == "Cvar_Set":
        subst = f'{cs[1]}Cvar_SetVar ({cs[3]}, {val});{cs[5]}\n'
    if subst:
        line_substitutions.append((s[0], (s[1], s[1] + 1), subst))
line_substitutions.sort(reverse=True, key=lambda s: s[1][1])
for substitution in line_substitutions:
    file, lines, subst = substitution
    modified.add(file)
    files[file][lines[0]:lines[1]] = subst
for f in files.keys():
    if f in modified:
        file = open(f, "wt")
        file.writelines(files[f])
        file.close ()
