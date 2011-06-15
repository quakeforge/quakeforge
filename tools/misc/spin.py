import sys
import time
spinner = ("('-'  )","(._.  )","( ._. )","(  ._.)","(  '-')","( '-' )")
for i in range (24):
	sys.stdout.write (spinner[i%6] + '\r')
	sys.stdout.flush ()
	time.sleep (1)
