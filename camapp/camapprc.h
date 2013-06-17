/****************************************************************************/
// camapprc.h
//
//  - resource ids, excluding NLS stringtable ids
//
/****************************************************************************/

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Cameraderie.
 *
 * The Initial Developer of the Original Code is Richard L. Walsh.
 * 
 * Portions created by the Initial Developer are Copyright (C) 2006,2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

/****************************************************************************/

#ifndef _CAMAPPRC_H_
#define _CAMAPPRC_H_

/****************************************************************************/

#define IDI_APPICO              1
#define IDI_BLKICO              2
#define IDI_SAVICO              3
#define IDI_DELICO              4
#define IDI_DONEICO             5
#define IDI_SORTPTR             6
#define IDB_UPBMP               7
#define IDB_DOWNBMP             8
#define IDB_THUMBBMP            9
#define IDB_ROTCCBMP            10
#define IDB_ROTCWBMP            11
#define IDI_EJECTICO            12

#define IDB_ROTCCBMP_NBR        "#10"
#define IDB_ROTCWBMP_NBR        "#11"
#define IDI_EJECTICO_NBR        "#12"

#define IDA_MAIN                21
#define IDA_UPDOWN              22

#define IDD_MAIN                100
#define IDC_STATUS              101
#define IDC_SIDEBAR             102

#define IDD_LOADING             111
#define IDD_FATAL               112
#define IDD_EJECTALL            113

#define IDD_SAVE                120
#define IDD_SAVEEXIT            121
#define IDD_SAVEFILE            122
#define IDC_REPLACEALL          123
#define IDC_SAVENOW             124
#define IDC_SAVEOPTS            125
#define IDC_SAVEDONT            125
#define IDC_SAVELATER           126
#define IDC_SAVEAFTER           127
#define IDC_ERASENOW            128
#define IDC_SAVECHGS            129
#define IDD_SAVEDONE            130

#define IDD_COLUMNS             140
#define IDC_COLLIST             143
#define IDC_COLUP               144
#define IDC_COLDOWN             145

#define IDD_GROUPS              150
#define IDC_GRPLIST             151
#define IDC_GRPUP               152
#define IDC_GRPDOWN             153

#define IDC_GRPNAME             154
#define IDC_GRPNAMETXT          155
#define IDC_GRPPATH             156
#define IDC_GRPPATHTXT          157
#define IDC_GRPFILE             158
#define IDC_GRPFILETXT          159
#define IDC_GRPSEQ              160
#define IDC_GRPSEQTXT           161
#define IDC_GRPDIG              162
#define IDC_GRPDIGTXT           163
#define IDC_GRPERRTXT           164
#define IDC_GRPNEW              165
#define IDC_GRPDEL              166
#define IDC_GRPFIND             167
#define IDC_GRPCREATE           168
#define IDC_GRPUNDEL            169
#define IDC_GRPAPPLY            170
#define IDC_GRPUNDO             171
#define IDC_GRPINFO1            172
#define IDC_GRPINFO2            173
#define IDC_GRPINFO3            174

#define IDC_TEXT1               181
#define IDC_TEXT2               182
#define IDC_TEXT3               183
#define IDC_TEXT4               184
#define IDC_TEXT5               185
#define IDC_TEXT6               186

#define IDC_RECT                190
#define IDC_BTN1                191
#define IDC_BTN2                192
#define IDC_BTN3                193
#define IDC_BTN4                194

#define IDD_SHOW                400
#define IDC_BMP                 401
#define IDC_TITLE               402
#define IDC_INFO                403

#define IDC_GRPBTNFIRST         418
#define IDC_ROTCW               418
#define IDC_ROTCC               419
#define IDC_EDITGRP             420
#define IDC_GRPFIRST            421
#define IDC_GRPLAST             432
#define IDC_GRPBTNLAST          432

#define IDD_COLORS              450
#define IDC_CLRCBTXT            451
#define IDC_CLRCB               452
#define IDC_CLRWINDOW           453
#define IDC_CLRHILITE           454
#define IDC_CLRGROUP            455
#define IDC_CLRSEPARATOR        456

#define IDD_FILEDLG             500
#define IDC_DIREF               0x1001
#define IDC_DIREFTXT            0x1002
#define IDC_DIRMSG1             0x1003
#define IDC_DIRMSG2             0x1004

#define IDD_INFO                550
#define IDC_MAKEMODEL           551
#define IDC_SERNBRTXT           552
#define IDC_SERNBR              553
#define IDC_DEVVERTXT           554
#define IDC_DEVVER              555
#define IDC_BUSDEVTXT           556
#define IDC_BUSDEV              557
#define IDC_VENDPRODTXT         558
#define IDC_VENDPROD            559
#define IDC_USBVERTXT           560
#define IDC_USBVER              561
#define IDC_FILEFMTTXT          562
#define IDC_FILEFMT             563
#define IDC_USBTXT              564
#define IDC_CAMERATXT           565
#define IDC_CAPTURETXT          566
#define IDC_PROPERTIES          567

#define IDD_INTRODLG            600
#define IDC_INTROPATH           601
#define IDC_INTROFIND           602
#define IDC_INTROWELCOME        603
#define IDC_INTROPATHTXT        604
#define IDC_INTROMSG1           605
#define IDC_INTROMSG2           606

#define IDD_DISCONNECT          620
#define IDC_DISCON1             621
#define IDC_DISCON2             622
#define IDC_DISCON3             623
#define IDC_DISCON4             624
#define IDC_DISCON5             625
#define IDC_DISCON6             626

#define IDD_SELECTCAMERA        640
#define IDC_CAMLIST             641

#define IDD_NOUSB               660
#define IDC_DONOTSHOW           661

#define IDD_LVMHANG             680
#define IDC_AUTOMOUNT           681

#define IDC_STOP                701
#define IDC_OK                  702
#define IDC_CANCEL              703
#define IDC_RETRY               704
#define IDC_YES                 705
#define IDC_NO                  706
#define IDC_NEXT                707
#define IDC_REPLACE             708
#define IDC_CREATE              709
#define IDC_EXIT                700
#define IDC_RETRYDEL            711
#define IDC_HALT                712
#define IDC_UP                  713
#define IDC_DOWN                714
#define IDC_DEFAULT             715
#define IDC_UNDO                716
#define IDC_CONTINUE            717
#define IDC_EJECT               718

/****************************************************************************/

#define IDM_MAIN                200

#define IDM_FILE                210
#define IDM_SYNCDATA            211
#define IDM_SAVE                212
#define IDM_EXIT                213
#define IDM_GETDATA             214
#define IDM_INFO                215
#define IDM_OPEN                216

#define IDM_SORT                220
#define IDM_SORTSYM             221
#define IDM_INTUITIVE           222
#define IDM_LITERAL         	223
#define IDM_SORTDONE            224
#define IDM_SORTFIRST           225

#define IDM_VIEW                240
#define IDM_COLRESET            241
#define IDM_COLORDER            242
#define IDM_COLDONE             243
#define IDM_COLFIRST            244
#define IDM_DETAILS             245
#define IDM_SMALL               246
#define IDM_MEDIUM              247
#define IDM_LARGE               248
#define IDM_COLORS              249

#define IDM_GROUPS              250
#define IDM_EDITGRP             251

#define IDM_OPTIONS             260
#define IDM_ROTATEOPTS          261
#define IDM_ROTONLOAD           262
#define IDM_ROTONSAVE           263
#define IDM_REMOVABLES          264
#define IDM_AUTOMOUNT           265
#define IDM_EJECTINFO           266
#define IDM_EJECTDLG            267
#define IDM_EJECTALWAYS         268
#define IDM_EJECTNEVER          269
#define IDM_RESTOREDEF          270

#define IDM_SINGLE              280
#define IDM_WRITE               281
#define IDM_DELETE              282
#define IDM_UNMARK              283
#define IDM_UNDOTTL             284
#define IDM_SHOW                285
#define IDM_SAVED               286
#define IDM_SAVEDMARK           287
#define IDM_UNSAVED             288

#define IDM_MULTIPLE            290
#define IDM_WRITEALL            291
#define IDM_DELETEALL           292
#define IDM_UNMARKALL           293
#define IDM_UNDOTTLALL          294

#define IDM_ROTLEFT             296
#define IDM_ROTRIGHT            297

#define IDM_GRPMENU             300
#define IDM_GRPMNUFIRST         301
#define IDM_GRPMNULAST          399

/****************************************************************************/
#endif  //_CAMAPPRC_H_
/****************************************************************************/

