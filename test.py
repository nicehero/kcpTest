from subprocess import Popen, PIPE
import time
p1 = Popen("./kcpTest")
p2 = Popen(["./kcpTest","1"])
while 1:
        time.sleep(0.5)
        pass
