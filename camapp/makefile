#****************************************************************************/
#  makefile for Cameraderie
#
#  - tools used by the author:  gnu make 3.81, gcc 3.3.5, ilink 5.0, rc 5.0
#
#****************************************************************************/
#
#* ***** BEGIN LICENSE BLOCK *****
#* Version: MPL 1.1
#*
#* The contents of this file are subject to the Mozilla Public License Version
#* 1.1 (the "License"); you may not use this file except in compliance with
#* the License. You may obtain a copy of the License at
#* http://www.mozilla.org/MPL/
#*
#* Software distributed under the License is distributed on an "AS IS" basis,
#* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
#* for the specific language governing rights and limitations under the
#* License.
#*
#* The Original Code is Cameraderie.
#*
#* The Initial Developer of the Original Code is Richard L. Walsh.
#* 
#* Portions created by the Initial Developer are Copyright (C) 2006,2007
#* the Initial Developer. All Rights Reserved.
#*
#* Contributor(s):
#*
#* ***** END LICENSE BLOCK ***** */
#
#****************************************************************************/

.SUFFIXES :
.SUFFIXES : .c .rc .o .res

CC = gcc
RC = rc

CFLAGS = -c -s -Zomf -Wall -O
LDFLAGS = -s -Zomf -Zexe -Zmap -Zlinker /EXEPACK:2 -Zlinker /PM:PM

# everything except modules with special includes or dependencies
OBJECTS = cammain.o camcam.o camcmd.o camcnr.o camcol.o camdata.o \
          camdrive.o camfile.o camgrp.o camparse.o camsave.o camshow.o \
          camsort.o camstrings.o camthumb.o camutil.o

LIBS = ../usbcalls/usbcalls.lib ../camlibusb/camlibusb.lib ../jpg/libjpeg.lib

all : camera.exe camapp.res
	$(RC) -n -x2 camapp.res camera.exe

camapp.res : camapp.rc camapp.dlg camapprc.h \
             camnls.rc camnls.h cam_en.txt makefile
	$(RC) -n -r camapp.rc

camera.exe : $(OBJECTS) camchklist.o caminfo.o camjpg.o camusb.o
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $^

caminfo.o : caminfo.c
	$(CC) $(CFLAGS) -I../camlibusb -o $@ $<

camjpg.o : camjpg.c
	$(CC) $(CFLAGS) -I../jpg -o $@ $<

camusb.o : camusb.c
	$(CC) $(CFLAGS) -I../camlibusb -I../usbcalls -o $@ $<

camcam.o camcmd.o camcnr.o camcol.o camdata.o camdrive.o camgrp.o caminfo.o \
cammain.o camsave.o camshow.o camsort.o camstrings.o camthumb.o camutil.o : \
    camapp.h camapprc.h camnls.h makefile

camcam.o camdata.o camfile.o caminfo.o camjpg.o camparse.o \
camsave.o camthumb.o camusb.o: \
    camusb.h camcodes.h makefile

camdata.o camfile.o camjpg.o camsave.o camstrings.o camthumb.o: \
    cammisc.h makefile

camfile.o camstrings.o : \
    camcodes.h makefile

camchklist.o camcol.o camgrp.o : \
    camchklist.h makefile

