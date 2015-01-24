# Makefile for Independent JPEG Group's software

# This makefile is for gcc 3.3.5

# The name of your C compiler:
CC= gcc

# You may need to adjust these cc options:
CFLAGS= -s -Zomf -Wall -O

# Link-time cc options:
LDFLAGS= -s -Zomf -Zexe -Zmap -Zlinker /EXEPACK:2

# To link any special libraries, add the necessary -l commands here.
LDLIBS= 

# Put here the object file name for the correct system-dependent memory
# manager file.  For Unix this is usually jmemnobs.o, but you may want
# to use jmemansi.o or jmemname.o if you have limited swap space.
SYSDEPMEM= jmemnobs.o

# miscellaneous OS-dependent stuff
# linker
LN= $(CC)
# file deletion command
# not used because DEL returns an error if the file doesn't exist
#RM= cmd /C del
# library (.a) file creation command
AR= emxomfar r
# second step in .a creation (use "touch" if not needed)
AR2= touch

# End of configurable options.


# source files: JPEG library proper
LIBSOURCES= jcapimin.c jcapistd.c jccoefct.c jccolor.c jcdctmgr.c jchuff.c \
        jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c \
        jcphuff.c jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c \
        jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c \
        jdinput.c jdmainct.c jdmarker.c jdmaster.c jdmerge.c jdphuff.c \
        jdpostct.c jdsample.c jdtrans.c jerror.c jfdctflt.c jfdctfst.c \
        jfdctint.c jidctflt.c jidctfst.c jidctint.c jidctred.c jquant1.c \
        jquant2.c jutils.c jmemmgr.c

# memmgr back ends: compile only one of these into a working library
SYSDEPSOURCES= jmemansi.c jmemname.c jmemnobs.c jmemdos.c jmemmac.c

# source files: cjpeg/djpeg/jpegtran applications, also rdjpgcom/wrjpgcom
APPSOURCES= cjpeg.c djpeg.c jpegtran.c rdjpgcom.c wrjpgcom.c cdjpeg.c \
        rdcolmap.c rdswitch.c transupp.c rdppm.c wrppm.c rdgif.c wrgif.c \
        rdtarga.c wrtarga.c rdbmp.c wrbmp.c rdrle.c wrrle.c

SOURCES= $(LIBSOURCES) $(SYSDEPSOURCES) $(APPSOURCES)

# files included by source files
INCLUDES= jchuff.h jdhuff.h jdct.h jerror.h jinclude.h jmemsys.h jmorecfg.h \
        jpegint.h jpeglib.h jversion.h cdjpeg.h cderror.h transupp.h

# library object files common to compression and decompression
#COMOBJECTS= jcomapi.o jutils.o jerror.o jmemmgr.o $(SYSDEPMEM)
COMOBJECTS= jcomapi.o jutils.o jerror.o jmemmgr.o $(SYSDEPMEM) transupp.o

# compression library object files
CLIBOBJECTS= jcapimin.o jcapistd.o jctrans.o jcparam.o jdatadst.o jcinit.o \
        jcmaster.o jcmarker.o jcmainct.o jcprepct.o jccoefct.o jccolor.o \
        jcsample.o jchuff.o jcphuff.o jcdctmgr.o jfdctfst.o jfdctflt.o \
        jfdctint.o

# decompression library object files
DLIBOBJECTS= jdapimin.o jdapistd.o jdtrans.o jdatasrc.o jdmaster.o \
        jdinput.o jdmarker.o jdhuff.o jdphuff.o jdmainct.o jdcoefct.o \
        jdpostct.o jddctmgr.o jidctfst.o jidctflt.o jidctint.o jidctred.o \
        jdsample.o jdcolor.o jquant1.o jquant2.o jdmerge.o

# These objectfiles are included in libjpeg.a
LIBOBJECTS= $(CLIBOBJECTS) $(DLIBOBJECTS) $(COMOBJECTS)

# object files for sample applications (excluding library files)
COBJECTS= cjpeg.o rdppm.o rdgif.o rdtarga.o rdrle.o rdbmp.o rdswitch.o \
        cdjpeg.o
DOBJECTS= djpeg.o wrppm.o wrgif.o wrtarga.o wrrle.o wrbmp.o rdcolmap.o \
        cdjpeg.o
#TROBJECTS= jpegtran.o rdswitch.o cdjpeg.o transupp.o
TROBJECTS= jpegtran.o rdswitch.o cdjpeg.o

# macros to simplify dependencies (and to add the makefile as one)
BASEDEP= makefile jinclude.h jconfig.h jpeglib.h jmorecfg.h jerror.h
MOSTDEP= makefile jinclude.h jconfig.h jpeglib.h jmorecfg.h jerror.h jpegint.h
APPDEP=  makefile jinclude.h jconfig.h jpeglib.h jmorecfg.h jerror.h cdjpeg.h cderror.h

all: libjpeg.lib cjpeg.exe djpeg.exe jpegtran.exe rdjpgcom.exe wrjpgcom.exe

libjpeg.lib: $(LIBOBJECTS) makefile
	cmd /c IF EXIST libjpeg.lib DEL libjpeg.lib
	$(AR) libjpeg.lib  $(LIBOBJECTS)
#	$(AR2) libjpeg.lib

cjpeg.exe: $(COBJECTS) libjpeg.lib
	$(LN) $(LDFLAGS) -o cjpeg.exe $(COBJECTS) libjpeg.lib $(LDLIBS)

djpeg.exe: $(DOBJECTS) libjpeg.lib
	$(LN) $(LDFLAGS) -o djpeg.exe $(DOBJECTS) libjpeg.lib $(LDLIBS)

jpegtran.exe: $(TROBJECTS) libjpeg.lib
	$(LN) $(LDFLAGS) -o jpegtran.exe $(TROBJECTS) libjpeg.lib $(LDLIBS)

rdjpgcom.exe: rdjpgcom.o
	$(LN) $(LDFLAGS) -o rdjpgcom.exe rdjpgcom.o $(LDLIBS)

wrjpgcom.exe: wrjpgcom.o
	$(LN) $(LDFLAGS) -o wrjpgcom.exe wrjpgcom.o $(LDLIBS)

jconfig.h: jconfig.doc
	echo You must prepare a system-dependent jconfig.h file.
	echo Please read the installation directions in install.doc.
	exit 1


jcapimin.o: jcapimin.c $(MOSTDEP)
jcapistd.o: jcapistd.c $(MOSTDEP)
jccoefct.o: jccoefct.c $(MOSTDEP)
jccolor.o:  jccolor.c  $(MOSTDEP)
jcdctmgr.o: jcdctmgr.c $(MOSTDEP) jdct.h
jchuff.o:   jchuff.c   $(MOSTDEP) jchuff.h
jcinit.o:   jcinit.c   $(MOSTDEP)
jcmainct.o: jcmainct.c $(MOSTDEP)
jcmarker.o: jcmarker.c $(MOSTDEP)
jcmaster.o: jcmaster.c $(MOSTDEP)
jcomapi.o:  jcomapi.c  $(MOSTDEP)
jcparam.o:  jcparam.c  $(MOSTDEP)
jcphuff.o:  jcphuff.c  $(MOSTDEP) jchuff.h
jcprepct.o: jcprepct.c $(MOSTDEP)
jcsample.o: jcsample.c $(MOSTDEP)
jctrans.o:  jctrans.c  $(MOSTDEP)
jdapimin.o: jdapimin.c $(MOSTDEP)
jdapistd.o: jdapistd.c $(MOSTDEP)
jdatadst.o: jdatadst.c $(BASEDEP)
jdatasrc.o: jdatasrc.c $(BASEDEP)
jdcoefct.o: jdcoefct.c $(MOSTDEP)
jdcolor.o:  jdcolor.c  $(MOSTDEP)
jddctmgr.o: jddctmgr.c $(MOSTDEP) jdct.h
jdhuff.o:   jdhuff.c   $(MOSTDEP) jdhuff.h
jdinput.o:  jdinput.c  $(MOSTDEP)
jdmainct.o: jdmainct.c $(MOSTDEP)
jdmarker.o: jdmarker.c $(MOSTDEP)
jdmaster.o: jdmaster.c $(MOSTDEP)
jdmerge.o:  jdmerge.c  $(MOSTDEP)
jdphuff.o:  jdphuff.c  $(MOSTDEP) jdhuff.h
jdpostct.o: jdpostct.c $(MOSTDEP)
jdsample.o: jdsample.c $(MOSTDEP)
jdtrans.o:  jdtrans.c  $(MOSTDEP)
jerror.o:   jerror.c   $(BASEDEP) jversion.h
jfdctflt.o: jfdctflt.c $(MOSTDEP) jdct.h
jfdctfst.o: jfdctfst.c $(MOSTDEP) jdct.h
jfdctint.o: jfdctint.c $(MOSTDEP) jdct.h
jidctflt.o: jidctflt.c $(MOSTDEP) jdct.h
jidctfst.o: jidctfst.c $(MOSTDEP) jdct.h
jidctint.o: jidctint.c $(MOSTDEP) jdct.h
jidctred.o: jidctred.c $(MOSTDEP) jdct.h
jquant1.o:  jquant1.c  $(MOSTDEP)
jquant2.o:  jquant2.c  $(MOSTDEP)
jutils.o:   jutils.c   $(MOSTDEP)
jmemmgr.o:  jmemmgr.c  $(MOSTDEP) jmemsys.h
jmemansi.o: jmemansi.c $(MOSTDEP) jmemsys.h
jmemname.o: jmemname.c $(MOSTDEP) jmemsys.h
jmemnobs.o: jmemnobs.c $(MOSTDEP) jmemsys.h
jmemdos.o:  jmemdos.c  $(MOSTDEP) jmemsys.h
jmemmac.o:  jmemmac.c  $(MOSTDEP) jmemsys.h

cjpeg.o:    cjpeg.c    $(APPDEP)  jversion.h
djpeg.o:    djpeg.c    $(APPDEP)  jversion.h
jpegtran.o: jpegtran.c $(APPDEP)  transupp.h jversion.h
rdjpgcom.o: rdjpgcom.c            jinclude.h jconfig.h
wrjpgcom.o: wrjpgcom.c            jinclude.h jconfig.h
cdjpeg.o:   cdjpeg.c   $(APPDEP)
rdcolmap.o: rdcolmap.c $(APPDEP)
rdswitch.o: rdswitch.c $(APPDEP)
transupp.o: transupp.c $(MOSTDEP) transupp.h
rdppm.o: 	rdppm.c    $(APPDEP)
wrppm.o:    wrppm.c    $(APPDEP)
rdgif.o:    rdgif.c    $(APPDEP)
wrgif.o: 	wrgif.c    $(APPDEP)
rdtarga.o:  rdtarga.c  $(APPDEP)
wrtarga.o:  wrtarga.c  $(APPDEP)
rdbmp.o:    rdbmp.c    $(APPDEP)
wrbmp.o:    wrbmp.c    $(APPDEP)
rdrle.o:    rdrle.c    $(APPDEP)
wrrle.o: 	wrrle.c    $(APPDEP)

