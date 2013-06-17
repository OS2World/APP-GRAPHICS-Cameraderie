/****************************************************************************/
// camsave.c
//
//  - save & delete files using container data
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
#include "camusb.h"
#include "cammisc.h"

/****************************************************************************/

typedef struct _CAMSaveErase {
    HEV         hEvent;
    HWND        hReply;
    CAMPtr      thisCam;
    CAMRecPtr   pcr;
    BOOL        fTerm;
    BOOL        fStop;
    ULONG       ulErr;
    ULONG       ulOp;
    char        szPath[CCHMAXPATH];
} CAMSaveErase;

typedef CAMSaveErase *CAMSaveErasePtr;

/****************************************************************************/

typedef struct _CAMSaveFile {
    CAMRecPtr       pcr;
    BOOL            fHalt;
    HWND            hCnr;
    HWND            hBtn1;
    HWND            hBtn2;
    HWND            hBtn3;
    HWND            hBtn4;
    HWND            hEdit;
    HWND            hChk;
    ULONG           cntSave;
    ULONG           cbSave;
    ULONG           cntErase;
    ULONG           cbErase;
    ULONG           flags;
    CAMSaveErasePtr pse;
    char            szPath[524];
} CAMSaveFile;

typedef CAMSaveFile *CAMSaveFilePtr;

/****************************************************************************/

#define CAMSAVE_REPONE      0x1
#define CAMSAVE_REPALL      0x2
#define CAMSAVE_REPMASK     0x3
#define CAMSAVE_DATESUB     0x4
#define CAMSAVE_RETRY       0x10
#define CAMSAVE_EDIT        0x20
#define CAMSAVE_ERRMASK     0x30
#define CAMSAVE_SAVE        0x100
#define CAMSAVE_SAVEERASE   0x200
#define CAMSAVE_SAVEMARK    0x400
#define CAMSAVE_ERASE       0x800
#define CAMSAVE_SAVEOK      0x1000
#define CAMSAVE_ERASEOK     0x2000
#define CAMSAVE_OPEN        0x4000
#define CAMSAVE_EXIT        0x8000

#define CAMSAVE_RC_INIT        ((ULONG)-1)
#define CAMSAVE_RC_OK          0
#define CAMSAVE_RC_TOOLONG     1
#define CAMSAVE_RC_FILEEXISTS  2
#define CAMSAVE_RC_BADPATH     3
#define CAMSAVE_RC_GETDATA     4
#define CAMSAVE_RC_DOSOPEN     5
#define CAMSAVE_RC_DOSWRITE    6
#define CAMSAVE_RC_DELDATA     7
#define CAMSAVE_RC_BADCREATE   8
#define CAMSAVE_RC_CANCELLED   9
#define CAMSAVE_RC_DONE        10
#define CAMSAVE_RC_FATAL       11

/****************************************************************************/

// this struct combines an FEA2LIST, an FEA2, plus additional fields
// needed to write a .SUBJECT EA in the correct EAT_ASCII format;

#pragma pack(1)
    typedef struct _CAMSubjectEA {
        struct {
            ULONG   ulcbList;
            ULONG   uloNextEntryOffset;
            BYTE    bfEA;
            BYTE    bcbName;
            USHORT  uscbValue;
            char    chszName[sizeof(".SUBJECT")];
        } hdr;
        struct {
            USHORT  usEAType;
            USHORT  usDataLth;
        } info;
        char    data[32];
    } CAMSubjectEA;
#pragma pack()

/****************************************************************************/

void            SaveChanges( HWND hwnd, ULONG ulMsg);
MRESULT _System ConfirmSaveDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
LONG            InitConfirmSave( HWND hwnd, MPARAM mp2);
void            CloseConfirmSave( HWND hwnd, ULONG ulBtn);
MRESULT _System SaveFilesDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
LONG            InitSaveFiles( HWND hwnd, MPARAM mp2);
CAMSaveErasePtr InitSaveEraseThread( HWND hwnd);
void            SaveNextFile( HWND hwnd, CAMSaveFilePtr pCSF);
void            DoneSavingFiles( HWND hwnd, CAMSaveFilePtr pCSF);
void            EraseFile( HWND hwnd, CAMSaveFilePtr pCSF);
void            SaveFile( HWND hwnd, CAMSaveFilePtr pCSF);
void            SaveEraseReply( HWND hwnd, MPARAM mp1);
void            SetSaveControls( HWND hwnd, CAMSaveFilePtr pCSF, ULONG ulErr);
ULONG           BuildFilename( CAMRecPtr pcr, CAMSaveFilePtr pCSF);
BOOL            InsertDate( CAMRecPtr pcr, char * pszName);
void            CreateSavePath( HWND hwnd, CAMSaveFilePtr pCSF);
ULONG           NextMarkedRec( HWND hCnr, CAMRecPtr * ppcrIn, ULONG flags);

void            SaveEraseThreadProc( void * saveeraseptr);
ULONG           PtpSaveObject( CAMSaveErasePtr pse);
uint32_t        PtpSaveObjectAsIs( CAMSaveErasePtr pse, HFILE hFile);
uint32_t        PtpSaveRotatedObject( CAMSaveErasePtr pse, HFILE hFile);
uint32_t        PtpEraseObject( CAMSaveErasePtr pse);
void            BuildSubjectEA( CAMRecPtr pcr, CAMSubjectEA * pEA);
ULONG       	SetFileDate( HFILE hFile, char * pszFile, CAMRecPtr pcr);

ULONG       	MsdSaveObject( CAMSaveErasePtr pse);
ULONG       	MsdSaveObjectAsIs( CAMSaveErasePtr pse);
ULONG           MsdSaveRotatedObject( CAMSaveErasePtr pse);
ULONG           MsdEraseObject( CAMSaveErasePtr pse);


/****************************************************************************/

typedef union _LRGB {
    LONG    l;
    RGB     rgb;
} LRGB;

static LRGB     lrgbEditBkgd        = {0x00ffffff};
static LRGB     lrgbWhite           = {0x00ffffff};

static BOOL     fInitSaveFilesMsgs  = FALSE;
static char     szX[]               = "xXx";

static char *   pszMSG_Erasing      = szX;
static char *   pszMSG_Saving       = szX;
static char *   pszMSG_SavedErased  = szX;
static char *   pszBTN_Retry        = szX;
static char *   pszBTN_Skip         = szX;
static char *   pszBTN_Stop         = szX;
static char *   pszBTN_Close        = szX;
static char *   pszBTN_Replace      = szX;
static char *   pszBTN_Create       = szX;
static char *   pszSUB_Year         = szX;
static char *   pszSUB_Month        = szX;
static char *   pszSUB_Day          = szX;

static CAMNLSPSZ nlsSaveFilesMsgs[] = {
    { &pszMSG_Erasing,      MSG_Erasing },
    { &pszMSG_Saving,       MSG_Saving },
    { &pszMSG_SavedErased,  MSG_SavedErased },
    { &pszBTN_Retry,        BTN_Retry },
    { &pszBTN_Skip,         BTN_Skip },
    { &pszBTN_Stop,         BTN_Stop },
    { &pszBTN_Close,        BTN_Close },
    { &pszBTN_Replace,      BTN_Replace },
    { &pszBTN_Create,       BTN_Create },
    { &pszSUB_Year,         SUB_Year },
    { &pszSUB_Month,        SUB_Month },
    { &pszSUB_Day,          SUB_Day },
    { 0,                    0 }};

static CAMNLS   nlsSaveDlg[] = {
    { IDC_SAVECHGS,         DLG_SaveChanges },
    { IDC_SAVEDONT,         DLG_DontErase },
    { IDC_SAVELATER,        DLG_EraseLater },
    { IDC_SAVEAFTER,        DLG_EraseAfterSave },
    { IDC_YES,              BTN_Yes },
    { IDC_NO,               BTN_No },
    { IDC_CANCEL,           BTN_Cancel },
    { 0,                    0 }};

static CAMNLS   nlsSaveFilesDlg[] = {
    { IDC_REPLACEALL,       DLG_ReplaceAll },
    { 0,                    0 }};

/****************************************************************************/
/****************************************************************************/

void        SaveChanges( HWND hwnd, ULONG ulMsg)

{
    STATSTUFF *     pss;

    pss = GetStatStuff();

    if (pss->ulSavCnt || pss->ulDelCnt)
        OpenWindow( hwnd, (ulMsg ? IDD_SAVEEXIT : IDD_SAVE), ulMsg);
    else
        if (ulMsg)
            WinPostMsg( hwnd, ulMsg, 0, 0);

    return;
}

/****************************************************************************/

MRESULT _System ConfirmSaveDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    switch (msg)
    {
        case WM_COMMAND:
            CloseConfirmSave( hwnd, (ULONG)SHORT1FROMMP( mp1));
            return 0;

        case WM_CONTROL:
            if (mp1 == MPFROM2SHORT( IDC_SAVENOW, BN_CLICKED)) {
                USHORT  usChk;

                usChk = (USHORT)(ULONG)WinSendMsg( (HWND)mp2, BM_QUERYCHECK, 0, 0);
                WinEnableWindow( WinWindowFromID( hwnd, IDC_SAVEAFTER), usChk);
                WinEnableWindow( WinWindowFromID( hwnd, IDC_SAVELATER), usChk);
                WinEnableWindow( WinWindowFromID( hwnd, IDC_SAVEDONT), usChk);
            }
            return 0;

        case WM_FOCUSCHANGE:
            if (SHORT1FROMMP(mp2) && !WinIsWindowEnabled( hwnd)) {
                WinPostMsg( WinQueryWindow( hwnd, QW_OWNER),
                            CAMMSG_FOCUSCHANGE, 0, 0);
                return (0);
            }
            break;

        case WM_INITDLG:
            InitConfirmSave( hwnd, mp2);
            return 0;

        case WM_CLOSE:
            CloseConfirmSave( hwnd, 0);
            return (0);

        case WM_SAVEAPPLICATION:
            WinStoreWindowPos( CAMINI_APP, CAMINI_SAVEPOS, hwnd);
            break;

    } //end switch (msg)

    return (WinDefDlgProc( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

LONG        InitConfirmSave( HWND hwnd, MPARAM mp2)

{
    STATSTUFF *     pss;
    ULONG           cnt;
    ULONG           ulSaveErase;
    HWND            hTemp;
    char            szText[128];
    char            szFmt[128];

    WinSetWindowULong( hwnd, QWL_USER, (ULONG)mp2);
    pss = GetStatStuff();
    LoadDlgStrings( hwnd, nlsSaveDlg);

    hTemp = WinWindowFromID( hwnd, IDC_ERASENOW);
    LoadString( DLG_Erase, szFmt);
    sprintf( szText, szFmt, (int)pss->ulDelCnt, (int)(pss->ulDelSize/1024));
    WinSetWindowText( hTemp, (PCSZ)szText);
    if (pss->ulDelCnt)
        WinSendMsg( hTemp, BM_SETCHECK, (MP)1, 0);
    else
        WinEnableWindow( hTemp, FALSE);

    hTemp = WinWindowFromID( hwnd, IDC_SAVENOW);
    LoadString( DLG_Save, szFmt);
    sprintf( szText, szFmt, (int)pss->ulSavCnt, (int)(pss->ulSavSize/1024));
    WinSetWindowText( hTemp, (PCSZ)szText);

    if (pss->ulSavCnt == 0) {
        WinEnableWindow( hTemp, FALSE);
        WinEnableWindow( WinWindowFromID( hwnd, IDC_SAVEAFTER), FALSE);
        WinEnableWindow( WinWindowFromID( hwnd, IDC_SAVELATER), FALSE);
        WinEnableWindow( WinWindowFromID( hwnd, IDC_SAVEDONT), FALSE);
    }
    else {
        WinSendMsg( hTemp, BM_SETCHECK, (MP)1, 0);

        ulSaveErase = 0;
        cnt = sizeof(ULONG);
        if (!PrfQueryProfileData( HINI_USERPROFILE, CAMINI_APP,
                                  CAMINI_SAVERASE, &ulSaveErase, &cnt) ||
            !cnt || ulSaveErase > 2)
            ulSaveErase = 0;

        if (WinQueryWindowUShort( hwnd, QWS_ID) == IDD_SAVEEXIT) {
            WinEnableWindow( WinWindowFromID( hwnd, IDC_SAVELATER), FALSE);
            if (ulSaveErase == 1)
                ulSaveErase = 0;
        }

        WinSendDlgItemMsg( hwnd, IDC_SAVEOPTS+ulSaveErase, BM_SETCHECK,
                           (MP)1, 0);
    }

    ShowDialog( hwnd, CAMINI_SAVEPOS);

    return 0;
}

/****************************************************************************/

void        CloseConfirmSave( HWND hwnd, ULONG ulBtn)

{
    HWND    hMain;
    HWND    hSave;
    ULONG   ulMsg;
    ULONG   ulSaveErase = 0;
    ULONG   flags = 0;

do {
    hMain = WinQueryWindow( hwnd, QW_OWNER);
    ulMsg = WinQueryWindowULong( hwnd, QWL_USER);

    if (ulBtn == IDC_NO)
        break;

    if (ulBtn != IDC_YES) {
        ulMsg = 0;
        break;
    }

    if (WinSendDlgItemMsg( hwnd, IDC_SAVENOW, BM_QUERYCHECK, 0, 0)) {
        flags |= CAMSAVE_SAVE;

        if (WinSendDlgItemMsg( hwnd, IDC_SAVEAFTER, BM_QUERYCHECK, 0, 0)) {
            flags |= CAMSAVE_SAVEERASE;
            ulSaveErase = 2;
        }
        else
        if (WinSendDlgItemMsg( hwnd, IDC_SAVELATER, BM_QUERYCHECK, 0, 0)) {
            flags |= CAMSAVE_SAVEMARK;
            ulSaveErase = 1;
        }

        PrfWriteProfileData( HINI_USER, CAMINI_APP, CAMINI_SAVERASE,
                             &ulSaveErase, sizeof(ulSaveErase));
    }

    if (WinSendDlgItemMsg( hwnd, IDC_ERASENOW, BM_QUERYCHECK, 0, 0))
        flags |= CAMSAVE_ERASE;

    if (!flags)
        break;

    if (ulMsg == CAMMSG_EXIT)
        flags |= CAMSAVE_EXIT;
    else
    if (ulMsg == CAMMSG_OPEN)
        flags |= CAMSAVE_OPEN;

    hSave = OpenWindow( hMain, IDD_SAVEFILE, flags);
    if (hSave) {
        SetFileSaveSortOrder( hMain, TRUE);
        WinPostMsg( hSave, CAMMSG_STARTSAVE, 0, 0);
        ulMsg = 0;
    }

} while (0);

    if (ulMsg)
        WinPostMsg( hMain, ulMsg, 0, 0);

    WindowClosed( (ULONG)WinQueryWindowUShort( hwnd, QWS_ID));
    WinDestroyWindow( hwnd);

    return;
}

/****************************************************************************/
/****************************************************************************/

// this is an exercise in non-modality (i.e. no annoying popup windows)

MRESULT _System SaveFilesDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{

    switch (msg)
    {
        case WM_COMMAND: {
            CAMSaveFilePtr  pCSF;

            pCSF = (CAMSaveFilePtr)WinQueryWindowULong( hwnd, QWL_USER);
            if (!pCSF) {
                WinSendMsg( hwnd, WM_CLOSE, 0, 0);
                return 0;
            }

            switch (SHORT1FROMMP( mp1)) {

                case IDC_NO:
                    WinSendMsg( hwnd, WM_CLOSE, 0, 0);
                    break;

                case IDC_NEXT:
                    SaveNextFile( hwnd, pCSF);
                    break;

                case IDC_RETRY:
                    SaveFile( hwnd, pCSF);
                    break;

                case IDC_RETRYDEL:
                    EraseFile( hwnd, pCSF);
                    break;

                case IDC_CREATE:
                    CreateSavePath( hwnd, pCSF);
                    break;

                case IDC_REPLACE:
                    pCSF->flags |= CAMSAVE_REPONE;
                    SaveFile( hwnd, pCSF);
                    break;

                case IDC_STOP:
                    DoneSavingFiles( hwnd, pCSF);
                    break;

                case IDC_HALT:
                    if (pCSF->pse) {
                        pCSF->pse->fStop = TRUE;
                        pCSF->fHalt = TRUE;
                    }
                    break;
            }
            return (0);
        }

        case CAMMSG_SAVEERASE:
            SaveEraseReply( hwnd, mp1);
            return (0);

        case CAMMSG_STARTSAVE: {
            CAMSaveFilePtr  pCSF;

            pCSF = (CAMSaveFilePtr)WinQueryWindowULong( hwnd, QWL_USER);
            if (!pCSF) {
                WinSendMsg( hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            pCSF->pcr = 0;
            ShowTime( "SaveFile", FALSE);
            SaveNextFile( hwnd, pCSF);
            return 0;
        }

        case WM_CONTROL:
            if (mp1 == MPFROM2SHORT( IDC_REPLACEALL, BN_CLICKED)) {
                CAMSaveFilePtr  pCSF;

                pCSF = (CAMSaveFilePtr)WinQueryWindowULong( hwnd, QWL_USER);
                if (!pCSF) {
                    WinSendMsg( hwnd, WM_CLOSE, 0, 0);
                    return 0;
                }
                if (WinSendMsg( (HWND)mp2, BM_QUERYCHECK, 0, 0))
                    pCSF->flags |= CAMSAVE_REPALL;
                else
                    pCSF->flags &= ~CAMSAVE_REPALL;
            }
            return 0;

        case WM_FOCUSCHANGE:
            if (SHORT1FROMMP(mp2) && !WinIsWindowEnabled( hwnd)) {
                WinPostMsg( WinQueryWindow( hwnd, QW_OWNER),
                            CAMMSG_FOCUSCHANGE, 0, 0);
                return (0);
            }
            break;

        case WM_CLOSE: {
            HWND    hOwner = WinQueryWindow( hwnd, QW_OWNER);

            SetFileSaveSortOrder( hOwner, FALSE);
            WindowClosed( IDD_SAVEFILE);
            WinDestroyWindow( hwnd);
            return (0);
        }

        case WM_DESTROY: {
            CAMSaveFilePtr  pCSF;
            HWND            hOwner;

            hOwner = WinQueryWindow( hwnd, QW_OWNER);
            pCSF = (CAMSaveFilePtr)WinQueryWindowULong( hwnd, QWL_USER);
            if (pCSF) {
                if (pCSF->flags & CAMSAVE_EXIT)
                    WinPostMsg( hOwner, CAMMSG_EXIT, 0, 0);
                else
                if (pCSF->flags & CAMSAVE_OPEN)
                    WinPostMsg( hOwner, CAMMSG_OPEN, 0, 0);

                if (pCSF->pse) {
                    pCSF->pse->fTerm = TRUE;
                    DosPostEventSem( pCSF->pse->hEvent);
                }

                free( pCSF);
            }
            break;
        }

        case WM_INITDLG:
            return ((MRESULT)InitSaveFiles( hwnd, mp2));

        case WM_SAVEAPPLICATION:
            WinStoreWindowPos( CAMINI_APP, CAMINI_FILEPOS, hwnd);
            break;

    } //end switch (msg)

    return (WinDefDlgProc( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

LONG        InitSaveFiles( HWND hwnd, MPARAM mp2)

{
    ULONG           ul;
    HWND            hOwner;
    CAMSaveFilePtr  pCSF = 0;

    pCSF = malloc( sizeof(CAMSaveFile));
    if (!pCSF)
        return (-1);

    memset( pCSF, 0, sizeof(CAMSaveFile));

    pCSF->pse = InitSaveEraseThread( hwnd);
    if (!pCSF->pse) {
        free( pCSF);
        return (-1);
    }

    WinSetWindowULong( hwnd, QWL_USER, (ULONG)pCSF);


    hOwner = WinQueryWindow( hwnd, QW_OWNER);
    pCSF->flags = (ULONG)mp2;
    pCSF->hCnr  = WinWindowFromID( hOwner, FID_CLIENT);
    pCSF->hBtn1 = WinWindowFromID( hwnd, IDC_BTN1);
    pCSF->hBtn2 = WinWindowFromID( hwnd, IDC_BTN2);
    pCSF->hBtn3 = WinWindowFromID( hwnd, IDC_BTN3);
    pCSF->hBtn4 = WinWindowFromID( hwnd, IDC_BTN4);
    pCSF->hChk  = WinWindowFromID( hwnd, IDC_REPLACEALL);

    pCSF->hEdit = WinWindowFromID( hwnd, IDC_TEXT2);
    WinSendMsg( pCSF->hEdit, EM_SETTEXTLIMIT, (MP)259, 0);
    if (!WinQueryPresParam( hwnd, PP_BACKGROUNDCOLOR, 0, &ul,
                            sizeof(RGB), &lrgbEditBkgd.rgb, 0))
        lrgbEditBkgd.l = WinQuerySysColor( HWND_DESKTOP,
                                           SYSCLR_DIALOGBACKGROUND, 0);

    if (!fInitSaveFilesMsgs) {
        LoadStaticStrings( nlsSaveFilesMsgs);
        fInitSaveFilesMsgs = TRUE;
    }
    LoadDlgStrings( hwnd, nlsSaveFilesDlg);
    SetSaveControls( hwnd, pCSF, CAMSAVE_RC_INIT);
    ShowDialog( hwnd, CAMINI_FILEPOS);

    return (0);
}

/****************************************************************************/

// setup to save/erase:  create a secondary thread to do the work,
// an event semaphore to trigger the thread, & a buffer to pass data;

CAMSaveErasePtr InitSaveEraseThread( HWND hwnd)

{
    BOOL            fRtn = FALSE;
    int             tid = 0;
    char *          pErr = 0;
    CAMSaveErasePtr pse = 0;

do {
    // create the buffer
    pse = (CAMSaveErasePtr)malloc( sizeof(CAMSaveErase));
    if (!pse) {
        pErr = "malloc";
        break;
    }
    memset( pse, 0, sizeof(CAMSaveErase));

    // create an event sem
    if (DosCreateEventSem( 0, &pse->hEvent, 0, 0)) {
        pErr = "DosCreateEventSem";
        break;
    }

    // fill in useful info
    pse->hReply  = hwnd;
    pse->thisCam = (CAMPtr)GetCamPtr();

    // start the thread (which will block on the sem)
    tid = _beginthread( SaveEraseThreadProc, 0, 0xF000, pse);
    if (tid == -1) {
        pErr = "_beginthread";
        break;
    }

    fRtn = TRUE;

} while (0);

    if (!fRtn) {
        printf( "InitSaveEraseThread - %s failed\n", pErr);

        if (pse->hEvent)
            DosCloseEventSem( pse->hEvent);
        if (pse) {
            free( pse);
            pse = 0;
        }
    }

    return (pse);
}

/****************************************************************************/

void        SaveNextFile( HWND hwnd, CAMSaveFilePtr pCSF)

{
    ULONG           ulOp;

    ulOp = NextMarkedRec( pCSF->hCnr, &pCSF->pcr, pCSF->flags);

    if (pCSF->pcr) {
        SetThumbWnd( WinQueryWindow( hwnd, QW_OWNER), pCSF->pcr);

        if (pCSF->flags & CAMSAVE_ERRMASK)
            SetSaveControls( hwnd, pCSF, CAMSAVE_RC_OK);

        if (ulOp == CAMSAVE_SAVE)
            SaveFile( hwnd, pCSF);
        else
        if (ulOp == CAMSAVE_ERASE)
            EraseFile( hwnd, pCSF);
    }
    else {
        ShowTime( "SaveFile", TRUE);
        UpdateThumbWnd( WinQueryWindow( hwnd, QW_OWNER));
        DoneSavingFiles( hwnd, pCSF);
    }

    return;
}

/****************************************************************************/

void        DoneSavingFiles( HWND hwnd, CAMSaveFilePtr pCSF)

{
    HWND            hRect;
    STATSTUFF *     pss;
    char            szText[128];
    char            szFmt[128];

    pCSF->flags &= ~CAMSAVE_ERRMASK;
    pCSF->pcr = 0;
    pCSF->fHalt = FALSE;

    pss = GetStatStuff();
    SetSaveControls( hwnd, pCSF, CAMSAVE_RC_DONE);
    hRect = WinWindowFromID( hwnd, IDC_RECT);

    if (pCSF->flags & CAMSAVE_SAVEERASE)
        LoadString( MSG_SaveEraseOK, szFmt);
    else
        LoadString( MSG_SavedOK, szFmt);
    sprintf( szText, szFmt, (int)pCSF->cntSave, (int)(pCSF->cbSave/1024));
    WinSetDlgItemText( hRect, IDC_TEXT1, (PCSZ)szText);

    LoadString( MSG_NotSaved, szFmt);
    sprintf( szText, szFmt, (int)pss->ulSavCnt, (int)(pss->ulSavSize/1024));
    WinSetDlgItemText( hRect, IDC_TEXT2, (PCSZ)szText);

    LoadString( MSG_ErasedOK, szFmt);
    sprintf( szText, szFmt, (int)pCSF->cntErase, (int)(pCSF->cbErase/1024));
    WinSetDlgItemText( hRect, IDC_TEXT3, (PCSZ)szText);

    LoadString( MSG_NotErased, szFmt);
    if (pCSF->flags & CAMSAVE_SAVEMARK)
        sprintf( szText, szFmt, (int)(pss->ulDelCnt - pCSF->cntSave),
                                (int)((pss->ulDelSize - pCSF->cbSave)/1024));
    else
        sprintf( szText, szFmt, (int)pss->ulDelCnt, (int)(pss->ulDelSize/1024));
    WinSetDlgItemText( hRect, IDC_TEXT4, (PCSZ)szText);

    WinShowWindow( hRect, TRUE);
    UpdateStatus( WinQueryWindow( hwnd, QW_OWNER), 0);

    return;
}

/****************************************************************************/

void        EraseFile( HWND hwnd, CAMSaveFilePtr pCSF)

{
    CAMSaveErasePtr pse = (CAMSaveErasePtr)pCSF->pse;
    CAMRecPtr       pcr;
    char            szText[256];

do {
    pcr = pCSF->pcr;

    if (pCSF->flags & CAMSAVE_ERRMASK)
        SetSaveControls( hwnd, pCSF, CAMSAVE_RC_OK);

    sprintf( szText, pszMSG_Erasing,
             (pcr->orgname ? pcr->orgname : pcr->title));
    WinSetDlgItemText( hwnd, IDC_TEXT1, szText);
    WinSetDlgItemText( hwnd, IDC_TEXT2, "");

    if (pCSF->fHalt) {
        SetSaveControls( hwnd, pCSF, CAMSAVE_RC_CANCELLED);
        break;
    }

    pse->ulErr = 0;
    pse->fStop = FALSE;
    pse->pcr   = pcr;
    pse->ulOp  = CAMSAVE_ERASE;

    if (DosPostEventSem( pse->hEvent)) {
        printf( "EraseFile - DosPostEventSem failed\n");
        SetSaveControls( hwnd, pCSF, CAMSAVE_RC_FATAL);
        break;
    }

} while (0);

    return;
}

/****************************************************************************/

void        SaveFile( HWND hwnd, CAMSaveFilePtr pCSF)

{
    ULONG           ulErr = CAMSAVE_RC_OK;
    APIRET          rc;
    ULONG           cbPath;
    ULONG           ul;
    char *          ptr;
    CAMRecPtr       pcr;
    CAMSaveErasePtr pse = (CAMSaveErasePtr)pCSF->pse;
    FILESTATUS3     fs3;
    char            chSave;
    char            szText[128];

do {
    pcr = pCSF->pcr;

    if (pCSF->flags & CAMSAVE_RETRY) {
        if ((pCSF->flags & CAMSAVE_EDIT) &&
            WinQueryWindowText( pCSF->hEdit, sizeof(pCSF->szPath), pCSF->szPath) == 0)
            cbPath = BuildFilename( pcr, pCSF);
        else
            cbPath = strlen( pCSF->szPath);

        ul = pCSF->flags & CAMSAVE_REPONE;
        SetSaveControls( hwnd, pCSF, ulErr);
        pCSF->flags |= ul;
    }
    else {
        sprintf( szText, pszMSG_Saving,
                 (pcr->orgname ? pcr->orgname : pcr->title));
        WinSetDlgItemText( hwnd, IDC_TEXT1, szText);
        WinSetWindowUShort( pCSF->hBtn4, QWS_ID, IDC_HALT);

        cbPath = BuildFilename( pcr, pCSF);
    }

    WinSetWindowText( pCSF->hEdit, pCSF->szPath);

    if (pCSF->fHalt) {
        ulErr = CAMSAVE_RC_CANCELLED;
        break;
    }

    if (cbPath >= CCHMAXPATH) {
        ulErr = CAMSAVE_RC_TOOLONG;
        break;
    }

    rc = DosQueryPathInfo( pCSF->szPath, FIL_STANDARD, &fs3, sizeof(FILESTATUS3));
    if (!rc && !(pCSF->flags & CAMSAVE_REPMASK)) {
        ulErr = CAMSAVE_RC_FILEEXISTS;
        break;
    }

    ptr = strrchr( pCSF->szPath, '\\');
    if (ptr && ptr != pCSF->szPath) {
        if (ptr[-1] == ':')
            ptr++;
        chSave = *ptr;
        *ptr = 0;

        rc = DosQueryPathInfo( pCSF->szPath, FIL_STANDARD, &fs3,
                               sizeof(FILESTATUS3));

        if (rc && (pCSF->flags & CAMSAVE_DATESUB))
            rc = CreatePath( pCSF->szPath);

        pCSF->flags &= ~CAMSAVE_DATESUB;
        *ptr = chSave;
        if (rc) {
            ulErr = CAMSAVE_RC_BADPATH;
            break;
        }
    }

    pse->ulErr = 0;
    pse->fStop = FALSE;
    pse->pcr   = pcr;
    pse->ulOp  = CAMSAVE_SAVE;
    pse->ulOp |= (pCSF->flags & CAMSAVE_SAVEERASE) ? CAMSAVE_ERASE : 0;
    pse->ulOp |= (pCSF->flags & CAMSAVE_REPMASK);
    strcpy( pse->szPath, pCSF->szPath);

    if (DosPostEventSem( pse->hEvent)) {
        printf( "SaveFile - DosPostEventSem failed\n");
        ulErr = CAMSAVE_RC_FATAL;
        break;
    }

} while (0);

    if (ulErr)
        SetSaveControls( hwnd, pCSF, ulErr);

    return;
}

/****************************************************************************/

void        SaveEraseReply( HWND hwnd, MPARAM mp1)

{
    ULONG           ulErr = CAMSAVE_RC_OK;
    STATSTUFF *     pss;
    CAMSaveErasePtr pse = (CAMSaveErasePtr)mp1;
    CAMSaveFilePtr  pCSF;
    CAMRecPtr       pcr;

    pCSF = (CAMSaveFilePtr)WinQueryWindowULong( hwnd, QWL_USER);
    if (!pCSF || !pse) {
        WinSendMsg( hwnd, WM_CLOSE, 0, 0);
        return;
    }

do {
    pss = GetStatStuff();
    ulErr = pse->ulErr;
    pcr = pse->pcr;

    if (pse->ulOp & CAMSAVE_SAVE) {
        if (!(pse->ulOp & CAMSAVE_SAVEOK))
            break;

        pCSF->cntSave++;
        pCSF->cbSave += pcr->size;

        if (!(pse->ulOp & CAMSAVE_ERASEOK)) {
            if (pCSF->flags & CAMSAVE_SAVEMARK)
                UpdateMark( pcr, pCSF->hCnr, pss, IDM_SAVEDMARK, FALSE);
            else
                UpdateMark( pcr, pCSF->hCnr, pss, IDM_SAVED, FALSE);
        }
    }

    if (pse->ulOp & CAMSAVE_ERASE) {
        if (!(pse->ulOp & CAMSAVE_ERASEOK))
            break;

        if (pse->ulOp & CAMSAVE_SAVEOK) {
            pss->ulTotCnt--;
            pss->ulTotSize -= pcr->size;
            pss->ulSavCnt--;
            pss->ulSavSize -= pcr->size;
        }
        else {
            pCSF->cntErase++;
            pCSF->cbErase += pcr->size;
            pss->ulTotCnt--;
            pss->ulTotSize -= pcr->size;
            pss->ulDelCnt--;
            pss->ulDelSize -= pcr->size;
        }

        pCSF->pcr = (CAMRecPtr)WinSendMsg( pCSF->hCnr, CM_QUERYRECORD,
                            (MP)pcr, MPFROM2SHORT( CMA_PREV, CMA_ITEMORDER));

        if (pcr->bmp)
            GpiDeleteBitmap( pcr->bmp);
        if (pcr->orgname)
            free( pcr->orgname);
        WinSendMsg( pCSF->hCnr, CM_REMOVERECORD, (MP)&pcr,
                    MPFROM2SHORT( 1, (CMA_FREE | CMA_INVALIDATE)));
    }

} while (0);

    SetSaveControls( hwnd, pCSF, ulErr);

    if (!ulErr)
        SaveNextFile( hwnd, pCSF);

    return;
}

/****************************************************************************/

void        SetSaveControls( HWND hwnd, CAMSaveFilePtr pCSF, ULONG ulErr)

{
    BOOL    fEdit = FALSE;
    BOOL    fBtn1 = FALSE;
    BOOL    fChk  = FALSE;
    BOOL    fDel  = FALSE;
    ULONG   ulText3 = 0;
    ULONG   ulText4 = 0;
    char    szText[256];

    pCSF->flags &= ~(CAMSAVE_ERRMASK | CAMSAVE_REPONE);

    if (ulErr == CAMSAVE_RC_INIT) {
        pCSF->flags &= ~CAMSAVE_REPALL;
        WinSetWindowUShort( pCSF->hBtn1, QWS_ID, (USHORT)-1);
        WinSetWindowText( pCSF->hBtn1, "");
        WinSetWindowUShort( pCSF->hBtn2, QWS_ID, IDC_RETRY);
        WinSetWindowText( pCSF->hBtn2, pszBTN_Retry);
        WinSetWindowUShort( pCSF->hBtn3, QWS_ID, IDC_NEXT);
        WinSetWindowText( pCSF->hBtn3, pszBTN_Skip);
        WinSetWindowUShort( pCSF->hBtn4, QWS_ID, IDC_HALT);
        WinSetWindowText( pCSF->hBtn4, pszBTN_Stop);
        ulErr = CAMSAVE_RC_OK;
    }

    if (ulErr == CAMSAVE_RC_OK) {
        WinShowWindow( pCSF->hBtn1, FALSE);
        WinShowWindow( pCSF->hBtn2, FALSE);
        WinShowWindow( pCSF->hBtn3, FALSE);

        WinSendMsg( pCSF->hEdit, EM_SETREADONLY, (MP)TRUE, 0);
        WinSetPresParam( pCSF->hEdit, PP_BACKGROUNDCOLOR,
                         sizeof(RGB), &lrgbEditBkgd.rgb);

        WinSetDlgItemText( hwnd, IDC_TEXT3, "");
        sprintf( szText, pszMSG_SavedErased,
                 (int)pCSF->cntSave, (int)pCSF->cntErase);
        WinSetDlgItemText( hwnd, IDC_TEXT4, szText);
        WinSetWindowUShort( pCSF->hBtn4, QWS_ID, IDC_HALT);
        return;
    }

    if (ulErr == CAMSAVE_RC_FATAL) {
        WinShowWindow( pCSF->hBtn1, FALSE);
        WinShowWindow( pCSF->hBtn2, FALSE);
        WinShowWindow( pCSF->hBtn3, FALSE);

        WinSendMsg( pCSF->hEdit, EM_SETREADONLY, (MP)TRUE, 0);
        WinSetPresParam( pCSF->hEdit, PP_BACKGROUNDCOLOR,
                         sizeof(RGB), &lrgbEditBkgd.rgb);

        LoadString( MSG_Fatal1, szText);
        WinSetDlgItemText( hwnd, IDC_TEXT3, szText);
        LoadString( MSG_Fatal2, szText);
        WinSetDlgItemText( hwnd, IDC_TEXT4, szText);
        WinSetWindowUShort( pCSF->hBtn4, QWS_ID, IDC_STOP);
        return;
    }

    if (ulErr == CAMSAVE_RC_DONE) {
        WinShowWindow( pCSF->hBtn1, FALSE);
        WinShowWindow( pCSF->hBtn2, FALSE);
        WinShowWindow( pCSF->hBtn3, FALSE);
        WinShowWindow( pCSF->hChk,  FALSE);
        WinShowWindow( pCSF->hEdit, FALSE);
        WinShowWindow( WinWindowFromID( hwnd, IDC_TEXT1), FALSE);
        WinShowWindow( WinWindowFromID( hwnd, IDC_TEXT3), FALSE);
        WinShowWindow( WinWindowFromID( hwnd, IDC_TEXT4), FALSE);
        WinSetWindowUShort( pCSF->hBtn4, QWS_ID, IDC_NO);
        WinSetWindowText( pCSF->hBtn4, pszBTN_Close);
        WinSendMsg( pCSF->hBtn4, BM_SETDEFAULT, (MP)TRUE, 0);
        return;
    }

    switch (ulErr) {

        case CAMSAVE_RC_TOOLONG: {
            ulText3 = MSG_TooLong1;
            ulText4 = MSG_TooLong2;
            fEdit = TRUE;
            break;
        }

        case CAMSAVE_RC_FILEEXISTS: {
            ulText3 = MSG_FileExists1;
            ulText4 = MSG_FileExists2;
            WinSetWindowUShort( pCSF->hBtn1, QWS_ID, IDC_REPLACE);
            WinSetWindowText( pCSF->hBtn1, pszBTN_Replace);
            fEdit = TRUE;
            fBtn1 = TRUE;
            fChk  = TRUE;
            break;
        }

        case CAMSAVE_RC_BADPATH: {
            ulText3 = MSG_PathNotFound1;
            ulText4 = MSG_PathNotFound2;
            WinSetWindowUShort( pCSF->hBtn1, QWS_ID, IDC_CREATE);
            WinSetWindowText( pCSF->hBtn1, pszBTN_Create);
            fEdit = TRUE;
            fBtn1 = TRUE;
            break;
        }

        case CAMSAVE_RC_BADCREATE: {
            ulText3 = MSG_CouldNotCreate1;
            ulText4 = MSG_CouldNotCreate2;
            WinSetWindowUShort( pCSF->hBtn1, QWS_ID, IDC_CREATE);
            WinSetWindowText( pCSF->hBtn1, pszBTN_Create);
            fEdit = TRUE;
            fBtn1 = TRUE;
            break;
        }

        case CAMSAVE_RC_GETDATA: {
            ulText3 = MSG_GetData;
            break;
        }

        case CAMSAVE_RC_CANCELLED: {
            pCSF->fHalt = FALSE;
            ulText3 = MSG_OpCancelled;
            break;
        }

        case CAMSAVE_RC_DOSOPEN: {
            ulText3 = MSG_OpenFile;
            fEdit = TRUE;
            break;
        }

        case CAMSAVE_RC_DOSWRITE: {
            ulText3 = MSG_WriteFile;
            fEdit = TRUE;
            break;
        }

        case CAMSAVE_RC_DELDATA: {
            ulText3 = MSG_ErasePhoto;
            fDel = TRUE;
            break;
        }

        default:
            return;
    }

    if (ulText3)
        LoadString( ulText3, szText);
    else
        szText[0] = 0;
    WinSetDlgItemText( hwnd, IDC_TEXT3, szText);

    if (ulText4)
        LoadString( ulText4, szText);
    else
        szText[0] = 0;
    WinSetDlgItemText( hwnd, IDC_TEXT4, szText);

    WinShowWindow( pCSF->hBtn1, fBtn1);
    WinShowWindow( pCSF->hBtn2, TRUE);
    WinShowWindow( pCSF->hBtn3, TRUE);
    WinShowWindow( pCSF->hChk,  fChk);

    WinSetWindowUShort( pCSF->hBtn2, QWS_ID,
                        (fDel ? IDC_RETRYDEL : IDC_RETRY));
    WinSetWindowUShort( pCSF->hBtn4, QWS_ID, IDC_STOP);

    WinSendMsg( pCSF->hEdit, EM_SETREADONLY, (MP)(fEdit ? FALSE : TRUE), 0);
    WinSetPresParam( pCSF->hEdit, PP_BACKGROUNDCOLOR,
                     sizeof(RGB), (fEdit ? &lrgbWhite.rgb : &lrgbEditBkgd.rgb));

    pCSF->flags |= CAMSAVE_RETRY | (fEdit ? CAMSAVE_EDIT : 0);

    return;
}

/****************************************************************************/

ULONG       BuildFilename( CAMRecPtr pcr, CAMSaveFilePtr pCSF)
{

    BOOL        fNbr;
    CAMGroupPtr pcg;
    char *  	pOut;
    char *      pTmpl;
    char *      pTmplName;
    char *      pTmplExt;
    char *      pFile;
    char *      pFileExt;
    char *      pEnd;
    char *      ptr;

do {
    pOut = pCSF->szPath;
    *pOut = 0;

    pcg = (CAMGroupPtr)pcr->grpptr;

    // if the title has been changed, use it as a template;
    // otherwise, use the group's filename template;
    // in either case, pFile points at the original filename
    if (pcr->orgname) {
        pTmpl = pcr->title;
        pFile = pcr->orgname;
    }
    else {
        pTmpl = pcg->file;
        pFile = pcr->title;
    }

    // see if the template includes a path
    // (this should only be true if the title was changed)
    pTmplName = strrchr( pTmpl, '\\');
    if (!pTmplName)
        pTmplName = strchr( pTmpl, ':');

    // if there's a path, use it, else use the group's path if present
    if (pTmplName) {
        memcpy( pOut, pTmpl, (pTmplName - pTmpl + 1));
        pOut += (pTmplName - pTmpl + 1);
    }
    else
    if (*pcg->path) {
        strcpy( pOut, pcg->path);
        pOut = strchr( pOut, 0);
        if (pOut[-1] != '\\')
            *pOut++ = '\\';
    }

    *pOut = 0;
    pCSF->flags &= ~CAMSAVE_DATESUB;    
    if (strchr( pCSF->szPath, '%')) {
        if (InsertDate( pcr, pCSF->szPath))
            pCSF->flags |= CAMSAVE_DATESUB;    
        pOut = strchr( pCSF->szPath, 0);
    }

    // point at the beginning of the name part of the template
    if (pTmplName)
        pTmplName++;
    else
        pTmplName = pTmpl;

    // a quick out for simple cases
    if (pTmplName[0] == 0 ||
        (pTmplName[0] == '*' && pTmplName[1] == 0) ||
        !strcmp( pTmplName, "*.*")) {
        strcpy( pOut, pFile);
        pOut = strchr( pOut, 0);
        break;
    }

    // locate the template's extension
    pTmplExt = strrchr( pTmplName, '.');
    if (pTmplExt)
        pEnd = pTmplExt;
    else
        pEnd = strchr( pTmplName, 0);

    // locate the file's extension
    pFileExt = strrchr( pFile, '.');

    // transfer the name portion of the template
    for (ptr = pTmplName, fNbr = FALSE; ptr < pEnd; ptr++) {

        // insert the original filename
        if (*ptr == '*') {
            if (pFileExt) {
                memcpy( pOut, pFile, pFileExt - pFile);
                pOut += pFileExt - pFile;
            }
            else {
                strcpy( pOut, pFile);
                pOut = strchr( pOut, 0);
            }
        }
        else
        // insert a sequence nbr & signal it needs to be incremented later
        if (*ptr == '#') {
            fNbr = TRUE;
            pOut += sprintf( pOut, "%0*d", (int)pcg->cntDigits, (int)pcg->seqNbr);
        }
        else
        if (*ptr == '%') {
            ptr++;
            if (ptr >= pEnd || *ptr == '%')
                *pOut++ = '%';
            else
            if (*ptr == pszSUB_Year[0] || *ptr == pszSUB_Year[1])
                pOut += sprintf( pOut, "%04d", pcr->date.year);
            else
            if (*ptr == pszSUB_Month[0] || *ptr == pszSUB_Month[1])
                pOut += sprintf( pOut, "%02d", pcr->date.month);
            else
            if (*ptr == pszSUB_Day[0] || *ptr == pszSUB_Day[1])
                pOut += sprintf( pOut, "%02d", pcr->date.day);
        }
        else
        // just copy the character
            *pOut++ = *ptr;
    }
    *pOut = 0;

    // increment the sequence nbr after the loop
    // to keep it from changing within a filename
    if (fNbr)
        pcg->seqNbr++;

    // if the template doesn't have an extension,
    // use the original file's if present, then exit
    if (!pTmplExt) {
        if (pFileExt) {
            strcpy( pOut, pFileExt);
            pOut = strchr( pOut, 0);
        }
        break;
    }

    // if there's a file extension, point it at the char after the dot
    if (pFileExt)
        pFileExt++;

    // transfer the template's extension
    for (ptr = pTmplExt; *ptr; ptr++) {
        // insert the original file's extension if there is one
        if (*ptr == '*') {
            if (pFileExt) {
                strcpy( pOut, pFileExt);
                pOut = strchr( pOut, 0);
            }
        }
        else
        // just copy the character
            *pOut++ = *ptr;
    }
    *pOut = 0;

} while (0);

    // remove CR & LF
    ptr = pCSF->szPath;
    while ((ptr = strpbrk( ptr, "\r\n")) != 0)
        strcpy( ptr, ptr + strspn( ptr, "\r\n"));

    return (strlen( pCSF->szPath));
}

/****************************************************************************/

BOOL        InsertDate( CAMRecPtr pcr, char * pszName)
{

    BOOL        fSub = FALSE;
    BOOL        fDate = FALSE;
    ULONG       cnt;
    char *      ptr;
    char *      pSrc;
    char *      pDst;
    char        szPath[CCHMAXPATH];

    pSrc = pszName;
    pDst = szPath;

    while ((ptr = strchr( pSrc, '%')) != 0) {
        cnt = ptr - pSrc;
        memcpy( pDst, pSrc, cnt);
        pDst += cnt;
        ptr++;
        if (*ptr == pszSUB_Year[0] || *ptr == pszSUB_Year[1]) {
            pDst += sprintf( pDst, "%04d", pcr->date.year);
            fDate = TRUE;
        }
        else
        if (*ptr == pszSUB_Month[0] || *ptr == pszSUB_Month[1]) {
            pDst += sprintf( pDst, "%02d", pcr->date.month);
            fDate = TRUE;
        }
        else
        if (*ptr == pszSUB_Day[0] || *ptr == pszSUB_Day[1]) {
            pDst += sprintf( pDst, "%02d", pcr->date.day);
            fDate = TRUE;
        }
        else {
            *pDst++ = '%';
            if (*ptr == '%')
                fSub = TRUE;
            else
                *pDst++ = *ptr;
        }
        pSrc = ptr + 1;
    }

    if (fDate || fSub) {
        strcpy( pDst, pSrc);
        strcpy( pszName, szPath);
    }

    return (fDate);
}

/****************************************************************************/

void        CreateSavePath( HWND hwnd, CAMSaveFilePtr pCSF)

{
    APIRET      rc;
    char *      ptr;
    char        szPath[CCHMAXPATH];
    char        szFile[CCHMAXPATH];

do {

    if (WinQueryWindowText( pCSF->hEdit, sizeof(szPath), szPath) == 0) {
        WinSetWindowText( pCSF->hEdit, pCSF->szPath);
        SetSaveControls( hwnd, pCSF, CAMSAVE_RC_BADCREATE);
        break;
    }

    ptr = strrchr( szPath, '\\');
    if (ptr) {
        strcpy( szFile, ptr+1);
        if (ptr == szPath || ptr[-1] == ':')
            ptr++;
        *ptr = 0;
    }
    else {
        strcpy( szFile, szPath);
        strcpy( szPath, ".");
    }

    rc = CreatePath( szPath);
    ptr = strchr( szPath, 0);
    if (ptr[-1] != '\\')
        *ptr++ = '\\';
    strcpy( ptr, szFile);
    WinSetWindowText( pCSF->hEdit, szPath);

    if (rc) {
        SetSaveControls( hwnd, pCSF, CAMSAVE_RC_BADCREATE);
        break;
    }

    SaveFile( hwnd, pCSF);

} while (0);

    return;

}

/****************************************************************************/

ULONG       NextMarkedRec( HWND hCnr, CAMRecPtr * ppcrIn, ULONG flags)

{
    ULONG       ulRtn = 0;
    ULONG       status;
    CAMRecPtr   pcrOut = *ppcrIn;

    pcrOut = (CAMRecPtr)WinSendMsg(
                 hCnr, CM_QUERYRECORD, (MP)pcrOut,
                 MPFROM2SHORT( (pcrOut ? CMA_NEXT : CMA_FIRST), CMA_ITEMORDER));

    while (pcrOut) {
        if (pcrOut == (CAMRecPtr)-1) {
            pcrOut = 0;
            break;
        }

        status = pcrOut->status;

        if ((flags & CAMSAVE_SAVE) &&
            (status & (STAT_SAVE | STAT_DONE)) == STAT_SAVE) {
            ulRtn = CAMSAVE_SAVE;
            break;
        }

        if ((flags & CAMSAVE_ERASE) && (status & STAT_DELETE)) {
            ulRtn = CAMSAVE_ERASE;
            break;
        }

        pcrOut = (CAMRecPtr)WinSendMsg( hCnr, CM_QUERYRECORD,
                    (MP)pcrOut,  MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER));
    }

    *ppcrIn = pcrOut;

    return (ulRtn);
}

/****************************************************************************/
// everything below runs on a secondary thread
/****************************************************************************/

void        SaveEraseThreadProc( void * saveeraseptr)

{
    uint32_t        rc = 0;
    CAMSaveErasePtr pse = (CAMSaveErasePtr)saveeraseptr;

    // do forever
    while (1) {

        // if we can't block, we have to terminate
        rc = DosWaitEventSem( pse->hEvent, (ULONG)-1);
        if (rc) {
            printf( "SaveEraseThreadProc - DosWaitEventSem  rc= 0x%x\n", (int)rc);
            break;
        }

        // set ourselves up to block when we're done here
        DosResetEventSem( pse->hEvent, (PULONG)&rc);

        if (pse->fTerm)
            break;

        if (DosRequestMutexSem( pse->thisCam->hmtxCam, CAM_MUTEXWAIT))
            pse->ulErr = CAMSAVE_RC_GETDATA;
        else {
            if (pse->ulOp & CAMSAVE_SAVE) {
                if (pse->pcr->type == CAM_TYPE_MSD)
                    pse->ulErr = MsdSaveObject( pse);
                else
                    pse->ulErr = PtpSaveObject( pse);
            }
            if (!pse->ulErr && (pse->ulOp & CAMSAVE_ERASE)) {
                if (pse->pcr->type == CAM_TYPE_MSD)
                    pse->ulErr = MsdEraseObject( pse);
                else
                    pse->ulErr = PtpEraseObject( pse);
            }
            DosReleaseMutexSem( pse->thisCam->hmtxCam);
        }

        // let the main thread know we finished processing this record
        if (!WinPostMsg( pse->hReply, CAMMSG_SAVEERASE, (MP)pse, 0))
            printf( "SaveEraseThreadProc - WinPostMsg failed\n");

        if (pse->fTerm)
            break;
    }

    // perform cleanup, then terminate the thread
    DosCloseEventSem( pse->hEvent);

    // we can only free pse if the main thread says we should terminate;
    // otherwise, the main thread could trap when it accesses it
    if (pse->fTerm) {
        free( pse);
        pse = 0;
    }

    return;
}

/***************************************************************************/
/***************************************************************************/

ULONG       PtpSaveObject( CAMSaveErasePtr pse)

{
    HFILE           hFile = 0;
    ULONG           ulErr = CAMSAVE_RC_OK;
    ULONG           ul;
    ULONG           rc;
    CAMRecPtr       pcr;
    EAOP2       	eaop2;
    CAMSubjectEA    ea;

do {
    if (pse->fStop) {
        ulErr = CAMSAVE_RC_CANCELLED;
        break;
    }

    pcr = pse->pcr;

    BuildSubjectEA( pcr, &ea);
    eaop2.fpGEA2List = 0;
    eaop2.fpFEA2List = (PFEA2LIST)&ea;

    rc = DosOpen( pse->szPath, &hFile, &ul, pcr->size, FILE_NORMAL,
                  OPEN_ACTION_CREATE_IF_NEW | ((pse->ulOp & CAMSAVE_REPMASK) ?
                  OPEN_ACTION_REPLACE_IF_EXISTS : OPEN_ACTION_FAIL_IF_EXISTS),
                  (OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_SEQUENTIAL |
                  OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE), &eaop2);
    if (rc) {
        ulErr = CAMSAVE_RC_DOSOPEN;
        break;
    }

    if ((QueryRotateOpts() & CAM_ROTATEONSAVE) &&
        (pcr->fmtnbr == PTP_OFC_EXIF_JPEG || pcr->fmtnbr == PTP_OFC_JFIF) &&
        STAT_GETROT(pcr->status))
        rc = PtpSaveRotatedObject( pse, hFile);
    else
        rc = PtpSaveObjectAsIs( pse, hFile);

    if (rc) {
        if (rc == CAMERR_REQCANCELLED)
            ulErr = CAMSAVE_RC_CANCELLED;
        else
            ulErr = CAMSAVE_RC_GETDATA;

        DosClose( hFile);
        DosDelete( pse->szPath);
        break;
    }

    SetFileDate( hFile, 0, pcr);
    DosClose( hFile);
    pse->ulOp |= CAMSAVE_SAVEOK;

} while (0);

    return (ulErr);
}

/***************************************************************************/

uint32_t    PtpSaveObjectAsIs( CAMSaveErasePtr pse, HFILE hFile)

{
    uint32_t        rtn;
    uint32_t        rc;
    uint32_t        size;
    uint32_t        alloc;
    uint32_t        cnt;
    uint32_t        wrote;
    uint32_t        fCancel = FALSE;
    uint32_t        hObject;
    char *          data = 0;
    CAMReadData     crd;

do {
    hObject = pse->pcr->hndl;

    if (pse->fStop) {
        rtn = CAMERR_REQCANCELLED;
        break;
    }

    rtn = ReadObjectBegin( pse->thisCam->hCam, hObject, &size, &crd);
    if (rtn != PTP_RC_OK)
        break;

    cnt = (size <= sizeof(crd.data)) ? size : sizeof(crd.data);
    size -= cnt;

    rc = DosWrite( hFile, crd.data, cnt, (PULONG)&wrote);
    if (rc) {
        camcli_error( "PtpSaveObjectAsIs - DosWrite  rc= 0x%04x", rc);
        rtn = CAMERR_WRITEFAILED;
        fCancel = TRUE;
        break;
    }

    if (size) {
        alloc = (size < 0x100000) ? size : 0x100000;

        rc = DosAllocMem( (void*)&data, alloc, PAG_COMMIT | PAG_READ | PAG_WRITE);
        if (rc) {
            camcli_error( "PtpSaveObjectAsIs - DosAllocMem  rc= 0x%04x", rc);
            rtn = CAMERR_MALLOC;
            fCancel = TRUE;
            break;
        }
    }

    while (size) {
        if (size <= alloc) {
            cnt = size;
            cnt++;
        }
        else
            cnt = alloc;

        if (pse->fStop) {
            rtn = CAMERR_REQCANCELLED;
            fCancel = TRUE;
            break;
        }

        rtn = ReadObjectData( pse->thisCam->hCam, &cnt, data);
        if (rtn != PTP_RC_OK)
            break;

        rc = DosWrite( hFile, data, cnt, (PULONG)&wrote);
        if (rc) {
            camcli_error( "PtpSaveObjectAsIs - DosWrite  rc= 0x%04x", rc);
            rtn = CAMERR_WRITEFAILED;
            fCancel = TRUE;
            break;
        }

        size -= cnt;
    }

    if (rtn != PTP_RC_OK)
        break;

    rtn = ReadObjectEnd( pse->thisCam->hCam);
    if (rtn == PTP_RC_OK)
        rtn = 0;

} while (0);

    if (fCancel)
        CancelRequest( pse->thisCam->hCam);

    if (data)
        DosFreeMem( data);

    return rtn;
}

/***************************************************************************/

uint32_t    PtpSaveRotatedObject( CAMSaveErasePtr pse, HFILE hFile)

{
    uint32_t    rtn;
    uint32_t    ulRotate;
    CAMSaveRot  csr;

    csr.hObject   = pse->pcr->hndl;
    csr.pszObject = 0;
    csr.hFile     = hFile;
    csr.thisCam   = pse->thisCam;
    csr.pStop     = (uint32_t*)&pse->fStop;
    csr.rtn       = PTP_RC_OK;

    ulRotate = STAT_GETROT(pse->pcr->status);
    if (ulRotate == STAT_ROT270)
        csr.ulRotate = 90;
    else
    if (ulRotate == STAT_ROT180)
        csr.ulRotate = 180;
    else
        csr.ulRotate = 270;

    rtn = JpgSaveRotatedJPEG( &csr);

    if (rtn == PTP_RC_OK)
        rtn = 0;

    return (rtn);
}

/***************************************************************************/

uint32_t    PtpEraseObject( CAMSaveErasePtr pse)

{
    uint32_t    rtn;

do {
    if (pse->fStop) {
        rtn = CAMERR_REQCANCELLED;
        break;
    }

    if (pse->pcr->status & STAT_PROTECTED) {
        rtn = SetObjectProtection( pse->thisCam->hCam, pse->pcr->hndl, 0);
        if (rtn)
            break;
    }

    rtn = DeleteObject( pse->thisCam->hCam, pse->pcr->hndl, 0);
    if (rtn)
        break;

    pse->ulOp |= CAMSAVE_ERASEOK;
    CheckDeviceStatus( pse->thisCam->hCam, 10);

} while (0);

    if (rtn) {
        if (rtn == CAMERR_REQCANCELLED)
            rtn = CAMSAVE_RC_CANCELLED;
        else
            rtn = CAMSAVE_RC_DELDATA;
    }

    return (rtn);
}

/****************************************************************************/
/***************************************************************************/

ULONG       MsdSaveObject( CAMSaveErasePtr pse)

{
    ULONG           ulErr = CAMSAVE_RC_OK;
    ULONG           rc;

do {
    if (pse->fStop) {
        ulErr = CAMSAVE_RC_CANCELLED;
        break;
    }

    if ((QueryRotateOpts() & CAM_ROTATEONSAVE) &&
        (pse->pcr->fmtnbr == PTP_OFC_EXIF_JPEG || pse->pcr->fmtnbr == PTP_OFC_JFIF) &&
        STAT_GETROT(pse->pcr->status))
        rc = MsdSaveRotatedObject( pse);
    else
        rc = MsdSaveObjectAsIs( pse);

    if (rc) {
        if (rc == CAMERR_REQCANCELLED)
            ulErr = CAMSAVE_RC_CANCELLED;
        else
            ulErr = CAMSAVE_RC_GETDATA;

        DosDelete( pse->szPath);
        break;
    }

    pse->ulOp |= CAMSAVE_SAVEOK;

} while (0);

    return (ulErr);
}

/***************************************************************************/

ULONG       MsdSaveObjectAsIs( CAMSaveErasePtr pse)

{
    ULONG           rc;
    EAOP2       	eaop2;
    CAMSubjectEA    ea;
    char            szSrc[CCHMAXPATH];

do {
    strcpy( szSrc, pse->pcr->path);
    strcat( szSrc, pse->pcr->title);

    rc = DosCopy( szSrc, pse->szPath,
                  ((pse->ulOp & CAMSAVE_REPMASK) ? DCPY_EXISTING : 0));
    if (rc) {
        printf( "MsdSaveObjectAsIs - DosCopy first attempt - rc= 0x%lx\n   file= %s\n",
                rc, szSrc);
        DosSleep( 95);
        rc = DosCopy( szSrc, pse->szPath,
                      ((pse->ulOp & CAMSAVE_REPMASK) ? DCPY_EXISTING : 0));
        if (rc) {
            printf( "MsdSaveObjectAsIs - DosCopy second attempt - rc= 0x%lx\n", rc);
            break;
        }
    }

    if (pse->fStop) {
        rc = CAMSAVE_RC_CANCELLED;
        break;
    }

    BuildSubjectEA( pse->pcr, &ea);
    eaop2.fpGEA2List = 0;
    eaop2.fpFEA2List = (PFEA2LIST)&ea;

    rc = DosSetPathInfo( pse->szPath, FIL_QUERYEASIZE, &eaop2, sizeof(EAOP2), 0);
    if (rc)
        printf( "MsdSaveObjectAsIs - DosSetPathInfo for EA - rc= 0x%lx\n", rc);

    rc = SetFileDate( 0, pse->szPath, pse->pcr);
    if (rc)
        printf( "MsdSaveObjectAsIs - SetFileDate - rc= 0x%lx\n", rc);

    rc = 0;

} while (0);

    return (rc);
}

/***************************************************************************/

ULONG       MsdSaveRotatedObject( CAMSaveErasePtr pse)

{
    ULONG           rc;
    ULONG           ul;
    ULONG           ulRotate;
    HFILE           hFile = 0;
    CAMRecPtr       pcr;
    EAOP2       	eaop2;
    CAMSubjectEA    ea;
    CAMSaveRot      csr;
    char            szSrc[CCHMAXPATH];

do {
    pcr = pse->pcr;

    BuildSubjectEA( pcr, &ea);
    eaop2.fpGEA2List = 0;
    eaop2.fpFEA2List = (PFEA2LIST)&ea;

    rc = DosOpen( pse->szPath, &hFile, &ul, pcr->size, FILE_NORMAL,
                  OPEN_ACTION_CREATE_IF_NEW | ((pse->ulOp & CAMSAVE_REPMASK) ?
                  OPEN_ACTION_REPLACE_IF_EXISTS : OPEN_ACTION_FAIL_IF_EXISTS),
                  (OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_SEQUENTIAL |
                  OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE), &eaop2);
    if (rc)
        break;

    strcpy( szSrc, pse->pcr->path);
    strcat( szSrc, pse->pcr->title);

    csr.hObject   = 0;
    csr.pszObject = szSrc;
    csr.hFile     = hFile;
    csr.thisCam   = pse->thisCam;
    csr.pStop     = (uint32_t*)&pse->fStop;
    csr.rtn       = PTP_RC_OK;

    ulRotate = STAT_GETROT(pse->pcr->status);
    if (ulRotate == STAT_ROT270)
        csr.ulRotate = 90;
    else
    if (ulRotate == STAT_ROT180)
        csr.ulRotate = 180;
    else
        csr.ulRotate = 270;

    rc = JpgSaveRotatedJPEG( &csr);
    if (rc != PTP_RC_OK)
        break;

    SetFileDate( hFile, 0, pcr);
    rc = 0;

} while (0);

    if (hFile)
        DosClose( hFile);

    return (rc);
}

/***************************************************************************/

ULONG       MsdEraseObject( CAMSaveErasePtr pse)

{
    ULONG       rc;
    FILESTATUS3 fs3;
    char        szSrc[CCHMAXPATH];

do {
    if (pse->fStop) {
        rc = CAMERR_REQCANCELLED;
        break;
    }

    strcpy( szSrc, pse->pcr->path);
    strcat( szSrc, pse->pcr->title);

    if (pse->pcr->status & STAT_PROTECTED) {
        memset( &fs3, 0, sizeof(FILESTATUS3));
        DosQueryPathInfo( szSrc, FIL_STANDARD, &fs3, sizeof(FILESTATUS3));
        fs3.attrFile &= ~FILE_READONLY;
        rc = DosSetPathInfo( szSrc, FIL_STANDARD, &fs3, sizeof(FILESTATUS3), 0);
        if (rc)
            printf( "MsdEraseObject - DosSetPathInfo - rc= 0x%lx\n", rc);
    }

    rc = DosDelete( szSrc);
    if (rc)
        printf( "MsdEraseObject - DosDelete - rc= 0x%lx\n", rc);
    else
        pse->ulOp |= CAMSAVE_ERASEOK;

} while (0);

    if (rc) {
        if (rc == CAMERR_REQCANCELLED)
            rc = CAMSAVE_RC_CANCELLED;
        else
            rc = CAMSAVE_RC_DELDATA;
    }

    return (rc);
}

/****************************************************************************/
/****************************************************************************/

// prepares the file's original name to be written as its
// .SUBJECT extended attribute

void        BuildSubjectEA( CAMRecPtr pcr, CAMSubjectEA * pEA)

{
    ULONG       cbName;
    char *      pszName;

    pszName = (char*)(pcr->orgname ? pcr->orgname : pcr->title);
    cbName = strlen( pszName);

    pEA->hdr.ulcbList = sizeof(pEA->hdr) + sizeof(pEA->info) + cbName;
    pEA->hdr.uloNextEntryOffset = 0;
    pEA->hdr.bfEA = 0;
    pEA->hdr.bcbName = sizeof(".SUBJECT") - 1;
    pEA->hdr.uscbValue = sizeof(pEA->info) + cbName;
    strcpy(pEA->hdr.chszName, ".SUBJECT");
    pEA->info.usEAType = EAT_ASCII;
    pEA->info.usDataLth = cbName;
    strcpy( pEA->data, pszName);

    return;
}

/****************************************************************************/

ULONG       SetFileDate( HFILE hFile, char * pszFile, CAMRecPtr pcr)

{
    ULONG       rc = 6;     // 6 = ERROR_INVALID_HANDLE
    FILESTATUS3 fs3;

do {
    if (hFile)
        rc = DosQueryFileInfo( hFile, FIL_STANDARD, &fs3, sizeof(FILESTATUS3));
    else
    if (pszFile)
        rc = DosQueryPathInfo( pszFile, FIL_STANDARD, &fs3, sizeof(FILESTATUS3));

    if (rc)
        break;

    fs3.fdateCreation.year  = pcr->date.year - 1980;
    fs3.fdateCreation.month = pcr->date.month;
    fs3.fdateCreation.day   = pcr->date.day;

    fs3.ftimeCreation.hours   = pcr->time.hours;
    fs3.ftimeCreation.minutes = pcr->time.minutes;
    fs3.ftimeCreation.twosecs = pcr->time.seconds / 2;

    fs3.fdateLastWrite = fs3.fdateCreation;
    fs3.ftimeLastWrite = fs3.ftimeCreation;

    if (hFile)
        rc = DosSetFileInfo( hFile, FIL_STANDARD, &fs3, sizeof(FILESTATUS3));
    else
        rc = DosSetPathInfo( pszFile, FIL_STANDARD, &fs3, sizeof(FILESTATUS3), 0);

} while (0);

    return (rc);
}

/****************************************************************************/
/****************************************************************************/

