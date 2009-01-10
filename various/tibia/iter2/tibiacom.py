import httplib
import re
import urllib

def __decode_html(string):
	from htmlentitydefs import name2codepoint as n2cp
	import re

	def substitute_entity(match):
		ent = match.group(2)
		if match.group(1) == "#":
			return chr(int(ent))
		else:
			cp = n2cp.get(ent)

		if cp:
			return unichr(cp)
		else:
			return match.group()

	def decode_entity(string):
		entity_re = re.compile("&(#?)(\d{1,5}|\w{1,8});")
		return entity_re.subn(substitute_entity, string)[0]

	return decode_entity(string).replace("\xa0", " ")

conn = httplib.HTTPConnection("www.tibia.com")

def http_get(url, params):
	conn.request("GET", url + "?" + urllib.urlencode(params))
	resp = conn.getresponse()
	return resp.read()

def __ci_info(html):
	FIELDS = (
			("name", "Name", lambda x: __decode_html(x)),
			("sex", "Sex", None),
			("vocation", "Profession", None),
			("level", "Level", None),
			("world", "World", None),
			("residence", "Residence", None),
			("guild", "Guild&#160;membership", lambda x: __decode_html(re.search(r"<A.+?>([^<]+)</A>", x).group(1))),
			("last login", "Last login", lambda x: __decode_html(x)),
			("account status", "Account&#160;Status", None))
	info = {}
	for a in FIELDS:
		hits = re.findall(
				r"<TR.*?><TD.*?>%s:</TD><TD>(.+?)</TD></TR>" % a[1],
				html)
		if len(hits) == 0:
			info[a[0]] = None
		elif len(hits) == 1:
			info[a[0]] = a[2](hits[0]) if a[2] != None else hits[0]
		else:
			assert False
	return info

def __ci_deaths(html):

	def slayer_tuple(group):
		assert len(group) == 2
		if group[0] == None:
			assert group[1] != None
			return (False, group[1])
		else:
			assert group[1] == None
			return (True, __decode_html(group[0]))

	# navigate to start of deaths table
	m = re.search("""<TABLE.*?>Character Deaths.*?</TR>""", html)
	if m == None: return None
	pos = m.end()

	CHAR_OR_MONSTER_PATTERN = """(?:<A.*?>([^<]*)</A>|([^<]*))"""
	# time, level, killer
	kill1_re = re.compile(
			"""<TR.*?><TD.*?>([^<]+)</TD><TD>\S+ at Level (\d+) by """
			+ CHAR_OR_MONSTER_PATTERN
			+ """</TD></TR>""")
	# killer
	kill2_re = re.compile(
			"""\s*<TR.*?><TD.*?></TD><TD>and by """
			+ CHAR_OR_MONSTER_PATTERN
			+ """</TD></TR>""")

	deaths = []
	while True:
		mo = kill1_re.search(html, pos)
		if mo is None: break
		time = __decode_html(mo.group(1))
		level = mo.group(2)
		death = [time, level, slayer_tuple(mo.group(3, 4)), None]
		pos = mo.end()
		mo2 = kill2_re.match(html, pos)
		if mo2 is not None:
			death[3] = slayer_tuple(mo2.group(1, 2))
			pos = mo2.end()
		deaths.append(tuple(death))

	return deaths

def char_info(name):
	html = http_get("/community/", {"subtopic": "characters", "name": name})
	try: rv = __ci_info(html)
	except Exception, a:
		print "name:", name
		raise
	assert not rv.has_key("deaths")
	rv["deaths"] = __ci_deaths(html)
	return rv

def pretty_print_char_info(info):
	simple = info.keys()
	simple.remove("deaths")
	for k in simple:
		print k + ":", info[k]
	a = info["deaths"]
	if a != None:
		for b in a:
			print b[0] + ":",
			print "killed" if b[2][0] else "died", "at Level", b[1],
			print "by", b[2][1]
			if b[3] is not None:
				print "\tand by", b[3][1]

def online_list(world):
	html = http_get("/community/", {"subtopic": "whoisonline", "world": world})
	FIELDS = (
			("name", lambda x: __decode_html(x)),
			("level", lambda x: x),
			("vocation", lambda x: x))
	players = []
	row_re = re.compile("""<TR BGCOLOR=#[A-F0-9]+><TD WIDTH=\d+%><A HREF="http://www.tibia.com/community/\?subtopic=characters&name=[^"]+">([^<]+)</A></TD><TD WIDTH=\d+%>(\d+)</TD><TD WIDTH=\d+%>([^<]+)</TD></TR>""")
	for a in row_re.finditer(html):
		assert len(a.groups()) == len(FIELDS)
		players.append(dict([(FIELDS[b][0], FIELDS[b][1](a.group(b+1))) for b in range(len(FIELDS))]))
	assert int(re.search(r"Currently (\d+) players are online\.", html).group(1)) == len(players)
	return players
