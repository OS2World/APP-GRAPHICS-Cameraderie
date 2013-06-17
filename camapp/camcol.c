/****************************************************************************/
// camcol.c
//
//  - container column initialization
//  - routines to implement container enhancements:
//      * click-to-sort column headings
//      * adjustable column widths
//      * rearrange column order
//      * show/hide columns
//  - columns dialog
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
#include "camchklist.h"

/****************************************************************************/

ULONG           InitCols( HWND hwnd);
ULONG           InitFieldInfo( HWND hCnr);
void            ExitDetailsView( HWND hCnr);
void            EnterDetailsView( HWND hCnr);
void            InitColumnWidths( HWND hCnr);
BOOL            SubclassColumnHeadings( HWND hCnr);
MRESULT _System LeftTitleSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT _System RightTitleSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
ULONG           InColOrMargin( HWND hwnd, HWND hCnr, MPARAM mp1, ULONG first, PULONG plast);
void            ResizeColumn( HWND hTitle, HWND hCnr, ULONG ulCol);
void            SetColumnVisibility( HWND hwnd, ULONG ulCmd);
void            GetColumnWidths( HWND hCnr);
LONG            GetAvgCharWidth( HWND hwnd);
void            ResetColumnWidths( HWND hwnd, BOOL fResetSplit);
void            SetSplitBar( HWND hCnr, BOOL fReset);
ULONG           QueryColumnIndex( ULONG flag);
void            RestoreColumns( void);
void            StoreColumns( HWND hwnd);
ULONG           CalcSplitbarCol( HWND hCnr);
void            FormatColumnHeadings( void);
MRESULT _System ColumnsDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
LONG            InitColumnsDlg( HWND hwnd);
void            ChangeColumnOrder( HWND hwnd, BOOL fUp);
void            ReformatColumnsDlg( HWND hwnd, PSWP pswp);

/****************************************************************************/

// in camsort.c

SHORT   _System SortByMark(   CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByHandle( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByNumber( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByTitle(  CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByGroup(  CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByFormat( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByPicXY(  CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortBySize(   CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByDay(    CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByDate(   CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByTime(   CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByTNFmt(  CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByTNXY(   CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByTNSize( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);

/****************************************************************************/

#define FIRSTLEFTCOL    0
#define LASTRIGHTCOL    (eCNTCOLS-1)

#define INNEITHER       0
#define INCOLUMN        1
#define INMARGIN        2

#define CFA_ICO         (CFA_BITMAPORICON | CFA_FIREADONLY | CFA_HORZSEPARATOR | CFA_SEPARATOR | CFA_CENTER)
#define CFA_NBR         (CFA_ULONG  | CFA_FIREADONLY | CFA_HORZSEPARATOR | CFA_SEPARATOR | CFA_RIGHT)
#define CFA_STR         (CFA_STRING | CFA_FIREADONLY | CFA_HORZSEPARATOR | CFA_SEPARATOR | CFA_LEFT)
#define CFA_ESTR        (CFA_STRING |                  CFA_HORZSEPARATOR | CFA_SEPARATOR | CFA_LEFT)
#define CFA_CSTR        (CFA_STRING | CFA_FIREADONLY | CFA_HORZSEPARATOR | CFA_SEPARATOR | CFA_CENTER)
#define CFA_DAT         (CFA_DATE   | CFA_FIREADONLY | CFA_HORZSEPARATOR | CFA_SEPARATOR | CFA_CENTER)
#define CFA_TIM         (CFA_TIME   | CFA_FIREADONLY | CFA_HORZSEPARATOR | CFA_SEPARATOR | CFA_CENTER)
#define CFA_NBRC        (CFA_ULONG  | CFA_FIREADONLY | CFA_HORZSEPARATOR | CFA_SEPARATOR | CFA_CENTER)
#define CFA_CSTRI       (CFA_CSTR   | CFA_INVISIBLE)
#define CFA_NBRI        (CFA_NBR    | CFA_INVISIBLE)

#define CFA_L           (CFA_LEFT | CFA_FITITLEREADONLY)
#define CFA_C           (CFA_CENTER | CFA_BOTTOM | CFA_FITITLEREADONLY)
#define CFA_R           (CFA_RIGHT | CFA_FITITLEREADONLY)

// flag used to retrieve the width of a column's data -
// documented in PM Ref->Container Controls->Container Views->
// Details View->Determining the Width of a Column in Details View
#define CMA_DATAWIDTH       0x0200

/****************************************************************************/

static BOOL         fWidthSet = FALSE;
static HPOINTER     hSizePtr = 0;
static HPOINTER     hSortPtr = 0;
static ULONG        ulSplitCol = eCNTCOLS;
static LONG         lAvg = 6;
static PFNWP        pfnLeft = 0;
static PFNWP        pfnRight = 0;
static char         szX[]       = "xXx";
static char         szColSep[]  = "========";

static COLINFO      ciA[eCNTCOLS] =
    {
     { eMARK,   CFA_ICO,   CFA_R,   4, szX, szX, 0, 0, (PVOID)&SortByMark },
     { eNBR,    CFA_NBR,   CFA_R,   0, szX, szX, 0, 0, (PVOID)&SortByNumber },
     { eTITLE,  CFA_ESTR,  CFA_C, 264, szX, szX, 0, 0, (PVOID)&SortByTitle },
     { eGROUP,  CFA_STR,   CFA_C,   4, szX, szX, 0, 0, (PVOID)&SortByGroup },
     { eFMT,    CFA_CSTRI, CFA_C,   4, szX, szX, 0, 0, (PVOID)&SortByFormat },
     { ePICXY,  CFA_CSTRI, CFA_C,  20, szX, szX, 0, 0, (PVOID)&SortByPicXY },
     { eTNFMT,  CFA_CSTRI, CFA_C,   4, szX, szX, 0, 0, (PVOID)&SortByTNFmt },
     { eTNXY,   CFA_CSTRI, CFA_C,  20, szX, szX, 0, 0, (PVOID)&SortByTNXY },
     { eTNSIZE, CFA_NBRI,  CFA_C,   0, szX, szX, 0, 0, (PVOID)&SortByTNSize },
     { eHNDL,   CFA_NBRI,  CFA_R,   0, szX, szX, 0, 0, (PVOID)&SortByHandle },
     { eSIZE,   CFA_NBR,   CFA_C,   0, szX, szX, 0, 0, (PVOID)&SortBySize },
     { eDAY,    CFA_CSTR,  CFA_C,   0, szX, szX, 0, 0, (PVOID)&SortByDay },
     { eDATE,   CFA_DAT,   CFA_C,   0, szX, szX, 0, 0, (PVOID)&SortByDate },
     { eTIME,   CFA_TIM,   CFA_C,   0, szX, szX, 0, 0, (PVOID)&SortByTime },
    };

// the only global variable
COLINFO *   ciX[eCNTCOLS] = { &ciA[eMARK], &ciA[eNBR], &ciA[eTITLE],
                              &ciA[eGROUP], &ciA[eFMT], &ciA[ePICXY], 
                              &ciA[eTNFMT], &ciA[eTNXY], &ciA[eTNSIZE],
                              &ciA[eHNDL], &ciA[eSIZE], &ciA[eDAY],
                              &ciA[eDATE], &ciA[eTIME] };

static char * pszMSG_Show   = szX;
static char * pszMSG_Column = szX;

CAMNLSPSZ   nlsCamCol[] = {
    { &pszMSG_Show,             MSG_Show },
    { &pszMSG_Column,           MSG_Column },
    { &ciA[eMARK].pszMenu,      MNU_Action },
    { &ciA[eNBR].pszMenu,       MNU_Number },          
    { &ciA[eTITLE].pszMenu,     MNU_Title },        
    { &ciA[eGROUP].pszMenu,     MNU_Group },        
    { &ciA[eFMT].pszMenu,       MNU_Format },  
    { &ciA[ePICXY].pszMenu,     MNU_XY },      
    { &ciA[eTNFMT].pszMenu,     MNU_ThumbFmt },  
    { &ciA[eTNXY].pszMenu,      MNU_ThumbXY },      
    { &ciA[eTNSIZE].pszMenu,    MNU_ThumbSize },    
    { &ciA[eHNDL].pszMenu,      MNU_Handle },    
    { &ciA[eSIZE].pszMenu,      MNU_Size },       
    { &ciA[eDAY].pszMenu,       MNU_Day },          
    { &ciA[eDATE].pszMenu,      MNU_Date },         
    { &ciA[eTIME].pszMenu,      MNU_Time },         
    { 0,                        0 }};

CAMNLSPCI   nlsCamColHdg[] = {
    { &ciA[eMARK],              COL_Mark },
    { &ciA[eNBR],               COL_Nbr },          
    { &ciA[eTITLE],             COL_Title },        
    { &ciA[eGROUP],             COL_Group },        
    { &ciA[eFMT],               COL_ImageFormat },  
    { &ciA[ePICXY],             COL_ImageXY },      
    { &ciA[eTNFMT],             COL_ThumbFormat },  
    { &ciA[eTNXY],              COL_ThumbXY },      
    { &ciA[eTNSIZE],            COL_ThumbSize },    
    { &ciA[eHNDL],              COL_Handle },    
    { &ciA[eSIZE],              COL_FileSize },       
    { &ciA[eDAY],               COL_Day },          
    { &ciA[eDATE],              COL_Date },         
    { &ciA[eTIME],              COL_Time },         
    { 0,                        0 }};


/****************************************************************************/
/****************************************************************************/

ULONG       InitCols( HWND hwnd)

{
    HWND        hCnr;
    ULONG       rc = 0;
    HMODULE     hmod;

do {
    hCnr = WinWindowFromID( hwnd, FID_CLIENT);

    LoadStaticStrings( nlsCamCol);
    FormatColumnHeadings();

    // get the sort pointer
    hSortPtr = WinLoadPointer( HWND_DESKTOP, 0, IDI_SORTPTR);
    if (hSortPtr == 0)
        hSortPtr = WinQuerySysPointer( HWND_DESKTOP, SPTR_ARROW, FALSE);

    // get the sizing pointer that the container control uses
    if (DosQueryModuleHandle( (PCSZ)"PMCTLS", &hmod) ||
        (hSizePtr = WinLoadPointer( HWND_DESKTOP, hmod, 100)) == 0)
        hSizePtr = WinQuerySysPointer( HWND_DESKTOP, SPTR_SIZEWE, FALSE);

    RestoreColumns();
    rc = InitFieldInfo( hCnr);
    if (rc)
        break;

} while (0);

    return (rc);
}

/****************************************************************************/

ULONG       InitFieldInfo( HWND hCnr)

{
    ULONG           rc = 0;
    ULONG           ctr;
    PFIELDINFO      pfi;
    PFIELDINFO      ptr;
    FIELDINFOINSERT fii;

do
{
    // allocate FIELDINFO structs for each column
    pfi = (PFIELDINFO)WinSendMsg( hCnr, CM_ALLOCDETAILFIELDINFO, (MP)eCNTCOLS, 0);
    if (pfi == 0)
        ERRNBR( 1)

    // initialize those structs from our column-info table;
    // each column will get one DWORD at the end of the
    // (MINI)RECORDCORE struct to store its data

    for (ptr=pfi, ctr=0; ctr < eCNTCOLS; ctr++, ptr=ptr->pNextFieldInfo)
    {
        ciX[ctr]->pfi = ptr;
        ptr->flData = ciX[ctr]->flData;
        ptr->flTitle = ciX[ctr]->flTitle;
        ptr->pTitleData = ciX[ctr]->pszTitle;
        ptr->offStruct = sizeof(CAM_RECTYPE) +
                         ((ciX[ctr]->flags & CIF_NDXMASK) * sizeof(ULONG));
        ptr->pUserData = ciX[ctr];
    }

    // insert the column info into the container
    fii.cb = sizeof(FIELDINFOINSERT);
    fii.pFieldInfoOrder = (PFIELDINFO)CMA_FIRST;
    fii.fInvalidateFieldInfo = FALSE;
    fii.cFieldInfoInsert = eCNTCOLS;
    if (WinSendMsg( hCnr, CM_INSERTDETAILFIELDINFO, (MP)pfi, (MP)&fii) == 0)
        ERRNBR( 2)

} while (0);

    return (rc);
}

/****************************************************************************/

void        ExitDetailsView( HWND hCnr)

{
    ulSplitCol = CalcSplitbarCol( hCnr);
    return;
}

/****************************************************************************/

void        EnterDetailsView( HWND hCnr)

{
    InitColumnWidths( hCnr);
    SubclassColumnHeadings( hCnr);
    return;
}

/****************************************************************************/

// Column widths in details view always have to be known so that manual
// width adjustment & click-to-sort will work.  To ensure the initial
// default values are reasonable, we don't make them "permanent" until
// there are records in the container - otherwise, many of the columns
// would be too narrow.

void        InitColumnWidths( HWND hCnr)

{
    CNRINFO     cnri;

    if (!fWidthSet) {
        WinSendMsg( hCnr, CM_QUERYCNRINFO, (MP)&cnri, (MP)sizeof(cnri));
        if (cnri.flWindowAttr & CV_DETAIL) {
            ResetColumnWidths( WinQueryWindow( hCnr, QW_PARENT), FALSE);
            if (cnri.cRecords)
                fWidthSet = TRUE;
        }
    }

    return;
}

/****************************************************************************/

BOOL        SubclassColumnHeadings( HWND hCnr)

{
    HWND        hTmp;
    PFNWP       pfnTmp;

    hTmp = WinWindowFromID( hCnr, CID_LEFTCOLTITLEWND);
    if (hTmp) {
        pfnTmp = WinSubclassWindow( hTmp, &LeftTitleSubProc);
        if (pfnTmp != &LeftTitleSubProc)
            pfnLeft = pfnTmp;
    }
    else
        printf( "CID_LEFTCOLTITLEWND not found\n");

    hTmp = WinWindowFromID( hCnr, CID_RIGHTCOLTITLEWND);
    if (hTmp) {
        pfnTmp = WinSubclassWindow( hTmp, &RightTitleSubProc);
        if (pfnTmp != &RightTitleSubProc)
            pfnRight = pfnTmp;
    }

    return (pfnLeft && pfnRight);
}

/****************************************************************************/

MRESULT _System LeftTitleSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    HWND        hCnr;
    ULONG       ctr;
    ULONG       ul;

    switch (msg)
    {
        case WM_MOUSEMOVE:
        {
            hCnr = WinQueryWindow( hwnd, QW_PARENT);
            ctr = QueryColumnIndex( CAM_LASTLEFT);
            ul = InColOrMargin( hwnd, hCnr, mp1, FIRSTLEFTCOL, &ctr);
            if (ul == INNEITHER)
                break;

            if (ul == INMARGIN)
                WinSetPointer( HWND_DESKTOP, hSizePtr);
            else
                WinSetPointer( HWND_DESKTOP, hSortPtr);

            return ((MRESULT)TRUE);
        }

        case WM_BUTTON1DOWN:
        {
            hCnr = WinQueryWindow( hwnd, QW_PARENT);
            ctr = QueryColumnIndex( CAM_LASTLEFT);
            if (InColOrMargin( hwnd, hCnr, mp1, FIRSTLEFTCOL, &ctr) != INMARGIN)
                break;

            pfnLeft( hwnd, msg, mp1, mp2);
            ResizeColumn( hwnd, hCnr, ctr);
            return ((MRESULT)TRUE);
        }

        case WM_BUTTON1CLICK:
        {
            hCnr = WinQueryWindow( hwnd, QW_PARENT);
            ctr = QueryColumnIndex( CAM_LASTLEFT);
            if (InColOrMargin( hwnd, hCnr, mp1, FIRSTLEFTCOL, &ctr) == INCOLUMN)
                SetSortColumn( WinQueryWindow( hCnr, QW_PARENT), ctr);

            break;
        }

        case WM_PRESPARAMCHANGED:
        {
            if ((ULONG)mp1 != PP_FONTNAMESIZE)
                break;

            pfnLeft( hwnd, msg, mp1, mp2);

            while ((hwnd = WinQueryWindow( hwnd, QW_PARENT)) != 0)
                if (WinQueryWindowUShort( hwnd, QWS_ID) == IDD_MAIN)
                    break;

            WinPostMsg( hwnd, WM_COMMAND, (MP)IDM_COLRESET, 0);
            return (0);
        }
    }

    return (pfnLeft( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

MRESULT _System RightTitleSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    HWND        hCnr;
    ULONG       ctr;
    ULONG       ul;

    switch (msg)
    {
        case WM_MOUSEMOVE:
        {
            hCnr = WinQueryWindow( hwnd, QW_PARENT);
            ctr = LASTRIGHTCOL;
            ul = QueryColumnIndex( CAM_FIRSTRIGHT);
            ul = InColOrMargin( hwnd, hCnr, mp1, ul, &ctr);
            if (ul == INNEITHER)
                break;

            if (ul == INMARGIN)
                WinSetPointer( HWND_DESKTOP, hSizePtr);
            else
                WinSetPointer( HWND_DESKTOP, hSortPtr);

            return ((MRESULT)TRUE);
        }

        case WM_BUTTON1DOWN:
        {
            hCnr = WinQueryWindow( hwnd, QW_PARENT);
            ctr = LASTRIGHTCOL;
            ul = QueryColumnIndex( CAM_FIRSTRIGHT);
            if (InColOrMargin( hwnd, hCnr, mp1, ul, &ctr) != INMARGIN)
                break;

            pfnRight( hwnd, msg, mp1, mp2);
            ResizeColumn( hwnd, hCnr, ctr);
            return ((MRESULT)TRUE);
        }

        case WM_BUTTON1CLICK:
        {
            hCnr = WinQueryWindow( hwnd, QW_PARENT);
            ctr = LASTRIGHTCOL;
            ul = QueryColumnIndex( CAM_FIRSTRIGHT);
            if (InColOrMargin( hwnd, hCnr, mp1, ul, &ctr) == INCOLUMN)
                SetSortColumn( WinQueryWindow( hCnr, QW_PARENT), ctr);

            break;
        }
    }

    return (pfnRight( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

ULONG       InColOrMargin( HWND hwnd, HWND hCnr, MPARAM mp1, ULONG first, PULONG plast)

{
    ULONG       ulRtn = INNEITHER;
    LONG        x = SHORT1FROMMP( mp1);
    LONG        lAvgLeft;
    LONG        lAvgRight;
    RECTL       rcl;

do
{
    // establish a small margin at the top of the title windows
    // to separate their active areas from the menubar
    if (WinQueryWindowRect( hwnd, &rcl) &&
        rcl.yTop > 16 &&
        SHORT2FROMMP( mp1) >= rcl.yTop-4)
        break;

    // see how far the window has been scrolled to the right
    WinSendMsg( hCnr, CM_QUERYVIEWPORTRECT, (MP)&rcl,
                MPFROM2SHORT( CMA_WORKSPACE, (*plast == LASTRIGHTCOL)));

    // adjust x accordingly
    x += rcl.xLeft;

    // see if x lies within one of the columns or no more than half a
    // character's width to the right of it;  if so, identify whether
    // it's in the margin (right edge +/- half a character) or in the
    // body of the column;  return that info along with the column nbr

    lAvgLeft = lAvg / 2;
    lAvgRight = lAvg - lAvgLeft;
    while (first <= *plast)
    {
        if (!(ciX[first]->pfi->flData & CFA_INVISIBLE) &&
            (x <= ciX[first]->lRight + lAvgLeft))
        {
            if (x >= ciX[first]->lRight - lAvgRight)
                ulRtn = INMARGIN;
            else
                ulRtn = INCOLUMN;
            *plast = first;
            break;
        }
        first++;
    }

} while (0);

    return (ulRtn);
}

/****************************************************************************/

void        ResizeColumn( HWND hTitle, HWND hCnr, ULONG ulCol)

{
    ULONG       ulFirstRight;
    ULONG       ulLastLeft;
    LONG        lChange;
    TRACKINFO   ti;

do
{
    memset( &ti, 0, sizeof(ti));

    // see how far the window has been scrolled to the right
    ulFirstRight = QueryColumnIndex( CAM_FIRSTRIGHT);
    WinSendMsg( hCnr, CM_QUERYVIEWPORTRECT, (MP)&ti.rclTrack,
                MPFROM2SHORT( CMA_WORKSPACE, (ulCol >= ulFirstRight)));
    lChange = ti.rclTrack.xLeft;

    // calculate tracking rectangle size & position;  it's 3 pixels wide,
    // centered on the column divider & runs from top to bottom of the cnr

    WinQueryWindowRect( hCnr, &ti.rclTrack);
    ti.rclTrack.xLeft = ciX[ulCol]->lRight - 1 - lChange;
    WinMapWindowPoints( hTitle, hCnr, (PPOINTL)&ti.rclTrack.xLeft, 1);
    ti.rclTrack.yBottom = 0;
    ti.rclTrack.xRight = ti.rclTrack.xLeft + 3;

    // identify the first visible right window column
    ulFirstRight = QueryColumnIndex( CAM_FIRSTVISIBLERIGHT);

    // calculate bounding rectangle size & position;  it's the full height
    // of the cnr and runs from 3 pixels to the right of the current column
    // out to the right edge of the current split window

    WinQueryWindowRect( hTitle, &ti.rclBoundary);
    if (ulCol == FIRSTLEFTCOL || ulCol == ulFirstRight)
        ti.rclBoundary.xLeft = 3 - lChange;
    else
        ti.rclBoundary.xLeft = ciX[ulCol-1]->lRight + 3 - lChange;
    if (ti.rclBoundary.xLeft < 0)
        ti.rclBoundary.xLeft = 0;
    WinMapWindowPoints( hTitle, hCnr, (PPOINTL)&ti.rclBoundary, 2);
    ti.rclBoundary.yBottom = 0;
    ti.rclBoundary.yTop = ti.rclTrack.yTop;

    // limit tracking rectangle's size
    ti.ptlMinTrackSize.x = 3;
    ti.ptlMinTrackSize.y = ti.rclTrack.yTop;
    ti.ptlMaxTrackSize.x = 3;
    ti.ptlMaxTrackSize.y = ti.rclTrack.yTop;

    // set border & flags
    ti.cxBorder = 3;
    ti.fs = TF_MOVE | TF_ALLINBOUNDARY;

    // save starting value as a negative
    lChange = -ti.rclTrack.xLeft;

    // do it, exit if cancelled
    if (WinTrackRect( hCnr, 0, &ti) == FALSE)
        break;

    // calc change in position, exit if no change
    lChange += ti.rclTrack.xLeft;
    if (lChange == 0)
        break;

    // update fieldinfo for current column, then adjust the extents
    // for this and the remaining columns in the current split window
    ciX[ulCol]->pfi->cxWidth += lChange;
    ulLastLeft = QueryColumnIndex( CAM_LASTLEFT);
    do
    {
        ciX[ulCol]->lRight += lChange;
    } while (ulCol != ulLastLeft && ulCol++ != LASTRIGHTCOL);

    // force a redisplay using the new sizes
    WinSendMsg( hCnr, CM_INVALIDATEDETAILFIELDINFO, 0, 0);

} while (0);

    return;
}

/****************************************************************************/
/****************************************************************************/

void        SetColumnVisibility( HWND hwnd, ULONG ulCmd)

{
    HWND        hCnr;
    HWND        hMenu;
    ULONG       ulCol;
    ULONG       ulID;
    BOOL        fInvisible;

    hCnr = WinWindowFromID( hwnd, FID_CLIENT);
    ulCol = QueryColumnIndex( ulCmd - IDM_COLFIRST);

    // ensure the first column is always visible
    if ((ciX[ulCol]->flags & CIF_NDXMASK) == eMARK)
        if (!(ciX[ulCol]->pfi->flData & CFA_INVISIBLE)) {
            ciX[ulCol]->flags &= ~CIF_INVISIBLE;
            return;
        }

    ciX[ulCol]->pfi->flData ^= CFA_INVISIBLE;
    fInvisible = (ciX[ulCol]->pfi->flData & CFA_INVISIBLE) ? TRUE : FALSE;
    if (fInvisible)
        ciX[ulCol]->flags |= CIF_INVISIBLE;
    else
        ciX[ulCol]->flags &= ~CIF_INVISIBLE;

    // force the container to recalc the width of newly-revealed columns
    if (fInvisible == FALSE) {
        WinSendMsg( hCnr, CM_INVALIDATEDETAILFIELDINFO, 0, 0);
        WinSendMsg( hCnr, CM_INVALIDATERECORD, 0, 0);
    }

    // update the container's layout
    GetColumnWidths( hCnr);

    if (ulCol <= QueryColumnIndex( CAM_LASTLEFT))
        SetSplitBar( hCnr, TRUE);

    hMenu = WinWindowFromID( hwnd, FID_MENU);

    ulID = IDM_COLFIRST + (ciX[ulCol]->flags & CIF_NDXMASK);
    WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( ulID, TRUE),
                MPFROM2SHORT( MIA_CHECKED, (fInvisible ?  0 : MIA_CHECKED)));

    ulID = IDM_SORTFIRST + (ciX[ulCol]->flags & CIF_NDXMASK);
    WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( ulID, TRUE),
                MPFROM2SHORT( MIA_DISABLED, (fInvisible ?  MIA_DISABLED : 0)));

    if (ciX[ulCol]->flags & CIF_SORTCOL)
        SetSortColumn( hwnd, QueryColumnIndex( CAM_DEFAULTSORTCOL));

    return;
}

/****************************************************************************/

void        GetColumnWidths( HWND hCnr)

{
    LONG            wide;
    LONG            narrow;
    LONG            width;
    LONG            col;
    LONG            total = 0;
    ULONG           ctr;
    ULONG           ulFirstRight;
    ULONG           curFirstRight;

    // since this function is called when the font gets changed,
    // we have to calc the average character width each time
    lAvg = GetAvgCharWidth( hCnr);

    // the docs say the left & right columns in a window have narrower
    // margins that the other columns;  AFAICT, only the left col does
    wide = 3 * lAvg;
    narrow = (5 * lAvg) / 2;

    // identify the first visible right window column
    ulFirstRight = QueryColumnIndex( CAM_FIRSTRIGHT);
    curFirstRight = QueryColumnIndex( CAM_FIRSTVISIBLERIGHT);

    for (ctr=0; ctr < eCNTCOLS; ctr++)
    {
        width = (LONG)WinSendMsg( hCnr, CM_QUERYDETAILFIELDINFO,
                                      (MP)ciX[ctr]->pfi, (MP)CMA_DATAWIDTH);

        ciX[ctr]->pfi->cxWidth = width;

        if (width == 0 || (ciX[ctr]->pfi->flData & CFA_INVISIBLE))
            col = 0;
        else
            if (ctr == FIRSTLEFTCOL || ctr == curFirstRight)
                col = width + narrow;
            else
                col = width + wide;

        if (ctr == FIRSTLEFTCOL || ctr == ulFirstRight)
            total = col;
        else
            total += col;
        ciX[ctr]->lRight = total;
    }

    WinSendMsg( hCnr, CM_INVALIDATEDETAILFIELDINFO, 0, 0);
//    WinSendMsg( hCnr, CM_INVALIDATERECORD, 0, 0);

    return;
}

/****************************************************************************/

LONG        GetAvgCharWidth( HWND hwnd)

{
    LONG        avg;
    HPS         hps = 0;
    FONTMETRICS fm;

    // get the average character width for the current font
    fm.lAveCharWidth = 0;
    hps = WinGetPS( hwnd);
    if (hps && GpiQueryFontMetrics( hps,
               LONGFIELDOFFSET( FONTMETRICS, lAveCharWidth)+sizeof(LONG), &fm))
        avg = fm.lAveCharWidth;
    else
        avg = 6;

    if (hps)
        WinReleasePS( hps);

    return (avg);
}

/****************************************************************************/

void        ResetColumnWidths( HWND hwnd, BOOL fResetSplit)

{
    HWND        hCnr;
    ULONG       ctr;

    hCnr = WinWindowFromID( hwnd, FID_CLIENT);

    // clear the fixed widths from the FIELDINFO structs
    for (ctr=0; ctr < eCNTCOLS; ctr++)
        ciX[ctr]->pfi->cxWidth = 0;

    // refresh all the records to recalculate the column widths
//    WinSendMsg( hCnr, CM_INVALIDATEDETAILFIELDINFO, 0, 0);
    WinSendMsg( hCnr, CM_INVALIDATERECORD, 0, 0);

    // get those widths, then reposition the splitbar
    GetColumnWidths( hCnr);
    SetSplitBar( hCnr, fResetSplit);

    return;
}

/****************************************************************************/

void        SetSplitBar( HWND hCnr, BOOL fReset)

{
    ULONG       ndx;
    CNRINFO     cnri;

    ndx = QueryColumnIndex( CAM_LASTVISIBLELEFT);

    if (fReset || ulSplitCol >= eCNTCOLS)
        ulSplitCol = ndx;
    else {
        if (ulSplitCol <= ndx && !(ciX[ulSplitCol]->flags & CIF_INVISIBLE))
            ndx = ulSplitCol;
        else
            ulSplitCol = ndx;
    }

    if (ciX[ndx]->lRight)
        cnri.xVertSplitbar = ciX[ndx]->lRight;
    else
        cnri.xVertSplitbar = 224;

    ndx = QueryColumnIndex( CAM_LASTLEFT);
    cnri.pFieldInfoLast = ciX[ndx]->pfi;
    WinSendMsg( hCnr, CM_SETCNRINFO, (MP)&cnri,
                (MP)(CMA_XVERTSPLITBAR | CMA_PFIELDINFOLAST));

    return;
}

/****************************************************************************/

ULONG       QueryColumnIndex( ULONG flag)

{
    ULONG       ctr;

    switch (flag) {

        case CAM_LASTLEFT:
            for (ctr = 0; ctr < eCNTCOLS; ctr++)
                if (ciX[ctr]->flags & CIF_LASTLEFT)
                    break;

            if (ctr >= eCNTCOLS)
                ctr = QueryColumnIndex( CAM_DEFAULTLASTLEFT);
            break;

        case CAM_LASTVISIBLELEFT:
            ctr = QueryColumnIndex( CAM_LASTLEFT);
            for (; ctr; ctr--)
                if (!(ciX[ctr]->pfi->flData & CFA_INVISIBLE))
                    break;
            break;

        case CAM_FIRSTRIGHT:
            ctr = QueryColumnIndex( CAM_LASTLEFT) + 1;
            break;

        case CAM_FIRSTVISIBLERIGHT:
            ctr = QueryColumnIndex( CAM_LASTLEFT) + 1;
            for (; ctr < eCNTCOLS; ctr++)
                if (!(ciX[ctr]->pfi->flData & CFA_INVISIBLE))
                    break;
            break;

        case CAM_DEFAULTLASTLEFT:
            for (ctr = 0; ctr < eCNTCOLS; ctr++)
                if (ciX[ctr] == &ciA[DEFAULTLASTLEFT])
                    break;
            break;

        case CAM_SORTCOL:
            for (ctr = 0; ctr < eCNTCOLS; ctr++)
                if (ciX[ctr]->flags & CIF_SORTCOL)
                    break;

            if (ctr >= eCNTCOLS)
                ctr = QueryColumnIndex( CAM_DEFAULTSORTCOL);
            break;

        case CAM_DEFAULTSORTCOL:
            for (ctr = 0; ctr < eCNTCOLS; ctr++)
                if (ciX[ctr] == &ciA[DEFAULTSORTCOL])
                    break;
/*
            if (ciX[ctr]->pfi->flData & CFA_INVISIBLE) {
                for (ctr = 1; ctr < eCNTCOLS; ctr++)
                    if (!(ciX[ctr]->pfi->flData & CFA_INVISIBLE))
                        break;

                if (ctr >= eCNTCOLS)
                    ctr = 0;
            }
*/
            break;

        default:
            if (flag < eCNTCOLS) {
                for (ctr = 0; ctr < eCNTCOLS; ctr++)
                    if (ciX[ctr] == &ciA[flag])
                        break;
                break;
            }

            ctr = eCNTCOLS;
            break;
    }

    return (ctr);
}

/****************************************************************************/
/****************************************************************************/

void        RestoreColumns( void)

{
    BOOL    fOK = FALSE;
    ULONG   ctr;
    ULONG   ulLeft = eCNTCOLS;
    ULONG   ulSort = eCNTCOLS;
    ULONG   ulSplit = eCNTCOLS;
    ULONG   aulOrder[eCNTCOLS];

do {
    // if the ini entry is missing or defective, use default values
    if (PrfQueryProfileSize( HINI_USERPROFILE, CAMINI_APP,
                             CAMINI_COLUMNS, &ctr) == FALSE ||
        ctr != sizeof(aulOrder) ||
        PrfQueryProfileData( HINI_USERPROFILE, CAMINI_APP,
                             CAMINI_COLUMNS, aulOrder, &ctr) == FALSE)
        break;

    // validate the indices & lastleft flag
    for (ctr = 0; ctr < eCNTCOLS; ctr++) {
        if ((aulOrder[ctr] & CIF_NDXMASK) >= eCNTCOLS)
            break;

        if (aulOrder[ctr] & CIF_LASTLEFT)
            if (ulLeft == eCNTCOLS)
                ulLeft = ctr;

        if (aulOrder[ctr] & CIF_SORTCOL)
            if (ulSort == eCNTCOLS)
                ulSort = ctr;

        if (aulOrder[ctr] & CIF_SPLITCOL)
            if (ulSplit == eCNTCOLS)
                ulSplit = ctr;
    }

    // if validation failed, use the defaults
    if (ctr < eCNTCOLS)
        break;

    // ensure the first column is visible
    aulOrder[0] &= ~CIF_INVISIBLE;

    // fill ciX with ptrs to ciA entries & set the entries' invisible flag
    for (ctr = 0; ctr < eCNTCOLS; ctr++) {
        ciX[ctr] = &ciA[(aulOrder[ctr] & CIF_NDXMASK)];
        if (aulOrder[ctr] & CIF_INVISIBLE) {
            ciX[ctr]->flags  |= CIF_INVISIBLE;
            ciX[ctr]->flData |= CFA_INVISIBLE;
        }
        else {
            ciX[ctr]->flags  &= ~CIF_INVISIBLE;
            ciX[ctr]->flData &= ~CFA_INVISIBLE;
        }
    }

    if (ulLeft >= eCNTCOLS)
        ulLeft = DEFAULTLASTLEFT;
    ciX[ulLeft]->flags |= CIF_LASTLEFT;

    if (ulSort >= eCNTCOLS)
        ulSort = DEFAULTSORTCOL;
    ciX[ulSort]->flags |= CIF_SORTCOL;

    if (ulSplit <= ulLeft)
        ulSplitCol = ulSplit;
    else
        ulSplitCol = eCNTCOLS;

    fOK = TRUE;

} while (0);

    if (!fOK) {
        ciA[DEFAULTLASTLEFT].flags |= CIF_LASTLEFT;
        ciA[DEFAULTSORTCOL].flags |= CIF_SORTCOL;
        for (ctr = 0; ctr < eCNTCOLS; ctr++)
            if (ciA[ctr].flData & CFA_INVISIBLE)
                ciA[ctr].flags |= CIF_INVISIBLE;
    }

    return;
}

/****************************************************************************/

void        StoreColumns( HWND hwnd)

{
    HWND        hCnr;
    ULONG       ctr;
    ULONG       aulOrder[eCNTCOLS];
    CNRINFO     cnri;

    for (ctr = 0; ctr < eCNTCOLS; ctr++)
        aulOrder[ctr] = ciX[ctr]->flags;

    hCnr = WinWindowFromID( hwnd, FID_CLIENT);
    WinSendMsg( hCnr, CM_QUERYCNRINFO, (MP)&cnri, (MP)sizeof(cnri));
    if (cnri.flWindowAttr & CV_DETAIL)
        ulSplitCol = CalcSplitbarCol( hCnr);

    aulOrder[ulSplitCol] |= CIF_SPLITCOL;

    PrfWriteProfileData( HINI_USERPROFILE, CAMINI_APP, CAMINI_COLUMNS,
                         aulOrder, sizeof(aulOrder));

    return;
}

/****************************************************************************/

ULONG       CalcSplitbarCol( HWND hCnr)

{
    ULONG       ctr;
    ULONG       prev;
    ULONG       last;
    LONG        x;
    RECTL       rcl;

    WinSendMsg( hCnr, CM_QUERYVIEWPORTRECT, (MP)&rcl,
                MPFROM2SHORT( CMA_WORKSPACE, FALSE));

    x = rcl.xRight;
    last = QueryColumnIndex( CAM_LASTVISIBLELEFT);
    for (ctr = prev = FIRSTLEFTCOL; ctr <= last; ctr++) {

        if (ciX[ctr]->pfi->flData & CFA_INVISIBLE)
            continue;

        if (x > ciX[ctr]->lRight) {
            prev = ctr;
            continue;
        }

        if (x < (ciX[prev]->lRight + ciX[ctr]->lRight) / 2)
            ctr = prev;
        break;
    }

    if (ctr > last)
        ctr = last;

    return (ctr);
}

/****************************************************************************/

void        FormatColumnHeadings( void)

{
    char *      pIn;
    char *      pOut;
    CAMNLSPCI * pnls;
    char        szIn[32];
    char        szOut[32];

    for (pnls = nlsCamColHdg; pnls->pci; pnls++) {
        LoadString( pnls->str, szIn);

        pIn = strchr( szIn, '\n');
        if (pIn) {
            *pIn++ = 0;
            strcpy( szOut, szIn);
            pOut = strchr( szOut, 0);
        }
        else {
            pIn = szIn;
            pOut = szOut;
        }

        *pOut++ = '\n';
        *pOut = 0;

        if (*pIn) {
            if (pnls->pci->flTitle & CFA_CENTER) {
                strcpy( pOut, "  ");
                pOut += 2;
            }
            strcpy( pOut, pIn);
        }

        strcat( pOut, "  ");
        pnls->pci->pszTitle = strdup( szOut);
    }

    return;
}

/****************************************************************************/
/****************************************************************************/

MRESULT _System ColumnsDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    switch (msg)
    {
        case WM_COMMAND:
            switch (SHORT1FROMMP( mp1)) {

                case IDC_UP:
                    ChangeColumnOrder( hwnd, TRUE);
                    return (0);

                case IDC_DOWN:
                    ChangeColumnOrder( hwnd, FALSE);
                    return (0);

                default:
                    WinDestroyWindow( hwnd);
                    return (0);
            }
            break;

        case WM_CONTROL:
            if (mp1 == MPFROM2SHORT( IDC_COLLIST, CLN_CLICKED)) {
                CLITEM *    pcli = (CLITEM *)mp2;
                COLINFO *   pci;

                if (pcli->hndl == (ULONG)-1)
                    pcli->chk = CLF_CHECKED | CLF_CHKDISABLED;
                else {
                    pci = (COLINFO*)pcli->hndl;
                    SetColumnVisibility( WinQueryWindow( hwnd, QW_OWNER),
                        IDM_COLFIRST + (pci->flags & CIF_NDXMASK));
                    if (pci->flags & CIF_INVISIBLE)
                        pcli->chk &= ~CLF_CHECKED;
                    else
                    	pcli->chk |= CLF_CHECKED;
                }
            }
            else
            if (mp1 == MPFROM2SHORT( IDC_COLLIST, CLN_SELECTED)) {
                CLITEM *    pcli = (CLITEM *)mp2;

                WinEnableWindow( WinWindowFromID( hwnd, IDC_UP),
                                 ((pcli->ndx > 1) ? TRUE : FALSE));
                WinEnableWindow( WinWindowFromID( hwnd, IDC_DOWN),
                                 ((pcli->ndx > 0 && pcli->ndx < eCNTCOLS) ?
                                  TRUE : FALSE));
            }

            return (0);

        case WM_FOCUSCHANGE:
            if (SHORT1FROMMP(mp2) && !WinIsWindowEnabled( hwnd)) {
                WinPostMsg( WinQueryWindow( hwnd, QW_OWNER),
                            CAMMSG_FOCUSCHANGE, 0, 0);
                return (0);
            }
            break;

        case WM_SYSCOMMAND:
            if (SHORT1FROMMP( mp1) != SC_CLOSE)
                break;
            WinDestroyWindow( hwnd);
            return (0);

        case WM_CLOSE:
            WinDestroyWindow( hwnd);
            return (0);

        case WM_INITDLG:
            return ((MRESULT)InitColumnsDlg( hwnd));

        case WM_WINDOWPOSCHANGED:
            WinDefDlgProc( hwnd, msg, mp1, mp2);
            if (((PSWP)mp1)->fl & SWP_SIZE)
                ReformatColumnsDlg( hwnd, (PSWP)mp1);
            return (0);

        case WM_SAVEAPPLICATION:
            WinStoreWindowPos( CAMINI_APP, CAMINI_COLPOS, hwnd);
            break;

        case WM_DESTROY: {
            HACCEL  haccel = WinQueryAccelTable( 0, hwnd);
            char *  pBuf = (char*)WinQueryWindowULong( hwnd, QWL_USER);

            if (pBuf)
                free( pBuf);
            if (haccel)
                WinDestroyAccelTable( haccel);

            WindowClosed( IDD_COLUMNS);
            break;
        }

    } //end switch (msg)

    return (WinDefDlgProc( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

LONG        InitColumnsDlg( HWND hwnd)

{
    LONG            lRtn = -1;
    HWND            hCL;
    HACCEL          haccel;
    ULONG           ctr;
    ULONG           cnt;
    char *          ptr;
    char *          pBuf;
    CLITEM *        pcli;
    CLITEM          cli[eCNTCOLS+1];

do
{
    hCL = WinWindowFromID( hwnd, IDC_COLLIST);
    if (!CL_Init( hCL, 0, pszMSG_Show, pszMSG_Column))
        break;

    // set up to create copies of the menu strings minus the tilde
    for (ctr=0, cnt=0; ctr < eCNTCOLS; ctr++)
        cnt += strlen( ciX[ctr]->pszMenu) + 1;
    cnt += strlen( szColSep) + 1;
    pBuf = malloc( cnt);
    if (!pBuf)
        break;
    WinSetWindowULong( hwnd, QWL_USER, (ULONG)pBuf);

    for (ctr=0, pcli = cli; ctr < eCNTCOLS; ctr++, pcli++) {
        pcli->ndx = -1;
        if (ctr == 0)
            pcli->chk = CLF_CHECKED | CLF_CHKDISABLED;
        else
            pcli->chk = (ciX[ctr]->flags & CIF_INVISIBLE) ?
                                        CLF_UNCHECKED : CLF_CHECKED;
        pcli->hndl = (ULONG)ciX[ctr];
        pcli->str = pBuf;

        for (ptr = ciX[ctr]->pszMenu; *ptr; ptr++)
            if (*ptr != '~')
                *pBuf++ = *ptr;
        *pBuf++ = 0;

        if (ciX[ctr]->flags & CIF_LASTLEFT) {
            pcli++;
            pcli->ndx = -1;
            pcli->chk = CLF_CHECKED | CLF_CHKDISABLED;
            pcli->hndl = (ULONG)-1;
            pcli->str = pBuf;
            strcpy( pBuf, szColSep);
            pBuf = strchr( pBuf, 0) + 1;
        }
    }

    if (!WinSendMsg( hCL, CLM_INSERTITEM, (MP)cli, (MP)eCNTCOLS+1))
        break;

    haccel = WinLoadAccelTable( 0, 0, IDA_UPDOWN);
    if (!haccel || !WinSetAccelTable( 0, haccel, hwnd))
        printf( "ColDlg - unable to load accelerator table - haccel= %x\n",
                (int)haccel);

    ShowDialog( hwnd, CAMINI_COLPOS);

    lRtn = 0;

} while (0);

    return (lRtn);
}

/****************************************************************************/

void        ChangeColumnOrder( HWND hwnd, BOOL fUp)

{
    BOOL        fMove;
    LONG        lCol;
    LONG        lSplit;
    LONG        lChg;
    LONG        lNewNdx;
    HWND        hCL;
    HWND        hCnr;
    HWND        hMain;
    COLINFO *   pSwap;
    FIELDINFOINSERT fii;
    CLITEM      cli;

do
{
    hMain = WinQueryWindow( hwnd, QW_OWNER);
    hCnr = WinWindowFromID( hMain, FID_CLIENT);
    hCL = WinWindowFromID( hwnd, IDC_COLLIST);

    if (!WinSendMsg( hCL, CLM_QUERYITEM, (MP)CLF_SELECTED, (MP)&cli))
        break;

    // prevent the first item from being moved & the other items
    // from being moved beyond the ends of the list
    if (cli.ndx <= 0 || (cli.ndx == 1 && fUp) ||
        (cli.ndx == eCNTCOLS && fUp == FALSE)) {
        WinAlarm( HWND_DESKTOP, WA_ERROR);
        break;
    }

    if (cli.hndl == (ULONG)-1)
        lCol = -1;
    else
        for (lCol = 0; lCol < eCNTCOLS; lCol++)
            if (ciX[lCol] == (COLINFO*)cli.hndl)
                break;

    if (lCol >= eCNTCOLS)
        break;

    // get the splitbar's current position & init some flags
    // that will be used when moving the splitbar and/or fieldinfo
    lSplit = (LONG)QueryColumnIndex( CAM_LASTLEFT);
    lChg = (fUp ? -1 : 1);
    lNewNdx = cli.ndx + lChg;
    fMove = FALSE;

    // determine if the spltbar's position will change and whether
    // that's the only real change - if so, we can avoid removing &
    // reinserting the fieldinfo
    ciX[lSplit]->flags &= ~CIF_LASTLEFT;
    if (lCol == -1)
        lSplit += lChg;
    else
    if (lCol == lSplit) {
        lSplit--;
        if (fUp)
            fMove = TRUE;
        else
            lChg = 0;
    }
    else
    if (lCol == lSplit+1 && fUp) {
        lSplit++;
        lChg = 0;
    }
    else
    if (lCol == lSplit-1 && !fUp) {
        lSplit--;
        fMove = TRUE;
    }
    else
        fMove = TRUE;
    ciX[lSplit]->flags |= CIF_LASTLEFT;

    // rearrange items in the checklist
    if (!WinSendMsg( hCL, CLM_MOVEITEM, (MP)cli.ndx, (MP)lNewNdx))
        break;

    WinEnableWindow( WinWindowFromID( hwnd, IDC_UP),
                                 ((lNewNdx > 1) ? TRUE : FALSE));
    WinEnableWindow( WinWindowFromID( hwnd, IDC_DOWN),
                                 ((lNewNdx > 0 && lNewNdx < eCNTCOLS) ?
                                  TRUE : FALSE));

    // remove & reinsert the fieldinfo, then update the column index array
    if (fMove) {
        fii.cb = sizeof(FIELDINFOINSERT);
        fii.fInvalidateFieldInfo = FALSE;
        fii.cFieldInfoInsert = 1;
        if (fUp)
            fii.pFieldInfoOrder = ciX[lCol-2]->pfi;
        else
            fii.pFieldInfoOrder = ciX[lCol+1]->pfi;

        WinSendMsg( hCnr, CM_REMOVEDETAILFIELDINFO,
                                (MP)&(ciX[lCol]->pfi), MPFROM2SHORT( 1, 0));
        WinSendMsg( hCnr, CM_INSERTDETAILFIELDINFO,
                                (MP)ciX[lCol]->pfi, (MP)&fii);

        pSwap = ciX[lCol];
        ciX[lCol] = ciX[lCol+lChg];
        ciX[lCol+lChg] = pSwap;
    }

    // make the changes visible
    ResetColumnWidths( hMain, TRUE);

    // recreate the menus in the new order
    InitSortMenu( hMain);

} while (0);

    return;
}

/****************************************************************************/

void        ReformatColumnsDlg( HWND hwnd, PSWP pswp)
 
{
    HWND    hTemp;
    LONG    lChg;
    ULONG   ctr;
    SWP     aswp[3];

    hTemp = WinWindowFromID( hwnd, IDC_COLLIST);
    WinQueryWindowPos( hTemp, &aswp[0]);
    aswp[0].cx += pswp[0].cx - pswp[1].cx;
    aswp[0].cy += pswp[0].cy - pswp[1].cy;
    aswp[0].fl  = SWP_SIZE;
    ctr = 1;

    if (pswp[0].cx != pswp[1].cx) {

        hTemp = WinWindowFromID( hwnd, IDC_UP);
        WinQueryWindowPos( hTemp, &aswp[1]);
        aswp[1].fl = SWP_MOVE;

        hTemp = WinWindowFromID( hwnd, IDC_DOWN);
        WinQueryWindowPos( hTemp, &aswp[2]);
        aswp[2].fl = SWP_MOVE;

        lChg = (pswp[0].cx - aswp[2].x - aswp[2].cx - aswp[1].x) / 2;
        aswp[1].x += lChg;
        aswp[2].x += lChg;
        ctr = 3;
    }

    WinSetMultWindowPos( 0, aswp, ctr);

    return;
}

/****************************************************************************/

