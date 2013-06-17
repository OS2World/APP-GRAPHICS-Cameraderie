/****************************************************************************/
// camchklist.c
//
//  - a checklist control that starts life as a static text control
//
//  Usage:
//  - in your dialog, create a static text control of the appropriate size
//  - in your init code, call CL_Init() with the handle of the static
//  - CL_Init() will subclass the static & create a container control
//    as its child that is the exact same size as itself
//  - communicate with the checklist using the (former) static's hwnd;
//    CLM_* messages and CLN_* notifications are listed in camchklist.c
//
//  Note:
//  - the control does _not_ make a copy of the strings you pass to it;
//    if you query an item's text, you will get back the pointer you gave it
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

#include <string.h>
#include <stdio.h>
#include "camchklist.h"

/****************************************************************************/

#ifndef MP
    #define MP          MPARAM
#endif

#define CLF_MINNDX      -2

typedef struct  _CLREC {
    MINIRECORDCORE  mrc;
    HBITMAP         bmp;
    char *          str;
    LONG            ndx;
    ULONG           chk;
    ULONG           hndl;
} CLREC;

typedef CLREC *PCLREC;

#define CLREC_EXTRA  (sizeof(CLREC)-sizeof(MINIRECORDCORE))

// gcc complains because the standard macro casts a pointer to a SHORT
#define LONGFIELDOFFSET(type, field)    ((LONG)&(((type *)0)->field))

/****************************************************************************/

MRESULT _System CL_StaticSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
BOOL            CL_RefreshItem( HWND hCnr, LONG ndx);
BOOL            CL_SelectItem( HWND hCnr, LONG ndx);
BOOL            CL_MoveItem( HWND hCnr, LONG oldNdx, LONG newNdx);
BOOL            CL_QueryItem( HWND hCnr, LONG ndx, CLITEM * pcli);
BOOL            CL_SetItem( HWND hCnr, LONG ndx, CLITEM * pcli);
BOOL            CL_QueryCheck( HWND hCnr, LONG ndx, ULONG * pulChk);
BOOL            CL_SetCheck( HWND hCnr, LONG ndx, ULONG ulChk);
BOOL            CL_InsertItem( HWND hCnr, CLITEM * pCL, ULONG cntCL);
BOOL            CL_RemoveItem( HWND hCnr, LONG ndx);
PCLREC          CL_RecFromNdx( HWND hCnr, LONG ndx);
LONG            CL_ReindexRecs( HWND hCnr);
BOOL            CL_Init( HWND hCL, char * pszCnrTitle,
                         char * pszChkTitle, char * pszStrTitle);
PFIELDINFO      CL_InitCols( HWND hCnr, char * pszChkTitle, char * pszStrTitle);
BOOL            CL_GlobalInit( void);
MRESULT _System CL_CnrSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT _System CL_LeftSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
void            CL_Clicked( HWND hCnr);
BOOL            CL_OwnerdrawCheck( POWNERITEM poi);

/****************************************************************************/

static BOOL         fGlobalInit = FALSE;
static HBITMAP      hbmpSyschk = 0;
static SIZEL        sizlChk = {13,13};
static PFNWP        pfnCL_Cnr = 0;
static PFNWP        pfnCL_Left = 0;

/****************************************************************************/

MRESULT _System CL_StaticSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    switch (msg) {
        case WM_CONTROL: {
            if (mp1 == MPFROM2SHORT( 1, CLN_CLICKED))
                return (WinSendMsg( WinQueryWindow( hwnd, QW_OWNER), msg,
                        MPFROM2SHORT( WinQueryWindowUShort( hwnd, QWS_ID), CLN_CLICKED),
                        mp2));

            if (mp1 == MPFROM2SHORT( 1, CN_EMPHASIS) &&
                (((PNOTIFYRECORDEMPHASIS)mp2)->fEmphasisMask & CRA_SELECTED)) {

                PCLREC      pclr  = (PCLREC)(((PNOTIFYRECORDEMPHASIS)mp2)->pRecord);
                CLITEM      cli;

                cli.ndx  = pclr->ndx;
                cli.chk  = pclr->chk;
                cli.hndl = pclr->hndl;
                cli.str  = pclr->str;

                WinSendMsg( WinQueryWindow( hwnd, QW_OWNER), WM_CONTROL,
                            MPFROM2SHORT( WinQueryWindowUShort( hwnd, QWS_ID),
                            ((pclr->mrc.flRecordAttr & CRA_SELECTED) ?
                            CLN_SELECTED : CLN_DESELECTED)), (MP)&cli);
            }

            return 0;
        }

        case CLM_REFRESHITEM:
            return ((MRESULT)CL_RefreshItem( WinWindowFromID( hwnd, 1),
                                             (LONG)mp1));
        case CLM_SELECTITEM:
            return ((MRESULT)CL_SelectItem( WinWindowFromID( hwnd, 1),
                                            (LONG)mp1));
        case CLM_INSERTITEM:
            return ((MRESULT)CL_InsertItem( WinWindowFromID( hwnd, 1),
                                            (CLITEM*)mp1, (ULONG)mp2));

        case CLM_REMOVEITEM:
            return ((MRESULT)CL_RemoveItem( WinWindowFromID( hwnd, 1),
                                            (LONG)mp1));
        case CLM_QUERYITEM:
            return ((MRESULT)CL_QueryItem( WinWindowFromID( hwnd, 1),
                                           (LONG)mp1, (CLITEM*)mp2));

        case CLM_SETITEM:
            return ((MRESULT)CL_SetItem( WinWindowFromID( hwnd, 1),
                                         (LONG)mp1, (CLITEM*)mp2));

        case CLM_MOVEITEM:
            return ((MRESULT)CL_MoveItem( WinWindowFromID( hwnd, 1),
                                          (LONG)mp1, (LONG)mp2));

        case CLM_QUERYCHECK:
            return ((MRESULT)CL_QueryCheck( WinWindowFromID( hwnd, 1),
                                            (LONG)mp1, (ULONG*)mp2));

        case CLM_SETCHECK:
            return ((MRESULT)CL_SetCheck( WinWindowFromID( hwnd, 1),
                                          (LONG)mp1, (ULONG)mp2));

        case WM_WINDOWPOSCHANGED:
            if (((PSWP)mp1)->fl & SWP_SIZE)
                WinSetWindowPos( WinWindowFromID( hwnd, 1), 0, 0, 0,
                                 ((PSWP)mp1)->cx, ((PSWP)mp1)->cy, SWP_SIZE);
            break;

        case WM_DRAWITEM:
            if (SHORT1FROMMP(mp1) == 1)
                return ((MRESULT)CL_OwnerdrawCheck( (POWNERITEM)mp2));
            return 0;

        case WM_SETFOCUS:
            if (SHORT1FROMMP(mp2))
                WinPostMsg( hwnd, WM_USER+2, 0, 0);
            return 0;

        case WM_USER+2:
            WinFocusChange( HWND_DESKTOP, WinWindowFromID( hwnd, 1), 0);
            return 0;
    }

    return (WinDefWindowProc( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

BOOL            CL_RefreshItem( HWND hCnr, LONG ndx)

{
    PCLREC      pclr;

    pclr = CL_RecFromNdx( hCnr, ndx);
    if (!pclr)
        return (FALSE);

    return ((BOOL)WinSendMsg( hCnr, CM_INVALIDATERECORD, (MP)&pclr,
                              MPFROM2SHORT( 1, CMA_NOREPOSITION)));
}

/****************************************************************************/

BOOL            CL_SelectItem( HWND hCnr, LONG ndx)

{
    PCLREC      pclr;

    pclr = CL_RecFromNdx( hCnr, ndx);
    if (!pclr)
        return (FALSE);

    return ((BOOL)WinSendMsg( hCnr, CM_SETRECORDEMPHASIS, (MP)pclr,
                              MPFROM2SHORT( TRUE, CRA_CURSORED | CRA_SELECTED)));
}

/****************************************************************************/

BOOL            CL_MoveItem( HWND hCnr, LONG oldNdx, LONG newNdx)

{
    BOOL        fRtn = FALSE;
    CNRINFO     cnri;
    TREEMOVE    tm;

do {
    cnri.cb = sizeof(CNRINFO);
    WinSendMsg( hCnr, CM_QUERYCNRINFO, (MP)&cnri, (MP)sizeof(CNRINFO));

    if (oldNdx == newNdx || oldNdx < CLF_MINNDX ||
        oldNdx >= cnri.cRecords || cnri.cRecords < 2)
        break;

    tm.preccMove = (PRECORDCORE)CL_RecFromNdx( hCnr, oldNdx);
    if (!tm.preccMove)
        break;

    tm.preccNewParent = 0;
    tm.flMoveSiblings = FALSE;

    // Using CMA_LAST to move a record to the end of the list causes a
    // trap in PMCTLS.  This workaround moves the record to the next-to-
    // last position (if it isn't there already), then sets up the last
    // record to be moved up one, leaving the target as the last item.
    if (newNdx < 0 || newNdx >= cnri.cRecords-1) {
        PCLREC  pclr;
        PCLREC  pclrPrev;

        pclr = CL_RecFromNdx( hCnr, CLF_LAST);
        if (!pclr)
            break;
        pclrPrev = (PCLREC)WinSendMsg( hCnr, CM_QUERYRECORD, (MP)pclr,
                                       MPFROM2SHORT( CMA_PREV, CMA_ITEMORDER));
        if (!pclrPrev)
            break;

        if (tm.preccMove == (PRECORDCORE)pclrPrev) {
            pclrPrev = (PCLREC)WinSendMsg( hCnr, CM_QUERYRECORD, (MP)pclrPrev,
                                       MPFROM2SHORT( CMA_PREV, CMA_ITEMORDER));
            if (!pclrPrev)
                tm.pRecordOrder = (PRECORDCORE)CMA_FIRST;
            else
                tm.pRecordOrder = (PRECORDCORE)pclrPrev;

            tm.preccMove = (PRECORDCORE)pclr;
        }
        else {
            tm.pRecordOrder = (PRECORDCORE)pclrPrev;
            if (!WinSendMsg( hCnr, CM_MOVETREE, (MP)&tm, 0)) {
                printf( "preliminary CM_MOVETREE failed\n");
                break;
            }

            tm.preccMove = (PRECORDCORE)pclr;
        }
    }
    else
    if (newNdx == 0)
        tm.pRecordOrder = (PRECORDCORE)CMA_FIRST;
    else {
        oldNdx = ((PCLREC)tm.preccMove)->ndx;
        if (newNdx == oldNdx + 1)
            tm.pRecordOrder = (PRECORDCORE)CL_RecFromNdx( hCnr, newNdx);
        else
            tm.pRecordOrder = (PRECORDCORE)CL_RecFromNdx( hCnr, newNdx - 1);
        if (!tm.pRecordOrder)
            break;
    }

    if (!WinSendMsg( hCnr, CM_MOVETREE, (MP)&tm, 0)) {
        printf( "CM_MOVETREE failed\n");
        break;
    }

    WinSendMsg( hCnr, CM_INVALIDATERECORD, (MP)&tm.preccMove,
                MPFROM2SHORT( 1, 0));
    CL_ReindexRecs( hCnr);

    fRtn = TRUE;

} while (0);

    return (fRtn);
}

/****************************************************************************/

BOOL            CL_QueryItem( HWND hCnr, LONG ndx, CLITEM * pcli)

{
    PCLREC      pclr;

    pclr = CL_RecFromNdx( hCnr, ndx);
    if (!pclr || !pcli)
        return (FALSE);

    pcli->ndx = pclr->ndx;
    pcli->chk = pclr->chk;
    pcli->hndl = pclr->hndl;
    pcli->str = pclr->str;

    return (TRUE);
}

/****************************************************************************/

BOOL            CL_SetItem( HWND hCnr, LONG ndx, CLITEM * pcli)

{
    PCLREC      pclr;

    pclr = CL_RecFromNdx( hCnr, ndx);
    if (!pclr || !pcli)
        return (FALSE);

    pcli->ndx = pclr->ndx;
    pcli->chk &= CLF_CHKMASK;

    pclr->hndl = pcli->hndl;
    pclr->chk = pcli->chk;
    pclr->str = pcli->str;
    WinSendMsg( hCnr, CM_INVALIDATERECORD, (MP)&pclr,
                MPFROM2SHORT( 1, CMA_ERASE | CMA_NOREPOSITION));

    return (TRUE);
}

/****************************************************************************/

BOOL            CL_QueryCheck( HWND hCnr, LONG ndx, ULONG * pulChk)

{
    PCLREC      pclr;

    pclr = CL_RecFromNdx( hCnr, ndx);
    if (!pclr || !pulChk)
        return (FALSE);

    *pulChk = pclr->chk;

    return (TRUE);
}

/****************************************************************************/

BOOL            CL_SetCheck( HWND hCnr, LONG ndx, ULONG ulChk)

{
    PCLREC      pclr;

    pclr = CL_RecFromNdx( hCnr, ndx);
    if (!pclr) {
        printf( "CL_SetCheck - rec not found, ndx= %d\n", (int)ndx);
        return (FALSE);
    }

    ulChk &= CLF_CHKMASK;

    if (pclr->chk != ulChk) {
        pclr->chk = ulChk;
        WinSendMsg( hCnr, CM_INVALIDATERECORD, (MP)&pclr,
                    MPFROM2SHORT( 1, CMA_NOREPOSITION));
    }

    return (TRUE);
}

/****************************************************************************/

BOOL            CL_InsertItem( HWND hCnr, CLITEM * pCL, ULONG cntCL)

{
    BOOL            fRtn = FALSE;
    LONG            ndx;
    ULONG           ctr;
    PCLREC          pclr;
    PCLREC          pclrNew;
    CNRINFO         cnri;
    RECORDINSERT    ri;

do {
    cnri.cb = sizeof(CNRINFO);
    WinSendMsg( hCnr, CM_QUERYCNRINFO, (MP)&cnri,
                (MP)(LONGFIELDOFFSET(CNRINFO, cRecords) + sizeof(ULONG)));

    if (pCL->ndx == CLF_LAST || pCL->ndx >= cnri.cRecords) {
        ndx = cnri.cRecords;
        ri.pRecordOrder = (PRECORDCORE)CMA_END;
    }
    else {
        ndx = pCL->ndx;
        if (ndx == 0)
            ri.pRecordOrder = (PRECORDCORE)CMA_FIRST;
        else {
            ri.pRecordOrder = (PRECORDCORE)CL_RecFromNdx( hCnr, ndx - 1);
            if (!ri.pRecordOrder) {
                ndx = cnri.cRecords;
                ri.pRecordOrder = (PRECORDCORE)CMA_END;
            }
        }
    }

    pclrNew = (PCLREC)WinSendMsg( hCnr, CM_ALLOCRECORD,
                                (MP)CLREC_EXTRA, (MP)cntCL);
    if (pclrNew == 0)
        break;

    for (ctr=0, pclr=pclrNew; ctr < cntCL;
         ctr++, pCL++, pclr=(PCLREC)pclr->mrc.preccNextRecord) {
        pclr->bmp  = 0;
        pclr->str  = pCL->str;
        pclr->ndx  = ndx;
        pCL->ndx   = ndx++;
        pCL->chk  &= CLF_CHKMASK;
        pclr->chk  = pCL->chk;
        pclr->hndl = pCL->hndl;
    }

    ri.cb = sizeof(RECORDINSERT);
    ri.pRecordParent = 0;
    ri.fInvalidateRecord = TRUE;
    ri.zOrder = CMA_TOP;
    ri.cRecordsInsert = cntCL;

    if (!WinSendMsg( hCnr, CM_INSERTRECORD, (MP)pclrNew, (MP)&ri)) {
        while (pclrNew) {
            pclr = (PCLREC)pclrNew->mrc.preccNextRecord;
            if (!WinSendMsg( hCnr, CM_FREERECORD, (MP)&pclrNew, (MP)1))
                printf( "InitColTestDlg:  failed to free a container record\n");
            pclrNew = pclr;
        }
        break;
    }

    if (ri.pRecordOrder != (PRECORDCORE)CMA_END)
        CL_ReindexRecs( hCnr);

    fRtn = TRUE;

} while (0);

    return (fRtn);
}

/****************************************************************************/

BOOL            CL_RemoveItem( HWND hCnr, LONG ndx)

{
    PCLREC      pclr;

    pclr = CL_RecFromNdx( hCnr, ndx);
    if (!pclr ||
        (LONG)WinSendMsg( hCnr, CM_REMOVERECORD, (MP)&pclr,
                          MPFROM2SHORT( 1, CMA_FREE | CMA_INVALIDATE)) == -1)
        return (FALSE);

    return (TRUE);
}

/****************************************************************************/
/****************************************************************************/

PCLREC          CL_RecFromNdx( HWND hCnr, LONG ndx)

{
    LONG        ctr;
    PCLREC      pclr;

    if (ndx == CLF_SELECTED)
        pclr = (PCLREC)WinSendMsg( hCnr, CM_QUERYRECORDEMPHASIS,
                                         (MP)CMA_FIRST, (MP)CRA_CURSORED);
    else
    if (ndx == CLF_LAST)
        pclr = (PCLREC)WinSendMsg( hCnr, CM_QUERYRECORD, (MP)0,
                                         MPFROM2SHORT( CMA_LAST, CMA_ITEMORDER));
    else
    if (ndx < 0)
        pclr = 0;
    else {
        ctr = 0;
        pclr = (PCLREC)WinSendMsg( hCnr, CM_QUERYRECORD, (MP)0,
                                         MPFROM2SHORT( CMA_FIRST, CMA_ITEMORDER));
        while (pclr) {
            if (pclr == (PCLREC)-1) {
                pclr = 0;
                break;
            }

            if (ctr != pclr->ndx)
                printf( "INVALID INDEX!  #%d should be #%d\n",
                        (int)pclr->ndx, (int)ctr);

            if (ctr == ndx)
                break;

            ctr++;
            pclr = (PCLREC)WinSendMsg( hCnr, CM_QUERYRECORD, (MP)pclr,
                                       MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER));
        }
    }

    return pclr;
}

/****************************************************************************/

LONG            CL_ReindexRecs( HWND hCnr)

{
    LONG        ndx = 0;
    PCLREC      pclr;

    pclr = (PCLREC)WinSendMsg( hCnr, CM_QUERYRECORD, (MP)0,
                               MPFROM2SHORT( CMA_FIRST, CMA_ITEMORDER));
    while (pclr) {
        if (pclr == (PCLREC)-1) {
            pclr = 0;
            break;
        }

        pclr->ndx = ndx++;
        pclr = (PCLREC)WinSendMsg( hCnr, CM_QUERYRECORD, (MP)pclr,
                                   MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER));
    }

    return ndx;
}

/****************************************************************************/
/****************************************************************************/

BOOL            CL_Init( HWND hCL, char * pszCnrTitle,
                         char * pszChkTitle, char * pszStrTitle)

{
    BOOL        fRtn = FALSE;
    HWND        hCnr = 0;
    HWND        hTemp;
    ULONG       ul;
    RECTL       rcl;
    CNRINFO     cnri;
    char        szText[64];

do
{
    if (!fGlobalInit && !CL_GlobalInit())
        break;

    WinQueryWindowRect( hCL, &rcl);
    hCnr =  WinCreateWindow( hCL, WC_CONTAINER, "",
                             CCS_MINIRECORDCORE | CCS_READONLY |
                             CCS_NOCONTROLPTR | CCS_SINGLESEL,
                             0, 0, rcl.xRight, rcl.yTop, hCL, HWND_TOP,
                             1, 0, 0);
    if (!hCnr)
        break;

    // have the container inherit whatever font was set for the static;
    ul = WinQueryPresParam( hCL, PP_FONTNAMESIZE, 0, 0,
                            sizeof(szText), szText, QPF_NOINHERIT);
    if (ul)
        WinSetPresParam( hCnr, PP_FONTNAMESIZE, ul, szText);

    // configure the container for Details view
    cnri.cb = sizeof(CNRINFO);
    cnri.slBitmapOrIcon = sizlChk;
    cnri.flWindowAttr = CV_DETAIL | CA_ORDEREDTARGETEMPH;
    ul = CMA_FLWINDOWATTR | CMA_SLBITMAPORICON;
    if (pszCnrTitle) {
        cnri.pszCnrTitle = pszCnrTitle;
        cnri.flWindowAttr |= CA_CONTAINERTITLE | CA_TITLESEPARATOR;
        ul |= CMA_CNRTITLE;
    }
    if (pszChkTitle || pszStrTitle)
        cnri.flWindowAttr |= CA_DETAILSVIEWTITLES;

    if (WinSendMsg( hCnr, CM_SETCNRINFO, (MP)&cnri, (MP)ul) == 0)
        break;

    if (!CL_InitCols( hCnr, pszChkTitle, pszStrTitle))
        break;

    // subclass the container, its left window, and the static
    if (!WinSubclassWindow( hCnr, &CL_CnrSubProc))
        break;

    hTemp = WinWindowFromID( hCnr, CID_LEFTDVWND);
    if (!hTemp || !WinSubclassWindow( hTemp, &CL_LeftSubProc))
        break;

    if (!WinSubclassWindow( hCL, &CL_StaticSubProc))
        break;

    WinShowWindow( hCnr, TRUE);

    fRtn = TRUE;

} while (0);

    if (!fRtn && hCnr)
        WinDestroyWindow( hCnr);

    return (fRtn);
}

/****************************************************************************/

PFIELDINFO      CL_InitCols( HWND hCnr, char * pszChkTitle, char * pszStrTitle)

{
    PFIELDINFO      pfi;
    PFIELDINFO      ptr;
    FIELDINFOINSERT fii;

do
{
    // allocate FIELDINFO structs for each column
    pfi = (PFIELDINFO)WinSendMsg( hCnr, CM_ALLOCDETAILFIELDINFO, (MP)2, 0);
    if (pfi == 0)
        break;

    // initialize those structs
    ptr = pfi;
    ptr->flData = CFA_BITMAPORICON | CFA_CENTER | CFA_OWNER | CFA_HORZSEPARATOR;
    ptr->flTitle = CFA_CENTER;
    ptr->pTitleData = pszChkTitle;
    ptr->offStruct = sizeof(MINIRECORDCORE) + 0;
    ptr->pUserData = 0;
    ptr->cxWidth = 0;

    ptr = ptr->pNextFieldInfo;
    ptr->flData = CFA_STRING | CFA_LEFT | CFA_HORZSEPARATOR;
    ptr->flTitle = CFA_LEFT;
    ptr->pTitleData = pszStrTitle;
    ptr->offStruct = sizeof(MINIRECORDCORE) + sizeof(ULONG);
    ptr->pUserData = 0;
    ptr->cxWidth = 0;

    // insert the column info into the container
    fii.cb = sizeof(FIELDINFOINSERT);
    fii.pFieldInfoOrder = (PFIELDINFO)CMA_FIRST;
    fii.fInvalidateFieldInfo = FALSE;
    fii.cFieldInfoInsert = 2;
    if (WinSendMsg( hCnr, CM_INSERTDETAILFIELDINFO, (MP)pfi, (MP)&fii) == 0) {
        WinSendMsg( hCnr, CM_FREEDETAILFIELDINFO, (MP)&pfi, (MP)1);
        WinSendMsg( hCnr, CM_FREEDETAILFIELDINFO, (MP)&ptr, (MP)1);
        pfi = 0;
        break;
    }

    // update the container's layout
    WinSendMsg( hCnr, CM_INVALIDATEDETAILFIELDINFO, 0, 0);

} while (0);

    return (pfi);
}

/****************************************************************************/

BOOL            CL_GlobalInit( void)

{
    CLASSINFO           clsi;
    BITMAPINFOHEADER2   bmih;

do {
    if (!WinQueryClassInfo( 0, WC_CONTAINER, &clsi))
        break;
    pfnCL_Cnr = clsi.pfnWindowProc;

    if (!WinQueryClassInfo( 0, "#50", &clsi))
        break;
    pfnCL_Left = clsi.pfnWindowProc;

    hbmpSyschk = WinGetSysBitmap( HWND_DESKTOP, SBMP_CHECKBOXES);
    if (!hbmpSyschk)
        break;

    memset( &bmih, 0, sizeof(bmih));
    bmih.cbFix = sizeof(bmih);
    if (!GpiQueryBitmapInfoHeader( hbmpSyschk, &bmih))
        break;

    sizlChk.cx = bmih.cx / 4;
    sizlChk.cy = bmih.cy / 3;

    fGlobalInit = TRUE;

} while (0);

    if (!fGlobalInit) {
        printf( "CL_GlobalInit failed\n");
        if (hbmpSyschk) {
            GpiDeleteBitmap( hbmpSyschk);
            hbmpSyschk = 0;
        }
    }

    return (fGlobalInit);
}

/****************************************************************************/

MRESULT _System CL_CnrSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    if (msg == WM_CHAR && !(SHORT1FROMMP( mp1) & KC_KEYUP) &&
        (((SHORT1FROMMP( mp1) & KC_VIRTUALKEY) && (SHORT2FROMMP( mp2) == VK_SPACE)) ||
         ((SHORT1FROMMP( mp1) & KC_CHAR) && (SHORT1FROMMP( mp2) == 0x20)))) {
        CL_Clicked( hwnd);
        return ((MRESULT)TRUE);
    }

    return (pfnCL_Cnr( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

MRESULT _System CL_LeftSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    if (msg == WM_SINGLESELECT) {
        MRESULT     mRtn = pfnCL_Left( hwnd, msg, mp1, mp2);
        HWND        hCnr = WinQueryWindow( hwnd, QW_PARENT);
        LONG        left = (LONG)WinQueryWindowULong( hCnr, QWL_USER);

        if (SHORT1FROMMP( mp1) > left &&
            SHORT1FROMMP( mp1) <= left + sizlChk.cx)
            CL_Clicked( hCnr);

        return (mRtn);
    }

    return (pfnCL_Left( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

void            CL_Clicked( HWND hCnr)

{
    CLITEM      cli;
    PCLREC      pclr;

do {
    pclr = (PCLREC)WinSendMsg( hCnr, CM_QUERYRECORDEMPHASIS,
                               (MP)CMA_FIRST, (MP)CRA_CURSORED);
    if (!pclr)
        break;

    if (pclr->chk & (CLF_CHKDISABLED | CLF_CHKHIDDEN))
        break;

    cli.ndx = pclr->ndx;
    pclr->chk = (pclr->chk & CLF_CHECKED) ? CLF_UNCHECKED : CLF_CHECKED;
    cli.chk = pclr->chk;
    cli.hndl = pclr->hndl;
    cli.str = pclr->str;

    WinSendMsg( WinQueryWindow( hCnr, QW_OWNER), WM_CONTROL,
                MPFROM2SHORT( 1, CLN_CLICKED), (MP)&cli);

    pclr->chk = cli.chk & CLF_CHKMASK;
    pclr->bmp = 0;
    WinSendMsg( hCnr, CM_INVALIDATERECORD, (MP)&pclr,
                MPFROM2SHORT( 1, CMA_NOREPOSITION));

} while (0);

    return;
}

/****************************************************************************/

BOOL            CL_OwnerdrawCheck( POWNERITEM poi)

{
    HWND        hCnr;
    ULONG       ul = 0;
    LONG        clr = 0;
    PCLREC      pclr;
    POINTL      ptl;
    RECTL       rcl;

    if (!hbmpSyschk)
        return (FALSE);

    if (WinQueryWindowUShort( poi->hwnd, QWS_ID) != CID_LEFTDVWND)
        return (FALSE);

    pclr = (PCLREC)((PCNRDRAWITEMINFO)poi->hItem)->pRecord;
    hCnr = WinQueryWindow( poi->hwnd, QW_PARENT);

    if (pclr->mrc.flRecordAttr & CRA_SELECTED) {
        ul = WinQueryPresParam( hCnr, PP_HILITEBACKGROUNDCOLORINDEX,
                                0, 0, sizeof(clr), &clr, 0);
        if (!ul)
            clr = SYSCLR_HILITEBACKGROUND;

        WinFillRect( poi->hps, &poi->rclItem, clr);
    }

    if (pclr->chk & CLF_CHKHIDDEN)
        return TRUE;

    rcl.yBottom = sizlChk.cy * 2;
    rcl.yTop = sizlChk.cy * 3;

    if (pclr->chk & CLF_CHECKED) {
        rcl.xLeft = sizlChk.cx;
        rcl.xRight = sizlChk.cx * 2;
    }
    else {
        rcl.xLeft = 0;
        rcl.xRight = sizlChk.cx;
    }

    ptl.x = (poi->rclItem.xRight + poi->rclItem.xLeft - sizlChk.cx) / 2;
    ptl.y = (poi->rclItem.yTop + poi->rclItem.yBottom - sizlChk.cy) / 2;

    if ((pclr->chk & (CLF_CHKHIDDEN | CLF_CHKDISABLED)) == CLF_CHKDISABLED)
        WinDrawBitmap( poi->hps, hbmpSyschk, &rcl,
                       &ptl, 0, 0, DBM_NORMAL | DBM_IMAGEATTRS | DBM_HALFTONE);
    else
        WinDrawBitmap( poi->hps, hbmpSyschk, &rcl,
                       &ptl, 0, 0, DBM_NORMAL | DBM_IMAGEATTRS);

    WinSetWindowULong( hCnr, QWL_USER, ptl.x);

    return (TRUE);
}

/****************************************************************************/

