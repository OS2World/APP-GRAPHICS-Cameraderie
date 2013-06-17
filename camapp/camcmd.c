/****************************************************************************/
// camcmd.c
//
//  - popup menu init
//  - WM_COMMAND handler
//  - mark/unmark & retitle container records
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

BOOL            InitMenus( HWND hwnd);
void            LoadMenuStrings( HWND hMenu, ULONG idMenu);
HWND            QueryGroupMenu( void);
BOOL            IsMultipleSel( HWND hwnd, CAMRecPtr pcr);
void            PopupMenu( HWND hwnd, CAMRecPtr pcr);
void            Command( HWND hwnd, ULONG ulCmd);
void            SetMultTitles( HWND hwnd, CAMRecPtr pcrSrc);
void            UndoTitle( HWND hwnd, ULONG ulCmd);
void            ToggleMark( HWND hwnd, ULONG ulCmd);
void            SetMark( HWND hwnd, ULONG ulCmd);
void            UpdateMark( CAMRecPtr pcr, HWND hCnr, STATSTUFF * pss,
                            ULONG ulOp, BOOL fToggle);
void            RotateThumbBtn( HWND hwnd, ULONG ulCmd);
void            RotateThumbMenu( HWND hwnd, ULONG ulCmd);

/****************************************************************************/

static HWND     hmnuSingle = 0;
static HWND     hmnuMultiple = 0;
static HWND     hmnuGroup = 0;


static CAMNLS   nlsSingle[] = {
    { IDM_WRITE,        MNU_Save },
    { IDM_DELETE,       MNU_Delete },
    { IDM_UNMARK,       MNU_Unmark },
    { IDM_UNDOTTL,      MNU_UndoTitle },
    { IDM_ROTLEFT,      MNU_RotateLeft },
    { IDM_ROTRIGHT,     MNU_RotateRight },
    { IDM_SHOW,         MNU_Show },
    { IDM_GRPMENU,      MNU_SetGroup },
    { IDM_GRPMNUFIRST,  GRP_Default },
    { 0,                0 }};

static CAMNLS   nlsMultiple[] = {
    { IDM_WRITEALL,     MNU_SaveAll },
    { IDM_DELETEALL,    MNU_DeleteAll },
    { IDM_UNMARKALL,    MNU_UnmarkAll },
    { IDM_UNDOTTLALL,   MNU_UndoAllTitles },
    { IDM_GRPMENU,      MNU_SetGroup },
    { 0,                0 }};

static CAMNLS   nlsMainMenu[] = {
    { IDM_FILE,         MNU_File },
    { IDM_OPEN,         MNU_Open },
    { IDM_SYNCDATA,     MNU_Refresh },
    { IDM_SAVE,         MNU_SaveNow },
    { IDM_INFO,         MNU_CameraInfo },
    { SC_CLOSE,         MNU_Exit },
    { IDM_SORT,         MNU_Sort },
    { IDM_SORTSYM,      MNU_Indicators },
    { IDM_SORTDONE,     MNU_Done },
    { IDM_VIEW,         MNU_View },
    { IDM_DETAILS,      MNU_Details },
    { IDM_SMALL,        MNU_Small },
    { IDM_MEDIUM,       MNU_Medium },
    { IDM_LARGE,        MNU_Large },
    { IDM_COLRESET,     MNU_Reset },
    { IDM_COLORDER,     MNU_Rearrange },
    { IDM_COLORS,       MNU_Colors },
    { IDM_GROUPS,       MNU_Groups },
    { IDM_EDITGRP,      MNU_EditGroups },
    { IDM_GRPMENU,      MNU_SetGroup },
    { IDM_OPTIONS,      MNU_Options },
    { IDM_ROTATEOPTS,   MNU_Rotate },
    { IDM_ROTONLOAD,    MNU_RotateOnLoad },
    { IDM_ROTONSAVE,    MNU_RotateOnSave },
    { IDM_REMOVABLES,   MNU_Removables },
    { IDM_AUTOMOUNT,    MNU_MountDrives },
    { IDM_EJECTINFO,    MNU_EjectInfo },
    { IDM_EJECTDLG,     MNU_ShowEjectDlg },
    { IDM_EJECTALWAYS,  MNU_AlwaysEject },
    { IDM_EJECTNEVER,   MNU_NeverEject },
    { IDM_RESTOREDEF,   MNU_Restore },
    { 0,                0 }};


/****************************************************************************/

BOOL        InitMenus( HWND hwnd)

{
    HWND        hMenu;
    MENUITEM    mi;

    // load the menubar as an object window then reassign it;
    // this ensures it will use the default system menu font
    // rather than the frame's font
    hMenu = WinLoadMenu( HWND_OBJECT, 0, IDM_MAIN);
    if (!hMenu)
        return (FALSE);
    WinSetParent( hMenu, hwnd, FALSE);
    WinSetOwner( hMenu, hwnd);
    LoadMenuStrings( hMenu, IDM_MAIN);

    hmnuSingle = WinLoadMenu( HWND_OBJECT, 0, IDM_SINGLE);
    WinSetWindowUShort( hmnuSingle, QWS_ID, IDM_SINGLE);
    LoadMenuStrings( hmnuSingle, IDM_SINGLE);

    hmnuMultiple = WinLoadMenu( HWND_OBJECT, 0, IDM_MULTIPLE);
    WinSetWindowUShort( hmnuMultiple, QWS_ID, IDM_MULTIPLE);
    LoadMenuStrings( hmnuMultiple, IDM_MULTIPLE);

    WinSendMsg( hmnuSingle, MM_QUERYITEM, MPFROM2SHORT( IDM_GRPMENU, 0), (MP)&mi);
    hmnuGroup = mi.hwndSubMenu;

    WinSendMsg( hmnuMultiple, MM_QUERYITEM, MPFROM2SHORT( IDM_GRPMENU, 0), (MP)&mi);
    mi.afStyle = MIS_SUBMENU;
    mi.hwndSubMenu = hmnuGroup;
    WinSendMsg( hmnuMultiple, MM_SETITEM, MPFROM2SHORT( IDM_GRPMENU, 0), (MP)&mi);

    WinSendMsg( hMenu, MM_QUERYITEM, MPFROM2SHORT( IDM_GRPMENU, TRUE), (MP)&mi);
    mi.afStyle = MIS_SUBMENU;
    mi.hwndSubMenu = hmnuGroup;
    WinSendMsg( hMenu, MM_SETITEM, MPFROM2SHORT( IDM_GRPMENU, TRUE), (MP)&mi);

    return (TRUE);
}

/****************************************************************************/

void        LoadMenuStrings( HWND hMenu, ULONG idMenu)

{
    CAMNLSPtr   pnls;
    char        szText[256];

    if (idMenu == IDM_MAIN)
        pnls = nlsMainMenu;
    else
    if (idMenu == IDM_SINGLE)
        pnls = nlsSingle;
    else
    if (idMenu == IDM_MULTIPLE)
        pnls = nlsMultiple;
    else
        return;

    while (pnls->id) {
        LoadString( pnls->str, szText);
        WinSendMsg( hMenu, MM_SETITEMTEXT, MPFROM2SHORT( pnls->id, TRUE),
                    szText);
        pnls++;
    }

    return;
}

/****************************************************************************/

HWND        QueryGroupMenu( void)

{
    return (hmnuGroup);
}

/****************************************************************************/

BOOL        IsMultipleSel( HWND hwnd, CAMRecPtr pcr)

{
    BOOL        fRtn = FALSE;
    HWND        hCnr;

    hCnr = WinWindowFromID( hwnd, FID_CLIENT);

    if (pcr->mrc.flRecordAttr & CRA_SELECTED)
        if (WinSendMsg( hCnr, CM_QUERYRECORDEMPHASIS, (MP)CMA_FIRST,
                        (MP)CRA_SELECTED) != (MP)pcr ||
            WinSendMsg( hCnr, CM_QUERYRECORDEMPHASIS, (MP)pcr,
                        (MP)CRA_SELECTED) != 0)
            fRtn = TRUE;

    return (fRtn);
}

/****************************************************************************/

void        PopupMenu( HWND hwnd, CAMRecPtr pcr)

{
    HWND        hMenu;
    HWND        hCnr;
    POINTL      ptl;

do {
    // only show the menu if we're over an item
    if (pcr == 0)
        break;

    hCnr = WinWindowFromID( hwnd, FID_CLIENT);

    // if 0 or 1 item is selected, show the single object menu;
    // if more than 1 is selected, show the multiple object menu
    // if we're using the single object menu, save the item
    // it refers to and set the appropriate menu text

    if (IsMultipleSel( hwnd, pcr))
        hMenu = hmnuMultiple;
    else {
        ULONG   ulStat = pcr->status;

        if (!(pcr->mrc.flRecordAttr & CRA_SELECTED))
            WinSendMsg( hCnr, CM_SETRECORDEMPHASIS, (MP)pcr,
                        MPFROM2SHORT( TRUE, CRA_SOURCE));

        hMenu = hmnuSingle;
        WinSetWindowULong( hMenu, QWL_USER, (ULONG)pcr);

        WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( IDM_WRITE, 0),
                    MPFROM2SHORT( MIA_DISABLED,
                                  (STAT_ISSAVEDONE(ulStat) ? MIA_DISABLED : 0)));

        WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( IDM_DELETE, 0),
                    MPFROM2SHORT( MIA_DISABLED,
                                  (STAT_ISDEL(ulStat) ? MIA_DISABLED : 0)));

        WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( IDM_UNMARK, 0),
                    MPFROM2SHORT( MIA_DISABLED,
                                  (STAT_ISNADA(ulStat) ? MIA_DISABLED : 0)));

        WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( IDM_UNDOTTL, 0),
                    MPFROM2SHORT( MIA_DISABLED,
                                  (pcr->orgname ? 0 : MIA_DISABLED))); 
    }

    WinSetOwner( hmnuGroup, hMenu);
    WinSetWindowULong( hmnuGroup, QWL_USER, hMenu);

    // popup the menu at the mouse pointer
    WinQueryPointerPos( HWND_DESKTOP, &ptl);
    WinPopupMenu( HWND_DESKTOP, hwnd, hMenu, ptl.x, ptl.y, 0,
                  PU_HCONSTRAIN | PU_VCONSTRAIN |
                  PU_MOUSEBUTTON1 | PU_KEYBOARD);

} while (0);

    return;
}

/****************************************************************************/

void        Command( HWND hwnd, ULONG ulCmd)

{
    switch (ulCmd) {

        case IDM_WRITE:
        case IDM_DELETE:
        case IDM_UNMARK:
            ToggleMark( hwnd, ulCmd);
        break;

        case IDM_WRITEALL:
        case IDM_DELETEALL:
        case IDM_UNMARKALL:
            SetMark( hwnd, ulCmd);
            break;

        case IDM_UNDOTTL:
        case IDM_UNDOTTLALL:
            UndoTitle( hwnd, ulCmd);
            break;

        case IDM_SORTDONE:
        case IDM_COLDONE:
            break;

        case IDC_ROTCC:
        case IDC_ROTCW:
            RotateThumbBtn( hwnd, ulCmd);
            break;

        case IDM_ROTLEFT:
        case IDM_ROTRIGHT:
            RotateThumbMenu( hwnd, ulCmd);
            break;

        case IDM_ROTONLOAD:
        case IDM_ROTONSAVE:
            ToggleRotateOpts( hwnd, ulCmd);
            break;

        case IDM_OPEN:
            SaveChanges( hwnd, CAMMSG_OPEN);
            break;

        case IDM_GETDATA:
            GetData( hwnd, FALSE);
            break;

        case IDM_SYNCDATA:
            GetData( hwnd, TRUE);
            break;

        case IDM_COLRESET:
            ResetColumnWidths( hwnd, TRUE);
            break;

        case IDM_RESTOREDEF:
            SetGlobalFlag( CAMFLAG_RESTORE, CAM_TOGGLE);
            WinSendDlgItemMsg( hwnd, FID_MENU, MM_SETITEMATTR,
                               MPFROM2SHORT( IDM_RESTOREDEF, TRUE),
                               MPFROM2SHORT( MIA_CHECKED,
                                     (CAM_IS_RESTORE ? MIA_CHECKED : 0)));
            break;

        case IDM_EJECTDLG:
        case IDM_EJECTALWAYS:
        case IDM_EJECTNEVER:
        case IDM_AUTOMOUNT:
            DrvMenuCmd( hwnd, ulCmd);
            break;

        case IDM_INTUITIVE:
        case IDM_LITERAL:
            SetSortIndicatorMenu( hwnd, ulCmd);
            break;

        case IDM_SAVE:
            SaveChanges( hwnd, 0);
            break;

        case IDM_SHOW: {
            CAMRecPtr pcr =
                (CAMRecPtr)WinQueryWindowULong( hmnuSingle, QWL_USER);
            if (pcr) {
                WinSetWindowULong( hmnuSingle, QWL_USER, 0);
                WinPostMsg( hwnd, CAMMSG_SHOWTHUMB, (MP)pcr, 0);
            }
            break;
        }

        case IDM_COLORDER:
            OpenWindow( hwnd, IDD_COLUMNS, 0);
            break;

        case IDC_EDITGRP:
        case IDM_EDITGRP:
            OpenWindow( hwnd, IDD_GROUPS, 0);
            break;

        case IDM_COLORS:
            OpenWindow( hwnd, IDD_COLORS, 0);
            break;

        case IDM_INFO:
            OpenWindow( hwnd, IDD_INFO, 0);
            break;

        case IDM_DETAILS:
        case IDM_SMALL:
        case IDM_MEDIUM:
        case IDM_LARGE:
            CnrSetView( hwnd, ulCmd);
            break;

        default:
            if (ulCmd >= IDM_SORTFIRST && ulCmd < (IDM_SORTFIRST + eCNTCOLS))
                SetSortColumn( hwnd, ulCmd);
            else
            if (ulCmd >= IDM_COLFIRST && ulCmd < (IDM_COLFIRST + eCNTCOLS))
                SetColumnVisibility( hwnd, ulCmd);
            else
            if (ulCmd >= IDC_GRPFIRST && ulCmd < IDC_GRPLAST)
                SetGroupFromButton( hwnd, ulCmd);
            else
            if (ulCmd >= IDM_GRPMNUFIRST && ulCmd <= IDM_GRPMNULAST)
                SetGroupFromMenu( hwnd, ulCmd);
            break;
    }

    return;
}

/****************************************************************************/
/****************************************************************************/

void        SetMultTitles( HWND hwnd, CAMRecPtr pcrSrc)

{
    HWND            hCnr;
    ULONG           lth;
    CAMRecPtr       pcr;
    char *          ptr;
    char *          pDot;

do {
    hCnr = WinWindowFromID( hwnd, FID_CLIENT);

    // look for the last dot that's part of the filename (vs the path)
    ptr = strrchr( pcrSrc->title, '\\');
    if (!ptr)
        ptr = pcrSrc->title;
    pDot = strrchr( ptr, '.');

    // look for the first '#' that's part of the filename (vs the path)
    ptr = strchr( ptr, '#');

    if (!ptr || (pDot && ptr > pDot)) {
        lth = strlen( pcrSrc->title);
        if (lth > 258)
            break;

        ptr = (char*)(pcrSrc->title + lth);

        if (!pDot) {
            ptr[0] = '#';
            ptr[1] = 0;
        }
        else {
            for (; ptr >= pDot; ptr--)
                ptr[1] = ptr[0];
            *pDot = '#';
        }
    }

    pcr = (CAMRecPtr)CMA_FIRST;
    while ((pcr = (CAMRecPtr)WinSendMsg( hCnr, CM_QUERYRECORDEMPHASIS,
                                         (MP)pcr, (MP)CRA_SELECTED)) != 0) {
        if (pcr == (CAMRecPtr)-1)
            break;

        if (!pcr->orgname)
            pcr->orgname = strdup( pcr->title);

        if (pcr != pcrSrc)
            strcpy( pcr->title, pcrSrc->title);

        WinSendMsg( hCnr, CM_INVALIDATERECORD, (MP)&pcr,
                    MPFROM2SHORT( 1, 0));
    }

} while (0);

    return;
}

/****************************************************************************/

void        UndoTitle( HWND hwnd, ULONG ulCmd)

{
    BOOL            fOK = FALSE;
    HWND            hCnr;
    CAMRecPtr       pcr;

do {
    hCnr = WinWindowFromID( hwnd, FID_CLIENT);

    if (ulCmd == IDM_UNDOTTL) {
        pcr = (CAMRecPtr)WinQueryWindowULong( hmnuSingle, QWL_USER);
        if (!pcr)
            break;

        WinSetWindowULong( hmnuSingle, QWL_USER, 0);
        if (pcr->orgname) {
            strcpy( pcr->title, pcr->orgname);
            free( pcr->orgname);
            pcr->orgname = 0;
            WinSendMsg( hCnr, CM_INVALIDATERECORD, (MP)&pcr,
                        MPFROM2SHORT( 1, 0));
            fOK = TRUE;
        }
        break;
    }

    if (ulCmd != IDM_UNDOTTLALL)
        break;

    pcr = (CAMRecPtr)CMA_FIRST;
    while ((pcr = (CAMRecPtr)WinSendMsg( hCnr, CM_QUERYRECORDEMPHASIS,
                                         (MP)pcr, (MP)CRA_SELECTED)) != 0) {
        if (pcr == (CAMRecPtr)-1)
            break;

        if (pcr->orgname) {
            strcpy( pcr->title, pcr->orgname);
            free( pcr->orgname);
            pcr->orgname = 0;
            WinSendMsg( hCnr, CM_INVALIDATERECORD, (MP)&pcr,
                        MPFROM2SHORT( 1, 0));
            fOK = TRUE;
        }
    }

} while (0);

    if (fOK) {
        pcr = (CAMRecPtr)WinSendMsg( hCnr, CM_QUERYRECORDEMPHASIS,
                                     (MP)CMA_FIRST, (MP)CRA_CURSORED);
        if (pcr)
            SetThumbWndTitle( hwnd, pcr->title);
    }

    return;
}

/****************************************************************************/

void        ToggleMark( HWND hwnd, ULONG ulCmd)

{
    HWND            hCnr;
    STATSTUFF *     pss;
    CAMRecPtr       pcr;

do {
    pss = GetStatStuff();
    hCnr = WinWindowFromID( hwnd, FID_CLIENT);

    pcr = (CAMRecPtr)WinQueryWindowULong( hmnuSingle, QWL_USER);
    WinSetWindowULong( hmnuSingle, QWL_USER, 0);

    if (ulCmd == IDM_UNMARK &&
        (WinGetKeyState( HWND_DESKTOP, VK_CTRL) & 0x8000))
        ulCmd = IDM_UNSAVED;

    // the single-object menu was used
    if (pcr) {
        UpdateMark( pcr, hCnr, pss, ulCmd, FALSE);
        break;
    }

    // the Delete or Enter key was pressed - get each selected object,
    // toggle its status, then have it repainted
    pcr = (CAMRecPtr)CMA_FIRST;
    while ((pcr = (CAMRecPtr)WinSendMsg( hCnr, CM_QUERYRECORDEMPHASIS,
                                         (MP)pcr, (MP)CRA_SELECTED)) != 0) {
        if (pcr == (CAMRecPtr)-1)
            break;

        UpdateMark( pcr, hCnr, pss, ulCmd, TRUE);
    }

} while (0);

    UpdateStatus( hwnd, 0);

    return;
}

/****************************************************************************/

void        SetMark( HWND hwnd, ULONG ulCmd)

{
    HWND            hCnr;
    STATSTUFF *     pss;
    CAMRecPtr       pcr;

    pss = GetStatStuff();
    hCnr = WinWindowFromID( hwnd, FID_CLIENT);

    if (ulCmd == IDM_WRITEALL)
        ulCmd = IDM_WRITE;
    else
    if (ulCmd == IDM_DELETEALL)
        ulCmd = IDM_DELETE;
    else
    if (ulCmd == IDM_UNMARKALL &&
        (WinGetKeyState( HWND_DESKTOP, VK_CTRL) & 0x8000))
        ulCmd = IDM_UNSAVED;
    else
        ulCmd = IDM_UNMARK;

    // the multiple-object menu was used - get each selected object,
    // set its status, then have it repainted
    pcr = (CAMRecPtr)CMA_FIRST;
    while ((pcr = (CAMRecPtr)WinSendMsg( hCnr, CM_QUERYRECORDEMPHASIS,
                                         (MP)pcr, (MP)CRA_SELECTED)) != 0) {
        if (pcr == (CAMRecPtr)-1)
            break;

        UpdateMark( pcr, hCnr, pss, ulCmd, FALSE);
    }

    UpdateStatus( hwnd, 0);

    return;
}

/****************************************************************************/

void        UpdateMark( CAMRecPtr pcr, HWND hCnr, STATSTUFF * pss,
                        ULONG ulOp, BOOL fToggle)

{
    ULONG   ulStat = 0;

do {
    if (STAT_ISDONE(pcr->status)) {
        if (ulOp == IDM_WRITE || ulOp == IDM_SAVED || ulOp == IDM_SAVEDMARK)
            break;
        ulStat = STAT_DONE;
    }

    if (STAT_ISSAV( pcr->status)) {
        pss->ulSavCnt--;
        pss->ulSavSize -= pcr->size;
        if (fToggle && ulOp == IDM_WRITE)
            ulOp = IDM_UNMARK;
    }
    else
    if (STAT_ISDEL( pcr->status)) {
        pss->ulDelCnt--;
        pss->ulDelSize -= pcr->size;
        if (fToggle && ulOp == IDM_DELETE)
            ulOp = IDM_UNMARK;
    }

    if (ulOp == IDM_WRITE)
        ulStat |= STAT_SAVE;
    else
    if (ulOp == IDM_DELETE)
        ulStat |= STAT_DELETE;
    else
    if (ulOp == IDM_SAVEDMARK)
        ulStat |= STAT_DELETE | STAT_DONE;
    else
    if (ulOp == IDM_SAVED)
        ulStat |= STAT_DONE;
    else
    if (ulOp == IDM_UNSAVED)
        ulStat = 0;
    else
    if (ulOp != IDM_UNMARK) {
        printf( "UpdateMark - invalid command - %ld\n", ulOp);
        break;
    }

    pcr->status &= ~STAT_MASK;
    pcr->status |= ulStat;

    if (ulStat & STAT_DONE)
        pcr->mrc.flRecordAttr |= CRA_INUSE;
    else
        pcr->mrc.flRecordAttr &= ~CRA_INUSE;

    if (ulStat == STAT_DELETE || ulStat == (STAT_DELETE | STAT_DONE)) {
        pcr->mark = pss->hDelIco;
        pss->ulDelCnt++;
        pss->ulDelSize += pcr->size;
    }
    else
    if (ulStat == STAT_SAVE) {
        pss->ulSavCnt++;
        pss->ulSavSize += pcr->size;
        pcr->mark = pss->hSavIco;
    }
    else
    if (ulStat == STAT_DONE)
        pcr->mark = pss->hDoneIco;
    else
    if (ulStat == 0)
        pcr->mark = pss->hBlkIco;
    else
        printf( "UpdateMark - invalid status - %ld\n", ulStat);

    WinSendMsg( hCnr, CM_INVALIDATERECORD, (MP)&pcr,
                MPFROM2SHORT( 1, CMA_NOREPOSITION));

} while (0);

    return;
}

/****************************************************************************/
//  Rotate
/****************************************************************************/

void        RotateThumbBtn( HWND hwnd, ULONG ulCmd)

{
    HWND        hSidebar;
    HWND        hStatic;
    ULONG       ulRot;
    CAMRecPtr   pcr;

do {
    if (ulCmd == IDC_ROTCC)
        ulRot = STAT_ROT90;
    else
    if (ulCmd == IDC_ROTCW)
        ulRot = STAT_ROT270;
    else
        break;

    hSidebar = WinWindowFromID( hwnd, IDC_SIDEBAR);
    hStatic = WinWindowFromID( hSidebar, IDC_BMP);
    pcr = (CAMRecPtr)WinQueryWindowULong( hStatic, QWL_USER);
    if (!pcr)
        break;

    if (!RotateThumbBmp( pcr, ulRot))
        break;

    WinSendMsg( hStatic, SM_SETHANDLE, (MP)pcr->bmp, 0);
    WinSendDlgItemMsg( hwnd, FID_CLIENT, CM_INVALIDATERECORD,
                       (MP)&pcr, MPFROM2SHORT( 1, CMA_NOREPOSITION | CMA_ERASE));

} while (0);

    return;
}

/****************************************************************************/

void        RotateThumbMenu( HWND hwnd, ULONG ulCmd)

{
    HWND        hSidebar;
    HWND        hStatic;
    ULONG       ulRot;
    CAMRecPtr   pcr;

do {
    if (ulCmd == IDM_ROTLEFT)
        ulRot = STAT_ROT90;
    else
    if (ulCmd == IDM_ROTRIGHT)
        ulRot = STAT_ROT270;
    else
        break;

    pcr = (CAMRecPtr)WinQueryWindowULong( hmnuSingle, QWL_USER);
    WinSetWindowULong( hmnuSingle, QWL_USER, 0);
    if (!pcr)
        break;

    if (!RotateThumbBmp( pcr, ulRot))
        break;

    hSidebar = WinWindowFromID( hwnd, IDC_SIDEBAR);
    hStatic = WinWindowFromID( hSidebar, IDC_BMP);
    if (pcr == (CAMRecPtr)WinQueryWindowULong( hStatic, QWL_USER))
        WinSendMsg( hStatic, SM_SETHANDLE, (MP)pcr->bmp, 0);

    WinSendDlgItemMsg( hwnd, FID_CLIENT, CM_INVALIDATERECORD,
                       (MP)&pcr, MPFROM2SHORT( 1, CMA_NOREPOSITION | CMA_ERASE));

} while (0);

    return;
}

/****************************************************************************/

