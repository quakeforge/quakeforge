import re
import sys
import pprint
import StringIO
import os

cvar_def_re = re.compile(r'^cvar_t\s+(\w+)\s*=\s*\{\s*("[^"]*")\s*,\s*("[^"]*")(\s*,\s*(\w+)(\s*,\s*(\w+))?)?\s*\}\s*;(\s*//\s*(.*))?\n')
cvar_reg_re = re.compile(r'Cvar_RegisterVariable\s*\(\s*&\s*(\w+)\s*\)\s*;\s*')
cvar_val_re = re.compile(r'(\w+)\s*\.\s*(name|string|value)')
cvar_set_re = re.compile(r'(Cvar_Set|Cvar_SetValue)\s*\(\s*"(\w+)"\s*,\s*')

class cvar:
	def __init__(self, cv_def):
		(self.c_name,
		 self.name,
		 self.default,
		 self.arch,
		 self.serv,
		 self.desc) = cv_def
		if self.arch==None or self.arch=='false' or self.arch=='0':
			self.arch = 0
		elif self.arch=='true' or self.arch=='1':
			self.arch = 1
		if self.serv==None or self.serv=='false' or self.serv=='0':
			self.serv = 0
		elif self.serv=='true' or self.serv=='1':
			self.serv = 1
		if self.desc==None:
			self.desc='None'
	def __repr__(self):
		return 'cvar(('+`self.c_name`+','+\
				`self.name`+','+\
				`self.default`+','+\
				`self.arch`+','+\
				`self.serv`+','+\
				`self.desc`+')'
	def __str__(self):
		return self.__repr__()
	def get(self):
		str=StringIO.StringIO()
		str.write(self.c_name)
		str.write(' = Cvar_Get(')
		str.write(self.name)
		str.write(', ')
		str.write(self.default)
		str.write(', ')
		if self.arch and self.serv:
			str.write('CVAR_ARCHIVE|CVAR_SERVERINFO')
		elif self.arch:
			str.write('CVAR_ARCHIVE')
		elif self.serv:
			str.write('CVAR_SERVERINFO')
		else:
			str.write('CVAR_NONE')
		str.write(', "')
		str.write(self.desc)
		str.write('");')
		return str.getvalue()


cvar_dict = {}
files = {}

def get_cvar_defs(fname):
	fi=open(fname,'rt')
	f=files[fname]=fi.readlines()
	for i in range(len(f)):
		cv=cvar_def_re.match(f[i])
		if cv!=None:
			cv=cv.groups()
			cv_def=cvar(cv[:3]+(cv[4],)+(cv[6],)+(cv[8],))
			if cvar_dict.has_key(cv_def.c_name):
				x=cvar_dict[cv_def.c_name]
				cv_def.arch = cv_def.arch or x.arch
				cv_def.serv = cv_def.serv or x.serv
				if cv_def.desc=='None':
					cv_def.desc=x.desc
				if `cv_def`!=`x`:
					print 'old: '+`cvar_dict[cv_def.c_name]`
					print 'new: '+`cv_def`
			cvar_dict[cv_def.c_name]=cv_def
			f[i]='cvar_t\t*'+cv_def.c_name+';'
	fi.close()

for f in sys.argv[1:]:
	get_cvar_defs(f)
for file in files.items():
	fname,f=file
	print fname
	for i in range(len(f)):
		reg=cvar_reg_re.search(f[i])
		while reg:
			cv=reg.groups(1)[0]
			s,e=reg.start(0),reg.end(0)
			try:
				f[i]=f[i][:s]+cvar_dict[cv].get()+f[i][e:]
				reg=cvar_reg_re.search(f[i])
			except KeyError:
				reg=None
		if f[i][-1]!='\n':
			f[i]=f[i]+'\n'
		val=cvar_val_re.search(f[i])
		while val:
			cv,field=val.group(1,2)
			s,e=val.start(0),val.end(0)
			if cvar_dict.has_key(cv):
				f[i]=f[i][:s]+cv+'->'+field+f[i][e:]
				val=cvar_val_re.search(f[i],e)
			else:
				val=None
		set=cvar_set_re.search(f[i])
		while set:
			func,cv=set.group(1,2)
			s,e=set.start(0),set.end(0)
			if cvar_dict.has_key(cv):
				f[i]=f[i][:s]+func+'('+cv+', '+f[i][e:]
				set=cvar_val_re.search(f[i],e)
			else:
				set=None
	os.rename(fname,fname+'.bak')
	fi=open(fname,'wt')
	fi.writelines(f)
	fi.close()
