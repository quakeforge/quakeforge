# vim:ts=4:et
from .script import Script

MAX_FLAGS = 8

class EntityClass:
    def __init__(self, text, filename, line = 0):
        script = Script(filename, text)
        if line:
            script.line = line
        script.getToken()   # skip over the leading '/*QUAKED'
        self.name = script.getToken()
        self.color = self.parse_vector(script)
        self.size = self.parse_size(script)
        self.flagnames = self.parse_flags(script)
        self.comment = self.extract_comment(script)
    def parse_vector(self, script):
        if script.getToken() != "(":
            raise SyntaxError
        v = (float(script.getToken()), float(script.getToken()),
             float(script.getToken()))
        if script.getToken() != ")":
            raise SyntaxError
        return v
    def parse_size(self, script):
        if script.getToken() == "?":
            return None                 # use brush size
        script.ungetToken()
        return self.parse_vector(script), self.parse_vector(script)
    def parse_flags(self, script):
        flagnames = []
        while script.tokenAvailable():
            #any remaining words on the line are flag names, but only MAX_FLAGS
            #names are kept.
            script.getToken()
            if len(flagnames) < MAX_FLAGS:
                flagnames.append(script.token)
        return tuple(flagnames)
    def extract_comment(selk, script):
        if not script.tokenAvailable(True):
            return ""
        start = pos = script.pos
        while pos < len(script.text) and script.text[pos:pos + 2] != "*/":
            if script.text[pos] == "\n":
                script.line += 1
            pos += 1
        comment = script.text[start:pos]
        if pos < len(script.text):
            pos += 2
        script.pos = pos
        return comment

import os

def scan_source(fname, entity_classes):
    text = open(fname, "rt").read()
    line = 1
    pos = 0
    while pos < len(text):
        if text[pos:pos + 8] == "/*QUAKED":
            start = pos
            start_line = line
            while pos < len(text) and text[pos:pos + 2] != "*/":
                if text[pos] == "\n":
                    line += 1
                pos += 1
            if pos < len(text):
                pos += 2
            ec = EntityClass(text[start:pos], fname, start_line)
            entity_classes[ec.name] = ec
            print(ec.name)
        else:
            if text[pos] == "\n":
                line += 1
            pos += 1

def scan_directory(path, entity_classes):
    files = os.listdir(path)
    files.sort()

    for f in files:
        if f[0] in [".", "_"]:
            continue
        if os.path.isdir(os.path.join(path, f)):
            build_entityclasses(os.path.join(path, f), entity_classes)
        else:
            if f[-3:] == ".qc":
                scan_source(os.path.join(path, f), entity_classes)

def build_entityclasses(path):
    entity_classes = {}
    scan_directory(path, entity_classes)
    return entity_classes
