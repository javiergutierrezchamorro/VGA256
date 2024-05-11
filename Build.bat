@ECHO OFF
:oc20
rem *** OPENWATCOM C++ 2.0 *** 
SET WATCOM=C:\FICHEROS\OC
SET OLDPATH=%PATH%
SET PATH=%WATCOM%\BINNT;%WATCOM%\BINW;%WATCOM%\BINP;%PATH%
SET EDPATH=%WATCOM%\EDDAT
SET INCLUDE=%WATCOM%\H;%WATCOM%\MFC\INCLUDE;%WATCOM%\H\NT;INCLUDE;
SET LIB=LIB;
DEL vga256.exe
WCL386 -oneatx -ohirbk -ol -ol+ -oi -ei -zp16 -6r -fpi87 -fp6 -mf -s -ri -zm /bt=dos /l=dos32a /fhwe /"OPTION ELIMINATE" /"OPTION VFREMOVAL" Source\main.c Source\VGA256.c Source\data.c
SET WATCOM=
SET PATH=%OLDPATH%
SET EDPATH=
SET INCLUDE=
SET LIB=

:FIN
DEL *.obj
DEL *.pch
DEL *.tds
DEL *.bak
DEL *.err
DEL *.map
DEL *.sym