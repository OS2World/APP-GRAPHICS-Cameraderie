/****************************************************************************/
// camgrp.c
//
//  - everything related to the Groups feature:  loading, storing,
//    manipulating the buttons, the Group dialog, etc.
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

#define INCL_DOSERRORS
#include "camapp.h"
#include "camchklist.h"

/****************************************************************************/

#define GRP_UP      0x0001
#define GRP_DOWN    0x0002
#define GRP_NEW     0x0004
#define GRP_DEL     0x0008
#define GRP_FIND    0x0010
#define GRP_CREATE  0x0020
#define GRP_UNDO    0x0040
#define GRP_APPLY   0x0080
#define GRP_UNDEL   0x0100

#define GRP_MOVE    0x0003
#define GRP_ALL     0x00FF
#define GRP_DELBTN  0x0108

/****************************************************************************/

void            InitGroups( HWND hwnd);
void            InitGroupPaths( HWND hwnd);
CAMGroupPtr     GetDefaultGroup( char ** ppszName);
void            SetGroupFromButton( HWND hwnd, ULONG ulCmd);
void            SetGroupFromMenu( HWND hwnd, ULONG ulCmd);
void            SetGroup( HWND hwnd, CAMGroupPtr pcg, CAMRecPtr pcr);
void            InitGroupButtonPos( HWND hSidebar);
LONG            RepositionGroupButtons( HWND hSidebar, LONG cySidebar);
void            RestoreGroups( void);
void            StoreGroups( void);
void            AddSampleGroups( void);
void            InitGroupMenu( void);
void            UpdateGroupMenuName( CAMGroupPtr pcg);
MRESULT _System GroupDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
void            InitGroupDlg( HWND hwnd);
void            SetGroupBtn( HWND hwnd, BOOL fEnable);
void            GroupItemSelected( HWND hwnd, CLITEM * pcli);
void            GroupItemDeselected( HWND hwnd, CLITEM * pcli);
void            UndoGroupItem( HWND hwnd);
void            GroupItemClicked( HWND hwnd, CLITEM * pcli);
void            ChangeGroupOrder( HWND hwnd, BOOL fUp);
void            NewGroup( HWND hwnd);
void            DeleteGroup( HWND hwnd);
void            UndeleteGroup( HWND hwnd);
void            FindGroupPath( HWND hwnd);
void            CreateGroupPath( HWND hwnd);
void            ShowGroupItem( HWND hwnd, CAMGroupPtr pcg);
BOOL            ValidateGroupItem( HWND hwnd, CLITEM * pcli);
void            ValidateGroupFile( HWND hwnd, CAMGroupPtr pcg);
BOOL            ValidateGroupName( HWND hwnd, CLITEM * pcli);
BOOL            ValidateGroupPath( HWND hwnd, CAMGroupPtr pcg);
void            SetValidationText( HWND hwnd, ULONG ulID, ULONG ulStr, BOOL fErr);
void            SetGroupDlgCtrls( HWND hwnd, CAMGroupPtr pcg, BOOL fErr);
LONG            AdjustGroupIndex( void);
BOOL            GroupButtonsAvailable( void);
void            AdjustGroupButtons( HWND hwnd);
void            UnmarkDeletedGroup( HWND hwnd, CAMGroupPtr pcgDel);
char *          InsertGroupMnemonic( char * szName, char * szOut, LONG ndx);
LONG            RemoveGroupMnemonic( char * szName);
BOOL            MatchGroupMnemonic( HWND hwnd, USHORT usChar);

/****************************************************************************/

static LONG         cyTopMargin = 0;
static LONG         cyHeight = 0;
static ULONG    	ulErr = 0;
static RGB      	rgbBlack = { 0, 0, 0};
static RGB          rgbRed   = { 0, 0, 255};
static CAMGroupPtr  pcgDeleted = 0;

static CAMGroup     cgUndo;
static CAMGroup     cgDefault = {
                                0,              // pNext
                                0,              // ndx
                                -1,             // mnemonic (ndx)
                                IDC_GRPFIRST,   // btn
                                0,              // hWPS
                                1,              // seqNbr
                                4,              // cntDigits
                                "",             // name
                                "",             // file
                                "",             // path
                                };

static CAMGroupPtr  pcgFirst = &cgDefault;

static CAMGroup     cgSample[4] = {
    { 0, 1, 0, IDC_GRPFIRST+1, 0, 1, 4, "xXx", "xXx", "" },
    { 0, 2, 0, IDC_GRPFIRST+2, 0, 1, 4, "xXx", "xXx", "" },
    { 0, 3, 0, IDC_GRPFIRST+3, 0, 1, 4, "xXx", "xXx", "" },
    { 0, 4, 0, IDC_GRPFIRST+4, 0, 1, 4, "xXx", "xXx", "" }};

// stringtable IDs for sample group names & file templates
static ULONG        ulSampleNames[4] =
    { GRP_Family, GRP_Travel, GRP_Nature, GRP_Other };
static ULONG        ulSampleTempls[4] =
    { GRP_FamilyTempl, GRP_TravelTempl, GRP_NatureTempl, GRP_OtherTempl };

static char     szYMD[16]       = "yYmMdD";
static char     szIniDefault[]  = "default";
static char     szX[]           = "xXx";

static char *   pszGRP_Default  = szX;
static char *   pszMSG_Deleted  = szX;
static char *   pszMSG_Btn      = szX;
static char *   pszMSG_Group    = szX;
static char *   pszMSG_NewGroup = szX;
static char *   pszBTN_Delete   = szX;
static char *   pszBTN_Undelete = szX;
static char *   pszSUB_Year     = szX;
static char *   pszSUB_Month    = szX;
static char *   pszSUB_Day      = szX;

static CAMNLSPSZ    nlsCamGrp[] = {
    { &pszGRP_Default,      GRP_Default },
    { &pszMSG_Deleted,      MSG_Deleted },
    { &pszMSG_Btn,          MSG_Btn },
    { &pszMSG_Group,        MSG_Group },
    { &pszMSG_NewGroup,     MSG_NewGroup },
    { &pszBTN_Delete,       BTN_Delete },
    { &pszBTN_Undelete,     BTN_Undelete },
    { &pszSUB_Year,         SUB_Year },
    { &pszSUB_Month,        SUB_Month },
    { &pszSUB_Day,          SUB_Day },
    { 0,                    0 }};

static CAMNLS       nlsGroupDlg[] = {
    { IDC_GRPNAMETXT,       DLG_GroupName },
    { IDC_GRPPATHTXT,       DLG_SaveDir },
    { IDC_GRPFILETXT,       DLG_Template },
    { IDC_GRPSEQTXT,        DLG_Serial },
    { IDC_GRPDIGTXT,        DLG_Digits },
    { IDC_GRPINFO1,         DLG_InsertsSerial },
    { IDC_GRPINFO2,         DLG_CopiesFilename },
    { IDC_GRPINFO3,         DLG_InsertYMD },
    { IDC_GRPNEW,           BTN_New },
    { IDC_GRPDEL,           BTN_Delete },
    { IDC_GRPFIND,          BTN_Find },
    { IDC_GRPCREATE,        BTN_Create },
    { IDC_GRPAPPLY,         BTN_Apply },
    { IDC_GRPUNDO,          BTN_Undo },
    { 0,                    0 }};


/****************************************************************************/
/****************************************************************************/

void        InitGroups( HWND hwnd)

{
    HWND        hSidebar;
    LONG        ndx;
    ULONG       btn;
    CAMGroupPtr pcg;
    char *      ptr;
    char        szText[32];

    LoadStaticStrings( nlsCamGrp);
    RestoreGroups();
    hSidebar = WinWindowFromID( hwnd, IDC_SIDEBAR);

    ndx = 0;
    btn = IDC_GRPFIRST;

    pcgFirst->ndx = ndx++;
    pcgFirst->btn = btn++;
    WinSetDlgItemText( hSidebar, pcgFirst->btn, pszGRP_Default);

    for (pcg = pcgFirst->pNext; pcg; pcg = pcg->pNext, ndx++) {
        pcg->ndx = ndx;
        if (pcg->btn && btn < IDC_GRPLAST) {
            pcg->btn = btn++;
            ptr = InsertGroupMnemonic( pcg->name, szText, pcg->mnemonic);
            WinSetDlgItemText( hSidebar, pcg->btn, ptr);
        }
        else
            pcg->btn = 0;
    }

    InitGroupButtonPos( hSidebar);
    InitGroupPaths( hwnd);
    InitGroupMenu();

    return;
}

/****************************************************************************/

void        InitGroupPaths( HWND hwnd)

{
    HWND        hSidebar;
    BOOL        fErr = FALSE;
    APIRET      rc;
    char *      ptr;
    CAMGroupPtr pcg;
    char        szPath[CCHMAXPATH];

    hSidebar = WinWindowFromID( hwnd, IDC_SIDEBAR);
    sprintf( szYMD, "%s%s%s", pszSUB_Year, pszSUB_Month, pszSUB_Day);

    for (pcg = pcgFirst; pcg; pcg = pcg->pNext) {
        if (*pcg->path == 0) {
            pcg->hWPS = 0;
            continue;
        }

        rc = ValidatePath( pcg->path);
        if (!rc) {
            pcg->hWPS = WinQueryObject( pcg->path);
            continue;
        }

        if (pcg->hWPS && pcg->hWPS != (ULONG)-1 &&
            WinQueryObjectPath( pcg->hWPS, szPath, sizeof(szPath)) &&
            !ValidatePath( szPath)) {
            strcpy( pcg->path, szPath);
            continue;
        }

        pcg->hWPS = (ULONG)-1;

        if (rc != ERROR_FILE_NOT_FOUND && rc != ERROR_PATH_NOT_FOUND) {
            if (rc)
                fErr = TRUE;
            continue;
        }

        // look for date metacharacters & escaped %
        strcpy( szPath, pcg->path);
        ptr = szPath;
        while ((ptr = strchr( ptr, '%')) != 0) {
            ptr++;
            if (*ptr == '%') {
                strcpy( ptr, ptr+1);
                rc = (APIRET)-1;
            }
            else
            if (strchr( szYMD, *ptr)) {
                rc = 0;
                break;
            }
        }

        // if the error was caused by an escaped %, revalidate
        if (rc == (APIRET)-1 && ValidatePath( szPath))
        	fErr = TRUE;
    }

    if (fErr)
        WinSetPresParam( WinWindowFromID( hSidebar, IDC_EDITGRP),
                         PP_FOREGROUNDCOLOR, sizeof(RGB), &rgbRed);

    return;
}

/****************************************************************************/

CAMGroupPtr GetDefaultGroup( char ** ppszName)

{
    if (ppszName)
        *ppszName = pcgFirst->name;
    return (pcgFirst);
}

/****************************************************************************/

void        SetGroupFromButton( HWND hwnd, ULONG ulCmd)

{
    CAMGroupPtr     pcg;

    for (pcg = pcgFirst; pcg; pcg = pcg->pNext)
        if (ulCmd == pcg->btn)
            break;

    SetGroup( hwnd, pcg, 0);

    return;
}

/****************************************************************************/

void        SetGroupFromMenu( HWND hwnd, ULONG ulCmd)

{
    HWND            hMenu;
    CAMGroupPtr     pcg;
    CAMRecPtr       pcr;

    ulCmd -= IDM_GRPMNUFIRST;

    for (pcg = pcgFirst; pcg; pcg = pcg->pNext)
        if (ulCmd == pcg->ndx)
            break;

    if (!pcg)
        return;

    hMenu = QueryGroupMenu();
    if (!hMenu)
        return;
    hMenu = WinQueryWindowULong( hMenu, QWL_USER);
    if (!hMenu)
        pcr = 0;
    else
        pcr = (CAMRecPtr)WinQueryWindowULong( hMenu, QWL_USER);
    if (pcr)
        WinSetWindowULong( hMenu, QWL_USER, 0);

    SetGroup( hwnd, pcg, pcr);

    return;
}

/****************************************************************************/

void        SetGroup( HWND hwnd, CAMGroupPtr pcg, CAMRecPtr pcr)

{
    HWND            hCnr;
    STATSTUFF *     pss;

    hCnr = WinWindowFromID( hwnd, FID_CLIENT);
    pss = GetStatStuff();
    if (pcg == 0)
        pcg = pcgFirst;

    if (pcr) {
        if (!STAT_ISDONE(pcr->status)) {
            pcr->group = pcg->name;
            pcr->grpptr = pcg;
            UpdateMark( pcr, hCnr, pss, IDM_WRITE, (pcg == pcgFirst));
            UpdateStatus( hwnd, 0);
        }

        return;
    }


    pcr = (CAMRecPtr)CMA_FIRST;
    while ((pcr = (CAMRecPtr)WinSendMsg( hCnr, CM_QUERYRECORDEMPHASIS,
                                         (MP)pcr, (MP)CRA_SELECTED)) != 0) {
        if (pcr == (CAMRecPtr)-1)
            break;

        if (STAT_ISDONE(pcr->status))
            continue;

        pcr->group = pcg->name;
        pcr->grpptr = pcg;
        UpdateMark( pcr, hCnr, pss, IDM_WRITE, (pcg == pcgFirst));
    }

    UpdateStatus( hwnd, 0);
    return;
}

/****************************************************************************/

// cyTopMargin & cyHeight constrain the positions of the group buttons.
// cyTopMargin is the minimum distance from the origin of the EditGroup
// button to the top of the window.  cyHeight is the height of all the
// group buttons plus the height of a horizontal scrollbar.  When the
// window is short, cyTopMargin will force the lower buttons to be hidden.
// When the window is tall, cyHeight will keep the buttons at the bottom
// of the sidebar.

void        InitGroupButtonPos( HWND hSidebar)

{
    SWP         swp;

    WinQueryWindowPos( hSidebar, &swp);
    cyTopMargin = swp.cy;

    WinQueryWindowPos( WinWindowFromID( hSidebar, IDC_EDITGRP), &swp);
    cyTopMargin -= swp.y;
    cyHeight = swp.y;

    WinQueryWindowPos( WinWindowFromID( hSidebar, IDC_GRPLAST-1), &swp);
    cyHeight -= swp.y;
    cyHeight += WinQuerySysValue( HWND_DESKTOP, SV_CYHSCROLL);

    return;
}

/****************************************************************************/

LONG        RepositionGroupButtons( HWND hSidebar, LONG cySidebar)

{
    HWND        hTemp;
    LONG        yTop;
    LONG        lChg;
    LONG        lRtn;
    ULONG       ctr;
    ULONG       cntGrp;
    ULONG       cntBtn;
    PSWP        pswp;
    PSWP        ptr;
    CAMGroupPtr pcg;
    POINTL      ptl = {0,0};

do {

    hTemp = WinWindowFromID( hSidebar, IDC_EDITGRP);
    WinMapWindowPoints( hTemp, hSidebar, &ptl, 1);
    yTop = cySidebar - cyTopMargin;

    if (ptl.y > yTop || cyHeight >= yTop)
        lChg = yTop - ptl.y;
    else
        lChg = cyHeight - ptl.y;

    lRtn = yTop - cyHeight;

    if (!lChg)
        break;

    cntBtn = IDC_GRPLAST - IDC_GRPFIRST;
    pswp = (PSWP)calloc( cntBtn + 1, sizeof(SWP));
    if (!pswp)
        break;

    ptr = pswp;
    ptr->hwnd = hTemp;
    ptr->x = ptl.x;
    ptr->y = ptl.y + lChg;
    ptr->fl = SWP_MOVE | ((ptr->y < 3) ? SWP_HIDE : SWP_SHOW);

    for (pcg = pcgFirst, cntGrp = 0; pcg; pcg = pcg->pNext, cntGrp++);

    for (ctr = 0, ptr++; ctr < cntBtn; ctr++, ptr++) {
        hTemp = WinWindowFromID( hSidebar, IDC_GRPFIRST+ctr);
        if (!hTemp)
            break;
        WinQueryWindowPos( hTemp, ptr);
        ptr->y += lChg;
        if (ptr->y < 3 || ctr >= cntGrp)
            ptr->fl = SWP_MOVE | SWP_HIDE;
        else
            ptr->fl = SWP_MOVE | SWP_SHOW;
    }

    WinSetMultWindowPos( 0, pswp, cntBtn + 1);
    free( pswp);

} while (0);

    return (lRtn);
}

/****************************************************************************/
/****************************************************************************/

void        RestoreGroups( void)

{
    BOOL        fSearch;
    ULONG       cnt;
    CAMGroupPtr pcg;
    CAMGroupPtr pcgPrev;
    char *      ptr;
    char *      pKeys;

    cnt = 0;
    if (!PrfQueryProfileSize( HINI_USERPROFILE, CAMINI_GRPAPP, 0, &cnt) ||
        cnt == 0) {
        AddSampleGroups();
        return;
    }

    pKeys = malloc( cnt);
    if (!pKeys)
        return;

do {
    if (!PrfQueryProfileString( HINI_USERPROFILE, CAMINI_GRPAPP, 0,
                                0, pKeys, cnt))
        break;

    cnt = sizeof(CAMGroup);
    PrfQueryProfileData( HINI_USERPROFILE, CAMINI_GRPAPP, szIniDefault,
                         pcgFirst, &cnt);
    pcgPrev = pcgFirst;

    for (ptr = pKeys, fSearch = TRUE; *ptr; ptr = strchr( ptr, 0) + 1) {
        if (fSearch & !stricmp( ptr, szIniDefault)) {
            fSearch = FALSE;
            continue;
        }

        pcg = malloc( sizeof(CAMGroup));
        if (!pcg)
            break;

        cnt = sizeof(CAMGroup);
        if (!PrfQueryProfileData( HINI_USERPROFILE, CAMINI_GRPAPP, ptr,
                                  pcg, &cnt) ||
            cnt > sizeof(CAMGroup) ||
            cnt < sizeof(CAMGroup) - sizeof(pcg->path) + 1) {
            PrfWriteProfileData( HINI_USERPROFILE, CAMINI_GRPAPP, ptr,
                                 0, 0);
            free( pcg);
            continue;
        }

        pcgPrev->pNext = pcg;
        pcgPrev = pcg;
    }

    pcgPrev->pNext = 0;

} while (0);

    if (pKeys)
        free( pKeys);

    return;
}

/****************************************************************************/

void        StoreGroups( void)

{
    ULONG       cnt;
    CAMGroupPtr pcg;
    char        szKey[32];

    pcg = pcgFirst;
    cnt = strlen( pcg->file);
    memset( &(pcg->file)[cnt], 0, sizeof(pcg->file) - cnt);
    cnt = sizeof(CAMGroup) - sizeof(pcg->path) + strlen( pcg->path) + 1 ;

    if (!PrfWriteProfileData( HINI_USERPROFILE, CAMINI_GRPAPP, szIniDefault,
                              pcg, cnt))
        printf( "PrfWriteProfileData failed for key %s\n", szIniDefault);

    for (pcg = pcgFirst->pNext; pcg; pcg = pcg->pNext) {

        cnt = strlen( pcg->name);
        memset( &(pcg->name)[cnt], 0, sizeof(pcg->name) - cnt);
        cnt = strlen( pcg->file);
        memset( &(pcg->file)[cnt], 0, sizeof(pcg->file) - cnt);
        cnt = sizeof(CAMGroup) - sizeof(pcg->path) + strlen( pcg->path) + 1 ;

        strcpy( szKey, pcg->name);
        WinUpper( 0, 0, 0, szKey);
        if (!PrfWriteProfileData( HINI_USERPROFILE, CAMINI_GRPAPP, szKey,
                                  pcg, cnt))
            printf( "PrfWriteProfileData failed for key %s\n", szKey);
    }

    for (pcg = pcgDeleted; pcg; pcg = pcg->pNext) {
        strcpy( szKey, pcg->name);
        WinUpper( 0, 0, 0, szKey);
        PrfWriteProfileData( HINI_USERPROFILE, CAMINI_GRPAPP, szKey, 0, 0);
    }

    return;
}

/****************************************************************************/

void        AddSampleGroups( void)

{
    ULONG       cnt;
    ULONG       ctr;
    char *      ptr;
    CAMGroupPtr pcg;
    CAMGroupPtr pcgSample;
    CAMGroupPtr pcgPrev;

    cnt = sizeof(cgSample) / sizeof(CAMGroup);
    pcgPrev = pcgFirst;

    for (ctr = 0, pcgSample = cgSample; ctr < cnt; ctr++, pcgSample++) {
        pcg = malloc( sizeof(CAMGroup));
        if (!pcg)
            break;

        memcpy( pcg, pcgSample, sizeof(CAMGroup));
        LoadString( ulSampleNames[ctr], pcg->name);
        LoadString( ulSampleTempls[ctr], pcg->file);

        strcpy( pcg->path, pcgFirst->path);
        ptr = strchr( pcg->path, 0);
        if (ptr[-1] != '\\')
            *ptr++ = '\\';
        strcpy( ptr, pcg->name);
        if (CreatePath( pcg->path))
            strcpy( pcg->path, pcgFirst->path);

        pcgPrev->pNext = pcg;
        pcgPrev = pcg;
    }
    pcgPrev->pNext = 0;

    return;
}

/****************************************************************************/
/****************************************************************************/

void        InitGroupMenu( void)

{
    ULONG       ctr;
    ULONG       cnt;
    HWND        hMenu;
    CAMGroupPtr pcg;
    MENUITEM    mi;
    char *      ptr;
    char        szText[32];

    hMenu = QueryGroupMenu();
    if (!hMenu)
        return;

    cnt = (ULONG)WinSendMsg( hMenu, MM_QUERYITEMCOUNT, 0, 0);
    for (ctr = 1; ctr <= cnt; ctr++)
        WinSendMsg( hMenu, MM_DELETEITEM,
                    MPFROM2SHORT( IDM_GRPMNUFIRST + ctr, 0), 0);

    memset( &mi, 0, sizeof(mi));
    mi.afStyle = MIS_TEXT;

    for (pcg = pcgFirst->pNext; pcg && pcg->ndx <= IDM_GRPMNULAST;
         pcg = pcg->pNext) {
        mi.iPosition = pcg->ndx;
        mi.id = (USHORT)(IDM_GRPMNUFIRST + pcg->ndx);
        ptr = InsertGroupMnemonic( pcg->name, szText, pcg->mnemonic);
        WinSendMsg( hMenu, MM_INSERTITEM, (MP)&mi, (MP)ptr);
    }

    return;
}

/****************************************************************************/

void        UpdateGroupMenuName( CAMGroupPtr pcg)

{
    HWND        hMenu;
    char *      ptr;
    char        szText[32];

    hMenu = QueryGroupMenu();
    if (!hMenu)
        return;

    ptr = InsertGroupMnemonic( pcg->name, szText, pcg->mnemonic);
    WinSendMsg( hMenu, MM_SETITEMTEXT, (MP)(IDM_GRPMNUFIRST + pcg->ndx),
                (MP)ptr);

    return;
}

/****************************************************************************/
/****************************************************************************/

MRESULT _System GroupDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    switch (msg)
    {
        case WM_COMMAND:
            switch (SHORT1FROMMP( mp1)) {

                case IDC_UP:
                    ChangeGroupOrder( hwnd, TRUE);
                    return (0);

                case IDC_DOWN:
                    ChangeGroupOrder( hwnd, FALSE);
                    return (0);

                case IDC_GRPDEL:
                    DeleteGroup( hwnd);
                    return (0);

                case IDC_GRPUNDEL:
                    UndeleteGroup( hwnd);
                    return (0);

                case IDC_GRPNEW:
                    NewGroup( hwnd);
                    return (0);

                case IDC_GRPCREATE:
                    CreateGroupPath( hwnd);
                    return (0);

                case IDC_GRPUNDO:
                    UndoGroupItem( hwnd);
                    return (0);

                case IDC_GRPFIND:
                    FindGroupPath( hwnd);
                    return (0);

                case IDC_GRPAPPLY: {
                    CLITEM      cli;

                    WinSendDlgItemMsg( hwnd, IDC_GRPLIST, CLM_QUERYITEM,
                                       (MP)CLF_SELECTED, (MP)&cli);
                    ValidateGroupItem( hwnd, &cli);
                    return (0);
                }

                default:
                    return (0);
            }
            break;

        case WM_CONTROL:
            if (mp1 == MPFROM2SHORT( IDC_GRPLIST, CLN_DESELECTED))
                GroupItemDeselected( hwnd, (CLITEM*)mp2);
            else
            if (mp1 == MPFROM2SHORT( IDC_GRPLIST, CLN_SELECTED))
                GroupItemSelected( hwnd, (CLITEM*)mp2);
            else
            if (mp1 == MPFROM2SHORT( IDC_GRPLIST, CLN_CLICKED))
                GroupItemClicked( hwnd, (CLITEM*)mp2);
            return 0;

        case WM_CLOSE: {
            CLITEM      cli;

            WinSendDlgItemMsg( hwnd, IDC_GRPLIST, CLM_QUERYITEM,
                               (MP)CLF_SELECTED, (MP)&cli);
            if (ValidateGroupItem( hwnd, &cli))
                WinDestroyWindow( hwnd);
            return (0);
        }

        case WM_FOCUSCHANGE:
            if (SHORT1FROMMP(mp2) && !WinIsWindowEnabled( hwnd)) {
                WinPostMsg( WinQueryWindow( hwnd, QW_OWNER),
                            CAMMSG_FOCUSCHANGE, 0, 0);
                return (0);
            }
            break;

        case WM_INITDLG:
            InitGroupDlg( hwnd);
            SetGroupBtn( hwnd, FALSE);
            return (0);

        case WM_SAVEAPPLICATION:
            InitGroupMenu();
            WinStoreWindowPos( CAMINI_APP, CAMINI_GRPPOS, hwnd);
            break;

        case WM_DESTROY:
            SetGroupBtn( hwnd, TRUE);
            WindowClosed( IDD_GROUPS);
            break;

    } //end switch (msg)

    return (WinDefDlgProc( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

void        InitGroupDlg( HWND hwnd)

{
    BOOL        fErr;
    LONG        ndx;
    HACCEL      haccel;
    HWND        hCL;
    HWND        hTemp;
    CAMGroupPtr pcg;
    CLITEM      cli;

do {
    LoadDlgStrings( hwnd, nlsGroupDlg);

    WinSendDlgItemMsg( hwnd, IDC_GRPNAME, EM_SETTEXTLIMIT,
                       (MP)sizeof(pcgFirst->name)-1, 0);
    WinSendDlgItemMsg( hwnd, IDC_GRPPATH, EM_SETTEXTLIMIT,
                       (MP)sizeof(pcgFirst->path)-1, 0);
    WinSendDlgItemMsg( hwnd, IDC_GRPFILE, EM_SETTEXTLIMIT,
                       (MP)sizeof(pcgFirst->file)-1, 0);
    WinSendDlgItemMsg( hwnd, IDC_GRPDIG, EM_SETTEXTLIMIT, (MP)2, 0);

    hCL = WinWindowFromID( hwnd, IDC_GRPLIST);
    if (!CL_Init( hCL, 0, pszMSG_Btn, pszMSG_Group))
        break;

    for (pcg = pcgFirst; pcg; pcg = pcg->pNext) {
        cli.ndx = -1;
        cli.chk = (pcg == pcgFirst) ? (CLF_CHKDISABLED | CLF_CHECKED) :
                  ((pcg->btn) ? CLF_CHECKED : CLF_UNCHECKED);
        cli.hndl = (ULONG)pcg;
        cli.str = (pcg == pcgFirst) ? pszGRP_Default : pcg->name;
        WinSendMsg( hCL, CLM_INSERTITEM, (MP)&cli, (MP)1);
    }

    if (pcgDeleted) {
        cli.ndx = -1;
        cli.chk = CLF_CHKHIDDEN;
        cli.hndl = (ULONG)0;
        cli.str = pszMSG_Deleted;
        WinSendMsg( hCL, CLM_INSERTITEM, (MP)&cli, (MP)1);

        for (pcg = pcgDeleted; pcg; pcg = pcg->pNext) {
            cli.ndx = -1;
            cli.chk = FALSE;
            cli.hndl = (ULONG)pcg;
            cli.str = pcg->name;
            WinSendMsg( hCL, CLM_INSERTITEM, (MP)&cli, (MP)1);
        }
    }

    for (pcg = pcgFirst, ndx = 0, fErr = FALSE; pcg; pcg = pcg->pNext, ndx++)
        if (pcg->hWPS == (ULONG)-1) {
            fErr = TRUE;
            break;
        }

    if (fErr) {
        hTemp = WinQueryWindow( hwnd, QW_OWNER);
        hTemp = WinWindowFromID( hTemp, IDC_SIDEBAR);
        WinSetPresParam( WinWindowFromID( hTemp, IDC_EDITGRP),
                         PP_FOREGROUNDCOLOR, sizeof(RGB), &rgbBlack);
        if (ndx != 0) {
            ulErr = 1;
            WinSendMsg( hCL, CLM_SELECTITEM, (MP)ndx, 0);
        }
        WinPostMsg( hwnd, WM_COMMAND, (MP)IDC_GRPAPPLY, 0);
    }

    haccel = WinLoadAccelTable( 0, 0, IDA_UPDOWN);
    if (!haccel || !WinSetAccelTable( 0, haccel, hwnd))
        printf( "GrpDlg - unable to load accelerator table - haccel= %x\n",
                (int)haccel);

    ShowDialog( hwnd, CAMINI_GRPPOS);

} while (0);

    return;
}

/****************************************************************************/

void        SetGroupBtn( HWND hwnd, BOOL fEnable)

{
    HWND    hOwner;
    HWND    hTemp;

    hOwner = WinQueryWindow( hwnd, QW_OWNER);

    hTemp = WinWindowFromID( hOwner, IDC_SIDEBAR);
    hTemp = WinWindowFromID( hTemp, IDC_EDITGRP);
    WinEnableWindow( hTemp, fEnable);

    WinSendDlgItemMsg( hOwner, FID_MENU, MM_SETITEMATTR,
                       MPFROM2SHORT( IDM_EDITGRP, TRUE),
                       MPFROM2SHORT( MIA_DISABLED, (fEnable ? 0 : MIA_DISABLED)));

    return;
}

/****************************************************************************/
/****************************************************************************/

void        GroupItemSelected( HWND hwnd, CLITEM * pcli)

{
    CAMGroupPtr pcg;

    if (ulErr)
        ulErr--;
    else {
        pcg = (CAMGroupPtr)pcli->hndl;
        if (pcg)
            memcpy( &cgUndo, pcg, sizeof(CAMGroup));

        ShowGroupItem( hwnd, pcg);
    }

    return;
}

/****************************************************************************/

void        GroupItemDeselected( HWND hwnd, CLITEM * pcli)

{
    if (ulErr)
        ulErr--;
    else
        if (!ValidateGroupItem( hwnd, pcli)) {
            ulErr = 3;
            WinPostMsg( WinWindowFromID( hwnd, IDC_GRPLIST), CLM_SELECTITEM,
                        (MP)pcli->ndx, 0);
        }

    return;
}

/****************************************************************************/

void        UndoGroupItem( HWND hwnd)

{
    CAMGroupPtr pcg;
    CLITEM      cli;

    WinSendDlgItemMsg( hwnd, IDC_GRPLIST, CLM_QUERYITEM,
                       (MP)CLF_SELECTED, (MP)&cli);

    pcg = (CAMGroupPtr)cli.hndl;

    if (pcg && pcg->ndx >= 0) {
        cgUndo.ndx = pcg->ndx;
        cgUndo.btn = pcg->btn;
        cgUndo.hWPS = pcg->hWPS;
        ShowGroupItem( hwnd, &cgUndo);
        ValidateGroupItem( hwnd, &cli);
    }

    return;
}

/****************************************************************************/

void        GroupItemClicked( HWND hwnd, CLITEM * pcli)

{
    CAMGroupPtr pcg;

    pcg = (CAMGroupPtr)pcli->hndl;

    if (!pcg || pcg->ndx == -1)
        pcli->chk = CLF_CHKHIDDEN;
    else
    if (pcli->chk & CLF_CHECKED) {
        if (GroupButtonsAvailable()) {
            pcg->btn = (ULONG)-1;
            AdjustGroupButtons( hwnd);
        }
        if (pcg->btn == 0) {
            pcli->chk &= ~CLF_CHECKED;
            SetValidationText( hwnd, 0, MSG_ButtonsAssigned, TRUE);
            WinAlarm( HWND_DESKTOP, WA_ERROR);
        }
    }
    else {
        pcg->btn = 0;
        AdjustGroupButtons( hwnd);
    }

    return;
}

/****************************************************************************/

void        ChangeGroupOrder( HWND hwnd, BOOL fUp)

{
    HWND        hCL;
    LONG        ndx;
    CAMGroupPtr pcg;
    CAMGroupPtr pcgPrev;
    CAMGroupPtr pcgTemp;
    CLITEM      cli;

do {
    hCL = WinWindowFromID( hwnd, IDC_GRPLIST);

    WinSendMsg( hCL, CLM_QUERYITEM, (MP)CLF_SELECTED, (MP)&cli);
    ndx = cli.ndx;
    pcg = (CAMGroupPtr)cli.hndl;
    if (!pcg)
        break;

    if ((ndx == 0) ||
        (fUp && ndx == 1) ||
        (!fUp && pcg->pNext == 0)) {
        WinAlarm( HWND_DESKTOP, WA_ERROR);
        break;
    }
    ndx += (fUp ? -1 : 1);

    for (pcgPrev = pcgFirst; pcgPrev; pcgPrev = pcgPrev->pNext)
        if (pcgPrev->pNext == pcg)
            break;

    if (!pcgPrev)
        break;

    if (!fUp) {
        pcgTemp = pcg->pNext;
        pcgPrev->pNext = pcgTemp;
        pcg->pNext = pcgTemp->pNext;
        pcgTemp->pNext = pcg;
    }
    else {
        pcgTemp = pcgPrev;
        for (pcgPrev = pcgFirst; pcgPrev; pcgPrev = pcgPrev->pNext)
            if (pcgPrev->pNext == pcgTemp)
                break;
        if (!pcgPrev)
            break;

        pcgPrev->pNext = pcg;
        pcgTemp->pNext = pcg->pNext;
        pcg->pNext = pcgTemp;
    }

    if (!WinSendMsg( hCL, CLM_MOVEITEM, (MP)cli.ndx, (MP)ndx))
        break;

    AdjustGroupIndex();
    AdjustGroupButtons( hwnd);
    SetGroupDlgCtrls( hwnd, pcg, 0);
    InitGroupMenu();

} while (0);

    return;
}

/****************************************************************************/

void        NewGroup( HWND hwnd)

{
    HWND        hCL;
    LONG        ctr;
    CAMGroupPtr pcg;
    CAMGroupPtr pcgPrev;
    CAMGroupPtr pcgNew;
    CLITEM      cli;

do {
    hCL = WinWindowFromID( hwnd, IDC_GRPLIST);
    WinSendMsg( hCL, CLM_QUERYITEM, (MP)CLF_SELECTED, (MP)&cli);
    pcgPrev = (CAMGroupPtr)cli.hndl;
    if (!pcgPrev || pcgPrev->ndx == -1)
        break;

    if (!ValidateGroupItem( hwnd, &cli))
        break;

    pcgNew = malloc( sizeof(CAMGroup));
    if (!pcgNew)
        break;
    memset( pcgNew, 0, sizeof(CAMGroup));

    pcgNew->mnemonic = -1;
    pcgNew->seqNbr = 1;
    pcgNew->cntDigits = 4;
    strcpy( pcgNew->path, pcgFirst->path);

    ctr = 1;
    do {
        sprintf( pcgNew->name, pszMSG_NewGroup, (int)ctr++);
        for (pcg = pcgFirst->pNext; pcg; pcg = pcg->pNext)
            if (!stricmp( pcgNew->name, pcg->name))
                break;

    } while (pcg);

    cli.ndx++;
    cli.chk = (GroupButtonsAvailable()) ? CLF_CHECKED : CLF_UNCHECKED;
    cli.hndl = (ULONG)pcgNew;
    cli.str = pcgNew->name;

    if (!WinSendMsg( hCL, CLM_INSERTITEM, (MP)&cli, (MP)1)) {
        free( pcgNew);
        break;
    }

    pcgNew->pNext = pcgPrev->pNext;
    pcgPrev->pNext = pcgNew;

    AdjustGroupIndex();

    if (cli.chk == CLF_CHECKED) {
        pcgNew->btn = (ULONG)-1;
        AdjustGroupButtons( hwnd);
    }

    WinSendMsg( hCL, CLM_SELECTITEM, (MP)cli.ndx, 0);
    InitGroupMenu();

} while (0);

    return;
}

/****************************************************************************/

void        DeleteGroup( HWND hwnd)

{
    HWND        hCL;
    CAMGroupPtr pcg;
    CAMGroupPtr pcgPrev;
    CAMGroupPtr pcgDel;
    CLITEM      cli;

do {
    hCL = WinWindowFromID( hwnd, IDC_GRPLIST);
    WinSendMsg( hCL, CLM_QUERYITEM, (MP)CLF_SELECTED, (MP)&cli);
    pcgDel = (CAMGroupPtr)cli.hndl;
    if (!pcgDel || pcgDel->ndx == -1)
        break;

    for (pcgPrev = pcgFirst; pcgPrev; pcgPrev = pcgPrev->pNext)
        if (pcgPrev->pNext == pcgDel)
            break;

    if (!pcgPrev)
        break;

    if (!pcgDeleted) {
        CLITEM      cliSep;

        WinSendMsg( hCL, CLM_QUERYITEM, (MP)CLF_LAST, (MP)&cliSep);
        if (cliSep.hndl) {
            cliSep.ndx = CLF_LAST;
            cliSep.chk = CLF_CHKHIDDEN;
            cliSep.hndl = 0;
            cliSep.str = pszMSG_Deleted;
            WinSendMsg( hCL, CLM_INSERTITEM, (MP)&cliSep, (MP)1);
        }
    }

    if (!WinSendMsg( hCL, CLM_MOVEITEM, (MP)cli.ndx, (MP)CLF_LAST))
        break;

    pcgPrev->pNext = pcgDel->pNext;

    if (pcgDeleted) {
        for (pcg = pcgDeleted; pcg->pNext; pcg = pcg->pNext);
        pcg->pNext = pcgDel;
    }
    else
        pcgDeleted = pcgDel;

    pcgDel->pNext = 0;
    pcgDel->ndx = -1;
    pcgDel->btn = 0;

    WinSendMsg( hCL, CLM_SETCHECK, (MP)CLF_LAST, (MP)CLF_CHKHIDDEN);
    ShowGroupItem( hwnd, pcgDel);

    AdjustGroupIndex();
    AdjustGroupButtons( hwnd);
    UnmarkDeletedGroup( hwnd, pcgDel);
    InitGroupMenu();

} while (0);

    SetValidationText( hwnd, IDC_GRPNAMETXT, 0, 0);
    SetValidationText( hwnd, IDC_GRPPATHTXT, 0, 0);

    return;
}

/****************************************************************************/

void        UndeleteGroup( HWND hwnd)

{
    HWND        hCL;
    LONG        newNdx;
    CAMGroupPtr pcg;
    CAMGroupPtr pcgDel;
    CLITEM      cli;

do {
    hCL = WinWindowFromID( hwnd, IDC_GRPLIST);
    WinSendMsg( hCL, CLM_QUERYITEM, (MP)CLF_SELECTED, (MP)&cli);
    pcgDel = (CAMGroupPtr)cli.hndl;
    if (!pcgDel || pcgDel->ndx != -1)
        break;

    for (pcg = pcgFirst; pcg->pNext; pcg = pcg->pNext);
    newNdx = pcg->ndx + 1;

    if (!WinSendMsg( hCL, CLM_MOVEITEM, (MP)cli.ndx, (MP)newNdx))
        break;

    pcg->pNext = pcgDel;
    pcgDel->ndx = newNdx;

    if (pcgDel == pcgDeleted)
        pcgDeleted = pcgDel->pNext;
    else {
        for (pcg = pcgDeleted; pcg; pcg = pcg->pNext)
            if (pcg->pNext == pcgDel)
                break;
        if (pcg)
            pcg->pNext = pcgDel->pNext;
    }

    pcgDel->pNext = 0;

    cli.ndx = pcgDel->ndx;
    cli.chk = CLF_UNCHECKED;
    WinSendMsg( hCL, CLM_SETCHECK, (MP)cli.ndx, (MP)cli.chk);
    ValidateGroupItem( hwnd, &cli);
    InitGroupMenu();

} while (0);

    return;
}

/****************************************************************************/

void        FindGroupPath( HWND hwnd)

{
    CAMGroupPtr pcg;
    CLITEM      cli;
    char        szPath[CCHMAXPATH];

do {
    WinSendDlgItemMsg( hwnd, IDC_GRPLIST, CLM_QUERYITEM,
                       (MP)CLF_SELECTED, (MP)&cli);

    pcg = (CAMGroupPtr)cli.hndl;
    if (!pcg || pcg->ndx == -1)
        break;

    strcpy( szPath, pcg->path);
    if (!PathDlg( hwnd, szPath))
        break;

    WinSetDlgItemText( hwnd, IDC_GRPPATH, szPath);
    ValidateGroupItem( hwnd, &cli);

} while (0);

    return;
}

/****************************************************************************/

void        CreateGroupPath( HWND hwnd)

{
    APIRET  rc = 0;
    ULONG   ulErr = 0;
    char *  ptr;
    char    szPath[CCHMAXPATH];

do {
    if (!WinQueryDlgItemText( hwnd, IDC_GRPPATH, sizeof(szPath), szPath)) {
        rc = 1;
        ulErr = MSG_DirIsBlank;
        break;
    }

    // look for date metacharacters & escaped %
    ptr = szPath;
    while ((ptr = strchr( ptr, '%')) != 0) {
        ptr++;
        if (*ptr == '%')
            strcpy( ptr, ptr+1);
        else
        if (strchr( szYMD, *ptr)) {
            rc = 1;
            ulErr = MSG_DirNotCreated;
            break;
        }
    }

    if (rc)
        break;

    // if no error, then path already exists
    rc = ValidatePath( szPath);
    if (!rc)
        break;

    rc = CreatePath( szPath);

    if (!rc)
        ulErr = MSG_DirCreated;
    else
    if (rc == ERROR_FILE_EXISTS)
        ulErr = MSG_PathIsAFile;
    else
        ulErr = MSG_DirNotCreated;

} while (0);

    SetValidationText( hwnd, IDC_GRPPATHTXT, ulErr, (BOOL)rc);

    return;
}

/****************************************************************************/
/****************************************************************************/

void        ShowGroupItem( HWND hwnd, CAMGroupPtr pcg)

{
    char *      ptr;
    char        szText[32];


    if (!pcg) {
        WinSetDlgItemText( hwnd, IDC_GRPNAME, "");
        WinSetDlgItemText( hwnd, IDC_GRPPATH, "");
        WinSetDlgItemText( hwnd, IDC_GRPFILE, "");
        WinSetDlgItemText( hwnd, IDC_GRPSEQ, "");
        WinSetDlgItemText( hwnd, IDC_GRPDIG, "");
        SetGroupDlgCtrls( hwnd, pcg, 0);
        return;
    }

    if (pcg == pcgFirst)
        WinSetDlgItemText( hwnd, IDC_GRPNAME, pszGRP_Default);
    else {
        ptr = InsertGroupMnemonic( pcg->name, szText, pcg->mnemonic);
        WinSetDlgItemText( hwnd, IDC_GRPNAME, ptr);
    }

    WinSetDlgItemText( hwnd, IDC_GRPPATH, pcg->path);
    WinSetDlgItemText( hwnd, IDC_GRPFILE, pcg->file);
    sprintf( szText, "%0*d", (int)pcg->cntDigits, (int)pcg->seqNbr);
    WinSetDlgItemText( hwnd, IDC_GRPSEQ, szText);
    WinSetDlgItemShort( hwnd, IDC_GRPDIG, (USHORT)pcg->cntDigits, FALSE);

    SetGroupDlgCtrls( hwnd, pcg, 0);

    if (pcg->hWPS == (ULONG)-1)
        WinPostMsg( hwnd, WM_COMMAND, (MP)IDC_GRPAPPLY, 0);

    return;
}

/****************************************************************************/

BOOL        ValidateGroupItem( HWND hwnd, CLITEM * pcli)

{
    BOOL        fRtn = TRUE;
    ULONG       cnt;
    CAMGroupPtr pcg;
    char        szText[32];

do {
    pcg = (CAMGroupPtr)pcli->hndl;
    if (!pcg || pcg->ndx == -1)
        break;

    if (!ValidateGroupName( hwnd, pcli) ||
        !ValidateGroupPath( hwnd, pcg)) {
        fRtn = FALSE;
        break;
    }

    ValidateGroupFile( hwnd, pcg);

    WinQueryDlgItemText( hwnd, IDC_GRPSEQ, sizeof(szText), szText);
    pcg->seqNbr = strtoul( szText, 0, 10);

    WinQueryDlgItemText( hwnd, IDC_GRPDIG, sizeof(szText), szText);
    cnt = strtoul( szText, 0, 10);
    if (cnt == 0 || cnt > 10)
        cnt = 4;
    pcg->cntDigits = cnt;

} while (0);

    if (!fRtn)
        WinAlarm( HWND_DESKTOP, WA_ERROR);
    SetGroupDlgCtrls( hwnd, pcg, !fRtn);

    return (fRtn);
}

/****************************************************************************/

void        ValidateGroupFile( HWND hwnd, CAMGroupPtr pcg)

{
    ULONG       cnt;
    char *      pDot;
    char *      pEnd;
    char        szText[32];

do {
    cnt = WinQueryDlgItemText( hwnd, IDC_GRPFILE, sizeof(szText), szText);
    if (!cnt) {
        *pcg->file = 0;
        break;
    }

    pDot = strrchr( szText, '.');
    if (pDot)
        *pDot = 0;

    if (strchr( szText, '#') || strchr( szText, '*')) {
        if (pDot)
            *pDot = '.';
        strcpy( pcg->file, szText);
        break;
    }

    strcpy( pcg->file, szText);
    pEnd = strchr( pcg->file, 0);
    if (cnt >= sizeof(pcg->file) - 1)
        pEnd--;

    *pEnd++ = '#';
    if (pDot) {
        *pDot = '.';
        strcpy( pEnd, pDot);
    }
    else
        *pEnd = 0;

    WinSetDlgItemText( hwnd, IDC_GRPFILE, pcg->file);

} while (0);

    return;
}

/****************************************************************************/

BOOL        ValidateGroupName( HWND hwnd, CLITEM * pcli)

{
    BOOL        fRtn = FALSE;
    HWND        hTemp;
    LONG        mnemonic;
    CAMGroupPtr pcg;
    CAMGroupPtr ptr;
    ULONG       ulErr = 0;
    char *      pText;
    char        szText[32];

do {
    pcg = (CAMGroupPtr)pcli->hndl;

    // if the selection is the default entry,
    // we aren't going to change its name, so exit OK
    if (pcg == pcgFirst) {
        fRtn = TRUE;
        break;
    }

    // if the name field is blank, exit with error
    if (!WinQueryDlgItemText( hwnd, IDC_GRPNAME, sizeof(szText), szText)) {
        ulErr = MSG_GroupNameBlank;
        break;
    }

    mnemonic = RemoveGroupMnemonic( szText);

    // if the name hasn't changed at all, exit OK
    if (!strcmp( szText, pcg->name)) {
        fRtn = TRUE;
        if (pcg->mnemonic == mnemonic)
            break;

        pcg->mnemonic = mnemonic;
        if (pcg->btn > IDC_GRPFIRST && pcg->btn < IDC_GRPLAST) {
            pText = InsertGroupMnemonic( pcg->name, szText, pcg->mnemonic);
            hTemp = WinQueryWindow( hwnd, QW_OWNER);
            WinSetDlgItemText( WinWindowFromID( hTemp, IDC_SIDEBAR),
                               pcg->btn, pText);
        }
        UpdateGroupMenuName( pcg);
        break;
    }

    // if this is more than a case change, validate further
    if (stricmp( szText, pcg->name)) {

        // see if the name matches another one
        for (ptr = pcgFirst->pNext; ptr; ptr = ptr->pNext) {
            // don't bother comparing to self
            if (ptr == pcg)
                continue;
            if (!stricmp( szText, ptr->name))
                break;
        }

        // name matches - exit with error
        if (ptr || !stricmp( szText, pszGRP_Default)) {
            ulErr = MSG_GroupNameInUse;
            break;
        }

        // delete the ini entry under the old name
        WinUpper( 0, 0, 0, pcg->name);
        PrfWriteProfileData( HINI_USERPROFILE, CAMINI_GRPAPP, pcg->name, 0, 0);
    }

    // name is OK - update the CAMGroup entry, the listbox, & its button
    strcpy( pcg->name, szText);
    pcg->mnemonic = mnemonic;

    pcli->str = pcg->name;
    WinSendDlgItemMsg( hwnd, IDC_GRPLIST, CLM_SETITEM,
                       (MP)pcli->ndx, (MP)pcli);

    hTemp = WinQueryWindow( hwnd, QW_OWNER);
    WinSendDlgItemMsg( hTemp, FID_CLIENT, CM_INVALIDATERECORD, 0,
                       MPFROM2SHORT( 0, CMA_NOREPOSITION));

    if (pcg->btn > IDC_GRPFIRST && pcg->btn < IDC_GRPLAST) {
        pText = InsertGroupMnemonic( pcg->name, szText, pcg->mnemonic);
        WinSetDlgItemText( WinWindowFromID( hTemp, IDC_SIDEBAR),
                           pcg->btn, pText);
    }

    UpdateGroupMenuName( pcg);

    fRtn = TRUE;

} while (0);

    SetValidationText( hwnd, IDC_GRPNAMETXT, ulErr, !fRtn);
        
    return (fRtn);
}

/****************************************************************************/

BOOL        ValidateGroupPath( HWND hwnd, CAMGroupPtr pcg)

{
    BOOL        fRtn = FALSE;
    BOOL        fSub = FALSE;
    APIRET      rc;
    ULONG       ulErr = 0;
    char *      ptr;
    char        szText[CCHMAXPATH];
    char        szTemp[CCHMAXPATH];

do {
    if (!WinQueryDlgItemText( hwnd, IDC_GRPPATH, sizeof(szText), szText)) {
        *pcg->path = 0;
        pcg->hWPS = 0;
        fRtn = TRUE;
        break;
    }

    rc = ValidatePath( szText);
    if (rc) {
        pcg->hWPS = (ULONG)-1;

        if (rc != ERROR_FILE_NOT_FOUND && rc != ERROR_PATH_NOT_FOUND) {
            if (rc == ERROR_FILE_EXISTS)
                ulErr = MSG_PathIsAFile;
            else
                ulErr = MSG_PathInvalid;
            break;
        }

        // see if the path contains date substitutions or escaped %
        strcpy( szTemp, szText);
        ptr = szTemp;
        while ((ptr = strchr( ptr, '%')) != 0) {
            ptr++;
            if (*ptr == '%') {
                strcpy( ptr, ptr+1);
                rc = (APIRET)-1;
            }
            else
            if (strchr( szYMD, *ptr)) {
                rc = 0;
                fSub = TRUE;
                break;
            }
        }

        if (rc == (APIRET)-1)
            rc = ValidatePath( szTemp);

        // if not, error
        if (rc) {
            ulErr = MSG_PathDoesNotExist;
            break;
        }
    }

    WinSetDlgItemText( hwnd, IDC_GRPPATH, szText);
    strcpy( pcg->path, szText);

    // don't get the handle for a path that doesn't exist
    if (!fSub)
        pcg->hWPS = WinQueryObject( pcg->path);
    fRtn = TRUE;

} while (0);

    SetValidationText( hwnd, IDC_GRPPATHTXT, ulErr, !fRtn);

    return (fRtn);
}

/****************************************************************************/
/****************************************************************************/

void        SetValidationText( HWND hwnd, ULONG ulID, ULONG ulStr, BOOL fErr)

{
    HWND    hTemp;
    char    szText[256];

    if (ulID) {
        hTemp = WinWindowFromID( hwnd, ulID);
        WinSetPresParam( hTemp, PP_FOREGROUNDCOLOR, sizeof(RGB),
                         (fErr ? &rgbRed : &rgbBlack));
    }

    if (ulStr)
        LoadString( ulStr, szText);
    else
        szText[0] = 0;
    WinSetDlgItemText( hwnd, IDC_GRPERRTXT, szText);

    return;
}

/****************************************************************************/

void        SetGroupDlgCtrls( HWND hwnd, CAMGroupPtr pcg, BOOL fErr)

{
    HWND    hTemp;
    BOOL    fEdit = TRUE;
    BOOL    fName = TRUE;
    ULONG   ulBtns = 0;

    if (!pcg) {
        ulBtns = GRP_ALL;
        fEdit = FALSE;
        fName = FALSE;
    }
    else
    if (pcg->ndx == (ULONG)-1) {
        ulBtns |= GRP_ALL - GRP_DEL + GRP_UNDEL;
        fEdit = FALSE;
        fName = FALSE;
    }
    else
    if (pcg == pcgFirst) {
        ulBtns |= GRP_MOVE | GRP_DEL;
        fName = FALSE;
    }
    else {
        if (pcg->ndx == 1)
            ulBtns |= GRP_UP;
        if (pcg->pNext == 0)
            ulBtns |= GRP_DOWN;
    }
    if (fErr)
        ulBtns |= GRP_MOVE | GRP_NEW;

    hTemp = WinWindowFromID( hwnd, IDC_GRPDEL);
    if (!hTemp)
        hTemp = WinWindowFromID( hwnd, IDC_GRPUNDEL);

    WinEnableWindow( hTemp, ((ulBtns & GRP_DEL) ? FALSE : TRUE));
    WinSetWindowText( hTemp, ((ulBtns & GRP_UNDEL) ?
                              pszBTN_Undelete : pszBTN_Delete));
    WinSetWindowUShort( hTemp, QWS_ID,
                        ((ulBtns & GRP_UNDEL) ? IDC_GRPUNDEL : IDC_GRPDEL));

    WinEnableWindow( WinWindowFromID( hwnd, IDC_UP),
                     ((ulBtns & GRP_UP) ? FALSE : TRUE));
    WinEnableWindow( WinWindowFromID( hwnd, IDC_DOWN),
                     ((ulBtns & GRP_DOWN) ? FALSE : TRUE));
    WinEnableWindow( WinWindowFromID( hwnd, IDC_GRPNEW),
                     ((ulBtns & GRP_NEW) ? FALSE : TRUE));
    WinEnableWindow( WinWindowFromID( hwnd, IDC_GRPFIND),
                     ((ulBtns & GRP_FIND) ? FALSE : TRUE));
    WinEnableWindow( WinWindowFromID( hwnd, IDC_GRPCREATE),
                     ((ulBtns & GRP_CREATE) ? FALSE : TRUE));

    WinEnableWindow( WinWindowFromID( hwnd, IDC_GRPUNDO),
                     ((ulBtns & GRP_UNDO) ? FALSE : TRUE));
    WinEnableWindow( WinWindowFromID( hwnd, IDC_GRPAPPLY),
                     ((ulBtns & GRP_APPLY) ? FALSE : TRUE));

    WinEnableWindow( WinWindowFromID( hwnd, IDC_GRPNAME), fName);
    WinEnableWindow( WinWindowFromID( hwnd, IDC_GRPPATH), fEdit);
    WinEnableWindow( WinWindowFromID( hwnd, IDC_GRPFILE), fEdit);
    WinEnableWindow( WinWindowFromID( hwnd, IDC_GRPSEQ), fEdit);
    WinEnableWindow( WinWindowFromID( hwnd, IDC_GRPDIG), fEdit);

    return;
}

/****************************************************************************/

LONG        AdjustGroupIndex( void)

{
    LONG        ndx = 0;
    CAMGroupPtr pcg;

    for (pcg = pcgFirst; pcg; pcg = pcg->pNext)
        pcg->ndx = ndx++;

    return (ndx);
}

/****************************************************************************/

BOOL        GroupButtonsAvailable( void)

{
    ULONG       btn;
    CAMGroupPtr pcg;

    for (pcg = pcgFirst->pNext, btn = IDC_GRPFIRST+1; pcg; pcg = pcg->pNext)
        if (pcg->btn)
            btn++;

    return (btn < IDC_GRPLAST);
}

/****************************************************************************/

void        AdjustGroupButtons( HWND hwnd)

{
    HWND        hTemp;
    HWND        hOwner;
    HWND        hSidebar;
    ULONG       btn;
    CAMGroupPtr pcg;
    char *      ptr;
    char        szText[32];

    hOwner = WinQueryWindow( hwnd, QW_OWNER);
    hSidebar = WinWindowFromID( hOwner, IDC_SIDEBAR);

    for (pcg = pcgFirst->pNext, btn = IDC_GRPFIRST+1; pcg; pcg = pcg->pNext) {
        if (!pcg->btn)
            continue;

        if (btn >= IDC_GRPLAST) {
            pcg->btn = 0;
            continue;
        }

        if (pcg->btn != btn) {
            pcg->btn = btn;
            hTemp = WinWindowFromID( hSidebar, pcg->btn);
            ptr = InsertGroupMnemonic( pcg->name, szText, pcg->mnemonic);
            WinSetWindowText( hTemp, ptr);
            WinShowWindow( hTemp, TRUE);
        }
        btn++;
    }

    while (btn < IDC_GRPLAST) {
        hTemp = WinWindowFromID( hSidebar, btn);
        WinShowWindow( hTemp, FALSE);
        btn++;
    }

    return;
}

/****************************************************************************/

void        UnmarkDeletedGroup( HWND hwnd, CAMGroupPtr pcgDel)

{
    HWND        hOwner;
    HWND        hCnr;
    STATSTUFF * pss;
    CAMRecPtr   pcr;

    hOwner = WinQueryWindow( hwnd, QW_OWNER);
    hCnr = WinWindowFromID( hOwner, FID_CLIENT);
    pss = GetStatStuff();

    pcr = (CAMRecPtr)WinSendMsg( hCnr, CM_QUERYRECORD, (MP)0,
                                 MPFROM2SHORT( CMA_FIRST, CMA_ITEMORDER));
    while (pcr) {
        if (pcr == (CAMRecPtr)-1)
            break;

        if (pcr->grpptr == pcgDel) {
            pcr->grpptr = pcgFirst;
            pcr->group = pcgFirst->name;
            UpdateMark( pcr, hCnr, pss, IDM_UNMARK, FALSE);
        }

        pcr = (CAMRecPtr)WinSendMsg( hCnr, CM_QUERYRECORD, (MP)pcr,
                                     MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER));
    }
    UpdateStatus( hOwner, 0);

    return;
}

/****************************************************************************/
/****************************************************************************/

char *      InsertGroupMnemonic( char * szName, char * szOut, LONG ndx)

{
    if (ndx == -1)
        return (szName);

    memcpy( szOut, szName, ndx);
    szOut[ndx] = '~';
    strcpy( &szOut[ndx+1], &szName[ndx]);
    return (szOut);
}

/****************************************************************************/

LONG        RemoveGroupMnemonic( char * szName)

{
    LONG    lRtn;
    char *  ptr;

    ptr = strchr( szName, '~');
    if (!ptr)
        return (-1);

    lRtn = ptr - szName;
    if (lRtn >= sizeof(cgDefault.name) - 2)
        lRtn = -1;

    strcpy( ptr, ptr + 1);
    return (lRtn);
}

/****************************************************************************/

BOOL        MatchGroupMnemonic( HWND hwnd, USHORT usChar)

{
    BOOL        fRtn = FALSE;
    CAMGroupPtr pcg;
    USHORT      usMatch;

    usMatch = (USHORT)WinUpperChar( 0, 0, 0, (ULONG)usChar);

    for (pcg = pcgFirst->pNext; pcg; pcg = pcg->pNext) {
        if (pcg->mnemonic < 0 || pcg->mnemonic >= sizeof(cgDefault.name) - 2)
            continue;

        if (usMatch == (USHORT)(pcg->name[pcg->mnemonic]))
            break;

        if (usMatch == (USHORT)WinUpperChar( 0, 0, 0,
                                             (ULONG)pcg->name[pcg->mnemonic]))
            break;
    }

    if (pcg)
        fRtn = WinPostMsg( hwnd, WM_COMMAND, (MP)(IDM_GRPMNUFIRST + pcg->ndx),
                           MPFROM2SHORT( CMDSRC_MENU, FALSE));

    return (fRtn);
}

/****************************************************************************/

