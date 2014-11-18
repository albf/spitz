import subprocess
import sys
import time 

p = subprocess.Popen(["./spitz", "0", "127.0.0.1", "prime.so", "30"],
                     stdout=subprocess.PIPE,
		     #stderr=subprocess.PIPE,
                     cwd="/home/alexandre/Codes/spitz/examples")

print "Starting Debug Print"
print "-----"
while 1:
	#for ln in iter(p.stdout.readline, ''):
	ln = p.stdout.readline()
	print ln
	print "DONE READING LINE"
	time.sleep(1)
print "-----"
print "Ending Debug Print"
