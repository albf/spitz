import subprocess
import sys
import time 

p = subprocess.Popen(["./spitz", "3", "127.0.0.1", "prime.so", "30"],
                     stdout=subprocess.PIPE,
		     #stderr=subprocess.PIPE,
                     cwd="/home/alexandre/Codes/spitz/examples")

print "Starting Debug Print"
print "-----"
while 1:
	#for ln in iter(p.stdout.readline, ''):
	ln = p.stdout.readline()
	if ln.startswith("[STATUS] "):
		print ln
		break
	print "DONE READING LINE"
	time.sleep(1)
print "-----"
print "Ending Debug Print"

ln = ln[9:]
print ln

lnlist = ln.split(';')
fnlist = []
for item in lnlist:
	fnlist.append(item.split('|'))

ln = ln[9:]
lnlist = ln.split(';')
fnlist = [] 
for item in lnlist:
	fnlist.append(item.split('|'))

fnlist.pop()

print fnlist
for line in fnlist:
	i = 0
	for value in line:
		print value










