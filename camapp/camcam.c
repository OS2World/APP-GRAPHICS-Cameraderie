/****************************************************************************/
// camcam.c
//
//  - camera selection, init & shutdown
//  - camera disconnected/reconnected notifications
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
#define INCL_WIN
#include "camapp.h"
#include "camusb.h"
#include <os2.h>

/****************************************************************************/

enum    eNotify     { eSEMADD, eSEMREMOVE, eSEMTERM, eCNTSEMS, eSEMERR};

typedef struct _CAMNotify {
    SEMRECORD   sr[eCNTSEMS];
    CAMPtr      thisCam;
    HMUX        hmux;
    HWND        hwnd;
    uint32_t    NotifyID;
} CAMNotify;

#define CAMSELECT_RETRY (CAM_TYPE_NUL + 1)
#define CAMSELECT_EXIT  (CAM_TYPE_NUL + 2)

/****************************************************************************/

ULONG           CamSetupCamera( HWND hwnd, void ** thisCamPtr, BOOL fStartup);
ULONG           CamReconnectCamera( CAMPtr thisCam);
ULONG           CamAbandonCamera( CAMPtr thisCam);
void            CamShutDownCamera( void ** thisCamPtr);

ULONG           CamInitPtpCamera( CAMPtr thisCam);
void            CamGetDeviceName( CAMUsbInfo * pUsb, char * pszText);

ULONG           CamInitNotification( HWND hwnd, void * voidCam);
void            CamNotifyThread( void * arg);
void APIENTRY   CamTermNotifyOnExit( ULONG ul);
void            CamHandleNotification( HWND hwnd, MPARAM mp1, MPARAM mp2);

ULONG           CamDlgBox( HWND hwnd, ULONG ulID, ULONG ulParm);
MRESULT _System CamSelectCameraDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
void            CamInitSelectCameraDlg( HWND hwnd, MPARAM mp2);
BOOL            CamEjectSelectedDevice( HWND hwnd);
ULONG           CamGetSelectedHandle( HWND hwnd, PSHORT pndx);

MRESULT _System CamFatalDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

/****************************************************************************/

static ULONG            ulNotifyID = 0;

static CAMNLS   nlsFatalDlg[] = {
    { IDC_TEXT4,        DLG_ToEnd },
    { IDC_EXIT,         BTN_Exit },
    { 0,                0 }};

static CAMNLS   nlsSelectDlg[] = {
    { IDC_TEXT1,        DLG_Select },
    { IDC_RETRY,        BTN_Retry },
    { IDC_AUTOMOUNT,    MNU_MountDrives },
    { IDC_OK,           BTN_Open },
    { IDC_CANCEL,       BTN_Cancel },
    { IDC_EXIT,         BTN_Exit },
    { 0,                0 }};

static CAMNLS   nlsNoSelectDlg[] = {
    { IDC_TEXT1,        DLG_NoCameras },
    { IDC_RETRY,        BTN_Retry },
    { IDC_AUTOMOUNT,    MNU_MountDrives },
    { IDC_TEXT4,        DLG_PlugIn },
    { IDC_OK,           BTN_Open },
    { IDC_CANCEL,       BTN_Cancel },
    { IDC_EXIT,         BTN_Exit },
    { 0,                0 }};


/****************************************************************************/

ULONG       CamSetupCamera( HWND hwnd, void ** thisCamPtr, BOOL fStartup)

{
    BOOL            fEjected = FALSE;
    ULONG           rc;
    ULONG           selection;
    CAMDevice       device = 0;
    CAMDevice       devList = 0;
    CAMPtr          thisCam = 0;
    CAMCameraList   camList;
    char            szText[256];

do
{
    printf( "\n");

    thisCam = (CAMPtr)malloc( sizeof(CAM));
    if (!thisCam) {
        rc = CAMERR_MALLOC;
        break;
    }
    memset( thisCam, 0, sizeof(CAM));

    // create the camera-access mutex in an owned state
    rc = DosCreateMutexSem( 0, (PHMTX)(&thisCam->hmtxCam), 0, TRUE);
    if (rc)
        break;

    while (1) {
        memset( &camList, 0, sizeof(CAMCameraList));
        camList.fStartup = fStartup;
        camList.thisCamPtr = thisCamPtr;
        camList.cntMSD = DrvGetDrives( hwnd, camList.achDrives);
        if (CAM_IS_USB) {
            rc = GetUsbDeviceList( &devList);
            if (rc)
                break;
            GetCameraList( &camList, devList, (uint32_t)CAM_IS_FORCE);
        }

        printf( "PTP= %d  MSD= %d  List= %d\n",
                 camList.cntPTP, camList.cntMSD, camList.cntList);

        if (!fStartup || camList.cntMSD + camList.cntList != 1) {
            selection = CamDlgBox( hwnd, IDD_SELECTCAMERA, (ULONG)&camList);

            if (camList.fEjected)
                fEjected = TRUE;

            if (selection == CAMSELECT_RETRY)
                continue;

            if (selection == CAMSELECT_EXIT) {
                rc = CAMERR_NOCAMERAS;
                WinPostMsg( hwnd, WM_QUIT, 0, 0);
                break;                
            }

            if (!(selection & (CAM_TYPE_USB | CAM_TYPE_MSD | CAM_TYPE_NUL)))
                selection = 0;
        }
        else
        if (camList.cntMSD == 1)
            selection = CAM_TYPE_MSD;
        else
        if (camList.cntList == 1)
            selection = CAM_TYPE_USB;
        else
            selection = 0;

        if (!selection) {
            if (fStartup || fEjected)
                selection = CAM_TYPE_NUL;
            else
                rc = CAMERR_NOCAMERAS;
        }

        break;
    } 

    if (rc)
        break;

    CamShutDownCamera( thisCamPtr);
    *thisCamPtr = thisCam;

    if (selection == CAM_TYPE_NUL) {
        printf( "NUL device selected\n");
        CamShutDownCamera( thisCamPtr);
        break;
    }

    if (selection & CAM_TYPE_MSD) {
        thisCam->camType = CAM_TYPE_MSD;
        selection &= 0x07;
        strcpy( thisCam->drive, "A:\\");
        thisCam->drive[0] = camList.achDrives[selection];

        DrvGetDriveName( thisCam->drive[0], szText);
        printf( "MSD device selected:  '%s'\n", szText);
        szText[sizeof(thisCam->szName) - 1] = 0;
        strcpy( thisCam->szName, szText);
        break;
    }

    if (!selection & CAM_TYPE_USB) {
        rc = CAMERR_NOCAMERAS;
        break;
    }

    selection &= 0x03;
    thisCam->camType = CAM_TYPE_USB;
    thisCam->pUsbInfo = (CAMUsbInfo*)malloc( sizeof(CAMUsbInfo));
    if (!thisCam->pUsbInfo) {
        rc = CAMERR_MALLOC;
        break;
    }
    memcpy( thisCam->pUsbInfo, &camList.Cameras[selection], sizeof(CAMUsbInfo));

    CamGetDeviceName( thisCam->pUsbInfo, szText);
    printf( "PTP device selected:  '%s'\n", szText);
    szText[sizeof(thisCam->szName) - 1] = 0;
    strcpy( thisCam->szName, szText);

    device = thisCam->pUsbInfo->Device;
    rc = CamInitPtpCamera( thisCam);
    if (rc)
        break;

    CamInitNotification( hwnd, thisCam);

} while (0);

    // free descriptors for all devices except
    // the one we're using (which may be none)
    FreeUsbDeviceList( devList, device);

    if (thisCam && thisCam->hmtxCam)
        DosReleaseMutexSem( thisCam->hmtxCam);

    // if there was an error, close the new device (if any)
    if (rc) {
        if (rc != CAMERR_NOCAMERAS)
            OpenWindow( hwnd, IDD_FATAL, MSG_InitCamera);
        if (*thisCamPtr == thisCam)
            *thisCamPtr = 0;
        CamShutDownCamera( (void**)&thisCam);
    }

    return rc;
}

/****************************************************************************/

ULONG       CamReconnectCamera( CAMPtr thisCam)

{
    ULONG   	 rc;

do {
    if (!thisCam || !thisCam->pUsbInfo)
        return (CAMERR_NOCAMERAS);

    rc = DosRequestMutexSem( thisCam->hmtxCam, CAM_MUTEXWAIT);
    if (rc)
        break;

    // when the camera is disconnected, the notify thread zeros hCam
    // but saves its address in hDisconnect so it can be freed when
    // nothing is using it
    if (thisCam->hDisconnect) {
        free( thisCam->hDisconnect);
        thisCam->hDisconnect = 0;
    }

    rc = GetSpecificCamera( thisCam->pUsbInfo);
    if (rc)
        break;

    rc = CamInitPtpCamera( thisCam);

} while (0);

    DosReleaseMutexSem( thisCam->hmtxCam);

    return rc;
}

/***************************************************************************/

ULONG       CamAbandonCamera( CAMPtr thisCam)

{
    if (!thisCam || !thisCam->pUsbInfo)
        return (CAMERR_NOCAMERAS);

    // note that hCam (actually, hDisconnect) isn't freed until the
    // camera is reconnected;  this avoids blocking the msg queue
    // while waiting for the mutex

    free( thisCam->pDevInfo);
    thisCam->pDevInfo = 0;

    FreeUsbDeviceList( thisCam->pUsbInfo->Device, 0);
    thisCam->pUsbInfo->Device = 0;
    thisCam->pUsbInfo->BusName = 0;
    thisCam->pUsbInfo->DeviceNbr = 0;

    return 0;
}

/****************************************************************************/

void        CamShutDownCamera( void ** thisCamPtr)

{
    if (*thisCamPtr) {
        CAMPtr  thisCam = (CAMPtr)*thisCamPtr;

        if (thisCam->hevTerm) {
            DosPostEventSem( thisCam->hevTerm);
            DosSleep( 30);
        }

        // don't worry if this times-out
        if (thisCam->hmtxCam)
            DosRequestMutexSem( thisCam->hmtxCam, CAM_MUTEXWAIT);

        // close session, close USB, free device, free hCam
        TermCamera( &thisCam->hCam);

        if (thisCam->hmtxCam) {
            DosReleaseMutexSem( thisCam->hmtxCam);
            DosCloseMutexSem( thisCam->hmtxCam);
        }

        free( thisCam->pUsbInfo);
        free( thisCam->pDevInfo);
        free( thisCam->paPaths);
        free( thisCam);
        *thisCamPtr = 0;
    }

    return;
}

/***************************************************************************/
/***************************************************************************/

ULONG       CamInitPtpCamera( CAMPtr thisCam)

{
    ULONG           rc;
    ULONG           ctr;
    ULONG           cnt;
    uint32_t *      ops;

do {
    rc = InitCamera( thisCam->pUsbInfo->Device, &thisCam->hCam);
    if (rc)
        break;

    rc = GetDeviceInfo( thisCam->hCam, &thisCam->pDevInfo);
    if (rc)
        break;

    ops = thisCam->pDevInfo->OperationsSupported;
    if (!ops)
        break;

    thisCam->camFlags &= ~CAM_PARTIAL_MASK;
    cnt = thisCam->pDevInfo->cntOperationsSupported;

    for (ctr = 0; ctr < cnt; ops++, ctr++)
        if (*ops == PTP_OC_GetPartialObject)
            thisCam->camFlags |= CAM_PARTIAL_STANDARD;
        else
        if (thisCam->pUsbInfo->VendorId == 0x04A9 &&
            *ops == PTP_OC_CANON_GetPartialObject)
            thisCam->camFlags |= CAM_PARTIAL_CANON;

} while (0);

    return (rc);
}

/****************************************************************************/
/****************************************************************************/

void        CamGetDeviceName( CAMUsbInfo * pUsb, char * pszText)

{
    if (!GetUsbProductName( pUsb->Device, pszText)) {
        if (pUsb->IsPTP)
            LoadString( MSG_PTPCamera, pszText);
        else
            LoadString( MSG_Unidentified, pszText);
    }

    return;
}

/****************************************************************************/
/****************************************************************************/

ULONG       CamInitNotification( HWND hwnd, void * voidCam)

{
    APIRET      rc = 0;
    CAMNotify * pCN = 0;
    CAMPtr      thisCam = (CAMPtr)voidCam;

    if (!thisCam || thisCam->camType == CAM_TYPE_MSD)
        return (rc);

do {
    pCN = malloc( sizeof(CAMNotify));
    if (!pCN) {
        rc = CAMERR_MALLOC;
        break;
    }
    memset( pCN, 0, sizeof(CAMNotify));

    pCN->hwnd = hwnd;
    pCN->thisCam = thisCam;
    if (!thisCam->pUsbInfo) {
        rc = CAMERR_MISCELLANEOUS;
        break;
    }

    pCN->sr[eSEMADD].ulUser = eSEMADD;
    rc = DosCreateEventSem( 0, (PHEV)&(pCN->sr[eSEMADD].hsemCur), DC_SEM_SHARED, 0);
    if (rc)
        break;

    pCN->sr[eSEMREMOVE].ulUser = eSEMREMOVE;
    rc = DosCreateEventSem( 0, (PHEV)&(pCN->sr[eSEMREMOVE].hsemCur), DC_SEM_SHARED, 0);
    if (rc)
        break;

    pCN->sr[eSEMTERM].ulUser = eSEMTERM;
    rc = DosCreateEventSem( 0, (PHEV)&(pCN->sr[eSEMTERM].hsemCur), 0, 0);
    if (rc)
        break;

    thisCam->hevTerm = (HEV)pCN->sr[eSEMTERM].hsemCur;

    rc = DosCreateMuxWaitSem( 0, &pCN->hmux, eCNTSEMS, pCN->sr, DCMW_WAIT_ANY);
    if (rc)
        break;

    rc = UsbStartNotification( thisCam->pUsbInfo, (HEV)pCN->sr[eSEMADD].hsemCur,
                            (HEV)pCN->sr[eSEMREMOVE].hsemCur, &pCN->NotifyID);
    if (rc)
        break;

    ulNotifyID = pCN->NotifyID;
    rc = DosExitList( (((UCHAR)0x7f << 8) | EXLST_ADD), CamTermNotifyOnExit);
    if (rc) {
        camcli_error( "InitNotification:  DosExitList ADD failed - rc= %d", (int)rc);
        rc = 0;
    }

    if (_beginthread( CamNotifyThread, 0, 0x4000, pCN) == -1) {
        rc = CAMERR_MISCELLANEOUS;
        break;
    }

} while (0);

    if (rc) {
        camcli_error( "CamInitNotification - rc= 0x%x\n", rc);

        if (pCN) {
            if (pCN->NotifyID)
                EndNotification( pCN->NotifyID);
            if (pCN->hmux)
                DosCloseMuxWaitSem( pCN->hmux);
            if (pCN->sr[eSEMTERM].hsemCur)
                DosCloseEventSem( (HEV)pCN->sr[eSEMTERM].hsemCur);
            if (pCN->sr[eSEMREMOVE].hsemCur)
                DosCloseEventSem( (HEV)pCN->sr[eSEMREMOVE].hsemCur);
            if (pCN->sr[eSEMADD].hsemCur)
                DosCloseEventSem( (HEV)pCN->sr[eSEMADD].hsemCur);
            free( pCN);
        }
        thisCam->hevTerm = 0;
    }

    return (rc);
}

/****************************************************************************/

void        CamNotifyThread( void * arg)

{
    APIRET      rc;
    CAMNotify * pCN = (CAMNotify*)arg;
    ULONG       ulSem;
    ULONG       ulCnt;

    if (!arg)
        return;

    DosResetEventSem( (HEV)pCN->sr[eSEMADD].hsemCur, &ulCnt);

    while (1) {
        rc = DosWaitMuxWaitSem( pCN->hmux, (ULONG)-1, &ulSem);
        if (rc)
            ulSem = eSEMERR;

        if (ulSem < eCNTSEMS) {
            ulCnt = 0;
            rc = DosResetEventSem( (HEV)pCN->sr[ulSem].hsemCur, &ulCnt);
            if (ulCnt != 1)
                camcli_error( "Multiple event sem postings - sem= %d  cnt= %d",
                              (int)ulSem, (int)ulCnt);
            if (rc)
                ulSem = eSEMERR;
        }
        else
            ulSem = eSEMERR;

        // if this is a disconnect, make hCam invalid immediately
        // but save its address so it can be freed by the primary
        // thread when it's safe to do so
        if (ulSem != eSEMTERM) {
            pCN->thisCam->hDisconnect = pCN->thisCam->hCam;
            pCN->thisCam->hCam = 0;
            WinPostMsg( pCN->hwnd, CAMMSG_NOTIFY, (MP)ulSem, (MP)pCN->thisCam);
        }
        if (rc || (ulSem != eSEMADD && ulSem != eSEMREMOVE))
            break;
    }

    if (rc)
        camcli_error( "exiting CamNotifyThread - rc= %d", (int)rc);

    rc = EndNotification( pCN->NotifyID);
    if (rc)
        camcli_error( "EndNotification - rc= %x", (int)rc);
    else {
        ulNotifyID = 0;
        rc = DosExitList( EXLST_REMOVE, CamTermNotifyOnExit);
        if (rc)
            camcli_error( "DosExitList REMOVE failed - rc= %d", (int)rc);
    }

    DosCloseMuxWaitSem( pCN->hmux);
    pCN->thisCam->hevTerm = 0;
    DosCloseEventSem( (HEV)pCN->sr[eSEMTERM].hsemCur);
    DosCloseEventSem( (HEV)pCN->sr[eSEMREMOVE].hsemCur);
    DosCloseEventSem( (HEV)pCN->sr[eSEMADD].hsemCur);
    free( pCN);

    return;
}

/****************************************************************************/

// this should clean up in case we crash

void APIENTRY   CamTermNotifyOnExit( ULONG ul)

{
    if (ulNotifyID) {
        APIRET rc = EndNotification( ulNotifyID);
        printf( "CamTermNotifyOnExit:  TermNotification - rc= %x\n", (int)rc);
        ulNotifyID = 0;
    }

    DosExitList( EXLST_EXIT, 0);
    return;
}

/****************************************************************************/

void        CamHandleNotification( HWND hwnd, MPARAM mp1, MPARAM mp2)

{
    ULONG   rc;
    HWND    hDlg;

    if (mp1 == (MP)eSEMREMOVE) {
        OpenWindow( hwnd, IDD_DISCONNECT, 0);
        CamAbandonCamera( (CAMPtr)mp2);

        return;
    }

    if (mp1 == (MP)eSEMADD) {
        rc = CamReconnectCamera( (CAMPtr)mp2);

        hDlg = GetHwnd( IDD_DISCONNECT);
        if (hDlg) {
            WindowClosed( IDD_DISCONNECT);
            WinDestroyWindow( hDlg);
        }

        if (!rc)
            WinPostMsg( hwnd, WM_COMMAND, (MP)IDM_SYNCDATA, 0);
        else
            OpenWindow( hwnd, IDD_FATAL, MSG_ReopenCamera);

        return;
    }

    if (mp2) {
        if (CamInitNotification( hwnd, (CAMPtr)mp2))
            WinAlarm( HWND_DESKTOP, WA_ERROR);
    }

    return;
}

/****************************************************************************/
// modal dialogs
/****************************************************************************/

ULONG       CamDlgBox( HWND hwnd, ULONG ulID, ULONG ulParm)

{
    ULONG   ulRtn = 0;
    HWND    hDlg;

    hDlg = OpenWindow( hwnd, ulID, ulParm);
    if (hDlg) {
        ulRtn = WinProcessDlg( hDlg);
        WindowClosed( ulID);
        WinDestroyWindow( hDlg);
    }

    return (ulRtn);
}

/****************************************************************************/

MRESULT _System CamSelectCameraDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    switch (msg) {

        case WM_INITDLG:
            CamInitSelectCameraDlg( hwnd, mp2);
            break;

        case WM_CONTROL:
            if (mp1 == MPFROM2SHORT( IDC_CAMLIST, LN_ENTER))
                WinPostMsg( hwnd, WM_COMMAND, (MP)IDC_OK, 0);
            else
            if (mp1 == MPFROM2SHORT( IDC_CAMLIST, LN_SELECT)) {
                ULONG  handle = CamGetSelectedHandle( hwnd, 0);
                WinEnableWindow( WinWindowFromID( hwnd, IDC_EJECT),
                                 ((handle & (CAM_TYPE_USB | CAM_TYPE_MSD)) ?
                                 TRUE : FALSE));
            }
            break;

        case WM_COMMAND: {
            ULONG  handle;

            if (mp1 == (MP)IDC_EJECT) {
                WinEnableWindow( WinWindowFromID( hwnd, IDC_EJECT), FALSE);
                CamEjectSelectedDevice( hwnd);
                break;
            }

            DrvSetUseLvm( WinQueryWindow( hwnd, QW_OWNER),
                          (ULONG)WinSendDlgItemMsg( hwnd, IDC_AUTOMOUNT,
                                                    BM_QUERYCHECK, 0, 0));

            if (mp1 == (MP)IDC_OK) {
                handle = (CamGetSelectedHandle( hwnd, 0) & 0xff);
                if (!handle)
                    break;
            }
            else
            if (mp1 == (MP)IDC_RETRY)
                handle = CAMSELECT_RETRY;
            else
            if (mp1 == (MP)IDC_EXIT)
                handle = CAMSELECT_EXIT;
            else
                handle = 0;

            WindowClosed( WinQueryWindowUShort( hwnd, QWS_ID));
            WinDismissDlg( hwnd, handle);
            break;
        }

        case WM_FOCUSCHANGE:
            if (SHORT1FROMMP(mp2) && !WinIsWindowEnabled( hwnd)) {
                WinPostMsg( WinQueryWindow( hwnd, QW_OWNER),
                            CAMMSG_FOCUSCHANGE, 0, 0);
                return (0);
            }
            break;

        case WM_SAVEAPPLICATION:
            WinStoreWindowPos( CAMINI_APP, CAMINI_OPENPOS, hwnd);
            break;

        default:
            return (WinDefDlgProc( hwnd, msg, mp1, mp2));
    }

    return (0);
}

/****************************************************************************/

void    CamInitSelectCameraDlg( HWND hwnd, MPARAM mp2)

{
    USHORT      ndx;
    ULONG       ctr;
    HWND        hList;
    CAMCameraListPtr pList = (CAMCameraListPtr)mp2;
    char        szText[256];

    WinSetWindowULong( hwnd, QWL_USER, (ULONG)pList);

    // if this is startup, change the Cancel button to an Exit button
    if (pList->fStartup)
        WinSetWindowUShort( WinWindowFromID( hwnd, IDC_CANCEL),
                            QWS_ID, IDC_EXIT);

    if (pList->cntMSD || pList->cntList)
        LoadDlgStrings( hwnd, nlsSelectDlg);
    else
        LoadDlgStrings( hwnd, nlsNoSelectDlg);

    WinSendDlgItemMsg( hwnd, IDC_AUTOMOUNT, BM_SETCHECK,
                       (MP)DrvQueryUseLvm(), 0);

    hList = WinWindowFromID( hwnd, IDC_CAMLIST);

    for (ctr = 0; ctr < pList->cntList; ctr++) {
        CamGetDeviceName( &pList->Cameras[ctr], szText);

        ndx = SHORT1FROMMR( WinSendMsg( hList, LM_INSERTITEM,
                                        (MP)LIT_END, (MP)szText));
        WinSendMsg( hList, LM_SETITEMHANDLE, MPFROM2SHORT( ndx, 0),
                    (MP)(ctr | CAM_TYPE_USB));
    }

    for (ctr = 0; ctr < pList->cntMSD; ctr++) {
        DrvGetDriveName( pList->achDrives[ctr], szText);

        ndx = SHORT1FROMMR( WinSendMsg( hList, LM_INSERTITEM,
                                        (MP)LIT_END, (MP)szText));
        WinSendMsg( hList, LM_SETITEMHANDLE, MPFROM2SHORT( ndx, 0),
                    (MP)(ctr | CAM_TYPE_MSD));
    }

    LoadString( MSG_NoCamera, szText);
    ndx = SHORT1FROMMR( WinSendMsg( hList, LM_INSERTITEM, (MP)LIT_END,
                                    (MP)szText));
    WinSendMsg( hList, LM_SETITEMHANDLE, MPFROM2SHORT( ndx, 0),
                (MP)CAM_TYPE_NUL);

    WinSendMsg( hList, LM_SELECTITEM, 0, (MP)TRUE);

    ShowDialog( hwnd, CAMINI_OPENPOS);

    return;
}

/****************************************************************************/

BOOL        CamEjectSelectedDevice( HWND hwnd)

{
    BOOL        fRtn = FALSE;
    ULONG       handle;
    CAMCameraListPtr pList;
    CAMPtr      thisCam;
    SHORT       ndx;
    char        chDrive;

do {
    pList = (CAMCameraListPtr)WinQueryWindowULong( hwnd, QWL_USER);
    thisCam = *((CAMPtr*)pList->thisCamPtr);
    handle = CamGetSelectedHandle( hwnd, &ndx);

    if (handle & CAM_TYPE_MSD) {
        chDrive = pList->achDrives[handle & 0x0f];
        LoadDlgItemString( hwnd, IDC_TEXT4, MSG_ThisMayTake);
        if (DrvEjectDrive( chDrive))
            break;

        if (thisCam && thisCam->camType == CAM_TYPE_MSD &&
            chDrive == thisCam->drive[0]) {
            CamShutDownCamera( pList->thisCamPtr);
            pList->fEjected = TRUE;
        }
    }
    else
    if (handle & CAM_TYPE_USB) {
        handle &= 0x03;
        if (thisCam && thisCam->camType == CAM_TYPE_USB &&
            pList->Cameras[handle].VendorId == thisCam->pUsbInfo->VendorId &&
            pList->Cameras[handle].ProductId == thisCam->pUsbInfo->ProductId) {
            CamShutDownCamera( pList->thisCamPtr);
            pList->fEjected = TRUE;
        }
    }
    else
        break;

    WinSendDlgItemMsg( hwnd, IDC_CAMLIST, LM_DELETEITEM,
                       MPFROM2SHORT( ndx, 0), 0);
    WinSendDlgItemMsg( hwnd, IDC_CAMLIST, LM_SELECTITEM,
                       (MP)(ndx ? ndx-1 : ndx), (MP)TRUE);

    fRtn = TRUE;

} while (0);

    LoadDlgItemString( hwnd, IDC_TEXT4,
                       (fRtn ? MSG_DisconnectDevice : MSG_UnableToEject));

    return (fRtn);
}

/****************************************************************************/

ULONG       CamGetSelectedHandle( HWND hwnd, PSHORT pndx)

{
    ULONG       ulRtn = 0;
    SHORT       ndx;

    ndx = SHORT1FROMMR( WinSendDlgItemMsg( hwnd, IDC_CAMLIST,
                                           LM_QUERYSELECTION,
                                           (MP)LIT_FIRST, 0));
    if (ndx != LIT_NONE) {
        ulRtn = (ULONG)WinSendDlgItemMsg( hwnd, IDC_CAMLIST,
                                         LM_QUERYITEMHANDLE,
                                         MPFROM2SHORT( ndx, 0), 0);
        if (pndx)
            *pndx = ndx;
    }

    return (ulRtn);
}

/****************************************************************************/
// non-modal dialog
/****************************************************************************/

MRESULT _System CamFatalDlg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    if (msg == WM_INITDLG) {
        LoadDlgStrings( hwnd, nlsFatalDlg);
        if (mp2)
            LoadDlgItemString( hwnd, IDC_TEXT1, (ULONG)mp2);
        ShowDialog( hwnd, 0);
        return 0;
    }

    if (msg == WM_COMMAND || msg == WM_SYSCOMMAND)
        WinPostMsg( hwnd, WM_QUIT, 0, 0);
    else
    if (msg == WM_FOCUSCHANGE) {
        if (SHORT1FROMMP(mp2) && !WinIsWindowEnabled( hwnd)) {
            WinPostMsg( WinQueryWindow( hwnd, QW_OWNER),
                        CAMMSG_FOCUSCHANGE, 0, 0);
            return (0);
        }
    }

    return (WinDefDlgProc( hwnd, msg, mp1, mp2));
}

/****************************************************************************/

