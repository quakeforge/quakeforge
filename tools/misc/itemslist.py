#! /usr/bin/env python
import sys

class Entity:
	pass

def get_token (text):
	start = 0
	length = len (text)
	while 1:
		while start < length and text[start].isspace ():
			start += 1
		if text[:2] == '//':
			while start < length and text[start] != '\n':
				start += 1
			continue
		break
	if start == length:
		return None, None
	end = start
	if text[end] == '"':
		end += 1
		while end < length and text[end] != '"':
			end += 1
		return text[end + 1:], text[start + 1:end]
	while end < length and not text[end].isspace ():
		end += 1
	return text[end + 1:], text[start:end]

text = sys.stdin.read ()
entity_list = []
items = {
	"shells":[0,0,0,0],
	"spikes":[0,0,0,0],
	"rockets":[0,0,0,0],
	"cells":[0,0,0,0],
	"health":[0,0,0,0],
	"armor1":[0,0,0,0],
	"armor2":[0,0,0,0],
	"armorInv":[0,0,0,0],
}
artifacts = {
	"invisibility":0,
	"super_damage":0,
	"envirosuit":0,
	"invulnerability":0,
}
weapons = {
	"supershotgun":0,
	"nailgun":0,
	"supernailgun":0,
	"grenadelauncher":0,
	"rocketlauncher":0,
	"lightning":0,
}
while 1:
	text, token = get_token (text)
	if token == None:
		break
	if token != '{':
		raise ParseError
	entity = Entity ()
	while 1:
		text, key = get_token (text)
		if key == None:
			raise ParseError
		if key == '}':
			break
		text, data = get_token (text)
		if data == None:
			raise ParseError
		setattr (entity, key, data)
	if hasattr (entity, "classname"):
		entity_list.append (entity)
	if hasattr (entity, "spawnflags"):
		try:
			entity.spawnflags = int (float (entity.spawnflags))
		except ValueError:
			entity.spawnflags = 0
	else:
		entity.spawnflags = 0
for ent in entity_list:
	if ent.classname[:5] == "item_":
		item = ent.classname[5:]
		if item == "weapon":
			# officially depricated but some maps use it
			big = (ent.spawnflags & 8) != 0
			if ent.spawnflags & 4:
				items["rockets"][big] += 1
			elif ent.spawnflags & 2:
				items["spikes"][big] += 1
			elif ent.spawnflags & 1:
				items["shells"][big] += 1
		elif item[:9] == "artifact_":
			artifact = item[9:]
			artifacts[artifact] += 1
		else:
			big = ent.spawnflags & 3
			items[item][big] += 1
	elif ent.classname[:7] == "weapon_":
		weapon = ent.classname[7:]
		weapons[weapon] += 1

print "Lightning Gun    :", weapons["lightning"]
print "6 cells          :", items["cells"][0]
print "12 cells         :", items["cells"][1]
print "ammo total       :", 6 * items["cells"][0] + 12 * items["cells"][1]
print
print "Rocket Launcher  :", weapons["rocketlauncher"]
print "Grenade Launcher :", weapons["grenadelauncher"]
print "5 rocks          :", items["rockets"][0]
print "10 rockss        :", items["rockets"][1]
print "ammo total       :", 5 * items["rockets"][0] + 10 * items["rockets"][1]
print
print "Super Nailgun    :", weapons["supernailgun"]
print "Nailgun          :", weapons["nailgun"]
print "25 nails         :", items["spikes"][0]
print "50 nails         :", items["spikes"][1]
print "ammo total       :", 25 * items["spikes"][0] + 50 * items["spikes"][1]
print
print "Super Shotgun    :", weapons["supershotgun"]
print "20 shells        :", items["shells"][0]
print "40 shells        :", items["shells"][1]
print "ammo total       :", 20 * items["shells"][0] + 40 * items["shells"][1]
print
print "Red Armor        :", items["armorInv"][0]
print "Yellow Armor     :", items["armor2"][0]
print "Green Armor      :", items["armor1"][0]
print
print "Biosuit                 :", artifacts["envirosuit"]
print "Quad Dammage            :", artifacts["super_damage"]
print "Pentagram of Protection :", artifacts["invulnerability"]
print "Ring of Shadows         :", artifacts["invisibility"]
