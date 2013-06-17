#****************************************************************************/
#  makefile for camlibusb
#
#  - tools used by the author:  gnu make 3.81, gcc 3.3.5, emxomfar 0.6.1
#
#****************************************************************************/

.SUFFIXES :
.SUFFIXES : .c .o

CC = gcc
AR = emxomfar

CFLAGS = -c -s -Zomf -Wall -I../usbcalls

camlibusb.lib : camlibusb.o
	$(AR) r $@ $^

camlibusb.o : \
    camlibusb.h ../usbcalls/usbcalls.h makefile

