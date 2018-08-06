import random
import math

str = """%(init)d00 DX(%(which)d) = %(x)d
%(init)d05 DY(%(which)d) = %(y)d
%(init)d10 C(%(which)d) = %(c)d
%(init)d15 T(%(which)d) = %(t)d"""

ts = 40

print "10 GR"
print "20 TS = %d" % ts
print "30 DIM DX(9),DY(9),C(9),X(9),Y(9),T(9)"

for i in range(0,10) :
    angle = random.uniform(0,.999)
    x = int(20 * math.cos(angle * 3.14159 * 2))
    y = int(20 * math.sin(angle * 3.14159 * 2))
    c = random.randrange(1,15)
    t = random.randrange(0,ts - 1)
    print str % {'init' : 10 + i, 'x' : x, 'y' : y, 'c' : c, 'which' : i, 't' : t}

print "2200 I = 0"
print "2210 OX = X(I) : OY = Y(I)"
print "2220 X(I) = 20 + DX(I) * T(I) / %d" % ts
print "2230 Y(I) = 20 + DY(I) * T(I) / %d" % ts
print "2235 T(I) = T(I) + 1 : IF T(I) = TS THEN T(I) = 0"
print "2240 COLOR=0 : PLOT OX, OY"
print "2250 COLOR=C(I) : PLOT X(I), Y(I)"
print "2260 I = I + 1 : IF I < 10 GOTO 2210"
print "2300 GOTO 2200"
