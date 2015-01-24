#****************************************************************************/
#  makefile for Cameraderie NLS Dll
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
.SUFFIXES : .c .o

CC = gcc

CFLAGS = -c -s -Zomf -Wall -O
LDFLAGS = -s -Zomf -Zdll -Zmap -Zlinker /EXEPACK:2

camnls.dll : camnls.o camnls.def
	$(CC) -o $@ $(LDFLAGS) $^

camnls.o : makefile

