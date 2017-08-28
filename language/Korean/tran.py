#!/usr/bin/python
 
from googletrans import Translator
import sys
import shlex
 
def main(string) :
	tr = Translator()
	i = 0
	for s in string:
		try:
			strs = shlex.split(s)
			if len(strs) > 1 and strs[0] == 'msgid' and strs[1] != "":
				trans = tr.translate(strs[1], dest='ko').text.encode('cp949')
				print s,
			elif strs[0] == 'msgstr' and trans != '':
				print strs[0], '"%s"'% trans
				trans = ''
			elif strs[0] != '': 
				print s,
			else:
				print " "
		except:
			print ""
if __name__ == "__main__" :
	if len(sys.argv) > 1:
		f = open(sys.argv[1], 'r')
		main(f.readlines())