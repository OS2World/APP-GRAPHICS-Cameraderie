/****************************************************************************/
// cammain.c
//
//  - main(), the main window's dialog proc, & top-level init stuff
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

typedef struct _CAMWnd {
    ULONG       ulID;
    HWND        hwnd;
    USHORT      usDisabled;
    USHORT      usModality;
    PFNWP       pfnwp;
} CAMWnd;

/****************************************************************************/

int             main( int argc, char * argv[]);
void            Morph( void);
void            InitGlobalOptions( int argc, char * argv[]);
BOOL            SetGlobalFlag( ULONG ulFlag, ULONG ulValue);
void            NewCamera( HWND hwnd, MPARAM mp1);
MRESULT _System MainWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
void            InitDlg( HWND hwnd);
MRESULT         FormatFrame( HWND hwnd, MPARAM mp1, MPARAM mp2);
BOOL            NavigateMainWnd( HWND hwnd, USHORT usVK);
void        	SetTitle( HWND hwnd, char * pszText);
void            InitFileMenu( HWND hMenu);
void            InitViewMenu( HWND hMenu);
void            UpdateStatus( HWND hwnd, ULONG ulMsg);
void            InitStatStuff( HWND hwnd);
STATSTUFF *     ClearStatStuff( void);
STATSTUFF *     GetStatStuff( void);
void *          GetCamPtr( void);
HWND            OpenWindow( HWND hwnd, ULONG ulID, ULONG ulParm);
void            WindowClosed( ULONG ulID);
HWND            GetHwnd( ULONG ulID);
HWND            GetModalHwnd( void);
MRESULT _System WaitOrExitDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
void            ShowDialog( HWND hwnd, char * pIniKey);

/****************************************************************************/

char    szCopyright[] = "Cameraderie - (C)Copyright 2006,2007  R.L.Walsh";
char    szAppTitle[]  = "Cameraderie v1.5";

// declared EXTERN in camapp.h - contains commandline flags
ULONG   ulGlobalFlags = 0;

// static variables
static STATSTUFF    ss;
static void *       thisCam = 0;

static CAMWnd       camWnd[] = {
    {IDD_MAIN,          0, 0, 0, MainWndProc},
    {IDD_INFO,          0, 0, 0, InfoDlgProc},
    {IDD_COLUMNS,       0, 0, 0, ColumnsDlgProc},
    {IDD_GROUPS,        0, 0, 0, GroupDlgProc},
    {IDD_COLORS,        0, 0, 0, ColorsDlgProc},
    {IDD_DISCONNECT,    0, 0, 5, WaitOrExitDlgProc},
    {IDD_LOADING,       0, 0, 4, WaitOrExitDlgProc},
    {IDD_SAVE,          0, 0, 1, ConfirmSaveDlgProc},
    {IDD_SAVEEXIT,      0, 0, 1, ConfirmSaveDlgProc},
    {IDD_SAVEFILE,      0, 0, 1, SaveFilesDlgProc},
    {IDD_SELECTCAMERA,  0, 0, 2, CamSelectCameraDlg},
    {IDD_LVMHANG,       0, 0, 3, DrvMountDrivesDlg},
    {IDD_FATAL,         0, 0, 6, CamFatalDlg},
    {IDD_EJECTALL,      0, 0, 2, DrvEjectAllDlg},
    {0,                 0, 0, 0, 0}};

static CAMNLS   nlsLoadingDlg[] = {
    { IDC_TEXT1,        DLG_Loading },
    { IDC_TEXT4,    	DLG_ToEndNow },
    { IDC_EXIT,     	BTN_Exit },
    { 0,                0 }};

static CAMNLS   nlsDisconnectDlg[] = {
    { IDC_DISCON1,      DLG_Disconnected1 },
    { IDC_DISCON2,      DLG_Disconnected2 },
    { IDC_DISCON3,      DLG_Disconnected3 },
    { IDC_DISCON4,      DLG_WindowWillClose1 },
    { IDC_DISCON5,      DLG_WindowWillClose2 },
    { IDC_DISCON6,      DLG_ToEndNow },
    { IDC_EXIT,         BTN_Exit },
    { 0,                0 }};

/****************************************************************************/

int         main( int argc, char * argv[])

{
    HAB     hab = 0;
    HMQ     hmq = 0;
    HWND    hwnd = 0;
    int     nRtn = 8;
    QMSG    qmsg;

do
{
    Morph();

    hab = WinInitialize( 0);
    if (!hab)
        break;

    hmq = WinCreateMsgQueue( hab, 0);
    if (!hmq)
        break;

    InitGlobalOptions( argc, argv);
    if (!UtilDoStartupTests()) {
        nRtn = 0;
        break;
    }

    hwnd = OpenWindow( 0, IDD_MAIN, (ULONG)thisCam);
    if (!hwnd)
        break;

    while (WinGetMsg( hab, &qmsg, 0, 0, 0))
        WinDispatchMsg( hab, &qmsg);

    nRtn = 0;

} while (0);

    if (nRtn)
        DosBeep( 440, 150);

    if (hwnd)
        WinDestroyWindow( hwnd);

    CamShutDownCamera( &thisCam);

    if (hmq)
        WinDestroyMsgQueue( hmq);
    if (hab)
        WinTerminate( hab);

    return (nRtn);
}

/****************************************************************************/

// enable this program to use PM functions
// even if it was linked or started as a VIO app

void        Morph( void)

{
    PPIB    ppib;
    PTIB    ptib;

    DosGetInfoBlocks( &ptib, &ppib);
    ppib->pib_ultype = 3;
    return;
}

/****************************************************************************/

// currently, this only looks for "demo", "force", "dave", & "nolvm";
// args can be preceeded by '/', '-', '--', or nothing

void        InitGlobalOptions( int argc, char * argv[])

{
    int     ctr;
    char *  ptr;

    ulGlobalFlags = 0;

    if (argc < 2)
        return;

    for (ctr = 1; ctr < argc; ctr++) {
        ptr = argv[ctr];
        if (*ptr == '/')
            ptr++;
        else
        if (*ptr == '-') {
            ptr++;
            if (*ptr == '-')
                ptr++;
        }

        // prevent Cam from trying to discover cameras
        if (!stricmp( ptr, "demo"))
            ulGlobalFlags |= CAMFLAG_DEMO;
        else
        // force the camera discovery routine to query devices
        // that don't identify themselves as PTP-class devices
        if (!stricmp( ptr, "force"))
            ulGlobalFlags |= CAMFLAG_FORCE;
        else
        // ignore handles at the beginning of handle list
        // that are out of sequence
        if (!stricmp( ptr, "dave"))
            ulGlobalFlags |= CAMFLAG_DAVE;
        else
        // don't use LVM to mount removables
        if (!stricmp( ptr, "nolvm"))
            ulGlobalFlags |= CAMFLAG_NOLVM;
        else
        // delete all options from os2.ini at startup & disable LVM use
        // N.B. - CAMFLAG_RESTORE deletes all options on exit
        if (!stricmp( ptr, "reset")) {
            ulGlobalFlags |= CAMFLAG_NOLVM;
            PrfWriteProfileData( HINI_USERPROFILE, CAMINI_APP, 0, 0, 0);
        }
    }

    return;
}

/****************************************************************************/

BOOL        SetGlobalFlag( ULONG ulFlag, ULONG ulValue)

{
    ulFlag &= CAMFLAG_ARGMASK;

    if (ulFlag) {
        if (ulValue == FALSE)
            ulGlobalFlags &= ~ulFlag;
        else
        if (ulValue == CAM_TOGGLE)
            ulGlobalFlags ^= ulFlag;
        else
            ulGlobalFlags |= ulFlag;
    }

    return ((ulGlobalFlags & ulFlag) ? TRUE : FALSE);
}

/****************************************************************************/

// mp1 == startup flag

void        NewCamera( HWND hwnd, MPARAM mp1)

{
    if ((mp1 && CAM_IS_DEMO) ||
        !CamSetupCamera( hwnd, &thisCam, (BOOL)mp1))
        GetData( hwnd, FALSE);

    return;
}

/****************************************************************************/
/****************************************************************************/

// just your basic dialog procedure

MRESULT _System MainWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    switch (msg)
    {

        case WM_COMMAND:
            Command( hwnd, SHORT1FROMMP( mp1));
            return (0);

        case WM_CONTROL:
            switch ((ULONG)mp1) {
                case MAKEULONG( FID_CLIENT, CN_CONTEXTMENU):
                    PopupMenu( hwnd, (CAMRecPtr)mp2);
                    break;

                case MAKEULONG( FID_CLIENT, CN_ENTER):
                    if (((PNOTIFYRECORDENTER)mp2)->pRecord)
                        Command( hwnd, IDM_WRITE);
                    break;

                case MAKEULONG( FID_CLIENT, CN_EMPHASIS): {
                    PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)mp2;

                    if ((pnre->fEmphasisMask & CRA_CURSORED) &&
                        (pnre->pRecord->flRecordAttr & CRA_CURSORED) &&
                        WinIsWindowEnabled( hwnd))
                        WinPostMsg( hwnd, CAMMSG_SHOWTHUMB,
                                    (MP)pnre->pRecord, 0);
                    break;
                }

                case MAKEULONG( FID_CLIENT, CN_BEGINEDIT): {
                    PCNREDITDATA    pCE = (PCNREDITDATA)mp2;
                    CAMRecPtr       pcr = (CAMRecPtr)pCE->pRecord;

                    if (pcr && pcr->orgname == 0)
                        pcr->orgname = strdup( pcr->title);
                    break;
                }

                case MAKEULONG( FID_CLIENT, CN_REALLOCPSZ): {
                    PCNREDITDATA    pCE = (PCNREDITDATA)mp2;

                    if (pCE->pRecord)
                        if (pCE->cbText > 1 && pCE->cbText <= 260)
                            return (MRESULT)TRUE;
                    break;
                }

                case MAKEULONG( FID_CLIENT, CN_ENDEDIT): {
                    PCNREDITDATA    pCE = (PCNREDITDATA)mp2;
                    CAMRecPtr       pcr = (CAMRecPtr)pCE->pRecord;

                    if (!pcr)
                        break;

                    if (pcr->orgname &&
                        !strcmp( pcr->orgname, pcr->title)) {
                        free( pcr->orgname);
                        pcr->orgname = 0;
                    }
                    else
                        if (IsMultipleSel( hwnd, pcr))
                            SetMultTitles( hwnd, pcr);

                    pcr = (CAMRecPtr)WinSendMsg( pCE->hwndCnr,
                                                 CM_QUERYRECORDEMPHASIS,
                                                 (MP)CMA_FIRST, (MP)CRA_CURSORED);
                    if (pcr)
                        SetThumbWndTitle( hwnd, pcr->title);

                    break;
                }

            }
            return (0);

        case WM_DRAWITEM:
            return ((MRESULT)CnrPaintRecord( (POWNERITEM)mp2));

        case CAMMSG_FOCUSCHANGE: {
            HWND    hModal = GetModalHwnd();
            if (hModal)
                WinFocusChange( HWND_DESKTOP, hModal, 0);
            return (0);
        }

        case CAMMSG_SHOWTHUMB:
            SetThumbWnd( hwnd, (CAMRecPtr)mp1);
            return (0);

        case CAMMSG_FETCHTHUMBS:
            FetchOneThumbnail( hwnd, mp1, mp2);
            return (0);

        case CAMMSG_GETDATA:
            GetDataReply( hwnd, mp1);
            return (0);

        case CAMMSG_GETLIST:
            GetListReply( hwnd, mp1);
            return (0);

        case CAMMSG_SYNCLIST:
            SyncReply( hwnd, mp1);
            return (0);

        case CAMMSG_NOTIFY:
            CamHandleNotification( hwnd, mp1, mp2);
            return (0);

        case CAMMSG_OPEN:
            NewCamera( hwnd, mp1);
            return (0);

        case CAMMSG_EXIT:
            if (mp1 || DrvEjectAll( hwnd))
                WinPostMsg( hwnd, WM_QUIT, 0, 0);
            return (0);

        case WM_INITMENU:
            if (mp1 == (MP)IDM_FILE)
                InitFileMenu( (HWND)mp2);
            else
            if (mp1 == (MP)IDM_VIEW)
                InitViewMenu( (HWND)mp2);
            else
            if (mp1 == (MP)IDM_GROUPS)
                WinSetOwner( QueryGroupMenu(), (HWND)mp2);

            return (0);

        case WM_MENUEND:
            if (mp1 == (MP)IDM_SINGLE) {

                // pcr is actually a CAMRecPtr * but who cares?
                ULONG pcr = WinQueryWindowULong( (HWND)mp2, QWL_USER);

                if (pcr)
                    WinSendMsg( WinWindowFromID( hwnd, FID_CLIENT),
                                CM_SETRECORDEMPHASIS, (MP)pcr,
                                MPFROM2SHORT( FALSE, CRA_SOURCE));
            }
            return (0);

        case WM_SYSCOMMAND:
            if (SHORT1FROMMP( mp1) != SC_CLOSE)
                break;

            SaveChanges( hwnd, CAMMSG_EXIT);
            return (0);

        case WM_CHAR:
            if ((SHORT1FROMMP(mp1) & (KC_VIRTUALKEY | KC_KEYUP)) ==
                                      KC_VIRTUALKEY) {
                switch (SHORT2FROMMP(mp2))
                {
                    // if this were handled as an accelerator, you
                    // couldn't delete characters when editing a title
                    case VK_DELETE:
                        Command( hwnd, IDM_DELETE);
                        return ((MRESULT)TRUE);

                    case VK_TAB:
                    case VK_BACKTAB:
                    case VK_RIGHT:
                    case VK_LEFT:
                    case VK_DOWN:
                    case VK_UP:
                        if (NavigateMainWnd( hwnd, SHORT2FROMMP(mp2)))
                            return ((MRESULT)TRUE);
                }
                break;
            }

            if ((SHORT1FROMMP(mp1) & (KC_SCANCODE | KC_CTRL | KC_KEYUP)) ==
                                     (KC_SCANCODE | KC_CTRL) &&
                SHORT1FROMMP(mp2) && SHORT1FROMMP(mp2) < 0x100) {
                if (MatchGroupMnemonic( hwnd, SHORT1FROMMP(mp2)))
                    return ((MRESULT)TRUE);
            }
            break;

        case WM_CLOSE:
            SaveChanges( hwnd, CAMMSG_EXIT);
            return (0);

        case WM_DESTROY:
            TermShow( hwnd);
            break;

        case WM_INITDLG:
            InitDlg( hwnd);
            return (0);

        case WM_QUERYFRAMECTLCOUNT:
            return ((MRESULT)(SHORT1FROMMR(
                              WinDefDlgProc( hwnd, msg, mp1, mp2)) + 2));

        case WM_FORMATFRAME:
            return (FormatFrame( hwnd, mp1, mp2));

        case WM_SAVEAPPLICATION:
            if (CAM_IS_RESTORE)
                PrfWriteProfileData( HINI_USERPROFILE, CAMINI_APP, 0, 0, 0);
            else
            {
                StoreColumns( hwnd);
                CnrStoreViews();
                StoreSort();
                StoreRotateOpts();
                WinStoreWindowPos( CAMINI_APP, CAMINI_WNDPOS, hwnd);
            }
            StoreGroups();
            break;

    } //end switch (msg)

    return (WinDefDlgProc( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

void        InitDlg( HWND hwnd)

{
    BOOL        fRtn = FALSE;
    HACCEL      haccel;
    HPOINTER    hIcon;

do
{
    if (WinWindowFromID( hwnd, FID_CLIENT) == 0 ||
        WinWindowFromID( hwnd, IDC_STATUS) == 0 ||
        WinWindowFromID( hwnd, IDC_SIDEBAR) == 0)
        break;

    // get icons, pointers, and menu checkmarks
    InitStatStuff( hwnd);

    // set the app icon
    hIcon = WinLoadPointer( HWND_DESKTOP, 0, IDI_APPICO);
    if (hIcon)
        WinSendMsg( hwnd, WM_SETICON, (MP)hIcon, 0);

    // init the menubar & popup menus
    if (!InitMenus( hwnd))
        break;

    // setup the container's columns, graphics, & menus
    if (CnrInit( hwnd))
        break;

    // init the sidebar windows & groups
    InitShow( hwnd);

    // init removable drive options
    DrvInit( hwnd);

    // make the frame aware of the menu, status, & side bars;
    WinSendMsg( hwnd, WM_UPDATEFRAME, (MP)FCF_MENU, 0);
    SetTitle( hwnd, 0);

    // load the accelerator table for the main window
    haccel = WinLoadAccelTable( 0, 0, IDA_MAIN);
    if (!haccel || !WinSetAccelTable( 0, haccel, hwnd))
        printf( "Main - unable to load accelerator table - haccel= %x\n",
                (int)haccel);

    // wait until main() has entered its msg loop before getting any data
    WinPostMsg( hwnd, CAMMSG_OPEN, (MP)TRUE, 0);

    ShowDialog( hwnd, CAMINI_WNDPOS);
    fRtn = TRUE;

} while (0);

    if (!fRtn)
        OpenWindow( hwnd, IDD_FATAL, MSG_InitDisplay);

    return;
}

/****************************************************************************/

// handles the positioning of the menu, status, & side bars

MRESULT     FormatFrame( HWND hwnd, MPARAM mp1, MPARAM mp2)

{
    USHORT  usClient = 0;
    USHORT  usCnt;
    HWND    hCnr;
    PSWP    pSWP = (PSWP)mp1;
    RECTL   rcl;

    // have the dlg format the controls it knows about
    // (which by now includes the menubar we added)
    usCnt = SHORT1FROMMR(WinDefDlgProc( hwnd, WM_FORMATFRAME, mp1, mp2));

    // find the client window's SWP
    hCnr = WinWindowFromID( hwnd, FID_CLIENT);
    while (pSWP[usClient].hwnd != hCnr)
        usClient++;

    // Status Bar

    // fill in the next SWP with some basic info
    pSWP[usCnt].hwnd = WinWindowFromID( hwnd, IDC_STATUS);
    pSWP[usCnt].fl = SWP_SIZE | SWP_MOVE | SWP_SHOW;

    // get the size of the statusbar
    WinQueryWindowRect( pSWP[usCnt].hwnd, &rcl);

    // position the statusbar at the client's current origin;
    // make it as wide as the client but leave its height as-is
    pSWP[usCnt].x  = pSWP[usClient].x;
    pSWP[usCnt].cx = pSWP[usClient].cx;
    pSWP[usCnt].y  = pSWP[usClient].y;
    pSWP[usCnt].cy = rcl.yTop;

    // reduce the client window's height & move its origin up
    // by the height of the statusbar
    pSWP[usClient].cy -= pSWP[usCnt].cy;
    pSWP[usClient].y += pSWP[usCnt].cy;

    usCnt++;

    // Side Bar

    // fill in the next SWP with some basic info
    pSWP[usCnt].hwnd = WinWindowFromID( hwnd, IDC_SIDEBAR);
    pSWP[usCnt].fl = SWP_SIZE | SWP_MOVE | SWP_SHOW;

    // get the size of the sidebar
    WinQueryWindowRect( pSWP[usCnt].hwnd, &rcl);

    // position the sidebar at the client's adjusted origin;
    // make it as tall as the client but leave its width as-is
    pSWP[usCnt].x  = pSWP[usClient].x;
    pSWP[usCnt].cx = rcl.xRight;
    pSWP[usCnt].y  = pSWP[usClient].y;
    pSWP[usCnt].cy = pSWP[usClient].cy;

    // reduce the client window's width & move its origin over
    // by the width of the sidebar
    pSWP[usClient].cx -= pSWP[usCnt].cx;
    pSWP[usClient].x += pSWP[usCnt].cx;

    usCnt++;

    // return total count of frame controls
    return( MRFROMSHORT( usCnt));
}

/****************************************************************************/

void        SetTitle( HWND hwnd, char * pszText)

{
    char *  ptr;
    char    szTitle[256];

    strcpy( szTitle, szAppTitle);
    ptr = strchr( szTitle, 0);
    strcpy( ptr, " - ");
    ptr += 3;
    if (pszText)
        strcpy( ptr, pszText);
    else
        LoadString( MSG_NoCamera, ptr);

    szTitle[63] = 0;
    WinSetWindowText( hwnd, szTitle);

    return;
}

/****************************************************************************/
/****************************************************************************/

// because the sidebar buttons are children of a static control rather
// than the main dialog, keyboard navigation has to be handled "manually"

BOOL        NavigateMainWnd( HWND hwnd, USHORT usVK)

{
    BOOL    fRtn = FALSE;
    USHORT  usID;
    ULONG   ulCode;
    HWND    hSide;
    HWND    hFocus;
    HWND    hNext;

do {
    hSide = WinWindowFromID( hwnd, IDC_SIDEBAR);
    hFocus = WinQueryFocus( HWND_DESKTOP);
    usID = WinQueryWindowUShort( hFocus, QWS_ID);

    if (usID == FID_CLIENT) {
        if (usVK == VK_TAB)
            ulCode = EDI_FIRSTTABITEM;
        else
        if (usVK == VK_BACKTAB)
            ulCode = EDI_LASTTABITEM;
        else
            break;

        hNext = WinEnumDlgItem( hSide, 0, ulCode);
        WinFocusChange( HWND_DESKTOP, hNext, 0);
        fRtn = TRUE;
        break;
    }

    if (usID < IDC_GRPBTNFIRST || usID >= IDC_GRPBTNLAST)
        break;

    ulCode = 99;
    switch (usVK) {
        case VK_TAB:
            ulCode = EDI_NEXTTABITEM;
            break;

        case VK_BACKTAB:
            // if the focus is on the first button, let the
            // dialog proc move the focus to the container
            if (hFocus == WinEnumDlgItem( hSide, 0, EDI_FIRSTTABITEM))
                break;
            ulCode = EDI_PREVTABITEM;
            break;

        case VK_RIGHT:
        case VK_DOWN:
            ulCode = EDI_NEXTGROUPITEM;
            break;

        case VK_LEFT:
        case VK_UP:
            ulCode = EDI_PREVGROUPITEM;
            break;
    }

    if (ulCode == 99)
        break;

    hNext = WinEnumDlgItem( hSide, hFocus, ulCode);

    // if the focus is about to loop back to the first button,
    // let the dialog proc move the focus to the container
    if (usVK == VK_TAB && hNext == WinEnumDlgItem( hSide, 0, EDI_FIRSTTABITEM))
        break;

    WinFocusChange( HWND_DESKTOP, hNext, 0);
    fRtn = TRUE;

} while (0);

    return (fRtn);
}

/****************************************************************************/

void        InitFileMenu( HWND hMenu)

{
    STATSTUFF * pss = GetStatStuff();

    WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( IDM_SAVE, FALSE),
                MPFROM2SHORT( MIA_DISABLED,
                        (pss->ulDelCnt || pss->ulSavCnt ? 0 : MIA_DISABLED)));

    WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( IDM_INFO, FALSE),
                MPFROM2SHORT( MIA_DISABLED,
                              (InfoAvailable() ? 0 : MIA_DISABLED)));

    return;
}

/****************************************************************************/

void        InitViewMenu( HWND hMenu)

{
    USHORT      usDetails  = 0;
    USHORT      usSmall    = 0;
    USHORT      usMedium   = 0;
    USHORT      usLarge    = 0;
    USHORT      usColReset = 0;
    USHORT      usColOrder = 0;
    USHORT      usColors   = 0;
    ULONG       ulView     = CnrGetView();

    if (ulView == IDM_DETAILS) {
        usDetails = MIA_CHECKED;

        if (GetHwnd( IDD_COLUMNS)) {
            usColOrder = MIA_DISABLED;
            usDetails  = MIA_DISABLED;
            usSmall    = MIA_DISABLED;
            usMedium   = MIA_DISABLED;
            usLarge    = MIA_DISABLED;
        }
    }
    else {
        usColReset = MIA_DISABLED;
        usColOrder = MIA_DISABLED;

        if (ulView == IDM_SMALL)
            usSmall = MIA_CHECKED;
        else
        if (ulView == IDM_MEDIUM)
            usMedium = MIA_CHECKED;
        else
        if (ulView == IDM_LARGE)
            usLarge = MIA_CHECKED;
    }

    if (GetHwnd( IDD_COLORS))
        usColors = MIA_DISABLED;

    WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( IDM_DETAILS, FALSE),
                MPFROM2SHORT( (MIA_CHECKED | MIA_DISABLED), usDetails));

    WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( IDM_SMALL, FALSE),
                MPFROM2SHORT( (MIA_CHECKED | MIA_DISABLED), usSmall));

    WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( IDM_MEDIUM, FALSE),
                MPFROM2SHORT( (MIA_CHECKED | MIA_DISABLED), usMedium));

    WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( IDM_LARGE, FALSE),
                MPFROM2SHORT( (MIA_CHECKED | MIA_DISABLED), usLarge));

    WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( IDM_COLRESET, FALSE),
                MPFROM2SHORT( MIA_DISABLED, usColReset));

    WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( IDM_COLORDER, FALSE),
                MPFROM2SHORT( MIA_DISABLED, usColOrder));

    WinSendMsg( hMenu, MM_SETITEMATTR, MPFROM2SHORT( IDM_COLORS, FALSE),
                MPFROM2SHORT( MIA_DISABLED, usColors));

    return;
}

/****************************************************************************/
/****************************************************************************/

// show the counters and the optional extra message

void        UpdateStatus( HWND hwnd, ULONG ulMsg)

{
    int         cnt;
    STATSTUFF * pss;
    char        szText[384];

    pss = GetStatStuff();
    cnt = sprintf( szText, pss->pszStatus,
                   pss->ulTotCnt, (pss->ulTotSize/1024),
                   pss->ulSavCnt, (pss->ulSavSize/1024),
                   pss->ulDelCnt, (pss->ulDelSize/1024));
    if (ulMsg)
        LoadString( ulMsg, &szText[cnt]);
    WinSetDlgItemText( hwnd, IDC_STATUS, (PCSZ)szText);
 
   return;
}

/****************************************************************************/

void        InitStatStuff( HWND hwnd)

{
    memset( &ss, 0, sizeof(STATSTUFF));
    ss.pszStatus = LoadString( STS_Status, 0);

    ss.hBlkIco = WinLoadPointer( HWND_DESKTOP, 0, IDI_BLKICO);
    ss.hSavIco = WinLoadPointer( HWND_DESKTOP, 0, IDI_SAVICO);
    ss.hDelIco = WinLoadPointer( HWND_DESKTOP, 0, IDI_DELICO);
    ss.hDoneIco = WinLoadPointer( HWND_DESKTOP, 0, IDI_DONEICO);

    return;
}

/****************************************************************************/

STATSTUFF * ClearStatStuff( void)

{
    ss.ulTotCnt = 0;
    ss.ulTotSize = 0;
    ss.ulSavCnt = 0;
    ss.ulSavSize = 0;
    ss.ulDelCnt = 0;
    ss.ulDelSize = 0;

    return (&ss);
}

/****************************************************************************/

STATSTUFF * GetStatStuff( void)

{
    return (&ss);
}

/****************************************************************************/

void *      GetCamPtr( void)
{
    return (thisCam);
}

/****************************************************************************/
/****************************************************************************/

HWND        OpenWindow( HWND hwnd, ULONG ulID, ULONG ulParm)

{
    CAMWnd *    ptr;
    CAMWnd *    ptr2;

do {
    for (ptr = camWnd; ptr->ulID && ptr->ulID != ulID; ptr++);
    if (!ptr->ulID)
        break;

    if (ptr->hwnd) {
        if (WinQueryWindowUShort( ptr->hwnd, QWS_ID) == ptr->ulID) {
            if (!ptr->usDisabled)
                WinSetWindowPos( ptr->hwnd, 0, 0, 0, 0, 0, SWP_SHOW | SWP_ACTIVATE);
            break;
        }
        ptr->hwnd = 0;
    }

    ptr->hwnd = WinLoadDlg( HWND_DESKTOP,       // parent-window
                            hwnd,               // owner-window
                            ptr->pfnwp,         // dialog proc
                            0,                  // EXE module handle
                            ptr->ulID,          // dialog id
                            (void*)ulParm);     // pointer to create params
    if (!ptr->hwnd)
        break;

    if (!ptr->usModality)
        break;

    for (ptr2 = camWnd; ptr2->ulID; ptr2++)
        if (ptr2->hwnd && ptr2->usModality < ptr->usModality) {
            WinEnableWindow( ptr2->hwnd, FALSE);
            ptr2->usDisabled++;
        }

} while (0);

    return (ptr->hwnd);
}

/****************************************************************************/

void        WindowClosed( ULONG ulID)

{
    CAMWnd *    ptr;
    CAMWnd *    ptr2;

do {
    for (ptr = camWnd; ptr->ulID && ptr->ulID != ulID; ptr++);
    if (!ptr->ulID || !ptr->hwnd)
        break;

    ptr->hwnd = 0;

    if (!ptr->usModality)
        break;

    for (ptr2 = camWnd; ptr2->ulID; ptr2++)
        if (ptr2->hwnd && ptr2->usModality < ptr->usModality) {
            if (ptr2->usDisabled)
                ptr2->usDisabled--;
            if (!ptr2->usDisabled)
                WinEnableWindow( ptr2->hwnd, TRUE);
        }

} while (0);

    return;
}

/****************************************************************************/

HWND        GetHwnd( ULONG ulID)

{
    CAMWnd *    ptr;

    for (ptr = camWnd; ptr->ulID && ptr->ulID != ulID; ptr++);

    return (ptr->hwnd);
}

/****************************************************************************/

HWND        GetModalHwnd( void)

{
    CAMWnd *    ptr;

    for (ptr = camWnd; ptr->ulID; ptr++)
        if (ptr->hwnd && ptr->usModality && !ptr->usDisabled)
            break;

    return (ptr->hwnd);
}

/****************************************************************************/
/****************************************************************************/

MRESULT _System WaitOrExitDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    switch (msg)
    {
        case WM_COMMAND:
            if (SHORT1FROMMP( mp1) == IDC_EXIT)
                WinPostMsg( WinQueryWindow( hwnd, QW_OWNER), WM_QUIT, 0, 0);
            return (0);

        case WM_SYSCOMMAND:
            if (SHORT1FROMMP( mp1) == SC_CLOSE) {
                WinPostMsg( WinQueryWindow( hwnd, QW_OWNER), WM_QUIT, 0, 0);
                return (0);
            }
            break;

        case WM_FOCUSCHANGE:
            if (SHORT1FROMMP(mp2) && !WinIsWindowEnabled( hwnd)) {
                WinPostMsg( WinQueryWindow( hwnd, QW_OWNER),
                            CAMMSG_FOCUSCHANGE, 0, 0);
                return (0);
            }
            break;

        case WM_CLOSE:
            WinPostMsg( WinQueryWindow( hwnd, QW_OWNER), WM_QUIT, 0, 0);
            return (0);

        case WM_INITDLG: {
            if (WinQueryWindowUShort( hwnd, QWS_ID) == IDD_DISCONNECT)
                LoadDlgStrings( hwnd, nlsDisconnectDlg);
            else
                LoadDlgStrings( hwnd, nlsLoadingDlg);

            ShowDialog( hwnd, 0);
            return (0);
        }

    } //end switch (msg)

    return (WinDefDlgProc( hwnd, msg, mp1, mp2));
}

/****************************************************************************/
/****************************************************************************/

void        ShowDialog( HWND hwnd, char * pIniKey)

{
    HWND    hOwner;
    RECTL   rcl;
    RECTL   rclOwner;

do {
    if (pIniKey && WinRestoreWindowPos( CAMINI_APP, pIniKey, hwnd))
        break;

    WinQueryWindowRect( hwnd, &rcl);

    hOwner = WinQueryWindow( hwnd, QW_OWNER);
    if (hOwner) {
        WinQueryWindowRect( hOwner, &rclOwner);
        WinMapWindowPoints( hOwner, HWND_DESKTOP, (PPOINTL)&rclOwner, 1);
    }
    else {
        rclOwner.xLeft   = 0;
        rclOwner.yBottom = 0;
        rclOwner.xRight  = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN);
        rclOwner.yTop    = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN);
    }

    WinSetWindowPos( hwnd, 0,
                     rclOwner.xLeft + ((rclOwner.xRight - rcl.xRight) / 2),
                     rclOwner.yBottom+ ((rclOwner.yTop - rcl.yTop) / 2),
                     0, 0, SWP_MOVE | SWP_SHOW);

} while (0);

    // in rare cases, a window will be invisible when restored;
    // this ensures it's visible
    WinShowWindow( hwnd, TRUE);

    return;
}

/****************************************************************************/

