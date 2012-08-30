# vim:ts=4:et

class Script:
    def __init__(self, filename, text, single="{}()':"):
        self.filename = filename
        self.text = text
        self.single = single
        self.pos = 0
        self.line = 1
        self.unget = False
    def tokenAvailable(self, crossline=False):
        if self.unget:
            return True
        while self.pos < len(self.text):
            while self.pos < len(self.text) and self.text[self.pos].isspace():
                if self.text[self.pos] == "\n":
                    if not crossline:
                        return False
                    self.line += 1
                self.pos += 1
            if self.pos == len(self.text):
                return False
            if self.text[self.pos] in ["\x1a", "\x04"]:
                # end of file characters
                self.pos += 1
                continue
            if self.text[self.pos:self.pos + 2] == "//":
                while (self.pos < len(self.text)
                       and self.text[self.pos] != "\n"):
                    self.pos += 1
                if self.pos == len(self.text):
                    return False
                if not crossline:
                    return False
                continue
            return True
        return False
    def getToken(self, crossline=False):
        if self.unget:
            self.unget = False
            return self.token
        if not self.tokenAvailable(crossline):
            if not crossline:
                self.error(self, "line is incomplete")
            return None
        if self.text[self.pos] == "\"":
            self.pos += 1
            start = self.pos
            if self.text[self.pos] == len(self.text):
                self.error(self, "EOF inside quoted string")
            while self.text[self.pos] != "\"":
                if self.pos == len(self.text):
                    self.error(self, "EOF inside quoted string")
                    return None
                if self.text[self.pos] == "\n":
                    self.line += 1
                self.pos += 1
            self.token = self.text[start:self.pos]
            self.pos += 1
        else:
            start = self.pos
            if self.text[self.pos] in self.single:
                self.pos += 1
            else:
                while (self.pos < len(self.text)
                       and not self.text[self.pos].isspace()
                       and self.text[self.pos] not in self.single):
                    self.pos += 1
            self.token = self.text[start:self.pos]
        return self.token
    def ungetToken(self):
        self.unget = True
