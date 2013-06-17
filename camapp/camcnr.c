/****************************************************************************/
// camcnr.c
//
//  container functions not related to Details view or FIELDINFOs:
//  - init & subclass container
//  - change view
//  - paint thumbnails
//  - set colors dialog
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

#define CAM_DARKGRAY     0x007f7f7f
#define CAM_PALEGRAY     0x00bfbfbf
#define CAM_CHARCOAL     0x003f3f3f

typedef struct _CAMView {
    ULONG   ndx;
    ULONG   id;
    ULONG   str;
    LONG    scale;
    LONG    margin;
    LONG    attr;
    LONG    clrFore;
    LONG    clrBack;
    LONG    clrGroupFore;
    LONG    clrGroupBack;
    LONG    clrHiliteFore;
    LONG    clrHiliteBack;
    char    font[FACESIZE+4];
} CAMView;

typedef CAMView *CAMViewPtr;

enum    eViews          { eDETAILS, eSMALL, eMEDIUM, eLARGE, eCNTVIEWS };

/****************************************************************************/

ULONG           CnrInit( HWND hwnd);
void            CnrSetView( HWND hwnd, ULONG ulCmd);
ULONG           CnrGetView( void);
MRESULT _System CnrSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
BOOL            CnrSetSelection( HWND hwnd, MPARAM mp1, MPARAM mp2);
void            CnrStoreViews( void);
void            CnrRestoreViews( void);
BOOL            CnrPaintRecord( POWNERITEM poi);
BOOL            CnrPaintBmp( POWNERITEM poi);
BOOL            CnrPaintTitle( POWNERITEM poi);
MRESULT _System ColorsDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
LONG            ColorsDlgInit( HWND hwnd);
void            ColorsViewChanged( HWND hwnd, HWND hCB);
void            ColorsUndo( HWND hwnd);
void            ColorsDefault( HWND hwnd);
void            ColorsSetDlgPP( HWND hwnd, LONG ndx);
void            ColorsSetCnrPP( HWND hwnd, LONG ndx);
void            ColorsPPChanged( HWND hwnd, ULONG pp, ULONG id);
MRESULT _System ColorsWndSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

/****************************************************************************/

static CAMView      ViewDflt[eCNTVIEWS] = {
        {eDETAILS,  IDM_DETAILS, MNU_Details, 0, 0,
         CV_DETAIL | CA_DRAWICON | CA_DETAILSVIEWTITLES | CV_MINI,
         RGB_BLACK,     RGB_WHITE,
         CAM_DARKGRAY,  CAM_DARKGRAY,
         RGB_WHITE,     CAM_DARKGRAY,
         "10.Helv"},
        {eSMALL,    IDM_SMALL, MNU_Small, 80, 4,
         CV_ICON | CA_DRAWBITMAP | CA_OWNERDRAW,
         RGB_BLACK,     RGB_WHITE,
         RGB_BLACK,     CAM_PALEGRAY,
         RGB_WHITE,     CAM_DARKGRAY,
         "8.Helv"},
        {eMEDIUM,   IDM_MEDIUM, MNU_Medium, 160, 4,
         CV_ICON | CA_DRAWBITMAP | CA_OWNERDRAW,
         RGB_BLACK,     RGB_WHITE,
         RGB_BLACK,     CAM_PALEGRAY,
         RGB_WHITE,     CAM_DARKGRAY,
         "10.Helv"},
        {eLARGE,    IDM_LARGE, MNU_Large, 240, 5,
         CV_ICON | CA_DRAWBITMAP | CA_OWNERDRAW,
         RGB_BLACK,     RGB_WHITE,
         RGB_BLACK,     CAM_PALEGRAY,
         RGB_WHITE,     CAM_DARKGRAY,
         "10.Helv"}
};

static CAMView      ViewInfo[eCNTVIEWS];
static CAMView      ViewUndo[eCNTVIEWS];

static CAMViewPtr   pcvView = &ViewInfo[0];
static PFNWP        pfnCnr = 0;

static CAMNLS   nlsColorsDlg[] = {
    { IDC_CLRCBTXT,         DLG_View },
    { IDC_CLRWINDOW,        DLG_WindowColors },
    { IDC_CLRHILITE,        DLG_HighlightColors },
    { IDC_CLRGROUP,         DLG_GroupColors },
    { IDC_CLRSEPARATOR,     DLG_SeparatorColor },
    { IDC_DEFAULT,          BTN_Default },
    { IDC_UNDO,             BTN_Undo },
    { 0,                    0 }};

/****************************************************************************/
/****************************************************************************/

ULONG       CnrInit( HWND hwnd)

{
    ULONG       rc = 0;
    HWND        hCnr;

do {
    rc = InitCols( hwnd);
    if (rc)
        break;

    InitSort( hwnd);
    CnrRestoreViews();

    hCnr = WinWindowFromID( hwnd, FID_CLIENT);
    pfnCnr = WinSubclassWindow( hCnr, &CnrSubProc);

    CnrSetView( hwnd, 0);

} while (0);

    return (rc);
}

/****************************************************************************/

void        CnrSetView( HWND hwnd, ULONG ulCmd)

{
    HWND            hCnr;
    HWND            hTemp;
    CNRINFO         cnri;

do {
    hCnr = WinWindowFromID( hwnd, FID_CLIENT);

    if (ulCmd == IDM_DETAILS)
        pcvView = &ViewInfo[eDETAILS];
    else
    if (ulCmd == IDM_SMALL)
        pcvView = &ViewInfo[eSMALL];
    else
    if (ulCmd == IDM_MEDIUM)
        pcvView = &ViewInfo[eMEDIUM];
    else
    if (ulCmd == IDM_LARGE)
        pcvView = &ViewInfo[eLARGE];
    else
    if (ulCmd == 0)
        ulCmd = pcvView->id;
    else
        break;

    // if switching out of details view, save the split column
    if (WinSendMsg( hCnr, CM_QUERYCNRINFO, (MP)&cnri, (MP)sizeof(cnri)) &&
        (cnri.flWindowAttr & CV_DETAIL) &&
        !(pcvView->attr & CV_DETAIL))
        ExitDetailsView( hCnr);

    cnri.cb = sizeof(CNRINFO);
    cnri.slBitmapOrIcon.cx = pcvView->scale + (2 * pcvView->margin);
    cnri.slBitmapOrIcon.cy = pcvView->scale + (2 * pcvView->margin);
    cnri.flWindowAttr = pcvView->attr;

    if (!WinSendMsg( hCnr, CM_SETCNRINFO, (MP)&cnri, 
         (MP)(CMA_FLWINDOWATTR | CMA_SLBITMAPORICON)))
        break;

    WinEnableWindowUpdate( hCnr, FALSE);

    if (pcvView->attr & CV_DETAIL)
        WinSetPresParam( hCnr, PP_BORDERCOLOR, 4, &pcvView->clrGroupFore);
    WinSetPresParam( hCnr, PP_FOREGROUNDCOLOR, 4, &pcvView->clrFore);
    WinSetPresParam( hCnr, PP_BACKGROUNDCOLOR, 4, &pcvView->clrBack);
    WinSetPresParam( hCnr, PP_HILITEFOREGROUNDCOLOR, 4, &pcvView->clrHiliteFore);
    WinSetPresParam( hCnr, PP_HILITEBACKGROUNDCOLOR, 4, &pcvView->clrHiliteBack);
    WinSetPresParam( hCnr, PP_FONTNAMESIZE,
                     strlen( pcvView->font)+1, pcvView->font);

    // if the window isn't visible before calling EnterDetailsView(),
    // the column widths may be miscalculated
    WinShowWindow( hCnr, TRUE);
    if (pcvView->attr & CV_DETAIL)
        EnterDetailsView( hCnr);

    hTemp = WinWindowFromID( hwnd, IDC_SIDEBAR);
    hTemp = WinWindowFromID( hTemp, IDC_INFO);
    WinShowWindow( hTemp, ((pcvView->attr & CV_DETAIL) ? FALSE : TRUE));

} while (0);

    return;
}

/****************************************************************************/

ULONG       CnrGetView( void)

{
    return (pcvView->id);
}

/****************************************************************************/

MRESULT _System CnrSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{

do {
    if (msg == WM_CHAR) {
        if (CnrSetSelection( hwnd, mp1, mp2))
            return ((MRESULT)TRUE);
        break;
    }

    if (msg == WM_PRESPARAMCHANGED) {
        HWND    hColorDlg = GetHwnd( IDD_COLORS);

        if (mp1 == (MP)PP_FOREGROUNDCOLOR)
            WinQueryPresParam( hwnd, (ULONG)mp1, 0, 0, 4, &pcvView->clrFore,
                               QPF_PURERGBCOLOR | QPF_NOINHERIT);
        else
        if (mp1 == (MP)PP_BACKGROUNDCOLOR)
            WinQueryPresParam( hwnd, (ULONG)mp1, 0, 0, 4, &pcvView->clrBack,
                               QPF_PURERGBCOLOR | QPF_NOINHERIT);
        else
        if (mp1 == (MP)PP_FONTNAMESIZE)
            WinQueryPresParam( hwnd, (ULONG)mp1, 0, 0, sizeof(pcvView->font),
                               pcvView->font, QPF_NOINHERIT);
        else
            break;

        if (hColorDlg)
            WinPostMsg( hColorDlg, CAMMSG_PPCHANGED, mp1, (MP)FID_CLIENT);

        break;
    }

} while (0);

    return (pfnCnr( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

// enable the user to select records using Shift + PgUp/PgDn/Home/End

#define KC_SCA  (KC_SHIFT | KC_CTRL | KC_ALT)

BOOL        CnrSetSelection( HWND hwnd, MPARAM mp1, MPARAM mp2)
 
{
    BOOL            fRtn = FALSE;
    BOOL            fUp;
    BOOL            fRev = FALSE;
    USHORT          usDir;
    CAMRecPtr       precCur;
    CAMRecPtr       precNew;
    CAMRecPtr       precTmp;
    CAMRecPtr       precOrg;
    QUERYRECORDRECT qrr;
    RECTL           rclRec;
    RECTL           rcl;

do {

    if ((SHORT1FROMMP(mp1) & (KC_VIRTUALKEY | KC_KEYUP | KC_SCA)) !=
                             (KC_VIRTUALKEY | KC_SHIFT))
        break;

    if (SHORT2FROMMP(mp2) == VK_DOWN || SHORT2FROMMP(mp2) == VK_PAGEDOWN || SHORT2FROMMP(mp2) == VK_END)
        fUp = FALSE;
    else
    if (SHORT2FROMMP(mp2) == VK_UP || SHORT2FROMMP(mp2) == VK_PAGEUP || SHORT2FROMMP(mp2) == VK_HOME)
        fUp = TRUE;
    else
        break;

    precCur = (CAMRecPtr)WinSendMsg( hwnd, CM_QUERYRECORDEMPHASIS,
                                      (MP)CMA_FIRST, (MP)CRA_CURSORED);
    if (!precCur || precCur == (CAMRecPtr)-1)
        break;

    fRtn = TRUE;
    precOrg = precCur;

    usDir = fUp ? CMA_PREV : CMA_NEXT;
    precNew = (CAMRecPtr)WinSendMsg( hwnd, CM_QUERYRECORD, (MP)precCur,
                                       MPFROM2SHORT( usDir, CMA_ITEMORDER));
    if (!precNew || precNew == (CAMRecPtr)-1)
        break;

    if (precNew->mrc.flRecordAttr & CRA_SELECTED)
        fRev = TRUE;
    else
        usDir = fUp ? CMA_NEXT : CMA_PREV;

    precTmp = precCur;
    do {
        precCur = precTmp;
        precTmp = (CAMRecPtr)WinSendMsg( hwnd, CM_QUERYRECORD, (MP)precTmp,
                                           MPFROM2SHORT( usDir, CMA_ITEMORDER));
        if (!precTmp || precTmp == (CAMRecPtr)-1)
            break;

    } while (precTmp->mrc.flRecordAttr & CRA_SELECTED);

    if (SHORT2FROMMP(mp2) == VK_DOWN || SHORT2FROMMP(mp2) == VK_UP) {
        WinSendMsg( hwnd, CM_SETRECORDEMPHASIS, (MP)precNew,
                    MPFROM2SHORT( TRUE, (CRA_CURSORED | CRA_SELECTED)));
        WinSendMsg( hwnd, CM_SETRECORDEMPHASIS, (MP)precOrg,
                    MPFROM2SHORT( FALSE, CRA_SELECTED));
    }
    else {
        mp1 = (MP)((ULONG)mp1 ^ KC_SHIFT);
        pfnCnr( hwnd, WM_CHAR, mp1, mp2);

        if (SHORT2FROMMP(mp2) != VK_PAGEDOWN) {
            precNew = (CAMRecPtr)WinSendMsg( hwnd, CM_QUERYRECORDEMPHASIS,
                                               (MP)CMA_FIRST, (MP)CRA_CURSORED);
            if (!precNew || precNew == (CAMRecPtr)-1)
                break;
        }
        else {
            precNew = (CAMRecPtr)WinSendMsg( hwnd, CM_QUERYRECORDEMPHASIS,
                                              (MP)precOrg, (MP)CRA_CURSORED);
            if (precNew == (CAMRecPtr)-1)
                break;

            if (!precNew) {
                precNew = (CAMRecPtr)WinSendMsg( hwnd, CM_QUERYRECORD, 0,
                                       MPFROM2SHORT( CMA_LAST, CMA_ITEMORDER));
                if (!precNew || precNew == (CAMRecPtr)-1)
                    break;

                WinSendMsg( hwnd, CM_SETRECORDEMPHASIS, (MP)precNew,
                            MPFROM2SHORT( TRUE, (CRA_CURSORED | CRA_SELECTED)));
                WinSendMsg( hwnd, CM_SETRECORDEMPHASIS, (MP)precOrg,
                            MPFROM2SHORT( FALSE, CRA_SELECTED));
            }
        }
    }

    qrr.cb = sizeof(qrr);
    qrr.pRecord = (PRECORDCORE)precNew;
    qrr.fRightSplitWindow = 0;
    qrr.fsExtent = CMA_ICON | CMA_TEXT;
    WinSendMsg( hwnd, CM_QUERYRECORDRECT, (MP)&rclRec, (MP)&qrr);
    WinSendMsg( hwnd, CM_QUERYVIEWPORTRECT, (MP)&rcl,
                MPFROM2SHORT( CMA_WINDOW, FALSE));

    if (rclRec.yBottom < rcl.yBottom)
        WinSendMsg( hwnd, CM_SCROLLWINDOW, (MP)CMA_VERTICAL,
                    (MP)rcl.yBottom - rclRec.yBottom);
    else {
        if (rclRec.yTop > rcl.yTop)
            WinSendMsg( hwnd, CM_SCROLLWINDOW, (MP)CMA_VERTICAL,
                        (MP)(rcl.yTop - rclRec.yTop));
    }

    if (precNew == precCur)
        break;

    usDir = fUp ? CMA_PREV : CMA_NEXT;
    if (fRev) {
        precTmp = (CAMRecPtr)WinSendMsg( hwnd, CM_QUERYRECORDEMPHASIS,
                                           (MP)precCur, (MP)CRA_CURSORED);

        if (fUp == (precTmp && precTmp != (CAMRecPtr)-1))
            usDir = fUp ? CMA_NEXT : CMA_PREV;
    }

    do {
        WinSendMsg( hwnd, CM_SETRECORDEMPHASIS, (MP)precCur,
                    MPFROM2SHORT( TRUE, CRA_SELECTED));
        precCur = (CAMRecPtr)WinSendMsg( hwnd, CM_QUERYRECORD, (MP)precCur,
                                         MPFROM2SHORT( usDir, CMA_ITEMORDER));
    } while (precCur && precCur != (CAMRecPtr)-1 && precCur != precNew);

} while (0);

    return (fRtn);
}

#undef KC_SCA

/****************************************************************************/
/****************************************************************************/

void        CnrStoreViews( void)

{
    ULONG   ctr;

    for (ctr = 0; ctr < eCNTVIEWS; ctr++)
        if (&ViewInfo[ctr] == pcvView)
            break;

    if (ctr >= eCNTVIEWS)
        ctr = 0;

    PrfWriteProfileData( HINI_USERPROFILE, CAMINI_APP, CAMINI_VIEWNDX,
                         &ctr, sizeof(ctr));

    PrfWriteProfileData( HINI_USERPROFILE, CAMINI_APP, CAMINI_VIEWS,
                         ViewInfo, sizeof(ViewInfo));

    return;
}

/****************************************************************************/

void        CnrRestoreViews( void)

{
    ULONG   ctr;
    ULONG   ndx;

    if (PrfQueryProfileSize( HINI_USERPROFILE, CAMINI_APP,
                             CAMINI_VIEWS, &ctr) &&
        ctr == sizeof(ViewInfo))
        PrfQueryProfileData( HINI_USERPROFILE, CAMINI_APP,
                             CAMINI_VIEWS, ViewInfo, &ctr);
    else
        memcpy( ViewInfo, ViewDflt, sizeof(ViewDflt));

    ctr = sizeof(ndx);
    if (PrfQueryProfileData( HINI_USERPROFILE, CAMINI_APP,
                             CAMINI_VIEWNDX, &ndx, &ctr) == FALSE ||
        ndx >= eCNTVIEWS)
        ndx = 0;

    pcvView = &ViewInfo[ndx];

    return;
}

/****************************************************************************/
/****************************************************************************/

BOOL        CnrPaintRecord( POWNERITEM poi)

{
    BOOL    fRtn;

    if (poi->idItem == CMA_ICON)
        fRtn = CnrPaintBmp( poi);
    else
    if (poi->idItem == CMA_TEXT)
        fRtn = CnrPaintTitle( poi);
    else
        fRtn = FALSE;

    return (fRtn);
}

/****************************************************************************/

BOOL        CnrPaintBmp( POWNERITEM poi)

{
    HBITMAP     hbmp;
    LONG        cxBmp;
    LONG        cyBmp;
    LONG        cxIco;
    LONG        margin;
    LONG        adj;
    char *      pText;
    CAMRecPtr   pcr;
    POINTL      ptl;
    RECTL       rcl;
    BITMAPINFOHEADER2   bmih;

do {
    // get the image, or use the blank if needed
    pcr = (CAMRecPtr)((PCNRDRAWITEMINFO)poi->hItem)->pRecord;
    hbmp = pcr->bmp;
    if (!hbmp)
        hbmp = GetBlankBmp();

    // get the bmp's dimensions
    memset( &bmih, 0, sizeof(bmih));
    bmih.cbFix = sizeof(bmih);
    if (!GpiQueryBitmapInfoHeader( hbmp, &bmih)) {
        printf( "CnrPaintBmp - GpiQueryBitmapInfoHeader failed\n");
        break;
    }

    // scale them
    if (bmih.cx >= bmih.cy) {
        cxBmp = pcvView->scale;
        cyBmp = (bmih.cy * pcvView->scale) / bmih.cx;
        margin = (poi->rclItem.xRight - poi->rclItem.xLeft - cxBmp) / 2;
    }
    else {
        cyBmp = pcvView->scale;
        cxBmp = (bmih.cx * pcvView->scale) / bmih.cy;
        margin = (poi->rclItem.yTop - poi->rclItem.yBottom - cyBmp) / 2;
    }

    // center the scaled image horizontally
    ptl.x = (poi->rclItem.xRight + poi->rclItem.xLeft - cxBmp) / 2;
    ptl.y = poi->rclItem.yBottom + margin;

    // set the scaled image's bounding box, then draw it
    rcl.xLeft = ptl.x;
    rcl.yBottom = ptl.y;
    rcl.xRight = ptl.x + cxBmp;
    rcl.yTop = ptl.y + cyBmp;

    if (!WinDrawBitmap( poi->hps, hbmp, 0, (PPOINTL)&rcl, 0, 0,
                         DBM_NORMAL | DBM_IMAGEATTRS | DBM_STRETCH))
        printf( "CnrPaintBmp - WinDrawBitmap failed\n");

    if (!GpiCreateLogColorTable( poi->hps, 0, LCOLF_RGB, 0, 0, 0))
        printf( "CnrPaintBmp - GpiCreateLogColorTable failed\n");

    // expand the image box, then draw a border
    rcl.xLeft -= 1;
    rcl.yBottom -= 1;
    rcl.xRight += 1;
    rcl.yTop += 1;
    if (!WinDrawBorder( poi->hps, &rcl, 1, 1, CAM_DARKGRAY, CAM_DARKGRAY,
                        0))
        printf( "CnrPaintBmp - WinDrawBorder failed\n");

    // draw a shadow line along the bottom & right sides
    rcl.xLeft += 1;
    rcl.yBottom -= 1;
    if (!GpiMove( poi->hps, (PPOINTL)&rcl.xLeft))
        printf( "CnrPaintBmp - GpiMove failed\n");

    rcl.yTop -= 4;
    rcl.xLeft = rcl.xRight;
    GpiSetColor( poi->hps, CAM_CHARCOAL);
    if (GpiPolyLine( poi->hps, 2, (PPOINTL)&rcl) == GPI_ERROR)
        printf( "CnrPaintBmp - GpiPolyLine failed\n");

    // draw/erase selection hilite
    rcl.xLeft = ptl.x - margin + 1;
    rcl.yBottom = ptl.y - margin;
    rcl.xRight = ptl.x + cxBmp + margin;
    rcl.yTop = ptl.y + cyBmp + margin - 1;
    GpiSetLineType( poi->hps, LINETYPE_SOLID);
    if (!WinDrawBorder( poi->hps, &rcl, margin/2, margin/2,
                        ((pcr->mrc.flRecordAttr & CRA_SELECTED) ?
                         pcvView->clrHiliteBack : pcvView->clrBack),
                        ((pcr->mrc.flRecordAttr & CRA_SELECTED) ?
                         pcvView->clrHiliteBack : pcvView->clrBack),
                         DB_PATCOPY))
        printf( "CnrPaintBmp - WinDrawBorder failed for emph \n");

    // if the record isn't marked or saved & isn't assigned to a group, exit
    if (STAT_ISNADA(pcr->status) && !(*pcr->group))
        break;

    // get the appropriate icon & its size
    cxIco = WinQuerySysValue( HWND_DESKTOP, SV_CXICON) / 2;
    hbmp = pcr->mark;

    // even if there's no group, we still have to get the height
    // of a line of text so the icon will be positioned consistently
    pText = ((*pcr->group) ? pcr->group : "lj");

    // get the dimensions of the group name text
    rcl.xLeft = ptl.x + cxIco;
    rcl.yBottom = ptl.y;
    rcl.xRight = ptl.x + cxBmp;
    rcl.yTop = ptl.y + cxIco;
    if (WinDrawText( poi->hps, -1, pText, &rcl, 0, 0,
                     DT_VCENTER | DT_QUERYEXTENT) == 0) {
        printf( "CnrPaintBmp - WinDrawText DT_QUERYEXTENT failed\n");
        break;
    }

    // reposition the rectangle, expand it to include the icon
    adj = rcl.yBottom - ptl.y;
    rcl.xLeft = ptl.x;
    rcl.yBottom = ptl.y;
    rcl.yTop -= adj;
    if (*pcr->group) {
        rcl.xRight += 3;
        if (rcl.xRight > ptl.x + cxBmp)
            rcl.xRight = ptl.x + cxBmp;
    }
    else
        rcl.xRight = ptl.x + cxIco;

    // fill the box with either a solid color or shading
    if (STAT_ISDONE(pcr->status))
        GpiSetPattern( poi->hps, PATSYM_DENSE8);
    else
        GpiSetPattern( poi->hps, PATSYM_NOSHADE);

    if (!WinDrawBorder( poi->hps, &rcl, rcl.xRight - rcl.xLeft, 0,
                        pcvView->clrGroupFore, pcvView->clrGroupBack,
                        DB_PATCOPY))
            printf( "CnrPaintBmp - WinDrawBorder for tag failed\n");

    // draw the icon, adjusting it's Y coordinate
    // so the image aligns with the text
    if (!WinDrawPointer( poi->hps, ptl.x, ptl.y - adj, hbmp, DP_MINI))
        printf( "CnrPaintBmp - WinDrawPointer for sav/del failed\n");

    // draw the text to the right of the icon
    if (*pcr->group) {
        rcl.xLeft = ptl.x + cxIco;
        if (WinDrawText( poi->hps, -1, pcr->group, &rcl,
                         pcvView->clrGroupFore, pcvView->clrGroupBack,
                         DT_VCENTER) == 0)
            printf( "CnrPaintBmp - WinDrawText failed\n");
    }

} while (0);

    return (TRUE);
}

/****************************************************************************/

BOOL        CnrPaintTitle( POWNERITEM poi)

{
    CAMRecPtr   pcr;
    RECTL       rcl;

    pcr = (CAMRecPtr)((PCNRDRAWITEMINFO)poi->hItem)->pRecord;

    // expand the rect so it's as wide as the image's
    // landscape selection rect and abutts it at the top
    rcl = poi->rclItem;
    rcl.xLeft -= 1;
    rcl.xRight += 1;
    rcl.yTop += 1;

    if (!GpiCreateLogColorTable( poi->hps, 0, LCOLF_RGB, 0, 0, 0))
        printf( "CnrPaintTitle - GpiCreateLogColorTable failed\n");

    if (WinFillRect( poi->hps, &rcl,
                     ((pcr->mrc.flRecordAttr & CRA_SELECTED) ?
                      pcvView->clrHiliteBack : pcvView->clrBack)) == 0)
        printf( "CnrPaintTitle - WinFillRect failed\n");

    if (WinDrawText( poi->hps, -1, pcr->title, &rcl,
                     ((pcr->mrc.flRecordAttr & CRA_SELECTED) ?
                      pcvView->clrHiliteFore : pcvView->clrFore),
                     ((pcr->mrc.flRecordAttr & CRA_SELECTED) ?
                      pcvView->clrHiliteBack : pcvView->clrBack),
                     DT_CENTER | DT_VCENTER) == 0)
        printf( "CnrPaintTitle - WinDrawText failed\n");

    return (TRUE);
}

/****************************************************************************/
/****************************************************************************/

MRESULT _System ColorsDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    static BOOL fInChg = FALSE;

    switch (msg)
    {
        case WM_COMMAND:
            if (mp1 == (MP)IDC_UNDO) {
                fInChg = TRUE;
                ColorsUndo( hwnd);
                fInChg = FALSE;
            }
            else
            if (mp1 == (MP)IDC_DEFAULT) {
                fInChg = TRUE;
                ColorsDefault( hwnd);
                fInChg = FALSE;
            }
            return (0);

        case WM_CONTROL:
            if (mp1 == MPFROM2SHORT( IDC_CLRCB, CBN_EFCHANGE)) {
                fInChg = TRUE;
                ColorsViewChanged( hwnd, (HWND)mp2);
                fInChg = FALSE;
            }
            return (0);

        case CAMMSG_PPCHANGED:
            if (!fInChg) {
                fInChg = TRUE;
                ColorsPPChanged( hwnd, (ULONG)mp1, (ULONG)mp2);
                fInChg = FALSE;
            }
            return (0);

        case WM_SYSCOMMAND:
            if (SHORT1FROMMP( mp1) != SC_CLOSE)
                break;
            WinDestroyWindow( hwnd);
            return (0);

        case WM_CLOSE:
            WinDestroyWindow( hwnd);
            return (0);

        case WM_FOCUSCHANGE:
            if (SHORT1FROMMP(mp2) && !WinIsWindowEnabled( hwnd)) {
                WinPostMsg( WinQueryWindow( hwnd, QW_OWNER),
                            CAMMSG_FOCUSCHANGE, 0, 0);
                return (0);
            }
            break;

        case WM_INITDLG:
            return ((MRESULT)ColorsDlgInit( hwnd));

        case WM_SAVEAPPLICATION:
            WinStoreWindowPos( CAMINI_APP, CAMINI_CLRPOS, hwnd);
            break;

        case WM_DESTROY: {
            WindowClosed( IDD_COLORS);
            break;
        }

    } //end switch (msg)

    return (WinDefDlgProc( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

LONG        ColorsDlgInit( HWND hwnd)

{
    HWND            hTemp;
    LONG            ndx;
    PFNWP           pfnTemp;
    char            buf[64];

    LoadDlgStrings( hwnd, nlsColorsDlg);
    memcpy( ViewUndo, ViewInfo, sizeof(ViewInfo));

    hTemp = WinWindowFromID( hwnd, IDC_CLRCB);
    for (ndx = 0; ndx < eCNTVIEWS; ndx++) {
        LoadStringNoTilde( ViewInfo[ndx].str, buf);
        WinSendMsg( hTemp, LM_INSERTITEM, (MP)ndx, (MP)buf);
    }

    // this has to happen before we set the windows' colors
    // or else they'll be replaced
    ShowDialog( hwnd, CAMINI_CLRPOS);

    WinSendMsg( hTemp, LM_SELECTITEM, (MP)pcvView->ndx, (MP)TRUE);

    hTemp = WinWindowFromID( hwnd, IDC_CLRWINDOW);
    pfnTemp = WinSubclassWindow( hTemp, &ColorsWndSubProc);
    WinSetWindowULong( hTemp, QWL_USER, (ULONG)pfnTemp);

    hTemp = WinWindowFromID( hwnd, IDC_CLRHILITE);
    pfnTemp = WinSubclassWindow( hTemp, &ColorsWndSubProc);
    WinSetWindowULong( hTemp, QWL_USER, (ULONG)pfnTemp);

    hTemp = WinWindowFromID( hwnd, IDC_CLRGROUP);
    pfnTemp = WinSubclassWindow( hTemp, &ColorsWndSubProc);
    WinSetWindowULong( hTemp, QWL_USER, (ULONG)pfnTemp);

    hTemp = WinWindowFromID( hwnd, IDC_CLRSEPARATOR);
    pfnTemp = WinSubclassWindow( hTemp, &ColorsWndSubProc);
    WinSetWindowULong( hTemp, QWL_USER, (ULONG)pfnTemp);

    return 0;
}

/****************************************************************************/

void        ColorsViewChanged( HWND hwnd, HWND hCB)

{
    LONG    ndx;

    ndx = (LONG)WinSendMsg( hCB, LM_QUERYSELECTION, (MP)LIT_FIRST, 0);
    ColorsSetDlgPP( hwnd, ndx);

    return;
}

/****************************************************************************/

void        ColorsUndo( HWND hwnd)

{
    LONG    ndx;

    ndx = (LONG)WinSendDlgItemMsg( hwnd, IDC_CLRCB, LM_QUERYSELECTION,
                                   (MP)LIT_FIRST, 0);
    ViewInfo[ndx] = ViewUndo[ndx];
    ColorsSetDlgPP( hwnd, ndx);
    ColorsSetCnrPP( hwnd, ndx);

    return;
}

/****************************************************************************/

void        ColorsDefault( HWND hwnd)

{
    LONG    ndx;

    ndx = (LONG)WinSendDlgItemMsg( hwnd, IDC_CLRCB, LM_QUERYSELECTION,
                                   (MP)LIT_FIRST, 0);
    ViewInfo[ndx] = ViewDflt[ndx];
    ColorsSetDlgPP( hwnd, ndx);
    ColorsSetCnrPP( hwnd, ndx);

    return;
}

/****************************************************************************/

void        ColorsSetDlgPP( HWND hwnd, LONG ndx)

{
    HWND        hTemp;
    ULONG       lth;
    CAMViewPtr  pView;

    pView = &ViewInfo[ndx];
    lth = strlen( pView->font) + 1;

    hTemp = WinWindowFromID( hwnd, IDC_CLRWINDOW);
    WinSetPresParam( hTemp, PP_FOREGROUNDCOLOR, 4, &pView->clrFore);
    WinSetPresParam( hTemp, PP_BACKGROUNDCOLOR, 4, &pView->clrBack);
    WinSetPresParam( hTemp, PP_FONTNAMESIZE,  lth, &pView->font);

    hTemp = WinWindowFromID( hwnd, IDC_CLRHILITE);
    WinSetPresParam( hTemp, PP_FOREGROUNDCOLOR, 4, &pView->clrHiliteFore);
    WinSetPresParam( hTemp, PP_BACKGROUNDCOLOR, 4, &pView->clrHiliteBack);
    WinSetPresParam( hTemp, PP_FONTNAMESIZE,  lth, &pView->font);

    if (ndx == eDETAILS) {
        hTemp = WinWindowFromID( hwnd, IDC_CLRSEPARATOR);
        WinShowWindow( hTemp, TRUE);
        WinShowWindow( WinWindowFromID( hwnd, IDC_CLRGROUP), FALSE);
        WinSetPresParam( hTemp, PP_FOREGROUNDCOLOR, 4, &pView->clrGroupFore);
        WinSetPresParam( hTemp, PP_BACKGROUNDCOLOR, 4, &pView->clrBack);
        WinSetPresParam( hTemp, PP_FONTNAMESIZE,  lth, &pView->font);
    }
    else {
        hTemp = WinWindowFromID( hwnd, IDC_CLRGROUP);
        WinShowWindow( hTemp, TRUE);
        WinShowWindow( WinWindowFromID( hwnd, IDC_CLRSEPARATOR), FALSE);
        WinSetPresParam( hTemp, PP_FOREGROUNDCOLOR, 4, &pView->clrGroupFore);
        WinSetPresParam( hTemp, PP_BACKGROUNDCOLOR, 4, &pView->clrGroupBack);
        WinSetPresParam( hTemp, PP_FONTNAMESIZE,  lth, &pView->font);
    }

    return;
}

/****************************************************************************/

void        ColorsSetCnrPP( HWND hwnd, LONG ndx)

{
    HWND    hCnr;

    if (ndx != pcvView->ndx)
        return;

    hCnr = WinWindowFromID( WinQueryWindow( hwnd, QW_OWNER), FID_CLIENT);

    WinEnableWindowUpdate( hCnr, FALSE);

    WinSetPresParam( hCnr, PP_FOREGROUNDCOLOR, 4, &pcvView->clrFore);
    WinSetPresParam( hCnr, PP_BACKGROUNDCOLOR, 4, &pcvView->clrBack);
    WinSetPresParam( hCnr, PP_HILITEFOREGROUNDCOLOR, 4, &pcvView->clrHiliteFore);
    WinSetPresParam( hCnr, PP_HILITEBACKGROUNDCOLOR, 4, &pcvView->clrHiliteBack);
    WinSetPresParam( hCnr, PP_FONTNAMESIZE,
                     strlen( pcvView->font)+1, pcvView->font);

    if (pcvView->ndx == eDETAILS)
        WinSetPresParam( hCnr, PP_BORDERCOLOR, 4, &pcvView->clrGroupFore);

    WinShowWindow( hCnr, TRUE);

    return;
}

/****************************************************************************/

void        ColorsPPChanged( HWND hwnd, ULONG pp, ULONG id)

{
    HWND        hSrc;
    HWND    	hTemp;
    LONG    	clr;
    LONG        ndx;
    ULONG       lth;
    CAMViewPtr  pView;

do {
    hSrc = WinWindowFromID( hwnd, id);
    hTemp = WinWindowFromID( hwnd, IDC_CLRCB);
    ndx = (ULONG)WinSendMsg( hTemp, LM_QUERYSELECTION, (MP)LIT_FIRST, 0);
    pView = &ViewInfo[ndx];

    if (pp == PP_FONTNAMESIZE) {
        if (id != FID_CLIENT &&
            !WinQueryPresParam( hSrc, pp, 0, 0, sizeof(pView->font),
                                &pView->font, QPF_NOINHERIT))
            break;

        lth = strlen( pView->font) + 1;

        if (id != IDC_CLRWINDOW) {
            hTemp = WinWindowFromID( hwnd, IDC_CLRWINDOW);
            WinSetPresParam( hTemp, PP_FONTNAMESIZE, lth, &pView->font);
        }
        if (id != IDC_CLRHILITE) {
            hTemp = WinWindowFromID( hwnd, IDC_CLRHILITE);
            WinSetPresParam( hTemp, PP_FONTNAMESIZE, lth, &pView->font);
        }

        if (ndx == eDETAILS) {
            if (id != IDC_CLRSEPARATOR) {
                hTemp = WinWindowFromID( hwnd, IDC_CLRSEPARATOR);
                WinSetPresParam( hTemp, PP_FONTNAMESIZE, lth, &pView->font);
            }
        }
        else
            if (id != IDC_CLRGROUP) {
                hTemp = WinWindowFromID( hwnd, IDC_CLRGROUP);
                WinSetPresParam( hTemp, PP_FONTNAMESIZE, lth, &pView->font);
            }

        if (id != FID_CLIENT && pView == pcvView) {
            hTemp = WinQueryWindow( hwnd, QW_OWNER);
            hTemp = WinWindowFromID( hTemp, FID_CLIENT);
            WinSetPresParam( hTemp, PP_FONTNAMESIZE, lth, &pView->font);
        }

        break;
    }

    if (pp != PP_FOREGROUNDCOLOR && pp != PP_BACKGROUNDCOLOR)
        break;

    if (id != FID_CLIENT &&
        !WinQueryPresParam( hSrc, pp, 0, 0, sizeof(clr),
                            &clr, QPF_PURERGBCOLOR | QPF_NOINHERIT))
        break;

    if (id == IDC_CLRWINDOW) {
        if (pp == PP_FOREGROUNDCOLOR)
            pView->clrFore = clr;
        else
            pView->clrBack = clr;
    }
    else
    if (id == IDC_CLRHILITE) {
        if (pp == PP_FOREGROUNDCOLOR) {
            pView->clrHiliteFore = clr;
            pp = PP_HILITEFOREGROUNDCOLOR;
        }
        else {
            pView->clrHiliteBack = clr;
            pp = PP_HILITEBACKGROUNDCOLOR;
        }
    }
    else
    if (id == IDC_CLRGROUP) {
        if (pp == PP_FOREGROUNDCOLOR)
            pView->clrGroupFore = clr;
        else
            pView->clrGroupBack = clr;
    }
    else
    if (id == IDC_CLRSEPARATOR) {
        if (pp == PP_FOREGROUNDCOLOR) {
            pView->clrGroupFore = clr;
            pp = PP_BORDERCOLOR;
        }
        else
            pView->clrBack = clr;
    }

    if (pView != pcvView)
        break;

    if (id == FID_CLIENT) {
        WinSetPresParam( WinWindowFromID( hwnd, IDC_CLRWINDOW), pp, 4,
                         ((pp == PP_FOREGROUNDCOLOR) ? 
                          &pView->clrFore : &pView->clrBack));
        if (pp == PP_BACKGROUNDCOLOR)
            WinSetPresParam( WinWindowFromID( hwnd, IDC_CLRSEPARATOR), pp, 4,
                             &pView->clrBack);
    }
    else {
        hTemp = WinQueryWindow( hwnd, QW_OWNER);
        hTemp = WinWindowFromID( hTemp, FID_CLIENT);

        if (id == IDC_CLRGROUP) {
            WinInvalidateRect( hTemp, 0, TRUE);
            WinUpdateWindow( hTemp);
        }
        else
            WinSetPresParam( hTemp, pp, 4, &clr);
    }

} while (0);

    return;
}

/****************************************************************************/

MRESULT _System ColorsWndSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    PFNWP   pfn = (PFNWP)WinQueryWindowULong( hwnd, QWL_USER);

    if (msg == WM_PRESPARAMCHANGED &&
        (mp1 == (MP)PP_FOREGROUNDCOLOR ||
         mp1 == (MP)PP_BACKGROUNDCOLOR ||
         mp1 == (MP)PP_FONTNAMESIZE))
        WinSendMsg( WinQueryWindow( hwnd, QW_PARENT), CAMMSG_PPCHANGED,
                    mp1, (MP)(ULONG)WinQueryWindowUShort( hwnd, QWS_ID));

    return (pfn( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

