/****************************************************************************/
// camutil.c
//
//  - miscellanea:  version & usbresmg checks, including intro dialogs
//  - file dialog, path validation & creation,
//  - NLS string functions
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

#ifndef ERROR_PATH_NOT_FOUND
  #define ERROR_PATH_NOT_FOUND      3
#endif

#ifndef ERROR_FILE_EXISTS
  #define ERROR_FILE_EXISTS         80
#endif

/****************************************************************************/

BOOL            UtilDoStartupTests( void);
BOOL            UtilCheckVersion( void);
MRESULT _System UtilIntroDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
void        	UtilSetIntroPath( HWND hwnd);
BOOL            UtilTestUsbResMg( void);
MRESULT _System UtilNoUsbResMgDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
void            UtilShowUsbcallsPath( void);

BOOL            PathDlg( HWND hwnd, char * pszPath);
MRESULT _System PathDlgSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
BOOL            ValidateDirectoryEF( HWND hwnd, PFILEDLG pfd);
void            SetDirectoryEF( HWND hwnd, PFILEDLG pfd);
APIRET          CreatePath( char * pszPath);
APIRET          ValidatePath( char * pszPath);
void            UtilLoadNLS( void);
char *          LoadString( ULONG str, char * pszIn);
void            LoadDlgStrings( HWND hwnd, CAMNLSPtr pnls);
void            LoadDlgItemString( HWND hwnd, ULONG ulID, ULONG ulStr);
char *          LoadStringNoTilde( ULONG str, char * pszIn);
void            LoadStaticStrings( CAMNLSPSZ * pnls);

/****************************************************************************/

static HMODULE  hmodNLS = 0;

static CAMNLS  nlsIntro[] = {
    { IDC_INTROWELCOME, DLG_Welcome },
    { IDC_INTROPATHTXT, DLG_PleaseSelect },
    { IDC_INTROFIND,    BTN_Find },
    { IDC_INTROMSG1,    DLG_IntroMsg1 },
    { IDC_INTROMSG2,    DLG_IntroMsg2 },
    { IDC_OK,           BTN_OK },
    { IDC_EXIT,         BTN_Exit },
    { 0,                0 }};

static CAMNLS   nlsNoResMgDlg[] = {
    { IDC_TEXT1,        DLG_NoUsbRes1 },
    { IDC_TEXT2,        DLG_NoUsbRes2 },
    { IDC_TEXT3,        DLG_NoUsbRes3 },
    { IDC_DONOTSHOW,    DLG_DoNotShow },
    { IDC_CONTINUE,     BTN_Continue },
    { IDC_EXIT,         BTN_Exit },
    { 0,                0 }};

static CAMNLS  nlsDirDlg[] = {
    { IDC_DIREFTXT,     DLG_ChooseDir },
    { DID_DRIVE_TXT,    DLG_Drive },
    { DID_DIRECTORY_TXT, DLG_Directory },
    { IDC_DIRMSG1,      DLG_DirDlgMsg1 },
    { IDC_DIRMSG2,      DLG_DirDlgMsg2 },
    { DID_OK_PB,        BTN_OK },
    { DID_CANCEL_PB,    BTN_Cancel },
    { 0,                0 }};


/****************************************************************************/

BOOL        UtilDoStartupTests( void)

{
    BOOL    fRtn;

do {
    UtilLoadNLS();

    fRtn = UtilCheckVersion();
    if (!fRtn)
        break;

    fRtn = UtilTestUsbResMg();
    if (!fRtn)
        break;

    UtilShowUsbcallsPath();

} while (0);

    return (fRtn);
}

/****************************************************************************/
/****************************************************************************/

BOOL        UtilCheckVersion( void)

{
    BOOL    fRtn = TRUE;
    ULONG   ul = 0;
    ULONG   ulVer = 0;

do {
    // see if the Groups ini-app exists;  if not, display the Intro dialog;
    if (!PrfQueryProfileSize( HINI_USERPROFILE, CAMINI_GRPAPP, 0, &ul) ||
        ul == 0)
        if (WinDlgBox( HWND_DESKTOP, 0, UtilIntroDlg,
                       NULLHANDLE, IDD_INTRODLG, NULL) == IDC_EXIT) {
            fRtn = FALSE;
            break;
    }

    // if this is the correct version, exit
    ul = sizeof(ulVer);
    if (PrfQueryProfileData( HINI_USERPROFILE, CAMINI_APP,
                             CAMINI_VERSION, &ulVer, &ul) &&
        ul == sizeof(ulVer) && ulVer == CAMVERSION)
        break;

    // delete any existing CAMINI_APP, then save the current version
    ulVer = CAMVERSION;
    PrfWriteProfileData( HINI_USERPROFILE, CAMINI_APP, 0, 0, 0);
    PrfWriteProfileData( HINI_USERPROFILE, CAMINI_APP,
                         CAMINI_VERSION, &ulVer, sizeof(ulVer));

} while (0);

    return (fRtn);
}

/****************************************************************************/

MRESULT _System UtilIntroDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{

    if (msg == WM_INITDLG) {
        LoadDlgStrings( hwnd, nlsIntro);
        WinSendDlgItemMsg( hwnd, IDC_INTROPATH, EM_SETTEXTLIMIT,
                           (MP)CCHMAXPATH-1, 0);
        UtilSetIntroPath( hwnd);
        ShowDialog( hwnd, 0);
        return 0;
    }

    if ((msg == WM_COMMAND && SHORT1FROMMP( mp1) != IDC_EXIT) ||
        (msg == WM_SYSCOMMAND && SHORT1FROMMP( mp1) == SC_CLOSE)) {

        char *  ptr;
        char    szPath[CCHMAXPATH];

        if (SHORT1FROMMP( mp1) == IDC_INTROFIND) {
            WinQueryDlgItemText( hwnd, IDC_INTROPATH, sizeof(szPath), szPath);
            PathDlg( hwnd, szPath);
            WinSetDlgItemText( hwnd, IDC_INTROPATH, szPath);
            return 0;
        }

        WinQueryDlgItemText( hwnd, IDC_INTROPATH, sizeof(szPath), szPath);

        ptr = szPath;
        while (*ptr && *ptr == 0x20)
            ptr++;

        if (!*ptr) {
            UtilSetIntroPath( hwnd);
            WinAlarm( HWND_DESKTOP, WA_ERROR);
            return 0;
        }

        if (CreatePath( szPath)) {
            WinAlarm( HWND_DESKTOP, WA_ERROR);
            return 0;
        }

        strcpy( GetDefaultGroup( 0)->path, szPath);
    }

    return (WinDefDlgProc( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

void        UtilSetIntroPath( HWND hwnd)

{
    ULONG   ulBoot = 3;
    char *  ptr;
    char    szPath[CCHMAXPATH];

    strcpy( szPath, ".");
    if (ValidatePath( szPath)) {
        DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &ulBoot, sizeof(ulBoot));
        ulBoot += 0x40;
        ptr = szPath;
        *ptr++ = (char)ulBoot;
        *ptr++ = ':';
    }
    else {
        ptr = strchr( szPath, 0);
        if (ptr[-1] == '\\')
            ptr--;
    }

    strcpy( ptr, "\\photos");
    WinSetDlgItemText( hwnd, IDC_INTROPATH, szPath);

    return;
}

/****************************************************************************/
/****************************************************************************/

// This is an easy way to see usbresmg.sys is installed.
// If it isn't, DosQueryPathInfo() should return ERROR_PATH_NOT_FOUND (3).

BOOL        UtilTestUsbResMg( void)

{
    BOOL        fRtn = TRUE;
    ULONG       ul;
    ULONG       ulNoUsbErr = 0;
    FILESTATUS3 fs;

do {
    if (DosQueryPathInfo( "\\DEV\\USBRESM$", FIL_STANDARD,
                          &fs, sizeof(fs)) == ERROR_PATH_NOT_FOUND) {
        printf( "DosQueryPathInfo \\DEV\\USBRESM$ not found\n");
        SetGlobalFlag( CAMFLAG_USB, FALSE);
    }
    else {
        SetGlobalFlag( CAMFLAG_USB, TRUE);
        break;
    }

    ul = sizeof(ulNoUsbErr);
    if (!PrfQueryProfileData( HINI_USERPROFILE, CAMINI_APP, CAMINI_NOUSBERR,
                              &ulNoUsbErr, &ul) || ul != sizeof(ulNoUsbErr))
        ulNoUsbErr = 0;

    if (!ulNoUsbErr &&
        WinDlgBox( HWND_DESKTOP, 0, UtilNoUsbResMgDlg,
                   0, IDD_NOUSB, 0) == IDC_EXIT)
        fRtn = 0;

} while (0);

    return (fRtn);
}

/****************************************************************************/

MRESULT _System UtilNoUsbResMgDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    if (msg == WM_INITDLG) {
        LoadDlgStrings( hwnd, nlsNoResMgDlg);
        ShowDialog( hwnd, 0);
        return (0);
    }

    if (msg == WM_COMMAND || msg == WM_SYSCOMMAND)
        if (WinSendDlgItemMsg( hwnd, IDC_DONOTSHOW, BM_QUERYCHECK, 0, 0)) {
            ULONG   ulNoUsbErr = TRUE;

            PrfWriteProfileData( HINI_USERPROFILE, CAMINI_APP, CAMINI_NOUSBERR,
                                 &ulNoUsbErr, sizeof(ulNoUsbErr));
        }

    return (WinDefDlgProc( hwnd, msg, mp1, mp2));
}

/****************************************************************************/
/****************************************************************************/

void        UtilShowUsbcallsPath( void)

{
    HMODULE     hmod;
    char        szPath[CCHMAXPATH];

    if (!DosQueryModuleHandle( "USBCALLS", &hmod) &&
        !DosQueryModuleName( hmod, sizeof(szPath), szPath))
        printf( "using %s\n", szPath);
    else
        printf( "unable to get path for USBCALLS.DLL\n");

    return;
}

/****************************************************************************/
/****************************************************************************/

BOOL        PathDlg( HWND hwnd, char * pszPath)

{
    BOOL        fRtn = FALSE;
    char *      ptr;
    POINTL      ptl;
    FILEDLG     fd;

do {
    ptl.x = 0;
    ptl.y = 0;
    WinMapWindowPoints( hwnd, HWND_DESKTOP, &ptl, 1);

    memset( &fd, 0, sizeof(fd));
    fd.cbSize = sizeof(fd);
    fd.fl = FDS_OPEN_DIALOG | FDS_CUSTOM;
    fd.usDlgId = DID_FILE_DIALOG;
    fd.pfnDlgProc = &PathDlgSubProc;
    fd.x = ptl.x;
    fd.y = ptl.y;

    strcpy( fd.szFullFile, pszPath);
    ptr = strchr( fd.szFullFile, 0);
    if (ptr > fd.szFullFile && ptr[-1] == '\\')
        ptr--;
    strcpy( ptr, "\\^");

    if (!WinFileDlg( HWND_DESKTOP, hwnd, &fd)) {
        printf( "WinFileDlg failed - lSRC= %d\n", (int)fd.lSRC);
        break;
    }
    if (fd.lReturn != DID_OK)
        break;

    ptr = strrchr( fd.szFullFile, '\\');
    if (!ptr)
        break;
    if (ptr[-1] == ':')
        ptr++;
    *ptr = 0;
    strcpy( pszPath, fd.szFullFile);
    fRtn = TRUE;

} while (0);

    return (fRtn);
}

/****************************************************************************/
/****************************************************************************/

MRESULT _System PathDlgSubProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
  switch (msg) {

    case WM_CONTROL:
        if (mp1 == MPFROM2SHORT( DID_DIRECTORY_LB, LN_ENTER) ||
            mp1 == MPFROM2SHORT( DID_DRIVE_CB, CBN_LBSELECT)) {

            MRESULT     mr = WinDefFileDlgProc( hwnd, msg, mp1, mp2);
            PFILEDLG    pfd = (PFILEDLG)WinQueryWindowULong( hwnd, QWL_USER);

            SetDirectoryEF( hwnd, pfd);
            return (mr);
        }
        break;

    case FDM_VALIDATE: {
        PFILEDLG    pfd = (PFILEDLG)WinQueryWindowULong( hwnd, QWL_USER);

        return ((MRESULT)ValidateDirectoryEF( hwnd, pfd));
    }

    case WM_INITDLG: {

        MRESULT     mr = WinDefFileDlgProc( hwnd, msg, mp1, mp2);
        PFILEDLG    pfd = (PFILEDLG)WinQueryWindowULong( hwnd, QWL_USER);

        LoadDlgStrings( hwnd, nlsDirDlg);
        WinSendDlgItemMsg( hwnd, IDC_DIREF, EM_SETTEXTLIMIT,
                           (MP)(CCHMAXPATH - 1), 0);
        SetDirectoryEF( hwnd, pfd);
        return (mr);
    }

    case WM_FOCUSCHANGE:
        if (SHORT1FROMMP(mp2) && !WinIsWindowEnabled( hwnd)) {
            WinPostMsg( WinQueryWindow( hwnd, QW_OWNER),
                        CAMMSG_FOCUSCHANGE, 0, 0);
            return (0);
        }
        break;

  } // end switch()

    return (WinDefFileDlgProc( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

BOOL        ValidateDirectoryEF( HWND hwnd, PFILEDLG pfd)

{
    char *      ptr;
    char        szText[CCHMAXPATH];

    if (!WinQueryDlgItemText( hwnd, IDC_DIREF, sizeof(szText) - 2, szText)) {
        SetDirectoryEF( hwnd, pfd);
        return (FALSE);
    }

    if (strchr( szText, '*') || strchr( szText, '?'))
        return (FALSE);

    if (CreatePath( szText)) {
        WinSetDlgItemText( hwnd, IDC_DIREF, szText);
        return (FALSE);
    }

    ptr = strchr( szText, 0);
    if (ptr[-1] != '\\')
        *ptr++ = '\\';
    *ptr++ = '^';
    *ptr = 0;

    strcpy( pfd->szFullFile, szText);

    return (TRUE);
}

/****************************************************************************/

void        SetDirectoryEF( HWND hwnd, PFILEDLG pfd)

{
    char        chSave;
    char *      ptr;

    ptr = strrchr( pfd->szFullFile, '\\');
    if (ptr) {
        if (ptr[-1] == ':')
            ptr++;
        chSave = *ptr;
        *ptr = 0;
        WinSetDlgItemText( hwnd, IDC_DIREF, pfd->szFullFile);
        *ptr = chSave;
    }

    return;
}

/****************************************************************************/
/****************************************************************************/

APIRET      CreatePath( char * pszPath)

{
    APIRET      rc = ERROR_PATH_NOT_FOUND;
    char *      ptr;
    char *  	pStart;
    char *  	pEnd;
    FILESTATUS3 fs;
    char        szPath[CCHMAXPATH];

do {
    if (!pszPath || !*pszPath)
        break;

    pStart = pszPath;
    while (*pStart && *pStart == 0x20)
        pStart++;
    if (!*pStart)
        break;

    ptr = strchr( pStart, '\0');
    if (ptr[-1] == '\\' && &ptr[-1] > pStart && ptr[-2] != ':')
        ptr[-1] = '\0';

    rc = DosQueryPathInfo( pStart, FIL_QUERYFULLNAME, szPath, sizeof(szPath));
    if (rc)
        break;

    pEnd = strchr( szPath, '\0');
    ptr = pEnd;

    do {
        *ptr = '\0';

        rc = DosQueryPathInfo( szPath, FIL_STANDARD, &fs, sizeof(fs));
        if (!rc) {
            if (!(fs.attrFile & FILE_DIRECTORY))
                rc = ERROR_FILE_EXISTS;
            break;
        }

        ptr = strrchr( szPath, '\\');

    } while (ptr > &szPath[2]);

    if (ptr == &szPath[2])
        rc = 0;
    else
        if (ptr < &szPath[2])
            rc = ERROR_PATH_NOT_FOUND;

    if (rc)
        break;

    while (ptr < pEnd)
    {
        *ptr = '\\';
        rc = DosCreateDir( szPath, NULL);
        if (rc)
            break;
        printf( "Created directory '%s'\n", szPath);
        ptr = strchr( ptr, '\0');
    }

} while (0);

    if (!rc)
        strcpy( pszPath, szPath);

    return (rc);
}

/****************************************************************************/

APIRET      ValidatePath( char * pszPath)

{
    APIRET      rc = ERROR_PATH_NOT_FOUND;
    char *  	ptr;
    char *  	pStart;
    FILESTATUS3 fs;
    char        szTemp[CCHMAXPATH];

do {
    if (!pszPath || !*pszPath)
        break;

    pStart = pszPath;
    while (*pStart && *pStart == 0x20)
        pStart++;
    if (!*pStart)
        break;

    ptr = strchr( pStart, '\0');
    if (ptr[-1] == '\\' && &ptr[-1] > pStart && ptr[-2] != ':')
        ptr[-1] = '\0';

    rc = DosQueryPathInfo( pStart, FIL_QUERYFULLNAME, szTemp, sizeof(szTemp));
    if (rc)
        break;

    strcpy( pszPath, szTemp);
    rc = DosQueryPathInfo( pszPath, FIL_STANDARD, &fs, sizeof(fs));
    if (!rc && !(fs.attrFile & FILE_DIRECTORY))
        rc = ERROR_FILE_EXISTS;

} while (0);

    return (rc);
}

/****************************************************************************/
/****************************************************************************/

void        UtilLoadNLS( void)

{
    APIRET  rc;
    BOOL    fOK = FALSE;
    ULONG * pulVer;
    PSZ     pszLang;
    char    szLang[4];
    char    szName[CCHMAXPATH];
    char    szDLL[32];

    if (!DosScanEnv( "LANG", &pszLang) &&
        pszLang[0] && pszLang[1] && pszLang[2] == '_') {
        szLang[0] = pszLang[0];
        szLang[1] = pszLang[1];
        szLang[2] = 0;
        fOK = TRUE;
        printf( "found language in env - lang= %s\n", szLang);
    }
    else
        printf( "couldn't find language\n");

    if (fOK) {
        strcpy( szDLL, ".\\cam_");
        strcat( szDLL, szLang);
        strcat( szDLL, ".dll");
        rc = DosLoadModule( szName, sizeof(szName), szDLL, &hmodNLS);

        if (rc) {
            fOK = FALSE;
            // don't display an error for cam_en.dll since it shouldn't exist
            if (stricmp( szLang, "en"))
                printf( "unable to load %s - rc= %d  err= %s\n", szDLL, (int)rc, szName);
        }
        else
            printf( "successfully loaded %s - hmod= %x\n", szDLL, (int)hmodNLS);
    }

    if (fOK) {
        rc = DosQueryProcAddr( hmodNLS, 1, 0, (PFN*)&pulVer);
        if (rc) {
            fOK = FALSE;
            printf( "unable to load address of NLS version - rc= %d\n", (int)rc);
        }
        else
            if (*pulVer != CAMNLSVER) {
                fOK = FALSE;
                printf( "wrong NLS version - expected 0x%x, got 0x%x\n",
                        (int)CAMNLSVER, (int)*pulVer);
            }

        if (!fOK)
            DosFreeModule( hmodNLS);
    }

    if (!fOK)
        hmodNLS = 0;

    return;
}

/****************************************************************************/

char *      LoadString( ULONG str, char * pszIn)

{
    char *  ptr;
    char *  pszRtn;
    char    szText[256];

    pszRtn = pszIn ? pszIn : szText;
    if (!WinLoadString( 0, hmodNLS, str, 256, pszRtn)) {
        printf( "LoadString error:  str= %x\n", (int)str);
        strcpy( pszRtn, "???");
    }

    ptr = strchr( pszRtn, '|');
    if (ptr)
        *ptr = '\n';

    if (!pszIn)
        pszRtn = strdup( szText);

    return (pszRtn);
}

/****************************************************************************/

void        LoadDlgStrings( HWND hwnd, CAMNLSPtr pnls)

{
    char *      ptr;
    char        szText[256];

    while (pnls->id) {
        if (!WinLoadString( 0, hmodNLS, pnls->str, 256, szText)) {
            printf( "LoadDlgStrings error:  str= %x\n", (int)pnls->str);
            strcpy( szText, "???");
        }

        ptr = strchr( szText, '|');
        if (ptr)
            *ptr = '\n';

        WinSetDlgItemText( hwnd, pnls->id, szText);
        pnls++;
    }

    return;
}

/****************************************************************************/

void        LoadDlgItemString( HWND hwnd, ULONG ulID, ULONG ulStr)

{
    char *      ptr;
    char        szText[256];

    if (!WinLoadString( 0, hmodNLS, ulStr, 256, szText)) {
        printf( "LoadDlgItemString error:  str= 0x%lx\n", ulStr);
        strcpy( szText, "???");
    }

    ptr = szText;
    while ((ptr = strchr( ptr, '|')) != 0)
       *ptr = '\n';

    WinSetDlgItemText( hwnd, ulID, szText);

    return;
}

/****************************************************************************/

char *      LoadStringNoTilde( ULONG str, char * pszIn)

{
    char *  pRtn;
    char *  ptr;

    pRtn = LoadString( str, pszIn);
    ptr = strchr( pRtn, '~');
    if (ptr)
        strcpy( ptr, ptr + 1);

    return (pRtn);
}

/****************************************************************************/

void        LoadStaticStrings( CAMNLSPSZ * pnls)

{
    while (pnls->ppsz) {
        *pnls->ppsz = LoadString( pnls->str, 0);
        pnls++;
    }

    return;
}

/****************************************************************************/

