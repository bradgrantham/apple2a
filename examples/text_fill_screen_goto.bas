10 HOME
20 CH = 65+128
30 AD = 1024
40 POKE AD,CH
50 AD = AD + 1
60 IF AD < 2048 GOTO 40
70 CH = CH + 1
80 IF CH < 70+128 GOTO 30
