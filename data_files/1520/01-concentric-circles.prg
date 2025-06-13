100 rem circle 1
110 open 4,6,0    :rem print ascii
120 open 1,6,1    :rem plot x,y data
130 open2,6,2:c=0 :rem pen color
135 print#4:print#4,"concentric circles"
140 print#1,"m";240, -240
150 print#1,"i"
160 for l=30 to 180 step 30
170 c=c+1: if c>= 4 then c=0
180 print#2,c :rem set color
185 for z=1 to 15
190 for i=0 to 360 step 10
200 x= (z + l)*sin(i*3.1416/180)
210 y=(z+l)*cos(i*3.1416/180)
220 if i=0 then print#1,"r";x,y: goto 240
230 print#1,"j";x,y
240 next i
245 next z
250 next l
260 print#1,"r";0, -240
270 print#4:print#4:print#4
280 open7,6,7 :rem reset plotter
290 print#7
300 close4:close1:close2:close7
310 end