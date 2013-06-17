/****************************************************************************/
// camdrive.c
//
//  - mount & eject removable drives
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

#define INCL_DOS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_WIN
#include "camapp.h"
#include "camusb.h"
#include <os2.h>

/****************************************************************************/

#pragma pack(1)

typedef struct _CAMIoCmd
{
   BYTE         bCommand;
   BYTE         bDrive;
} CAMIoCmd;

typedef struct _CAMIoDisk
{
   BYTE         abBPB[31];
   USHORT       usCyl;
   BYTE         bType;
   USHORT       usAttrib;
   BYTE         bReserved[6];
} CAMIoDisk;

#pragma pack()

#ifndef ERROR_INVALID_DRIVE
  #define ERROR_INVALID_DRIVE   15
#endif
#ifndef ERROR_TIMEOUT
  #define ERROR_TIMEOUT         640
#endif

#define CAMDRV_NOLVM            0x0001
#define CAMDRV_NEVEREJECT       0x0002
#define CAMDRV_ALWAYSEJECT  	0x0004

#define CAMDRV_MASK             0x0007
#define CAMDRV_EJECTMASK        (CAMDRV_NEVEREJECT | CAMDRV_ALWAYSEJECT)

#define CAMDRV_IS_NOLVM         (ulDrvOpts & CAMDRV_NOLVM)
#define CAMDRV_IS_NEVEREJECT    (ulDrvOpts & CAMDRV_NEVEREJECT)
#define CAMDRV_IS_ALWAYSEJECT   (ulDrvOpts & CAMDRV_ALWAYSEJECT)

/****************************************************************************/

void            DrvInit( HWND hwnd);
void            DrvMenuCmd( HWND hwnd, ULONG ulCmd);
void            DrvSetUseLvm( HWND hwnd, BOOL fUse);
BOOL            DrvQueryUseLvm( void);
ULONG           DrvGetDrives( HWND hwnd, char * achDrives);
ULONG           DrvMapDrives( char * achDrives);
ULONG           DrvValidateDrives( char * achDrives, uint32_t cntIn);
void            DrvMountDrives( HWND hwnd);
void            DrvMountDrivesThread( void * hev);
void            DrvGetDriveName( ULONG ulDrive, char * pszText);
BOOL            DrvEjectAll( HWND hwnd);
ULONG           DrvEjectDrive( char chDrive);
MRESULT _System DrvMountDrivesDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT _System DrvEjectAllDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
void            DrvInitEjectAllDlg( HWND hwnd, MPARAM mp2);
void            DrvEjectAllDrives( HWND hwnd);

/****************************************************************************/

static ULONG    ulDrvOpts = 0;

static CAMNLS   nlsMountDlg[] = {
    { IDC_TEXT1,        DLG_Removable1 },
    { IDC_TEXT2,        DLG_Removable2 },
    { IDC_TEXT3,        DLG_Removable3 },
    { IDC_TEXT4,        DLG_Removable4 },
    { IDC_TEXT5,        DLG_Removable5 },
    { IDC_AUTOMOUNT,    MNU_MountDrives },
    { IDC_CONTINUE,     BTN_Continue },
    { 0,                0 }};

static CAMNLS   nlsEjectAllDlg[] = {
    { IDC_TEXT1,        DLG_Eject },
    { IDC_YES,          BTN_Yes },
    { IDC_NO,           BTN_No },
    { 0,                0 }};

/****************************************************************************/

void        DrvInit( HWND hwnd)

{
    ULONG   ul;

    ul = sizeof(ulDrvOpts);
    if (!PrfQueryProfileData( HINI_USERPROFILE, CAMINI_APP, CAMINI_DRVOPTS,
                              &ulDrvOpts, &ul) || ul != sizeof(ulDrvOpts))
        ulDrvOpts = 0;

    ulDrvOpts &= CAMDRV_MASK;
    if (CAM_IS_NOLVM)
        DrvSetUseLvm( hwnd, FALSE);

    ul = ulDrvOpts & CAMDRV_EJECTMASK;
    if (!ul)
        ul = IDM_EJECTDLG;
    else
    if (ul == CAMDRV_NEVEREJECT)
        ul = IDM_EJECTNEVER;
    else
    if (ul == CAMDRV_ALWAYSEJECT)
        ul = IDM_EJECTALWAYS;
    else {
        // if both "always" & "never" eject are on, turn both off
        ulDrvOpts &= ~CAMDRV_EJECTMASK;
        ul = IDM_EJECTDLG;
    }

    WinSendDlgItemMsg( hwnd, FID_MENU, MM_SETITEMATTR,
                       MPFROM2SHORT( ul, TRUE),
                       MPFROM2SHORT( MIA_CHECKED, MIA_CHECKED));


    if (!(ulDrvOpts & CAMDRV_NOLVM))
        WinSendDlgItemMsg( hwnd, FID_MENU, MM_SETITEMATTR,
                           MPFROM2SHORT( IDM_AUTOMOUNT, TRUE),
                           MPFROM2SHORT( MIA_CHECKED, MIA_CHECKED));

    return;
}

/****************************************************************************/

void        DrvMenuCmd( HWND hwnd, ULONG ulCmd)

{
    ULONG   ulOld = ulDrvOpts;

do {
    if (ulCmd == IDM_AUTOMOUNT) {
        ulDrvOpts ^= CAMDRV_NOLVM;
        WinSendDlgItemMsg( hwnd, FID_MENU, MM_SETITEMATTR,
                           MPFROM2SHORT( ulCmd, TRUE),
                           MPFROM2SHORT( MIA_CHECKED,
                           ((ulDrvOpts & CAMDRV_NOLVM) ? 0 : MIA_CHECKED)));
        break;
    }

    ulDrvOpts &= ~CAMDRV_EJECTMASK;
    if (ulCmd == IDM_EJECTALWAYS)
        ulDrvOpts |= CAMDRV_ALWAYSEJECT;
    else
    if (ulCmd == IDM_EJECTNEVER)
        ulDrvOpts |= CAMDRV_NEVEREJECT;
    else
    if (ulCmd != IDM_EJECTDLG) {
        ulDrvOpts = ulOld;
        break;
    }

    WinSendDlgItemMsg( hwnd, FID_MENU, MM_SETITEMATTR,
                       MPFROM2SHORT( IDM_EJECTDLG, TRUE),
                       MPFROM2SHORT( MIA_CHECKED,
                       ((ulCmd == IDM_EJECTDLG) ? MIA_CHECKED : 0)));
    WinSendDlgItemMsg( hwnd, FID_MENU, MM_SETITEMATTR,
                       MPFROM2SHORT( IDM_EJECTNEVER, TRUE),
                       MPFROM2SHORT( MIA_CHECKED,
                       ((ulCmd == IDM_EJECTNEVER) ? MIA_CHECKED : 0)));
    WinSendDlgItemMsg( hwnd, FID_MENU, MM_SETITEMATTR,
                       MPFROM2SHORT( IDM_EJECTALWAYS, TRUE),
                       MPFROM2SHORT( MIA_CHECKED,
                       ((ulCmd == IDM_EJECTALWAYS) ? MIA_CHECKED : 0)));

} while (0);

    if (ulDrvOpts != ulOld)
        PrfWriteProfileData( HINI_USERPROFILE, CAMINI_APP, CAMINI_DRVOPTS,
                             &ulDrvOpts, sizeof(ulDrvOpts));

    return;
}


/****************************************************************************/

void        DrvSetUseLvm( HWND hwnd, BOOL fUse)

{
    if ((fUse && (ulDrvOpts & CAMDRV_NOLVM)) ||
        (!fUse && !(ulDrvOpts & CAMDRV_NOLVM))) {
        ulDrvOpts ^= CAMDRV_NOLVM;

        PrfWriteProfileData( HINI_USERPROFILE, CAMINI_APP, CAMINI_DRVOPTS,
                             &ulDrvOpts, sizeof(ulDrvOpts));

        WinSendDlgItemMsg( hwnd, FID_MENU, MM_SETITEMATTR,
                           MPFROM2SHORT( IDM_AUTOMOUNT, TRUE),
                           MPFROM2SHORT( MIA_CHECKED,
                           (CAMDRV_IS_NOLVM ? 0 : MIA_CHECKED)));
    }

    return;
}

/****************************************************************************/

BOOL        DrvQueryUseLvm( void)

{
    return ((ulDrvOpts & CAMDRV_NOLVM) ? FALSE : TRUE);
}

/****************************************************************************/

void        DrvGetDriveName( ULONG ulDrive, char * pszText)

{
    ULONG       cbAU;
    char *      ptr;
    FSINFO      fsi;
    FSALLOCATE  fsa;
    char        szMB[16];

    if (!DosQueryFSInfo( (ulDrive - 0x40), FSIL_VOLSER, &fsi, sizeof(fsi)))
        ptr = fsi.vol.szVolLabel;
    else
        ptr = "";

    if (!DosQueryFSInfo( (ulDrive - 0x40), FSIL_ALLOC, &fsa, sizeof(fsa))) {
        LoadString( MSG_MB, szMB);
        cbAU = fsa.cSectorUnit * fsa.cbSector;
        sprintf( pszText, "%c:  %s  (%lu / %lu%s)", (char)ulDrive, ptr,
                 (((fsa.cUnit - fsa.cUnitAvail) * cbAU) + 500000) / 1000000,
                 ((fsa.cUnit * cbAU) + 500000) / 1000000, szMB);
    }
    else
        sprintf( pszText, "%c:  %s", (char)ulDrive, ptr);

    return;
}

/****************************************************************************/

ULONG       DrvEjectDrive( char chDrive)

{
    ULONG       rc;
    ULONG       cbParm;
    CAMIoCmd    cmd;

    if (chDrive < 'A' || chDrive > 'Z') {
        printf( "DrvEjectDrive - invalid drive letter - drive= 0x%hhx\n",
                chDrive);
        return (ERROR_INVALID_DRIVE);
    }

    cmd.bCommand = 0x02;
    cmd.bDrive = (BYTE)(chDrive - 0x41);
    cbParm = sizeof(CAMIoCmd);
    rc = DosDevIOCtl( (HFILE)-1, IOCTL_DISK, DSK_UNLOCKEJECTMEDIA,
                      &cmd, cbParm, &cbParm, 0, 0, 0);
    if (rc)
        printf( "DosDevIOCtl DSK_UNLOCKEJECTMEDIA - drive= %c  rc= 0x%lx\n",
                chDrive, rc);

    return (rc);
}

/***************************************************************************/
/****************************************************************************/

ULONG       DrvGetDrives( HWND hwnd, char * achDrives)

{
    ULONG   cntDrives = 0;
    char    szText[12];

    if (!CAMDRV_IS_NOLVM) {
        printf( "mounting drives\n");
        DrvMountDrives( hwnd);
    }
    printf( "mapping drives\n");
    cntDrives = DrvMapDrives( achDrives);
    printf( "validating drives\n");
    cntDrives = DrvValidateDrives( achDrives, cntDrives);

    if (!cntDrives)
        strcpy( szText, "none");
    else {
        SetGlobalFlag( CAMFLAG_MSD, TRUE);
        memcpy( szText, achDrives, cntDrives);
        szText[cntDrives] = 0;
    }
    printf( "Drive(s) found:  %s\n", szText);

    return (cntDrives);
}

/****************************************************************************/

ULONG       DrvMapDrives( char * achDrives)

{
    ULONG       rc = 0;
    ULONG       ctr;
    ULONG       cntDrives = 0;
    ULONG       ulBmp = 0;
    ULONG       ulMask = 4;
    CAMIoCmd    cmd;
    CAMIoDisk   disk;
    ULONG       cbData;
    ULONG       cbParm;
    BYTE        bFixed;

    memset( achDrives, 0, CAM_MAXMSD+1);
    DosQueryCurrentDisk( &ctr, &ulBmp);

    for (ctr=2; ctr < 26; ctr++, ulMask <<= 1)
    {
        if (!(ulBmp & ulMask))
            continue;

        cmd.bCommand = 0;
        cmd.bDrive = ctr;
        cbParm = sizeof(CAMIoCmd);
        cbData = sizeof(BYTE);

        rc = DosDevIOCtl( (HFILE)-1, IOCTL_DISK, DSK_BLOCKREMOVABLE,
                          &cmd, cbParm, &cbParm,
                          &bFixed, cbData, &cbData);
        if (rc) {
            printf( "DosDevIOCtl DSK_BLOCKREMOVABLE - drive= %c  rc= 0x%lx\n",
                    (char)(ctr + 0x41), rc);
            continue;
        }
        if (!bFixed)
            continue;

        cmd.bCommand = 0;
        cmd.bDrive = ctr;
        cbParm = sizeof(CAMIoCmd);
        cbData = sizeof(CAMIoDisk);

        rc = DosDevIOCtl( (HFILE)-1, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                          &cmd, cbParm, &cbParm,
                          &disk, cbData, &cbData);
        if (rc) {
            printf( "DosDevIOCtl DSK_GETDEVICEPARAMS - drive= %c  rc= 0x%lx\n",
                    (char)(ctr + 0x41), rc);
            continue;
        }

        if (disk.usAttrib & 0x08) {
            achDrives[cntDrives++] = (char)(ctr + 0x41);
            if (cntDrives >= CAM_MAXMSD)
                break;
        }
    }

    return (cntDrives);
}

/****************************************************************************/

ULONG       DrvValidateDrives( char * achDrives, uint32_t cntIn)

{
    ULONG       rc = 0;
    ULONG       ctr;
    ULONG       cntOut = 0;
    FILESTATUS3 fs3;
    char        szPath[] = "A:\\DCIM";

    for (ctr = 0; ctr < cntIn; ctr++) {
        szPath[0] = achDrives[ctr];
        rc = DosQueryPathInfo( szPath, FIL_STANDARD, &fs3, sizeof(FILESTATUS3));
        if (!rc)
            achDrives[cntOut++] = achDrives[ctr];
    }

    achDrives[cntOut] = 0;

    return (cntOut);
}

/****************************************************************************/

void        DrvMountDrives( HWND hwnd)

{
    ULONG   rc = 0;
    HEV     hev = 0;
    TID     tid;
    char *  pErr = 0;

do {
    rc = DosCreateEventSem( 0, &hev, 0, FALSE);
    if (rc)
        ERRMSG( "DosCreateEventSem")

    tid = _beginthread( DrvMountDrivesThread, 0, 0x4000, (PVOID)hev);
    if (tid == -1)
        ERRMSG( "_beginthread")

    rc = WinWaitEventSem( hev, 4000);
    if (!rc)
        break;

    if (rc != ERROR_TIMEOUT)
        ERRMSG( "DosWaitEventSem")

    rc = CamDlgBox( hwnd, IDD_LVMHANG, hev);
    printf( "DrvMountDrives - semaphore %s posted\n",
            (rc ? "was" : "was not"));

} while (0);

    if (hev)
        DosCloseEventSem( hev);

    if (pErr)
        printf( "DrvMountDrives - %s - rc= 0x%lx\n", pErr, rc);

    return;
}

/****************************************************************************/

typedef void _System (Rediscover_PRMs)( PULONG Error_Code );
typedef Rediscover_PRMs *pRediscover_PRMs;

void        DrvMountDrivesThread( void * hev)

{
    ULONG       rc;
    HMODULE     hmod = 0;
    pRediscover_PRMs    pfn = 0;
    char *      pErr = 0;
    char        szErr[16];

    rc = DosLoadModule( szErr, sizeof(szErr), "LVM", &hmod);
    if (rc)
        pErr = "DosLoadModule";
    else {
        rc = DosQueryProcAddr( hmod, 0, "Rediscover_PRMs", (PFN*)&pfn);
        if (rc)
            pErr = "DosQueryProcAddr";
        else {
            pfn( &rc);
            if (rc)
                pErr = "Rediscover_PRMs";
        }
        rc = DosFreeModule( hmod);
        if (rc && !pErr)
            pErr = "DosFreeModule";
    }

    if (pErr)
        printf( "DrvMountDrivesThread - %s - rc= %lx\n", pErr, rc);

    rc = DosPostEventSem( (HEV)hev);
    if (rc)
        printf( "DrvMountDrivesThread - DosPostEventSem - rc= %lx\n", rc);

    return;
}

/****************************************************************************/

#define CAM_LVMTMR      (TID_USERMAX - 27)

MRESULT _System DrvMountDrivesDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    if (msg == WM_TIMER && mp1 == (MP)CAM_LVMTMR) {
        HEV  hev = WinQueryWindowULong( hwnd, QWL_USER);

        if (!WinWaitEventSem( hev, 0))
            WinSendMsg( hwnd, WM_COMMAND, (MP)CAM_LVMTMR, 0);
        return (0);
    }

    if (msg == WM_COMMAND || msg == WM_SYSCOMMAND) {
        ULONG   ulChk;

        if (!WinStopTimer( 0, hwnd, CAM_LVMTMR))
            printf( "DrvMountDrivesDlg - WinStopTimer\n");

        ulChk = (ULONG)WinSendDlgItemMsg( hwnd, IDC_AUTOMOUNT,
                                          BM_QUERYCHECK, 0, 0);
        DrvSetUseLvm( WinQueryWindow( hwnd, QW_OWNER), ulChk);

        WindowClosed( WinQueryWindowUShort( hwnd, QWS_ID));
        WinDismissDlg( hwnd, (mp1 == (MP)CAM_LVMTMR));
        return (0);
    }

    if (msg == WM_INITDLG) {
        HEV  hev = (HEV)mp2;

        WinSetWindowULong( hwnd, QWL_USER, hev);
        LoadDlgStrings( hwnd, nlsMountDlg);
        WinSendDlgItemMsg( hwnd, IDC_AUTOMOUNT, BM_SETCHECK,
                           (MP)(CAMDRV_IS_NOLVM ? 0 : 1), 0);
        ShowDialog( hwnd, 0);

        if (!WinWaitEventSem( hev, 0)) {
            WindowClosed( WinQueryWindowUShort( hwnd, QWS_ID));
            WinDismissDlg( hwnd, TRUE);
        }
        else
            if (!WinStartTimer( 0, hwnd, CAM_LVMTMR, 250))
                printf( "DrvMountDrivesDlg - WinStartTimer\n");
        return (0);
    }

    if (msg == WM_FOCUSCHANGE) {
        if (SHORT1FROMMP(mp2) && !WinIsWindowEnabled( hwnd)) {
            WinPostMsg( WinQueryWindow( hwnd, QW_OWNER),
                        CAMMSG_FOCUSCHANGE, 0, 0);
            return (0);
        }
    }

    return (WinDefDlgProc( hwnd, msg, mp1, mp2));
}

#undef CAM_LVMTMR

/****************************************************************************/
/****************************************************************************/

BOOL        DrvEjectAll( HWND hwnd)

{
    BOOL    fRtn = FALSE;

    if (!CAM_IS_MSD || CAMDRV_IS_NEVEREJECT ||
        !OpenWindow( hwnd, IDD_EJECTALL, 0))
        fRtn = TRUE;

    return (fRtn);
}

/****************************************************************************/

MRESULT _System DrvEjectAllDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    switch (msg) {

        case WM_INITDLG:
            DrvInitEjectAllDlg( hwnd, mp2);
            return (0);

        case WM_SAVEAPPLICATION:
            WinStoreWindowPos( CAMINI_APP, CAMINI_EJECTPOS, hwnd);
            return (0);

        case WM_SYSCOMMAND:
            if (SHORT1FROMMP( mp1) == SC_CLOSE)
                return (WinSendMsg( hwnd, WM_COMMAND, (MP)IDC_NO, 0));
            break;

        case WM_COMMAND:
            if (mp1 == (MP)IDC_YES) {
                WinEnableWindow( WinWindowFromID( hwnd, IDC_YES), FALSE);
                WinEnableWindow( WinWindowFromID( hwnd, IDC_NO), FALSE);
                LoadDlgItemString( hwnd, IDC_TEXT1, MSG_Ejecting);
                LoadDlgItemString( hwnd, IDC_TEXT4, MSG_ThisMayTake);
                DrvEjectAllDrives( hwnd);
            }
            WindowClosed( WinQueryWindowUShort( hwnd, QWS_ID));
            WinDestroyWindow( hwnd);
            return (0);

        case WM_FOCUSCHANGE:
            if (SHORT1FROMMP(mp2) && !WinIsWindowEnabled( hwnd)) {
                WinPostMsg( WinQueryWindow( hwnd, QW_OWNER),
                            CAMMSG_FOCUSCHANGE, 0, 0);
                return (0);
            }
            break;

        case WM_DESTROY:
            WinPostMsg( WinQueryWindow( hwnd, QW_OWNER), CAMMSG_EXIT,
                        (MP)TRUE, 0);
            break;

    }

    return (WinDefDlgProc( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

void        DrvInitEjectAllDlg( HWND hwnd, MPARAM mp2)

{
    SHORT       ndx;
    HWND        hList;
    ULONG       ctr;
    ULONG       cntDrives = 0;
    char        achDrives[CAM_MAXMSD+1];
    char        szText[256];

    cntDrives = DrvMapDrives( achDrives);
    if (cntDrives)
        cntDrives = DrvValidateDrives( achDrives, cntDrives);

    if (!cntDrives) {
        WindowClosed( WinQueryWindowUShort( hwnd, QWS_ID));
        WinDestroyWindow( hwnd);
        return;
    }

    LoadDlgStrings( hwnd, nlsEjectAllDlg);
    hList = WinWindowFromID( hwnd, IDC_CAMLIST);

    for (ctr = 0; ctr < CAM_MAXMSD && achDrives[ctr]; ctr++) {
        DrvGetDriveName( achDrives[ctr], szText);

        ndx = SHORT1FROMMR( WinSendMsg( hList, LM_INSERTITEM,
                                        (MP)LIT_END, (MP)szText));
        WinSendMsg( hList, LM_SETITEMHANDLE, MPFROM2SHORT( ndx, 0),
                    MPFROM2SHORT( achDrives[ctr], 0));
        WinSendMsg( hList, LM_SELECTITEM, MPFROM2SHORT( ndx, 0), (MP)TRUE);
    }

    ShowDialog( hwnd, CAMINI_EJECTPOS);

    if (CAMDRV_IS_ALWAYSEJECT)
        WinPostMsg( hwnd, WM_COMMAND, (MP)IDC_YES, 0);

    return;
}

/****************************************************************************/

void        DrvEjectAllDrives( HWND hwnd)

{
    HWND        hList;
    SHORT       ndx;
    char        chDrive;

    hList = WinWindowFromID( hwnd, IDC_CAMLIST);

    while ((ndx = SHORT1FROMMR( WinSendMsg( hList, LM_QUERYSELECTION,
                                            (MP)LIT_FIRST, 0))) != LIT_NONE) {

        chDrive = (char)SHORT1FROMMR( WinSendMsg( hList, LM_QUERYITEMHANDLE,
                                                  MPFROM2SHORT( ndx, 0), 0));
        if (!DrvEjectDrive( chDrive))
            WinSendMsg( hList, LM_DELETEITEM, MPFROM2SHORT( ndx, 0), 0);
        else
            WinSendMsg( hList, LM_SELECTITEM, MPFROM2SHORT( ndx, 0), (MP)FALSE);
    }

    return;
}

/****************************************************************************/

