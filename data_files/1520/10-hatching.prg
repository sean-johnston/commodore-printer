100 rem hatching
110 open4,6,0 :rem print ascii data
120 open1,6,1 :rem plot x,y data
125 print#4:print#4:print#4,"hatching"
130 open2,6,2:print#2,2 :rem green
140 x=0:for d=2 to 15 step 2
150 print#1,"m";x,-100:x = x + 60
160 print#1,"i" :rem set origin
170 a=50:b=100
180 print#1,"j";0;b
190 print#1,"j";a;b
200 print#1,"j";a;0
210 print#1,"j";0;0
220 p1= -b:q1=b:p2=0:q2=a+b
230 gosub 360
240 if q2< d then next d:goto 320
250 print#1,"r";x1;y1
260 print#1,"r";x1;y2
270 gosub 360 
280 print#1,"r";x2;y2
290 print#1,"j";x1;y1
300 goto 230
310 print#1,"m";0;-120
320 print#4:print#4:print#4
330 open7,6,7:print#7 :rem reset plotter
340 close4:close1:close2:close7
350 end
360 p1=p1+d:q1=q1-d
370 if q1 <0 then y1=0:x1=p1:goto 390
380 y1=q1:x1=0
390 p2=p2+d:q2=q2-d
400 if p2> a then x2= a:y2 = q2:goto 420
410 x2=p2:y2=b
420 return
