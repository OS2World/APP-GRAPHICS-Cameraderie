/****************************************************************************/
// camshow.c
//
//  - sidebar window initialization & maintenance
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

#include "camapp.h"

/****************************************************************************/

// used to create an NLS-appropriate date format
typedef struct _YMD {
    USHORT  y;
    USHORT  m;
    USHORT  d;
} YMD;

/****************************************************************************/

void            InitShow( HWND hwnd);
MRESULT _System SidebarSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
void            TermShow( HWND hwnd);
ULONG           QueryRotateOpts( void);
void            ToggleRotateOpts( HWND hwnd, ULONG ulCmd);
void            RestoreRotateOpts( HWND hwnd);
void            StoreRotateOpts( void);
HBITMAP         GetBlankBmp( void);
void            SetThumbWnd( HWND hwnd, CAMRecPtr pcr);
void            SetThumbWndTitle( HWND hwnd, char * pszTitle);
void            SetThumbWndInfo( HWND hSidebar, CAMRecPtr pcr);
void            InitThumbWndInfo( void);
void            UpdateThumbWnd( HWND hwnd);
BOOL            RotateThumbBmp( CAMRecPtr pcr, ULONG ulRot);
HBITMAP         RotateBitmap( HPS hps, HBITMAP hBmpIn,
                              PBITMAPINFOHEADER2 bmpInfo, ULONG ulRotate);

/****************************************************************************/

static ULONG    ulRotOpts = CAM_ROTATEONLOAD | CAM_ROTATEONSAVE;
static PFNWP    pfnSidebar = 0;
static HBITMAP  hbmpBlank = 0;
static char     szFormat[256];

/* mRot0 isn't used - it's just here to show what the default looks like
static MATRIXLF mRot0     = { 0x00010000, 0,          0,     //   1   0  0
                              0,          0x00010000, 0,     //   0   1  0
                              0,          0,          1 };   //   0   0  1
*/
static MATRIXLF mRot90    = { 0,          0x00010000, 0,     //   0   1  0
                              0xffff0000, 0,          0,     //  -1   0  0
                              120,        0,          1 };   // 120   0  1

static MATRIXLF mRot180   = { 0xffff0000, 0,          0,     //  -1   0  0
                              0,          0xffff0000, 0,     //   0  -1  0
                              160,        120,        1 };   // 160 120  1

static MATRIXLF mRot270   = { 0,          0xffff0000, 0,     //   0  -1  0
                              0x00010000, 0,          0,     //   1   0  0
                              0,          160,        1 };   //   0 160  1

// this identifies the ordering of year, month, & day when
// SetThumbWndInfo() constructs an NLS-appropriate date

static YMD      ymd[4] = {
                    {2,0,1},    // 0 MDY
                    {2,1,0},    // 1 DMY
                    {0,1,2},    // 2 YMD
                    {0,2,1}     // 3 YDM
                 };

static YMD *    pYMD;


static CAMNLS   nlsSidebar[] = {
    { IDC_EDITGRP,      BTN_EditGroups },
    { IDC_GRPFIRST,     GRP_Default },
    { 0,                0 }};


/****************************************************************************/

void        InitShow( HWND hwnd)

{
    HPS         hps;
    HWND        hSidebar;
    HWND        hTemp;
    SWP         swpInfo;

    hSidebar = WinWindowFromID( hwnd, IDC_SIDEBAR);
    LoadDlgStrings( hSidebar, nlsSidebar);
    InitThumbWndInfo();

    // the thumbnail window seems to ignore SM_SETHANDLE and won't
    // display new bitmaps if it is created with the bitmap style,
    // so we have to change the style & load the bitmap manually
    hps = WinGetPS( hSidebar);
    if (hps)
    {
        hbmpBlank = GpiLoadBitmap( hps, 0, IDB_THUMBBMP, 0, 0);
        WinReleasePS( hps);
    }

    hTemp = WinWindowFromID( hSidebar, IDC_BMP);
    WinSetWindowBits( hTemp, QWL_STYLE, SS_BITMAP, SS_TEXT | SS_BITMAP);
    WinSendMsg( hTemp, SM_SETHANDLE, (MP)hbmpBlank, 0);

    WinQueryWindowPos( hTemp, &swpInfo);

    hTemp = WinWindowFromID( hSidebar, IDC_ROTCC);
    swpInfo.y += 6;
    WinSetWindowPos( hTemp, 0, 8, swpInfo.y,
                     16, 16, SWP_MOVE | SWP_SIZE);

    hTemp = WinWindowFromID( hSidebar, IDC_ROTCW);
    swpInfo.x += swpInfo.cx - 16 - 8;
    WinSetWindowPos( hTemp, 0, swpInfo.x, swpInfo.y,
                     16, 16, SWP_MOVE | SWP_SIZE);

    RestoreRotateOpts( hwnd);
    InitGroups( hwnd);

    pfnSidebar = WinSubclassWindow( hSidebar, SidebarSubProc);

    return;
}

/****************************************************************************/

MRESULT _System SidebarSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{

    if (msg == WM_WINDOWPOSCHANGED &&
        (((PSWP)mp1)->fl & SWP_SIZE) &&
        ((PSWP)mp1)[0].cy != ((PSWP)mp1)[1].cy) {

        HWND    hTemp;
        LONG    lChg;
        LONG    lExtra;
        SWP     aswp[5];

        lChg = ((PSWP)mp1)[0].cy - ((PSWP)mp1)[1].cy;

        hTemp = WinWindowFromID( hwnd, IDC_BMP);
        WinQueryWindowPos( hTemp, &aswp[0]);
        aswp[0].y += lChg;
        aswp[0].fl = SWP_MOVE;

        hTemp = WinWindowFromID( hwnd, IDC_TITLE);
        WinQueryWindowPos( hTemp, &aswp[1]);
        aswp[1].y += lChg;
        aswp[1].fl = SWP_MOVE;

        hTemp = WinWindowFromID( hwnd, IDC_ROTCW);
        WinQueryWindowPos( hTemp, &aswp[2]);
        aswp[2].y += lChg;
        aswp[2].fl = SWP_MOVE;

        hTemp = WinWindowFromID( hwnd, IDC_ROTCC);
        WinQueryWindowPos( hTemp, &aswp[3]);
        aswp[3].y += lChg;
        aswp[3].fl = SWP_MOVE;

        lExtra = RepositionGroupButtons( hwnd, ((PSWP)mp1)[0].cy);

        hTemp = WinWindowFromID( hwnd, IDC_INFO);
        WinQueryWindowPos( hTemp, &aswp[4]);
        aswp[4].y  = aswp[1].y;
        aswp[4].fl = SWP_MOVE | SWP_SIZE;
        if (lExtra > 0) {
            aswp[4].y -= lExtra;
            aswp[4].cy = lExtra;
        }
        else
            aswp[4].cy = 0;

        WinSetMultWindowPos( 0, aswp, 5);
    }

    return (pfnSidebar( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

void        TermShow( HWND hwnd)

{
    HWND        hSidebar;

    hSidebar = WinWindowFromID( hwnd, IDC_SIDEBAR);
    WinSendDlgItemMsg( hSidebar, IDC_BMP, SM_SETHANDLE, 0, 0);

    if (hbmpBlank) {
        GpiDeleteBitmap( hbmpBlank);
        hbmpBlank = 0;
    }

    return;
}

/****************************************************************************/
/****************************************************************************/

ULONG       QueryRotateOpts( void)

{
    return (ulRotOpts);
}

/****************************************************************************/

void        ToggleRotateOpts( HWND hwnd, ULONG ulCmd)

{
    ULONG   opt;

    if (ulCmd == IDM_ROTONLOAD)
        opt = CAM_ROTATEONLOAD;
    else
    if (ulCmd == IDM_ROTONSAVE)
        opt = CAM_ROTATEONSAVE;
    else
        return;

    ulRotOpts ^= opt;
    WinSendDlgItemMsg( hwnd, FID_MENU, MM_SETITEMATTR,
                       MPFROM2SHORT( ulCmd, TRUE),
                       MPFROM2SHORT( MIA_CHECKED,
                       ((ulRotOpts & opt) ? MIA_CHECKED : 0)));

    return;
}

/****************************************************************************/

void        RestoreRotateOpts( HWND hwnd)

{
    ULONG   ul;

    // if the ini entry is missing or defective, use default values
    if (PrfQueryProfileSize( HINI_USERPROFILE, CAMINI_APP,
                             CAMINI_ROTATE, &ul) == FALSE ||
        ul != sizeof( ulRotOpts) ||
        PrfQueryProfileData( HINI_USERPROFILE, (PCSZ)CAMINI_APP,
                             CAMINI_ROTATE, &ulRotOpts, &ul) == FALSE)
        ulRotOpts = CAM_ROTATEONLOAD | CAM_ROTATEONSAVE;
    else
        ulRotOpts &= CAM_ROTATEONLOAD | CAM_ROTATEONSAVE;

    WinSendDlgItemMsg( hwnd, FID_MENU, MM_SETITEMATTR,
                       MPFROM2SHORT( IDM_ROTONLOAD, TRUE),
                       MPFROM2SHORT( MIA_CHECKED,
                       ((ulRotOpts & CAM_ROTATEONLOAD) ? MIA_CHECKED : 0)));
    WinSendDlgItemMsg( hwnd, FID_MENU, MM_SETITEMATTR,
                       MPFROM2SHORT( IDM_ROTONSAVE, TRUE),
                       MPFROM2SHORT( MIA_CHECKED,
                       ((ulRotOpts & CAM_ROTATEONSAVE) ? MIA_CHECKED : 0)));

    return;
}

/****************************************************************************/

void        StoreRotateOpts( void)

{
    if (ulRotOpts == (CAM_ROTATEONLOAD | CAM_ROTATEONSAVE))
        PrfWriteProfileData( HINI_USERPROFILE, CAMINI_APP, CAMINI_ROTATE, 0, 0);
    else
        PrfWriteProfileData( HINI_USERPROFILE, CAMINI_APP, CAMINI_ROTATE,
                             &ulRotOpts, sizeof(ulRotOpts));

    return;
}

/****************************************************************************/
/****************************************************************************/

HBITMAP     GetBlankBmp( void)

{
    return (hbmpBlank);
}

/****************************************************************************/

void        SetThumbWnd( HWND hwnd, CAMRecPtr pcr)

{
    HBITMAP     hbmp;
    HWND        hSidebar;
    HWND        hStatic;

    hSidebar = WinWindowFromID( hwnd, IDC_SIDEBAR);
    hStatic = WinWindowFromID( hSidebar, IDC_BMP);

    if (pcr->bmp)
        hbmp = pcr->bmp;
    else
        hbmp = hbmpBlank;

    WinSetWindowULong( hStatic, QWL_USER, (ULONG)pcr);
    SetThumbWndTitle( hwnd, pcr->title);
    WinSendMsg( hStatic, SM_SETHANDLE, (MP)hbmp, 0);
    SetThumbWndInfo( hSidebar, pcr);

    return;
}

/****************************************************************************/

void        SetThumbWndTitle( HWND hwnd, char * pszTitle)

{
    char *  ptr;
    char    szText[CCHMAXPATH];

    ptr = strrchr( pszTitle, '\\');
    if (ptr)
        ptr++;
    else {
        ptr = pszTitle;
        if (ptr[1] == ':')
            ptr += 2;
    }

    strcpy( szText, ptr);

    ptr = szText;
    while ((ptr = strpbrk( ptr, "\r\n")) != 0)
        strcpy( ptr, ptr + strspn( ptr, "\r\n"));

    WinSetDlgItemText( WinWindowFromID( hwnd, IDC_SIDEBAR), IDC_TITLE, szText);

    return;
}

/****************************************************************************/

void        SetThumbWndInfo( HWND hSidebar, CAMRecPtr pcr)

{
    USHORT  ausDate[3];
    char    szText[CCHMAXPATH];

    // put year, month, & day in the correct NLS order
    ausDate[pYMD->y] = pcr->date.year;
    ausDate[pYMD->m] = pcr->date.month;
    ausDate[pYMD->d] = pcr->date.day;

    sprintf( szText, szFormat, (int)pcr->size,
             ausDate[0], ausDate[1], ausDate[2], pcr->day,
             pcr->time.hours, pcr->time.minutes, pcr->time.seconds);

    WinSetDlgItemText( hSidebar, IDC_INFO, szText);

    return;
}

/****************************************************************************/

// constructs an NLS-apprpriate format string for SetThumbWndInfo();
// since GCC appears not to support %n$ to specify the order in which
// arguments should be used, we have to use our YMD structure to
// specify the ordering of year, month, & day

void        InitThumbWndInfo( void)

{
    ULONG   ul;
    char *  ptr;
    char    szSep[4];
    char    szWork[64];

    ptr = szFormat;

    // size
    LoadStringNoTilde( MNU_Size, szWork);
    ptr += sprintf( ptr, "\n%s:\t%%d", szWork);

    // date
    ul = (ULONG)PrfQueryProfileInt( HINI_USERPROFILE, "PM_National",
                                    "iDate", 2);
    if (ul > 3)
        ul = 2;
    pYMD = &ymd[ul];

    ul = PrfQueryProfileString( HINI_USERPROFILE, "PM_National",
                                "sDate", "-", szSep, sizeof(szSep));
    if (ul < 2 || ul > 3)
        strcpy( szSep, "-");

    LoadStringNoTilde( MNU_Date, szWork);
    ptr += sprintf( ptr, "\n%s:\t%%02d%s%%02d%s%%02d (%%s)",
                    szWork, szSep, szSep);

    // time
    ul = PrfQueryProfileString( HINI_USERPROFILE, "PM_National",
                                "sTime", ":", szSep, sizeof(szSep));
    if (ul < 2 || ul > 3)
        strcpy( szSep, ":");

    LoadStringNoTilde( MNU_Time, szWork);
    sprintf( ptr, "\n%s:\t%%02d%s%%02d%s%%02d", szWork, szSep, szSep);

    return;
}

/****************************************************************************/

// after deleting files, the current thumbnail may be invalid because the
// selection has changed or there aren't any records left;  this displays
// the t/n for the current record or the blank bitmap as appropriate

void        UpdateThumbWnd( HWND hwnd)

{
    HWND        	hSidebar;
    HWND        	hStatic;
    CAMRecPtr       pcr;
    CAMRecPtr       pcrOld;

do {
    hSidebar = WinWindowFromID( hwnd, IDC_SIDEBAR);
    hStatic = WinWindowFromID( hSidebar, IDC_BMP);

    pcrOld = (CAMRecPtr)WinQueryWindowULong( hStatic, QWL_USER);
    pcr = (CAMRecPtr)WinSendDlgItemMsg( hwnd, FID_CLIENT,
                    CM_QUERYRECORDEMPHASIS, (MP)CMA_FIRST, (MP)CRA_CURSORED);

    if (pcr && pcr != (CAMRecPtr)-1) {
        if (pcr != pcrOld) {
            WinSendDlgItemMsg( hwnd, FID_CLIENT, CM_SETRECORDEMPHASIS,
                               (MP)pcr, MPFROM2SHORT( TRUE, CRA_SELECTED));
            WinPostMsg( hwnd, CAMMSG_SHOWTHUMB, (MP)pcr, 0);
        }
        break;
    }

    WinSetWindowULong( hStatic, QWL_USER, 0);
    WinSetDlgItemText( hSidebar, IDC_TITLE, "");
    WinSetDlgItemText( hSidebar, IDC_INFO, "");
    UpdateStatus( hwnd, STS_NoFiles);
    WinSendMsg( hStatic, SM_SETHANDLE, (MP)hbmpBlank, 0);

} while (0);

    return;
}

/****************************************************************************/
/****************************************************************************/

BOOL        RotateThumbBmp( CAMRecPtr pcr, ULONG ulRot)

{
    BOOL        fRtn = FALSE;
    HBITMAP     hBmp;
    HBITMAP     hBmpRot;
    HDC         hdc = 0;
    HPS         hps = 0;
    SIZEL       sizl;
    BITMAPINFOHEADER2   bmih;

do {

    hBmp = pcr->bmp;
    if (hBmp == 0)
        break;

    memset( &bmih, 0, sizeof(bmih));
    bmih.cbFix = sizeof(bmih);

    if (!GpiQueryBitmapInfoHeader( hBmp, &bmih)) {
        printf( "RotateThumbBmp - GpiQueryBitmapInfoHeader failed\n");
        break;
    }

    hdc = DevOpenDC( 0, OD_MEMORY, "*", 0L, 0, 0);
    if (hdc == 0)
    {
        printf("DevOpenDC failed\n");
        break;
    }

    sizl.cx = ((bmih.cx >= bmih.cy) ? bmih.cx : bmih.cy);
    sizl.cy = sizl.cx;

    hps = GpiCreatePS( 0, hdc, &sizl, PU_PELS | GPIF_DEFAULT | GPIT_MICRO | GPIA_ASSOC);
    if (hps == 0)
    {
        printf("GpiCreatePS failed\n");
        break;
    }

    hBmpRot = RotateBitmap( hps, hBmp, &bmih, ulRot);
    if (!hBmpRot)
        break;

    pcr->bmp = hBmpRot;
    GpiDeleteBitmap( hBmp);

    if (ulRot == STAT_ROT90)
        pcr->status = STAT_SETROTCC(pcr->status);
    else
        pcr->status = STAT_SETROTCW(pcr->status);

    fRtn = TRUE;

} while (0);

    if (hps) {
        GpiSetBitmap( hps, 0);
        GpiDestroyPS( hps);
    }
    if (hdc)
        DevCloseDC( hdc);

    return (fRtn);
}

/****************************************************************************/

HBITMAP     RotateBitmap( HPS hps, HBITMAP hBmpIn,
                          PBITMAPINFOHEADER2 bmpInfo, ULONG ulRotate)
{
    MATRIXLF *  pRot;
    HBITMAP     hBmpOut = 0;
    HBITMAP     hBmpWork = 0;
    POINTL      aptl[4];

do {
    memset( aptl, 0, sizeof(aptl));
    aptl[1].x = bmpInfo->cx;
    aptl[1].y = bmpInfo->cy;
    aptl[3].x = bmpInfo->cx;
    aptl[3].y = bmpInfo->cy;

    if (ulRotate == STAT_ROT0)
        break;

    if (ulRotate == STAT_ROT180) {
        pRot = &mRot180;
        mRot180.lM31 = bmpInfo->cx;
        mRot180.lM32 = bmpInfo->cy;
    }
    else {
        bmpInfo->cx = aptl[1].y;
        bmpInfo->cy = aptl[1].x;

        if (ulRotate == STAT_ROT270) {
            pRot = &mRot270;
            mRot270.lM32 = bmpInfo->cy;
        }
        else {
            pRot = &mRot90;
            mRot90.lM31 = bmpInfo->cx;
        }
    }

    GpiSetBitmap( hps, 0);

    hBmpWork = GpiCreateBitmap( hps, bmpInfo, 0, 0, 0);
    if (hBmpWork == 0) {
        printf( "GpiCreateBitmap failed\n");
        break;
    }

    if (GpiSetBitmap( hps, hBmpWork) == HBM_ERROR) {
        printf( "GpiSetBitmap failed\n");
        break;
    }

    if (!GpiSetModelTransformMatrix( hps, 9, pRot, TRANSFORM_REPLACE)) {
        printf( "Rotate failed\n");
        break;
    }

    if (GpiWCBitBlt( hps, hBmpIn, 4, aptl, ROP_SRCCOPY,
                     BBO_IGNORE) == GPI_ERROR) {
        printf( "GpiWCBitBlt failed\n");
        break;
    }

    hBmpOut = hBmpWork;

} while (0);

    if (hBmpOut == 0) {
        if (hBmpWork)
            GpiDeleteBitmap( hBmpWork);
    }

    return hBmpOut;
}

/****************************************************************************/

