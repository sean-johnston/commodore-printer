100 rem rotate2
110 open4,6,0 :rem print ascii data
120 open1,6,1 :rem plot x,y data
125 print#4:print#4:print#4,"rotating figures"
130 open2,6,2:print#2,1 :rem blue
140 print#1,"m";240, -240
150 print#1,"i"
160 th= 3.14159 /180
170 for i= 1 to 4:read a,b:x(i) = a:y(i)= b:next i
180 for j=0 to 359 step 15
190 fori=1to4
200 xx(i) = x(i)* cos(j* th) - y(i)* sin(j* th)
210 yy(i) = x(i)*sin(j*th) + y(i)* cos(j*th)
220 if i= 1 then print#1,"r";xx(i);yy(i):goto 240
230 print#1,"j";xx(i);yy(i)
240 next i
250 print#1,"j";xx(1);yy(1)
260 next j
270 print#1,"m";0, -400
280 print#4:print#4:print#4
290 open7,6,7
300 print#7 :rem reset plotter
310 close4:close1:close2
320 end
340 data 70,80,90,130,110,130,130,80
