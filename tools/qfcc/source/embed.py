import sys
import re
import subprocess

outind = sys.argv.index("-o")
flags = sys.argv[1:outind]
if "-include" in flags:
    ind = flags.index("-include")
    flags = flags[:ind] + flags[ind + 2:]
args = sys.argv[outind:]

outfile = args[1]
infile = args[-1]

cmd = flags + ["-E","-"]

def escape(m):
    return f'\\{m[0]}'

escape_re = re.compile(r'["\\]')
STRING = r'\s*"((\\.|[^"\\])*)"\s*'
NUM=r'\s*([0-9]+)'
linemarker = re.compile(r'\s*#' + NUM + STRING + NUM + ".*")

pipe = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
pipe.stdin.write(bytes(f'#include "{infile}"\n',"utf-8"))
pipe.stdin.flush()
pipe.stdin.close()
lines=[]
dep=None
while True:
    l=pipe.stdout.readline()
    if not l:
        break
    l = "".join(map(lambda b: chr(b), l))
    l = l.rstrip()
    if l[:1] == '#' and infile in l:
        m = linemarker.match(l)
        if m[4] == "1":
            dep = m[2]
    if l and l[0] != '#':
        l = escape_re.sub(escape, l)
        lines.append(f'"{l}"\n')
output = open(outfile, "wt")
output.writelines(lines)
output.close()
