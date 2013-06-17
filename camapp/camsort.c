/****************************************************************************/
// camsort.c
//
//  - everything related to sorting the container:  init, save,
//    menus, and the sort functions themselves
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

void            InitSort( HWND hwnd);
void            InitSortMenu( HWND hwnd);
void            RestoreSort( void);
void            StoreSort( void);
void            SetSortIndicatorMenu( HWND hwnd, ULONG ulCmd);
void            SetSortIndicators( BOOL fLiteral);
void            SetSortColumn( HWND hwnd, ULONG ulCmd);
PVOID           QuerySortFunctionPtr( void);
void            SetFileSaveSortOrder( HWND hwnd, BOOL fStart);
SHORT   _System SortByMark( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByHandle( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByNumber( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByTitle( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByGroup( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByFormat( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByPicXY( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortBySize( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByDay( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByDate( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByTime( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByTNFmt( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByTNXY( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);
SHORT   _System SortByTNSize( CAMRecPtr p1, CAMRecPtr p2, PVOID pv);

/****************************************************************************/

#define CAM_UPCHAR      0x1E
#define CAM_DNCHAR      0x1F

// undocumented menu msg - thanks to Martin LaFaix
#define MM_SETITEMCHECKMARK 0x0210

/****************************************************************************/

static SHORT        sSortSense;
static char         chAsc;
static char         chDesc;
static HBITMAP      hUpBmp = 0;
static HBITMAP      hDownBmp = 0;
static HBITMAP      hbmAsc;
static HBITMAP      hbmDesc;

extern COLINFO  *   ciX[eCNTCOLS];

/****************************************************************************/

void        InitSort( HWND hwnd)

{
    HPS         hps;

    // load the sort menu's custom checkmarks
    hps = WinGetPS( hwnd);
    if (hps)
    {
        hUpBmp = GpiLoadBitmap( hps, 0, IDB_UPBMP, 0, 0);
        hDownBmp = GpiLoadBitmap( hps, 0, IDB_DOWNBMP, 0, 0);
        WinReleasePS( hps);
    }
    // if that failed, use the standard checkmark
    if (hUpBmp == 0)
        hUpBmp = WinGetSysBitmap( HWND_DESKTOP, SBMP_MENUCHECK);
    if (hDownBmp == 0)
        hDownBmp = WinGetSysBitmap( HWND_DESKTOP, SBMP_MENUCHECK);

    RestoreSort();
    InitSortMenu( hwnd);

    return;
}

/****************************************************************************/

void        InitSortMenu( HWND hwnd)

{
    SHORT       pos;
    ULONG       ctr;
    HWND        hMenu;
    MENUITEM    mi;

    SetSortIndicatorMenu( hwnd, 0);

    // get the Sort submenu
    hMenu = WinWindowFromID( hwnd, FID_MENU);
    WinSendMsg( hMenu, MM_QUERYITEM, MPFROM2SHORT( IDM_SORT, 0), &mi);
    hMenu = mi.hwndSubMenu;

    pos = SHORT1FROMMR( WinSendMsg( hMenu, MM_ITEMPOSITIONFROMID,
                                    MPFROM2SHORT( IDM_SORTFIRST, 0), 0));
    if (pos < 0)
        pos = 2;

    for (ctr = 0; ctr < eCNTCOLS; ctr++)
        if (SHORT1FROMMR( WinSendMsg( hMenu, MM_DELETEITEM,
                          MPFROM2SHORT( IDM_SORTFIRST + ctr, 0), 0) <= 0)) {
            printf( "InitSortMenu - early exit, ctr= %d\n", (int)ctr);
            break;
        }

    mi.afStyle = MIS_TEXT;
    mi.hwndSubMenu = 0;
    mi.hItem = 0;

    // insert a menuitem for each column
    for (ctr = 0; ctr < eCNTCOLS; ctr++, pos++) {
        mi.iPosition = pos;
        if (ciX[ctr]->flags & CIF_INVISIBLE)
            mi.afAttribute = MIA_DISABLED | MIA_NODISMISS;
        else
            mi.afAttribute = MIA_NODISMISS;
        mi.id = (USHORT)(IDM_SORTFIRST + (ciX[ctr]->flags & CIF_NDXMASK));

        WinSendMsg( hMenu, MM_INSERTITEM, (MP)&mi, (MP)ciX[ctr]->pszMenu);
    }

    // update the sort info without changing the sort column or direction
    SetSortColumn( hwnd, (ULONG)-1);

    return;
}

/****************************************************************************/

void        RestoreSort( void)

{
    USHORT  ausSort[2];
    ULONG   ctr;

    // if the ini entry is missing or defective, use default values
    if (PrfQueryProfileSize( HINI_USERPROFILE, CAMINI_APP,
                             CAMINI_SORT, &ctr) == FALSE ||
        ctr != sizeof( ausSort) ||
        PrfQueryProfileData( HINI_USERPROFILE, (PCSZ)CAMINI_APP,
                             CAMINI_SORT, ausSort, &ctr) == FALSE)
    {
        ausSort[0] = FALSE;
        ausSort[1] = FALSE;
    }

    // set the sort sense, and sort indicators
    sSortSense = (ausSort[0] ? -1 : 1);
    SetSortIndicators( ausSort[1]);

    return;
}

/****************************************************************************/

void        StoreSort( void)

{
    SHORT   asSort[2];

    // if these values are all set to their defaults, eliminate any
    // existing ini entry;  otherwise, construct the entry & save it
    if (sSortSense == 1 && chAsc == 0x1F)
        PrfWriteProfileData( HINI_USERPROFILE, CAMINI_APP, CAMINI_SORT, 0, 0);
    else
    {
        asSort[0] = (sSortSense < 0 ? TRUE : FALSE);
        asSort[1] = (chAsc == 0x1F ? FALSE : TRUE);
        PrfWriteProfileData( HINI_USERPROFILE, CAMINI_APP, CAMINI_SORT,
                             asSort, sizeof(asSort));
    }

    return;
}

/****************************************************************************/

// this may be called with ulCmd == 0 in which case fLiteral isn't changed

void        SetSortIndicatorMenu( HWND hwnd, ULONG ulCmd)

{
    BOOL    fLiteral;

    if (ulCmd == IDM_INTUITIVE)
        fLiteral = FALSE;
    else
    if (ulCmd == IDM_LITERAL)
        fLiteral = TRUE;
    else
        fLiteral = (chAsc == CAM_UPCHAR);

    WinSendDlgItemMsg( hwnd, FID_MENU, MM_SETITEMATTR,
                       MPFROM2SHORT( IDM_INTUITIVE, TRUE),
                       MPFROM2SHORT( MIA_CHECKED,
                                     (fLiteral ? 0 : MIA_CHECKED)));

    WinSendDlgItemMsg( hwnd, FID_MENU, MM_SETITEMATTR,
                       MPFROM2SHORT( IDM_LITERAL, TRUE),
                       MPFROM2SHORT( MIA_CHECKED,
                                     (fLiteral ? MIA_CHECKED : 0)));

    SetSortIndicators( fLiteral);

    return;
}

/****************************************************************************/

void        SetSortIndicators( BOOL fLiteral)

{
    if (fLiteral)
    {
        chAsc = CAM_UPCHAR;
        chDesc = CAM_DNCHAR;
        hbmAsc = hUpBmp;
        hbmDesc = hDownBmp;
    }
    else
    {
        chAsc = CAM_DNCHAR;
        chDesc = CAM_UPCHAR;
        hbmAsc = hDownBmp;
        hbmDesc = hUpBmp;
    }

    return;
}

/****************************************************************************/

void        SetSortColumn( HWND hwnd, ULONG ulCmd)

{
    HWND        hMenu;
    HWND        hCnr;
    ULONG       ulCol;
    ULONG       ulSortCol;
    ULONG       ulID;
    MENUITEM    mi;

    hMenu = WinWindowFromID( hwnd, FID_MENU);
    WinSendMsg( hMenu, MM_QUERYITEM, MPFROM2SHORT( IDM_SORT, FALSE), (MP)&mi);
    hMenu = mi.hwndSubMenu;

    if (ulCmd >= IDM_SORTFIRST && ulCmd < IDM_SORTFIRST + eCNTCOLS)
        ulCol = QueryColumnIndex( ulCmd - IDM_SORTFIRST);
    else
        ulCol = ulCmd;

    ulSortCol = QueryColumnIndex( CAM_SORTCOL);

    // every change requires us to uncheck the currently checked
    // menu item in order to get the menu to repaint properly;
    ulID = IDM_SORTFIRST + (ciX[ulSortCol]->flags & CIF_NDXMASK);
    WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( ulID, FALSE),
                MPFROM2SHORT( MIA_CHECKED, 0));

    // if the selected column is the current sort column, change the
    // sort direction;  otherwise, remove the sort indicator from the
    // previous sort column, save the new one, and set the direction
    // to ascending

    if (ulSortCol == ulCol)
        sSortSense = -sSortSense;
    else
    {
        *(strchr( ciX[ulSortCol]->pszTitle, 0) - 1) = ' ';

        if (ulCol != (ULONG)-1)
        {
            ciX[ulSortCol]->flags &= ~CIF_SORTCOL;
            ulSortCol = ulCol;
            sSortSense = 1;
            ciX[ulSortCol]->flags |= CIF_SORTCOL;
        }
    }

    ulID = IDM_SORTFIRST + (ciX[ulSortCol]->flags & CIF_NDXMASK);

    // set the customized checkmark - note:  the high order word
    // of mp1 has to be true for this to work correctly
    WinSendMsg( hMenu, MM_SETITEMCHECKMARK, MPFROM2SHORT( ulID, TRUE),
                (MP)(sSortSense > 0 ? hbmAsc : hbmDesc));

    // check the appropriate menu item
    WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( ulID, FALSE),
                MPFROM2SHORT( MIA_CHECKED, MIA_CHECKED));

    // replace the last character of the title (which is a space)
    // with the sort indicator;  it's done this way to keep the column
    // title from shifting position as the indicator is added & removed
    *(strchr( ciX[ulSortCol]->pszTitle, 0) - 1) =
                                    (sSortSense > 0 ? chAsc : chDesc);

    // sort the records using the sort function for this column;
    // this will cause the entire window to be repainted so we
    // don't need to do anything to get the column titles updated
    // unless we're in demo mode where there are no records to sort
    hCnr = WinWindowFromID( hwnd, FID_CLIENT);
    if (GetCamPtr())
        WinSendMsg( hCnr, CM_SORTRECORD, (MP)ciX[ulSortCol]->pvSort, 0);
    else {
        WinInvalidateRect( hCnr, 0, TRUE);
        WinUpdateWindow( hCnr);
    }

    return;
}

/****************************************************************************/

PVOID       QuerySortFunctionPtr( void)

{
    ULONG   ctr;

    ctr = QueryColumnIndex( CAM_SORTCOL);
    return (ciX[ctr]->pvSort);
}

/****************************************************************************/

void        SetFileSaveSortOrder( HWND hwnd, BOOL fStart)

{
static  ULONG   ulSortSave = 0;
static  SHORT   sSenseSave = 1;

    ULONG   ctr;

    if (fStart) {
        ulSortSave = QueryColumnIndex( CAM_SORTCOL);
        sSenseSave = sSortSense;
        ctr = QueryColumnIndex( eDATE);
        if (ulSortSave != ctr || sSenseSave != 1) {
            sSortSense = 1;
            *(strchr( ciX[ulSortSave]->pszTitle, 0) - 1) = ' ';
            WinSendDlgItemMsg( hwnd, FID_CLIENT, CM_SORTRECORD,
                               (MP)&SortByDate, 0);
        }
    }
    else {
        ctr = QueryColumnIndex( eDATE);
        if (ulSortSave != ctr || sSenseSave != sSortSense) {
            sSortSense = sSenseSave;
            *(strchr( ciX[ulSortSave]->pszTitle, 0) - 1) =
                                    (sSortSense > 0 ? chAsc : chDesc);
            WinSendDlgItemMsg( hwnd, FID_CLIENT, CM_SORTRECORD,
                               (MP)ciX[ulSortSave]->pvSort, 0);
        }
    }

    return;
}

/****************************************************************************/
/****************************************************************************/

SHORT   _System SortByMark( CAMRecPtr p1, CAMRecPtr p2, PVOID pv)

{
    SHORT       sRtn;
    SHORT       stat1;
    SHORT       stat2;

    stat1 = p1->status & STAT_MASK;
    stat2 = p2->status & STAT_MASK;

    // sort order: blank, save, delete
    sRtn = (stat1 - stat2) * sSortSense;
    if (sRtn == 0)
        sRtn = SortByNumber( p1, p2, pv);

    return (sRtn);
}

/****************************************************************************/

SHORT   _System SortByHandle( CAMRecPtr p1, CAMRecPtr p2, PVOID pv)

{
    LONG        lRtn;
    SHORT       sRtn;

    lRtn = (LONG)p1->hndl - (LONG)p2->hndl;

    if (lRtn == 0)
        sRtn = SortByNumber( p1, p2, pv);
    else
        sRtn = (SHORT)((lRtn > 0) ? sSortSense : -sSortSense);

    return (sRtn);
}

/****************************************************************************/

SHORT   _System SortByNumber( CAMRecPtr p1, CAMRecPtr p2, PVOID pv)

{
    LONG        lRtn;
    SHORT       sRtn;

    lRtn = (LONG)p1->nbr - (LONG)p2->nbr;

    if (lRtn == 0)
        sRtn = 0;
    else
        sRtn = (SHORT)((lRtn > 0) ? sSortSense : -sSortSense);

    return (sRtn);
}

/****************************************************************************/

SHORT   _System SortByTitle( CAMRecPtr p1, CAMRecPtr p2, PVOID pv)

{
    SHORT       sRtn;

    sRtn = (SHORT)stricmp( p1->title, p2->title) * sSortSense;

    if (sRtn == 0)
        sRtn = SortByNumber( p1, p2, pv);

    return (sRtn);
}

/****************************************************************************/

SHORT   _System SortByGroup( CAMRecPtr p1, CAMRecPtr p2, PVOID pv)

{
    SHORT       sRtn;

    sRtn = (SHORT)stricmp( p1->group, p2->group) * sSortSense;

    if (sRtn == 0)
        sRtn = SortByNumber( p1, p2, pv);

    return (sRtn);
}

/****************************************************************************/

SHORT   _System SortByFormat( CAMRecPtr p1, CAMRecPtr p2, PVOID pv)

{
    SHORT       sRtn;

    sRtn = (SHORT)stricmp( p1->fmt, p2->fmt) * sSortSense;

    if (sRtn == 0)
        sRtn = SortByNumber( p1, p2, pv);

    return (sRtn);
}

/****************************************************************************/

SHORT   _System SortByPicXY( CAMRecPtr p1, CAMRecPtr p2, PVOID pv)

{
    LONG        lRtn;
    SHORT       sRtn;

    lRtn = (LONG)p1->picx - (LONG)p2->picx;

    if (lRtn == 0)
        lRtn = (LONG)p1->picy - (LONG)p2->picy;

    if (lRtn == 0)
        sRtn = SortByNumber( p1, p2, pv);
    else
        sRtn = (SHORT)((lRtn > 0) ? sSortSense : -sSortSense);

    return (sRtn);
}

/****************************************************************************/

SHORT   _System SortBySize( CAMRecPtr p1, CAMRecPtr p2, PVOID pv)

{
    LONG        lRtn;
    SHORT       sRtn;

    lRtn = (LONG)p1->size - (LONG)p2->size;

    if (lRtn == 0)
        sRtn = SortByNumber( p1, p2, pv);
    else
        sRtn = (SHORT)((lRtn > 0) ? sSortSense : -sSortSense);

    return (sRtn);
}

/****************************************************************************/

SHORT   _System SortByDay( CAMRecPtr p1, CAMRecPtr p2, PVOID pv)

{
    LONG        lRtn;
    SHORT       sRtn;

    lRtn = (LONG)p1->day - (LONG)p2->day;

    if (lRtn == 0)
        sRtn = SortByNumber( p1, p2, pv);
    else
        sRtn = (SHORT)((lRtn > 0) ? sSortSense : -sSortSense);

    return (sRtn);
}

/****************************************************************************/

SHORT   _System SortByDate( CAMRecPtr p1, CAMRecPtr p2, PVOID pv)

{
    LONG        lRtn;
    SHORT       sRtn;

    lRtn = *((PLONG)(&p1->date)) - *((PLONG)(&p2->date));

    if (lRtn == 0)
        sRtn = SortByTime( p1, p2, pv);
    else
        sRtn = (SHORT)((lRtn > 0) ? sSortSense : -sSortSense);

    return (sRtn);
}

/****************************************************************************/

SHORT   _System SortByTime( CAMRecPtr p1, CAMRecPtr p2, PVOID pv)

{
    SHORT       sRtn;

    sRtn = (p1->time.hours - p2->time.hours);
    if (sRtn == 0) {
        sRtn = (p1->time.minutes - p2->time.minutes);
        if (sRtn == 0) 
            sRtn = (p1->time.seconds - p2->time.seconds);
    }

    if (sRtn == 0)
        sRtn = SortByNumber( p1, p2, pv);
    else
        sRtn *= sSortSense;

    return (sRtn);
}

/****************************************************************************/

SHORT   _System SortByTNFmt( CAMRecPtr p1, CAMRecPtr p2, PVOID pv)

{
    SHORT       sRtn;

    sRtn = (SHORT)stricmp( p1->tnfmt, p2->tnfmt) * sSortSense;

    if (sRtn == 0)
        sRtn = SortByNumber( p1, p2, pv);

    return (sRtn);
}

/****************************************************************************/

SHORT   _System SortByTNXY( CAMRecPtr p1, CAMRecPtr p2, PVOID pv)

{
    LONG        lRtn;
    SHORT       sRtn;

    lRtn = (LONG)p1->tnx - (LONG)p2->tnx;

    if (lRtn == 0)
        lRtn = (LONG)p1->tny - (LONG)p2->tny;

    if (lRtn == 0)
        sRtn = SortByNumber( p1, p2, pv);
    else
        sRtn = (SHORT)((lRtn > 0) ? sSortSense : -sSortSense);

    return (sRtn);
}

/****************************************************************************/

SHORT   _System SortByTNSize( CAMRecPtr p1, CAMRecPtr p2, PVOID pv)

{
    LONG        lRtn;
    SHORT       sRtn;

    lRtn = (LONG)p1->tnsize - (LONG)p2->tnsize;

    if (lRtn == 0)
        sRtn = SortByNumber( p1, p2, pv);
    else
        sRtn = (SHORT)((lRtn > 0) ? sSortSense : -sSortSense);

    return (sRtn);
}

/****************************************************************************/

