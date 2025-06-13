100 rem square =
110 open4.6.0 :rem print ascii
120 open1,6,1 :rem plot x,y data
130 open2,6,2 c= 0 :rem pen color
135 print#4:print#4,"concentric squares"
140 print#1,"m";240,-240
150 print#1,"i"
160 for i= 0 to 90 step 10
170 c=c+1: if c=>4 then c=0
180 print#2,c :rem set color
185 forj= 1to11
190 x=1 +j
200 y=1 +j
210 print#1,"r";x;-y
220 print#1,"j";x,y
230 print#1,"j";-x;y
240 print#1,"j";-x;-y
250 print#1,"j";x;-y
255 next j
260 next i
270 print#4:print#4:print#4
280 print#1,"r";0,-200
290 open7,6,7 :rem reset plotter
300 print#7
310 close4:close1:close2:close7
320 end