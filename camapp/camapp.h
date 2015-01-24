/****************************************************************************/
// camapp.h
//
//  - primary application header, used by modules that access PM
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
 * Copyright (c) 2015, bww bitwise works GmbH
 *
 * 2015-01-22 Silvan Scherrer version 1.5.3
 *            enhanced UsbQueryDeviceReport to 4096 byte 
 *
 * ***** END LICENSE BLOCK ***** */

/****************************************************************************/

#ifndef _CAMAPP_H_
#define _CAMAPP_H_

/****************************************************************************/

#define INCL_DOS
#define INCL_PM
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <os2.h>
#include "camapprc.h"
#include "camnls.h"

/****************************************************************************/

// version - the v1.5 betas were 1.5.0 & 1.5.1, the GA is 1.5.2
#define CAMVERSION      0x00010503
#define CAMNLSVER       0x00010503

#define CAMINI_APP      "CAMERADERIE"
#define CAMINI_GRPAPP   "CAMGROUPS"

// keys introduced in v1.0
#define CAMINI_VERSION  "VERSION"
#define CAMINI_COLUMNS  "COLUMNS"
#define CAMINI_SAVERASE "SAVEERASE"         // changed in v1.1
#define CAMINI_SORT     "SORT"
#define CAMINI_COLPOS   "COLPOS"
#define CAMINI_FILEPOS  "FILEPOS"
#define CAMINI_GRPPOS   "GRPPOS"
#define CAMINI_WNDPOS   "WNDPOS"

// keys introduced in v1.1
#define CAMINI_VIEWS    "VIEWS"
#define CAMINI_VIEWNDX  "VIEWNDX"
#define CAMINI_CLRPOS   "CLRPOS"

// keys introduced in v1.2
#define CAMINI_ROTATE   "ROTATE"
#define CAMINI_INFOPOS  "INFOPOS"
#define CAMINI_SAVEPOS  "SAVEPOS"

// keys introduced in v1.5
#define CAMINI_DRVOPTS  "DRVOPTS"
#define CAMINI_OPENPOS  "OPENPOS"
#define CAMINI_EJECTPOS "EJECTPOS"
#define CAMINI_NOUSBERR "NOUSBERR"

/****************************************************************************/

// used to set an error code
#define ERRNBR(x)   {rc = (x); break;}

// used to evaluate the error code returned by a function
#define ERRRTN(x)   {rc = (x); if (rc) break;}

#define ERRMSG(x)   {pErr = (x); break;}

#define ERRBRK(x)   if (rc) {pErr = x " - rc= %d"; break;}

/****************************************************************************/
//  Column-related Info
/****************************************************************************/

typedef struct _COLINFO {
    ULONG               flags;          // app flags
    ULONG               flData;         // data flags
    ULONG               flTitle;        // title flags
    ULONG               ulExtra;        // extra bytes for strings & data
    char *              pszTitle;       // column title
    char *              pszMenu;        // menu text
    PFIELDINFO          pfi;            // fieldinfo for this column
    LONG                lRight;         // right edge of column
    PVOID               pvSort;         // sort function for this column
} COLINFO;

enum    eCols           { eMARK, eNBR, eTITLE,
                          eGROUP, eFMT, ePICXY, 
                          eTNFMT, eTNXY, eTNSIZE, eHNDL,
                          eSIZE, eDAY, eDATE, eTIME,
                          eCNTCOLS,
                          eORGNAME, eGRPPTR,
                          eFMTNBR, ePICX, ePICY,
                          eTNFMTNBR, eTNX, eTNY,
                          eBMP, eTYPE, ePATH, eROT, eTNOFFS,
                          eSZPICXY,
                          eSZTNXY = eSZPICXY+3,
                          eSZTITLE = eSZTNXY+3,
                          eCNTDWORDS = eSZTITLE+65};

#define DEFAULTLASTLEFT eHNDL
#define DEFAULTSORTCOL  eNBR

// ColumnInfo.flags
#define CIF_NDXMASK     0x00ff
#define CIF_LASTLEFT    0x0100
#define CIF_SORTCOL     0x1000
#define CIF_INVISIBLE   0x2000
#define CIF_SPLITCOL    0x8000      // only used to save & restore colinfo

// this enables the code to be ignorant of whether we're
// using a RECORDCORE or MINIRECORDCORE structure
#define CAM_RECTYPE     MINIRECORDCORE

typedef struct _CAMRec {
    CAM_RECTYPE     mrc;
    HPOINTER        mark;
    ULONG           nbr;
    char *          title;
    char *          group;
    char *          fmt;
    char *          picxy;
    char *          tnfmt;
    char *          tnxy;
    ULONG           tnsize;
    ULONG           hndl;
    ULONG           size;
    char *          day;
    CDATE           date;
    CTIME           time;
    ULONG           status;
    char *          orgname;
    void *          grpptr;
    ULONG           fmtnbr;
    ULONG           picx;
    ULONG           picy;
    ULONG           tnfmtnbr;
    ULONG           tnx;
    ULONG           tny;
    HBITMAP         bmp;
    ULONG           type;        // 0x10 = USB; 0x20 = MSD
    char *          path;
    ULONG           rot;
    ULONG           tnoffs;
    char            szpicxy[12];
    char            sztnxy[12];
    char            sztitle[260];
} CAMRec;

typedef CAMRec *CAMRecPtr;

#define CAMREC_EXTRABYTES   (sizeof(CAMRec) - sizeof(CAM_RECTYPE))

/****************************************************************************/
//  Record Status Flags
/****************************************************************************/

#define eSTATUS         eCNTCOLS

#define STAT_SAVE       0x0001
#define STAT_DELETE     0x0002
#define STAT_DONE       0x0004
#define STAT_MASK       0x0007
#define STAT_SAVEDEL    0x0003
#define STAT_SAVEMASK   0x0005

#define STAT_ISBLK(x)   ((((x) & STAT_SAVEDEL) == 0) ? 1 : 0)
#define STAT_ISSAV(x)   ((((x) & STAT_SAVEDEL) == STAT_SAVE) ? 1 : 0)
#define STAT_ISDEL(x)   ((((x) & STAT_SAVEDEL) == STAT_DELETE) ? 1 : 0)

#define STAT_ISDONE(x)  (((x) & STAT_DONE) ? 1 : 0)
#define STAT_ISSAVEDONE(x)  (((x) & (STAT_SAVE | STAT_DONE)) ? 1 : 0)
#define STAT_ISNADA(x)  ((((x) & STAT_MASK) == 0) ? 1 : 0)
#define STAT_NOTSAV(x)  ((((x) & STAT_SAVEMASK) == STAT_SAVE) ? 1 : 0)

#define STAT_SETBLK(x)  ((x) & ~STAT_SAVEDEL)
#define STAT_SETSAV(x)  (((x) & ~STAT_SAVEDEL) | STAT_SAVE)
#define STAT_SETDEL(x)  (((x) & ~STAT_SAVEDEL) | STAT_DELETE)
#define STAT_SETNADA(x) ((x) & ~STAT_MASK)

#define STAT_PROTECTED  0x0010
#define STAT_PROTECTED2 0x0020

#define STAT_MATCHED    0x1000

#define STAT_ROT0       0x0000
#define STAT_ROT90      0x0100
#define STAT_ROT180     0x0200
#define STAT_ROT270     0x0300
#define STAT_ROTMASK    0x0300

#define STAT_GETROT(x)       ((x) & STAT_ROTMASK)
#define STAT_SETROT0(x)      ((x) & ~STAT_ROTMASK)
#define STAT_SETROT90(x)    (((x) & ~STAT_ROTMASK) | STAT_ROT90)
#define STAT_SETROT180(x)   (((x) & ~STAT_ROTMASK) | STAT_ROT180)
#define STAT_SETROT270(x)   (((x) & ~STAT_ROTMASK) | STAT_ROT270)
#define STAT_SETROTCC(x)    ((STAT_GETROT(x) > STAT_ROT180) ? \
                                        STAT_SETROT0(x) : ((x) + STAT_ROT90))
#define STAT_SETROTCW(x)    ((STAT_GETROT(x) < STAT_ROT90) ? \
                                        STAT_SETROT270(x) : ((x) - STAT_ROT90))

/***************************************************************************/
//  QueryColumnIndex() commands
/***************************************************************************/

#define CAM_SORTCOL             1001
#define CAM_LASTLEFT            1002
#define CAM_LASTVISIBLELEFT     1003
#define CAM_FIRSTRIGHT          1004
#define CAM_FIRSTVISIBLERIGHT   1005
#define CAM_DEFAULTLASTLEFT     1006
#define CAM_DEFAULTSORTCOL      1007

/***************************************************************************/
//  Window Messages & Stuff
/***************************************************************************/

#define CAMMSG_FOCUSCHANGE          (WM_USER+50)
#define CAMMSG_FETCHTHUMBS          (WM_USER+51)
#define CAMMSG_SHOWTHUMB            (WM_USER+52)
#define CAMMSG_NOTIFY               (WM_USER+53)
#define CAMMSG_STARTSAVE            (WM_USER+54)
#define CAMMSG_PPCHANGED            (WM_USER+55)
#define CAMMSG_SAVEERASE            (WM_USER+56)
#define CAMMSG_GETDATA              (WM_USER+57)
#define CAMMSG_GETLIST              (WM_USER+58)
#define CAMMSG_SYNCLIST             (WM_USER+59)
#define CAMMSG_OPEN                 (WM_USER+60)
#define CAMMSG_EXIT                 (WM_USER+61)

// my favorite macro
#define MP              MPARAM

/***************************************************************************/

#ifndef _CAM_ALIASES
#define _CAM_ALIASES
typedef void *  CAMHandle;      // alias for CAMCameraPtr
typedef void *  CAMDevice;      // alias for struct usb_device *
#endif

/****************************************************************************/
//  Groups
/****************************************************************************/

typedef struct _CAMGroup {
    struct _CAMGroup * pNext;
    LONG        ndx;
    LONG        mnemonic;
    ULONG       btn;
    ULONG       hWPS;
    ULONG       seqNbr;
    ULONG       cntDigits;
    char        name[32];
    char        file[32];
    char        path[256];
} CAMGroup;

typedef CAMGroup *CAMGroupPtr;

/****************************************************************************/
//  Global flags & fixes
/****************************************************************************/

// defined in cammain.h
extern ULONG    ulGlobalFlags;

#define CAMFLAG_DEMO        0x80000000
#define CAMFLAG_FORCE       0x40000000
#define CAMFLAG_DAVE        0x20000000
#define CAMFLAG_NOLVM       0x10000000
#define CAMFLAG_MSD         0x00020000
#define CAMFLAG_USB         0x00010000
#define CAMFLAG_RESTORE     0x00000001

#define CAMFLAG_ARGMASK     (CAMFLAG_DEMO | CAMFLAG_FORCE | CAMFLAG_DAVE | \
                             CAMFLAG_USB | CAMFLAG_MSD | CAMFLAG_RESTORE)

#define CAM_IS_DEMO         (ulGlobalFlags & CAMFLAG_DEMO)
#define CAM_IS_FORCE        (ulGlobalFlags & CAMFLAG_FORCE)
#define CAM_IS_DAVE         (ulGlobalFlags & CAMFLAG_DAVE)
#define CAM_IS_NOLVM        (ulGlobalFlags & CAMFLAG_NOLVM)
#define CAM_IS_USB          (ulGlobalFlags & CAMFLAG_USB)
#define CAM_IS_MSD          (ulGlobalFlags & CAMFLAG_MSD)
#define CAM_IS_RESTORE      (ulGlobalFlags & CAMFLAG_RESTORE)

#define CAM_TOGGLE          ((ULONG)-1)

/****************************************************************************/
//  Miscellanea
/****************************************************************************/

// rotate options
#define CAM_ROTATEONLOAD    1
#define CAM_ROTATEONSAVE    2
#define CAM_ROTATEMASK      3

// miscellanea used to mark & track records
typedef struct _STATSTUFF {
    char *      pszStatus;
    HPOINTER    hBlkIco;
    HPOINTER    hSavIco;
    HPOINTER    hDelIco;
    HPOINTER    hDoneIco;
    ULONG       ulTotCnt;
    ULONG       ulTotSize;
    ULONG       ulSavCnt;
    ULONG       ulSavSize;
    ULONG       ulDelCnt;
    ULONG       ulDelSize;
} STATSTUFF;

// gcc complains because the standard macro casts a pointer to a SHORT
#define LONGFIELDOFFSET(type, field)    ((LONG)&(((type *)0)->field))

/****************************************************************************/

// stringtable ids for dialogs & menu
typedef struct _CAMNLS {
    ULONG       id;
    ULONG   	str;
} CAMNLS;

typedef CAMNLS *CAMNLSPtr;

// stringtable ids for static strings
typedef struct _CAMNLSPSZ {
    char **     ppsz;
    ULONG       str;
} CAMNLSPSZ;

// stringtable ids for column headings
typedef struct _CAMNLSPCI {
    COLINFO*    pci;
    ULONG       str;
} CAMNLSPCI;

/****************************************************************************/

// in camcam.c
ULONG           CamSetupCamera( HWND hwnd, void ** thisCamPtr, BOOL fStartup);
void            CamShutDownCamera( void ** thisCamPtr);
ULONG           CamInitNotification( HWND hwnd, void * voidCam);
void            CamHandleNotification( HWND hwnd, MPARAM mp1, MPARAM mp2);
ULONG           CamDlgBox( HWND hwnd, ULONG ulID, ULONG ulParm);
MRESULT _System CamFatalDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT _System CamSelectCameraDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

// in camcmd.c
BOOL            InitMenus( HWND hwnd);
HWND            QueryGroupMenu( void);
BOOL            IsMultipleSel( HWND hwnd, CAMRecPtr pcr);
void            PopupMenu( HWND hwnd, CAMRecPtr pcr);
void            Command( HWND hwnd, ULONG ulCmd);
void            SetMultTitles( HWND hwnd, CAMRecPtr pcrSrc);
void            UpdateMark( CAMRecPtr pcr, HWND hCnr, STATSTUFF * pss,
                            ULONG ulOp, BOOL fToggle);

// in camcnr.c
ULONG           CnrInit( HWND hwnd);
void            CnrSetView( HWND hwnd, ULONG ulCmd);
ULONG           CnrGetView( void);
void            CnrStoreViews( void);
BOOL            CnrPaintRecord( POWNERITEM poi);
MRESULT _System ColorsDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

// in camcol.c
ULONG           InitCols( HWND hwnd);
void            ExitDetailsView( HWND hCnr);
void            EnterDetailsView( HWND hCnr);
void            InitColumnWidths( HWND hCnr);
void            SetColumnVisibility( HWND hwnd, ULONG ulCmd);
void            ResetColumnWidths( HWND hwnd, BOOL fResetSplit);
ULONG           QueryColumnIndex( ULONG flag);
void            StoreColumns( HWND hwnd);
MRESULT _System ColumnsDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

// in camdata.c
void            GetData( HWND hwnd, BOOL fSync);
void            GetDataReply( HWND hwnd, MPARAM mp1);
void            GetListReply( HWND hwnd, MPARAM mp1);
void            SyncReply( HWND hwnd, MPARAM mp1);

// in camdrive.c
void            DrvInit( HWND hwnd);
void            DrvMenuCmd( HWND hwnd, ULONG ulCmd);
void            DrvSetUseLvm( HWND hwnd, BOOL fUse);
BOOL            DrvQueryUseLvm( void);
ULONG           DrvGetDrives( HWND hwnd, char * achDrives);
void            DrvGetDriveName( ULONG ulDrive, char * pszText);
BOOL            DrvEjectAll( HWND hwnd);
ULONG           DrvEjectDrive( char chDrive);
MRESULT _System DrvMountDrivesDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT _System DrvEjectAllDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

// in camgrp.c
void            InitGroups( HWND hwnd);
CAMGroupPtr     GetDefaultGroup( char ** ppszName);
void            SetGroupFromButton( HWND hwnd, ULONG ulCmd);
void            SetGroupFromMenu( HWND hwnd, ULONG ulCmd);
LONG            RepositionGroupButtons( HWND hSidebar, LONG cySidebar);
void            StoreGroups( void);
MRESULT _System GroupDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
BOOL            MatchGroupMnemonic( HWND hwnd, USHORT usChar);

// in caminfo.c
BOOL            InfoAvailable( void);
MRESULT _System InfoDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

// in cammain.c
BOOL            SetGlobalFlag( ULONG ulFlag, ULONG ulValue);
void            SetTitle( HWND hwnd, char * pszText);
void            UpdateStatus( HWND hwnd, ULONG ulMsg);
STATSTUFF *     ClearStatStuff( void);
STATSTUFF *     GetStatStuff( void);
void *          GetCamPtr( void);
HWND            OpenWindow( HWND hwnd, ULONG ulID, ULONG ulParm);
void            WindowClosed( ULONG ulID);
HWND            GetHwnd( ULONG ulID);
void            ShowDialog( HWND hwnd, char * pIniKey);

// in camsave.c
void            SaveChanges( HWND hwnd, ULONG ulMsg);
MRESULT _System ConfirmSaveDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT _System SaveFilesDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

// in camshow.c
void            InitShow( HWND hwnd);
void            TermShow( HWND hwnd);
ULONG           QueryRotateOpts( void);
void            ToggleRotateOpts( HWND hwnd, ULONG ulCmd);
void            StoreRotateOpts( void);
HBITMAP         GetBlankBmp( void);
void            SetThumbWnd( HWND hwnd, CAMRecPtr pcr);
void            SetThumbWndTitle( HWND hwnd, char * pszTitle);
void            UpdateThumbWnd( HWND hwnd);
BOOL            RotateThumbBmp( CAMRecPtr pcr, ULONG ulRot);
HBITMAP         RotateBitmap( HPS hps, HBITMAP hBmpIn,
                              PBITMAPINFOHEADER2 bmpInfo, ULONG ulRotate);

// in camsort.c
void            InitSort( HWND hwnd);
void            InitSortMenu( HWND hwnd);
void            StoreSort( void);
void            SetSortIndicatorMenu( HWND hwnd, ULONG ulCmd);
void            SetSortColumn( HWND hwnd, ULONG ulCmd);
PVOID           QuerySortFunctionPtr( void);
void            SetFileSaveSortOrder( HWND hwnd, BOOL fStart);

// in camstrings.c
char *          GetUnknownString( void);
char *          GetFormatName( uint32_t code);
char *          GetFormatNameFromExt( char * szTitle);

// in camthumb.c
void            FetchAllThumbnails( HWND hwnd);
void            FetchOneThumbnail( HWND hwnd, MPARAM mp1, MPARAM mp2);

// in camutil.c
BOOL            UtilDoStartupTests( void);
BOOL            PathDlg( HWND hwnd, char * pszPath);
APIRET          CreatePath( char * pszPath);
APIRET          ValidatePath( char * pszPath);
char *          LoadString( ULONG str, char * pszIn);
void            LoadDlgStrings( HWND hwnd, CAMNLSPtr pnls);
void            LoadDlgItemString( HWND hwnd, ULONG ulID, ULONG ulStr);
char *          LoadStringNoTilde( ULONG str, char * pszIn);
void            LoadStaticStrings( CAMNLSPSZ * pnls);

/****************************************************************************/
#endif  //_CAMAPP_H_
/****************************************************************************/

