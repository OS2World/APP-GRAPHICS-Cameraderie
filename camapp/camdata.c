/****************************************************************************/
// camdata.c
//
//  - populate & refresh container with photo data
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

typedef struct _CAMGetData {
    ULONG       camType;        // 1 = USB; 2 = MSD
    ULONG       rc;
    BOOL        fSync;
    HWND        hReply;
    CAMPtr      thisCam;
    ULONG       cntItems;
    CAMObjectHandlePtr pHandles;
    CAMRecPtr   pcrFirst;
    CAMRecPtr   pcrLast;
    ULONG       cntRecords;
    ULONG       cntSync;
    CAMRecPtr * ppcrSync;
    char **     paPathsSync;
} CAMGetData;

/****************************************************************************/

#define CAM_FILEATTR    (FILE_ARCHIVED | FILE_SYSTEM | FILE_HIDDEN | FILE_READONLY)
#define CAM_ANYATTR     (FILE_DIRECTORY | CAM_FILEATTR)
#define CAM_DIRONLYATTR (FILE_DIRECTORY | MUST_HAVE_DIRECTORY)

#define CAM_FINDBUFSIZE         0x8000
#define CAM_PATHBUFSIZE         0x1000

// avoid defining DOS_ERRORS to get a few error codes
#ifndef ERROR_NO_MORE_FILES
  #define ERROR_NO_MORE_FILES   18
#endif

#ifndef ERROR_GEN_FAILURE
  #define ERROR_GEN_FAILURE     31
#endif

/****************************************************************************/

void            GetData( HWND hwnd, BOOL fSync);
void            GetDataReply( HWND hwnd, MPARAM mp1);
BOOL            GetList( HWND hCnr, CAMGetData * pcgd);
void            GetListReply( HWND hwnd, MPARAM mp1);
BOOL            SyncList( HWND hCnr, CAMGetData * pcgd);
void            SyncReply( HWND hwnd, MPARAM mp1);

void            PtpGetDataThread( void * camgetdataptr);
void            PtpGetListThread( void * camgetdataptr);
void            PtpSyncListThread( void * camgetdataptr);

void            MsdGetDataThread( void * camgetdataptr);
int         	MsdSortPaths( const void *key, const void *element);
void            MsdListThread( void * camgetdataptr);
BOOL            MsdSyncRecord( CAMGetData * pcgd, PULONG pndxSync,
                                char * pPath, char * pName);
void            MsdSyncPaths( CAMGetData * pcgd);
void            MsdGetObjInfo( PFILEFINDBUF3 pFF, CAMObjectInfoPtr pObjInfo,
                               char * pPath);

void            SetData( CAMRecPtr pcr, ULONG ulHndl, CAMObjectInfoPtr pObjInfo);
ULONG           ListRecords( HWND hCnr, CAMRecPtr ** pppcr);
void            NextRec( HWND hCnr, CAMRecPtr * ppcrIn);
void            FreeLinkedRecords( HWND hCnr, CAMRecPtr pcr);
void            CalcCnrTotals( HWND hCnr);
int             SortObjects( const void *key, const void *element);
void            AdjustList( CAMObjectHandlePtr pHandles);
void            CloseLoadingDlg( HWND hwnd);

/****************************************************************************/

static BOOL     fInitColMsgs        = FALSE;
static char     szDashes[]          = "---";
static char     szX[]               = "xXx";

static char *   pszCOL_TooBig       = szX;
static char *   aszDay[]            = {szX, szX, szX, szX, szX, szX, szX};

static CAMNLSPSZ nlsColMsgs[] = {
    { &pszCOL_TooBig,       COL_TooBig },
    { &aszDay[0],           COL_Sun },
    { &aszDay[1],           COL_Mon },
    { &aszDay[2],           COL_Tue },
    { &aszDay[3],           COL_Wed },
    { &aszDay[4],           COL_Thu },
    { &aszDay[5],           COL_Fri },
    { &aszDay[6],           COL_Sat },
    { 0,                    0 }};

/****************************************************************************/
// functions that operate on the primary thread
/****************************************************************************/

void        GetData( HWND hwnd, BOOL fSync)

{
    BOOL            fRtn = FALSE;
    HWND            hCnr;
    ULONG           ulErr = STS_GetList;
    char *          pErr = "";
    CAMPtr          thisCam;
    CAMGetData *    pcgd = 0;
    CAMRecPtr       pcr;
    PFNCTHREAD      pfn;

do
{
    ShowTime( (fSync ? "SyncData" : "GetData"), FALSE);

    // show the "please wait" dialog
    OpenWindow( hwnd, IDD_LOADING, 0);
    hCnr = WinWindowFromID( hwnd, FID_CLIENT);

    if (!fInitColMsgs) {
        LoadStaticStrings( nlsColMsgs);
        fInitColMsgs = TRUE;
    }

    // if there aren't any records, use GetList instead of SyncList;
    // otherwise, empty the container if GetList was requested
    thisCam  = (CAMPtr)GetCamPtr();
    if (!thisCam)
        fSync = FALSE;
    pcr = 0;
    NextRec( hCnr, &pcr);
    if (!pcr)
        fSync = FALSE;
    else
        if (!fSync) {
            while (pcr) {
                if (pcr->bmp)
                    GpiDeleteBitmap( pcr->bmp);
                if (pcr->orgname) 
                    free( pcr->orgname);

                NextRec( hCnr, &pcr);
            }

            if (WinSendMsg( hCnr, CM_REMOVERECORD, 0,
                            MPFROM2SHORT( 0, CMA_FREE | CMA_INVALIDATE)) != 0)
                printf( "GetData - CM_REMOVERECORD failed\n");
        }

    UpdateThumbWnd( hwnd);
    SetTitle( hwnd, (thisCam ? thisCam->szName : 0));

    if (!thisCam) {
        ClearStatStuff();
        InitColumnWidths( hCnr);
        ulErr = STS_Copyright;
        pErr = "demo mode";
        break;
    }

    pcgd = (CAMGetData*)malloc( sizeof(CAMGetData));
    if (!pcgd) {
        pErr = "malloc";
        break;
    }
    memset( pcgd, 0, sizeof(CAMGetData));

    pcgd->fSync    = fSync;
    pcgd->hReply   = hwnd;
    pcgd->thisCam  = thisCam;
    pcgd->camType  = pcgd->thisCam->camType;
    pcgd->paPathsSync = pcgd->thisCam->paPaths;

    if (pcgd->camType == CAM_TYPE_USB)
        pfn = PtpGetDataThread;
    else
    if (pcgd->camType == CAM_TYPE_MSD)
        pfn = MsdGetDataThread;
    else
        break;

    if (_beginthread( pfn, 0, 0xF000, pcgd) == -1) {
        pErr = "GetData - _beginthread failed";
        break;
    }

    fRtn = TRUE;

} while (0);

    if (!fRtn) {
        printf( "GetData - %s\n", pErr);
        UpdateStatus( hwnd, ulErr);
        CloseLoadingDlg( hwnd);
        if (pcgd)
            free( pcgd);

        ShowTime( (fSync ? "SyncData" : "GetData"), TRUE);
    }

    return;
}

/****************************************************************************/

void        GetDataReply( HWND hwnd, MPARAM mp1)

{
    CAMGetData *    pcgd = (CAMGetData*)mp1;
    BOOL            fRtn = FALSE;
    HWND            hCnr;
    ULONG           ulErr = STS_GetList;

do {
    hCnr = WinWindowFromID( hwnd, FID_CLIENT);

    if (pcgd->rc)
        break;

    // Dave's fix
    if (CAM_IS_DAVE && pcgd->camType == CAM_TYPE_USB) {
        AdjustList( pcgd->pHandles);
        pcgd->cntItems = pcgd->pHandles->cntHandles;
    }

    if (!pcgd->cntItems) {
        ulErr = STS_NoFiles;
        break;
    }

    if (pcgd->fSync)
        fRtn = SyncList( hCnr, pcgd);
    else
        fRtn = GetList( hCnr, pcgd);

} while (0);

    if (!fRtn) {
        UpdateStatus( hwnd, ulErr);
        CloseLoadingDlg( hwnd);

        if (pcgd->pHandles)
            free( pcgd->pHandles);
        if (pcgd->pcrFirst)
            FreeLinkedRecords( hCnr, pcgd->pcrFirst);
        if (pcgd->ppcrSync)
            free( pcgd->ppcrSync);

        ShowTime( (pcgd->fSync ? "SyncData" : "GetData"), TRUE);

        free( pcgd);
    }

    return;
}

/****************************************************************************/

BOOL        GetList( HWND hCnr, CAMGetData * pcgd)

{
    BOOL            fRtn = FALSE;
    PFNCTHREAD      pfn;
    char *          pErr = "";

do
{
    pcgd->pcrFirst = (CAMRecPtr)WinSendMsg( hCnr, CM_ALLOCRECORD,
                                           (MP)CAMREC_EXTRABYTES,
                                           (MP)pcgd->cntItems);
    if (!pcgd->pcrFirst) {
        pErr = "CM_ALLOCRECORD";
        break;
    }

    if (pcgd->camType == CAM_TYPE_MSD)
        pfn = MsdListThread;
    else
        pfn = PtpGetListThread;

    if (_beginthread( pfn, 0, 0xF000, pcgd) == -1) {
        pErr = "GetList - _beginthread failed";
        break;
    }

    fRtn = TRUE;

} while (0);

    if (!fRtn)
        printf( "GetList - %s\n", pErr);

    return (fRtn);
}

/****************************************************************************/

void        GetListReply( HWND hwnd, MPARAM mp1)

{
    CAMGetData *    pcgd = (CAMGetData*)mp1;
    HWND            hCnr;
    LONG            rc;
    ULONG           ulErr;
    CAMRecPtr       pcr = 0;
    RECORDINSERT    ri;

    hCnr = WinWindowFromID( hwnd, FID_CLIENT);

    if (pcgd->rc)
        rc = -1;
    else
    if (!pcgd->cntRecords)
        rc = 0;
    else {
        rc = 1;

        // break the linked list at pcrLast which was the last record used
        pcr = (CAMRecPtr)pcgd->pcrLast->mrc.preccNextRecord;
        pcgd->pcrLast->mrc.preccNextRecord = 0;

        // init the recordinsert struct
        ri.cb = sizeof(RECORDINSERT);
        ri.pRecordOrder = (PRECORDCORE)CMA_FIRST;
        ri.pRecordParent = 0;
        ri.fInvalidateRecord = /*TRUE*/ FALSE;
        ri.zOrder = CMA_TOP;
        ri.cRecordsInsert = pcgd->cntRecords;

        // insert all the records at once;  if this fails, relink the list
        if (!WinSendMsg( hCnr, CM_INSERTRECORD, (MP)pcgd->pcrFirst, (MP)&ri)) {
            pcgd->pcrLast->mrc.preccNextRecord = (CAM_RECTYPE*)pcr;
            rc = -1;
        }
    }

    // if there were any errors, set up to delete every record
    if (rc != 1)
        pcr = pcgd->pcrFirst;

    // free any records that weren't used;  normally,
    // these reflect folder entries that weren't listed
    FreeLinkedRecords( hCnr, pcr);

    CalcCnrTotals( hCnr);
    ShowTime( "GetData", TRUE);

    if (rc == -1)
        ulErr = STS_ListingFiles;
    else
    if (rc == 0)
        ulErr = STS_NoFiles;
    else {
        pcr = 0;
        NextRec( hCnr, &pcr);
        WinSendMsg( hCnr, CM_SETRECORDEMPHASIS, (MP)pcr,
                    MPFROM2SHORT( FALSE, CRA_SELECTED));
        WinSendMsg( hCnr, CM_SORTRECORD, (MP)QuerySortFunctionPtr(), 0);
        pcr = 0;
        NextRec( hCnr, &pcr);
        WinSendMsg( hCnr, CM_SETRECORDEMPHASIS, (MP)pcr,
                    MPFROM2SHORT( TRUE, CRA_CURSORED | CRA_SELECTED));

        ulErr = STS_Copyright;
        InitColumnWidths( hCnr);
        FetchAllThumbnails( hwnd);
    }

    // get rid of the wait dialog, then update the status bar
    UpdateStatus( hwnd, ulErr);
    CloseLoadingDlg( hwnd);

    free( pcgd->pHandles);
    free( pcgd);

    return;
}

/****************************************************************************/

BOOL        SyncList( HWND hCnr, CAMGetData * pcgd)

{
    BOOL            fRtn = FALSE;
    LONG            cnt;
    PFNCTHREAD      pfn;
    char *          pErr = "";

do {
    pcgd->cntSync = ListRecords( hCnr, &pcgd->ppcrSync);
    if (!pcgd->cntSync) {
        pErr = "ListRecords";
        break;
    }

    cnt = pcgd->cntItems - pcgd->cntSync;
    if (cnt < 32)
        cnt = 32;
    pcgd->pcrFirst = (CAMRecPtr)WinSendMsg( hCnr, CM_ALLOCRECORD,
                                            (MP)CAMREC_EXTRABYTES, (MP)cnt);
    if (!pcgd->pcrFirst) {
        pErr = "CM_ALLOCRECORD";
        break;
    }

    if (pcgd->camType == CAM_TYPE_MSD)
        pfn = MsdListThread;
    else
        pfn = PtpSyncListThread;

    if (_beginthread( pfn, 0, 0xF000, pcgd) == -1) {
        pErr = "_beginthread";
        break;
    }

    fRtn = TRUE;

} while (0);

    if (!fRtn)
        printf( "SyncList - %s\n", pErr);

    return (fRtn);
}

/****************************************************************************/

void        SyncReply( HWND hwnd, MPARAM mp1)

{
    CAMGetData *    pcgd = (CAMGetData*)mp1;
    HWND            hCnr;
    BOOL            fRtn;
    ULONG           ctr;
    ULONG           ulErr;
    CAMRecPtr       pcr;
    CAMRecPtr *     ppcr;
    RECORDINSERT    ri;

    hCnr = WinWindowFromID( hwnd, FID_CLIENT);
    fRtn = (pcgd->rc ? FALSE : TRUE);

    if (!fRtn || !pcgd->cntRecords)
        pcr = pcgd->pcrFirst;
    else {
        // break the linked list at pcrLast which was the last record used
        pcr = (CAMRecPtr)pcgd->pcrLast->mrc.preccNextRecord;
        pcgd->pcrLast->mrc.preccNextRecord = 0;

        // init the recordinsert struct
        ri.cb = sizeof(RECORDINSERT);
        ri.pRecordOrder = (PRECORDCORE)CMA_FIRST;
        ri.pRecordParent = 0;
        ri.fInvalidateRecord = /*TRUE*/ FALSE;
        ri.zOrder = CMA_TOP;
        ri.cRecordsInsert = pcgd->cntRecords;

        // insert all the records at once;  if this fails, relink the list
        if (!WinSendMsg( hCnr, CM_INSERTRECORD, (MP)pcgd->pcrFirst, (MP)&ri)) {
            pcgd->pcrLast->mrc.preccNextRecord = (CAM_RECTYPE*)pcr;
            pcr = pcgd->pcrFirst;
            fRtn = FALSE;
        }
    }

    // free any new records that weren't used
    FreeLinkedRecords( hCnr, pcr);

    // free existing records that weren't matched
    if (fRtn)
        for (ppcr = pcgd->ppcrSync, ctr = 0; ctr < pcgd->cntSync; ctr++) {
            if (ppcr[ctr] == 0)
                continue;

            if (ppcr[ctr]->bmp)
                GpiDeleteBitmap( ppcr[ctr]->bmp);
            if (ppcr[ctr]->orgname)
                free( ppcr[ctr]->orgname);
            WinSendMsg( hCnr, CM_REMOVERECORD, (MP)&ppcr[ctr],
                        MPFROM2SHORT( 1, CMA_FREE | CMA_INVALIDATE));
        }

    CalcCnrTotals( hCnr);
    WinSendMsg( hCnr, CM_SORTRECORD, (MP)QuerySortFunctionPtr(), 0);
    ShowTime( "SyncData", TRUE);

    if (!fRtn)
        ulErr = STS_RefreshList;
    else {
        ulErr = STS_Copyright;
        FetchAllThumbnails( hwnd);
    }

    // get rid of the wait dialog, then update the status bar
    UpdateStatus( hwnd, ulErr);
    CloseLoadingDlg( hwnd);

    // if sync failed because it ran out of records,
    // start over using GetList()
    if (pcgd->rc == CAMERR_NEEDRECORDS)
        WinPostMsg( hwnd, IDM_GETDATA, 0, 0);

    free( pcgd->pHandles);
    free( pcgd->ppcrSync);
    free( pcgd);

    return;
}

/****************************************************************************/
// functions that communicate with PTP cameras on a secondary thread
/****************************************************************************/

void        PtpGetDataThread( void * camgetdataptr)

{
    CAMGetData *    pcgd;
    BOOL            fMutex = FALSE;

do {
    pcgd  = (CAMGetData*)camgetdataptr;

    pcgd->rc = DosRequestMutexSem( pcgd->thisCam->hmtxCam, CAM_MUTEXWAIT);
    if (pcgd->rc)
        break;
    fMutex = TRUE;

    pcgd->rc = GetObjectHandles( pcgd->thisCam->hCam, 0xffffffff,
                          0, 0, &pcgd->pHandles);
    if (!pcgd->rc) {
        pcgd->cntItems = pcgd->pHandles->cntHandles;
        break;
    }

    printf( "PtpGetDataThread - GetObjectHandles #1 - rc= 0x%x\n",
            (int)pcgd->rc);
    if (pcgd->rc == CAMERR_NULLCAMERAPTR)
        break;

    pcgd->rc = ClearStall( pcgd->thisCam->hCam);
    if (pcgd->rc == CAMERR_NULLCAMERAPTR)
        break;

    DosSleep( 500);
    pcgd->rc = GetObjectHandles( pcgd->thisCam->hCam, 0xffffffff,
                                 0, 0, &pcgd->pHandles);
    if (!pcgd->rc) {
        pcgd->cntItems = pcgd->pHandles->cntHandles;
        break;
    }

    printf( "PtpGetDataThread - GetObjectHandles #2 - rc= 0x%x\n",
            (int)pcgd->rc);
    if (pcgd->rc != CAMERR_NULLCAMERAPTR)
        ClearStall( pcgd->thisCam->hCam);

} while (0);

    if (fMutex)
        DosReleaseMutexSem( pcgd->thisCam->hmtxCam);

    if (!WinPostMsg( pcgd->hReply, CAMMSG_GETDATA, (MP)pcgd, 0))
        printf( "PtpGetDataThread - WinPostMsg failed\n");

    return;
}

/****************************************************************************/

void        PtpGetListThread( void * camgetdataptr)

{
    CAMGetData *     pcgd;
    BOOL             fMutex = FALSE;
    PULONG           pHndl;
    ULONG            cnt;
    CAMRecPtr        pcr;
    CAMObjectInfoPtr pObjInfo = 0;

do {
    pcgd  = (CAMGetData*)camgetdataptr;
    pHndl = (ULONG*)pcgd->pHandles->Handles;
    cnt   = pcgd->cntItems;
    pcr   = pcgd->pcrFirst;

    pcgd->rc = DosRequestMutexSem( pcgd->thisCam->hmtxCam, CAM_MUTEXWAIT);
    if (pcgd->rc)
        break;
    fMutex = TRUE;

    // for each handle, get its info then format a record with its data
    for ( ; pcr && cnt; cnt--, pHndl++) {

        pcgd->rc = GetObjectInfo( pcgd->thisCam->hCam, *pHndl, &pObjInfo);
        if (pcgd->rc) {
            printf( "PtpGetListThread - GetObjectInfo #1 - rc= 0x%x\n", (int)pcgd->rc);
            if (pcgd->rc == CAMERR_NULLCAMERAPTR)
                break;

            pcgd->rc = ClearStall( pcgd->thisCam->hCam);
            if (pcgd->rc == CAMERR_NULLCAMERAPTR)
                break;

            DosSleep( 500);
            pcgd->rc = GetObjectInfo( pcgd->thisCam->hCam, *pHndl, &pObjInfo);
            if (pcgd->rc) {
                printf( "PtpGetListThread - GetObjectInfo #2 - rc= 0x%x\n", (int)pcgd->rc);
                if (pcgd->rc != CAMERR_NULLCAMERAPTR)
                    ClearStall( pcgd->thisCam->hCam);
                break;
            }
        }

        // construct a record for everything except folders
        if (pObjInfo->ObjectFormat != PTP_OFC_Association) {
            SetData( pcr, *pHndl, pObjInfo);
            pcgd->cntRecords++;
            pcgd->pcrLast = pcr;
            pcr = (CAMRecPtr)pcr->mrc.preccNextRecord;
        }

        free( pObjInfo);
        pObjInfo = 0;
    }

    if (pObjInfo)
        free( pObjInfo);

} while (0);

    if (fMutex)
        DosReleaseMutexSem( pcgd->thisCam->hmtxCam);

    if (!WinPostMsg( pcgd->hReply, CAMMSG_GETLIST, (MP)pcgd, 0))
        printf( "PtpGetListThread - WinPostMsg failed\n");

    return;
}

/****************************************************************************/

void        PtpSyncListThread( void * camgetdataptr)

{
    CAMGetData *     pcgd;
    BOOL             fMutex = FALSE;
    PULONG           pHndl;
    ULONG            cntHndl;
    ULONG            ctr;
    ULONG            ndxSync;
    CAMRecPtr        pcr;
    CAMRecPtr *      ppcr;
    CAMObjectInfoPtr pObjInfo = 0;

do {
    pcgd    = (CAMGetData*)camgetdataptr;
    pHndl   = (ULONG*)pcgd->pHandles->Handles;
    cntHndl = pcgd->cntItems;
    pcr     = pcgd->pcrFirst;
    ppcr    = pcgd->ppcrSync;
    ndxSync = 0;

    pcgd->rc = DosRequestMutexSem( pcgd->thisCam->hmtxCam, CAM_MUTEXWAIT);
    if (pcgd->rc)
        break;
    fMutex = TRUE;

    // review each handle on the list
    for ( ; cntHndl; cntHndl--, pHndl++) {

        // freeing pObjInfo here makes it easier to use 'continue' below
        if (pObjInfo) {
            free( pObjInfo);
            pObjInfo = 0;
        }

        pcgd->rc = GetObjectInfo( pcgd->thisCam->hCam, *pHndl, &pObjInfo);
        if (pcgd->rc) {
            printf( "PtpSyncListThread - GetObjectInfo #1 - rc= 0x%x\n", (int)pcgd->rc);
            if (pcgd->rc == CAMERR_NULLCAMERAPTR)
                break;

            pcgd->rc = ClearStall( pcgd->thisCam->hCam);
            if (pcgd->rc == CAMERR_NULLCAMERAPTR)
                break;

            DosSleep( 500);
            pcgd->rc = GetObjectInfo( pcgd->thisCam->hCam, *pHndl, &pObjInfo);
            if (pcgd->rc) {
                printf( "PtpSyncListThread - GetObjectInfo #2 - rc= 0x%x\n", (int)pcgd->rc);
                if (pcgd->rc != CAMERR_NULLCAMERAPTR)
                    ClearStall( pcgd->thisCam->hCam);
                break;
            }
        }

        // if this is a folder, skip it
        if (pObjInfo->ObjectFormat == PTP_OFC_Association)
            continue;

        // see if there's an existing record that matches this handle;
        // if so, zero-out the pointer to this record so that when we're
        // done, only records that weren't matched will remain;
        // ndxSync is a minor optimization to avoid going over the
        // beginning of the list repeatedly
        for (ctr = ndxSync; ctr < pcgd->cntSync; ctr++) {
            if (ppcr[ctr] == 0)
                continue;

            if (!strcmp( pObjInfo->Filename, (ppcr[ctr]->orgname ?
                         ppcr[ctr]->orgname : ppcr[ctr]->title))) {
                ppcr[ctr]->hndl = *pHndl;
                if (ctr == ndxSync)
                    ndxSync++;
                ppcr[ctr] = 0;
                break;
            }
        }

        // if a match was found, continue
        if (ctr < pcgd->cntSync)
            continue;

        if (!pcr) {
            pcgd->rc = CAMERR_NEEDRECORDS;
            printf( "PtpSyncListThread - no more records!\n");
            break;
        }

        // otherwise, format a new record
        SetData( pcr, *pHndl, pObjInfo);
        pcgd->cntRecords++;
        pcgd->pcrLast = pcr;
        pcr = (CAMRecPtr)pcr->mrc.preccNextRecord;
    }

    if (pObjInfo)
        free( pObjInfo);

} while (0);

    if (fMutex)
        DosReleaseMutexSem( pcgd->thisCam->hmtxCam);

    if (!WinPostMsg( pcgd->hReply, CAMMSG_SYNCLIST, (MP)pcgd, 0))
        printf( "PtpSyncListThread - WinPostMsg failed\n");

    return;
}

/****************************************************************************/
// functions that communicate with MSD cameras on a secondary thread
/****************************************************************************/
#if 0
// create a list of source directories with "\DCIM" and "\MISC";
// also, get a count of all files within them

void        MsdGetDataThread( void * camgetdataptr)

{
    CAMGetData *    pcgd;
    BOOL            fMutex = FALSE;
    CAMPtr          thisCam;
    ULONG           ulCnt;
    ULONG           cntDir;
    ULONG           cbSrc;
    ULONG           cbPathBuf = CAM_PATHBUFSIZE;
    ULONG           offs;
    HDIR            hDir = HDIR_CREATE;
    PFILEFINDBUF3   pFF;
    char *          pFindBuf = 0;
    char *      	pPathBuf = 0;
    char **         ppPath;
    char *          pSrc;
    char *          pDst;
    char            szFind[CCHMAXPATH];

do {
    pcgd  = (CAMGetData*)camgetdataptr;
    thisCam = pcgd->thisCam;

    // use of a mutex with mass storage devices is probably silly
    pcgd->rc = DosRequestMutexSem( pcgd->thisCam->hmtxCam, CAM_MUTEXWAIT);
    if (pcgd->rc)
        break;
    fMutex = TRUE;

    // confirm we actually have a drive - the 'drive' field
    // is formatted as "letter-colon-backslash-null"
    if (!pcgd->thisCam->drive[0]) {
        printf( "MsdGetDataThread - invalid drive\n");
        pcgd->rc = CAMERR_MISCELLANEOUS;
        break;
    }

    // alloc buffers for DosFindFirst/Next & for the found directories
    pFindBuf = malloc( CAM_FINDBUFSIZE);
    pPathBuf = malloc( cbPathBuf);
    if (!pFindBuf || !pPathBuf) {
        pcgd->rc = CAMERR_MALLOC;
        break;
    }

    pSrc = pPathBuf;
    pDst = pPathBuf;
    cntDir = 1;
    pcgd->cntItems = 0;

    // we confirmed DCIM was present when looking
    // for drives, so add it without further testing
    strcpy( pDst, pcgd->thisCam->drive);
    strcat( pDst, "DCIM\\");
    pDst = strchr( pDst, 0) + 1;

    // see if MISC is present
    strcpy( pDst, pcgd->thisCam->drive);
    strcat( pDst, "MISC");

    // if so, add a backslash & null then advance to next position;
    // if not, it will be nulled-out now and overwritten later
    if (!DosQueryPathInfo( pDst, FIL_STANDARD, pFindBuf, sizeof(FILESTATUS3))) {
        pDst = strchr( pDst, 0);
        *pDst++ = '\\';
        *pDst++ = 0;
        cntDir++;
    }
    *pDst = 0;

    // search for subdirectories by starting at the beginning of the list
    // and adding new entries at the end;  by stepping through the list,
    // we can get every subdirectory with having to use recursion
    while (*pSrc) {

        // construct a search string from the current entry
        cbSrc = strlen( pSrc);
        memcpy( szFind, pSrc, cbSrc);
        szFind[cbSrc] = '*';
        szFind[cbSrc+1] = 0;

        // get the first batch of entries;  this fetches directories
        // we can add them to the list, and files so we can get a count
        ulCnt = ~0;
        pcgd->rc = DosFindFirst( szFind, &hDir, CAM_ANYATTR, pFindBuf,
                                 CAM_FINDBUFSIZE, &ulCnt, FIL_STANDARD);
        if (pcgd->rc == ERROR_GEN_FAILURE) {
            printf( "MsdGetDataThread - DosFindFirst - first attempt - rc= 0x%lx\n", pcgd->rc);
            DosSleep( 100);
            pcgd->rc = DosFindFirst( szFind, &hDir, CAM_ANYATTR, pFindBuf,
                                     CAM_FINDBUFSIZE, &ulCnt, FIL_STANDARD);
        }
        
        if (pcgd->rc) {
            hDir = HDIR_CREATE;
            if (pcgd->rc != ERROR_NO_MORE_FILES) {
                printf( "DosFindFirst - rc= 0x%lx\n", pcgd->rc);
                break;
            }
        }

        // each time DosFindFirst/Next returns OK,
        // point at the beginning of the find buf
        while (!pcgd->rc) {
            pFF = (PFILEFINDBUF3)pFindBuf;
            offs = 0;

            // step through each entry until we reach
            // the end of the linked list
            do {
                pFF = (PFILEFINDBUF3)((char*)pFF + offs);

                // if this isn't a directory, add to the
                // file count, then continue
                if (!(pFF->attrFile & FILE_DIRECTORY)) {
                    pcgd->cntItems++;
                    continue;
                }

                // ignore the entries for current & parent directories
                if (!memcmp( pFF->achName, ".", 2) || !memcmp( pFF->achName, "..", 3))
                    continue;

                cntDir++;

                // if we run out of space in the path buffer,
                // reallocate it & adjust the pointers to it
                if (&pDst[CCHMAXPATH] >= &pPathBuf[cbPathBuf]) {
                    ULONG   offSrc = pSrc - pPathBuf;
                    ULONG   offDst = pDst - pPathBuf;

                    cbPathBuf += CAM_PATHBUFSIZE;
                    pPathBuf = realloc( pPathBuf, cbPathBuf);
                    pSrc = pPathBuf + offSrc;
                    pDst = pPathBuf + offDst;
                }

                // construct a f/q path from the source directory &
                // the found directory, add it to the end of the list,
                // and terminate with a backslash & two nulls
                memcpy( pDst, pSrc, cbSrc);
                pDst += cbSrc;
                memcpy( pDst, pFF->achName, pFF->cchName);
                pDst += pFF->cchName;
                memcpy( pDst, "\\\0", 3);
                pDst += 2;

            } while ((offs = pFF->oNextEntryOffset) != 0);

            // see if there are more files & directories
            ulCnt = ~0;
            pcgd->rc =  DosFindNext( hDir, pFindBuf, CAM_FINDBUFSIZE, &ulCnt);
            if (pcgd->rc == ERROR_GEN_FAILURE) {
                printf( "MsdGetDataThread - DosFindNext - first attempt - rc= 0x%lx\n", pcgd->rc);
                DosSleep( 100);
                pcgd->rc =  DosFindNext( hDir, pFindBuf, CAM_FINDBUFSIZE, &ulCnt);
            }
        }

        // if DosFindFirst/Next returns anything but zero, it trashes
        // hDir and attempts to reuse the previously valid handle fail;
        // consequently, we have to explicitly close hDir & get a new
        // handle for each new directory
        if (hDir != HDIR_CREATE) {
            if (DosFindClose( hDir))
                printf( "DosFindClose\n");
            hDir = HDIR_CREATE;
        }

        if (pcgd->rc && pcgd->rc != ERROR_NO_MORE_FILES) {
            printf( "DosFindNext - rc= 0x%lx\n", pcgd->rc);
            break;
        }

        // point at the next entry in the list;  if that
        // proves to be a null, the loop will exit
        pSrc += cbSrc + 1;
    }

    if (pcgd->rc && pcgd->rc != ERROR_NO_MORE_FILES)
        break;

    // allocate a buffer large enough to hold a list of pointers
    // to the paths, along with the paths themselves
    cbSrc = (pDst - pPathBuf) + 1;
    thisCam->paPaths = (char**)malloc( cbSrc + (cntDir + 1) * sizeof(char*));
    if (!thisCam->paPaths) {
        printf( "malloc failed for thisCam->paPaths\n");
        pcgd->rc = CAMERR_MALLOC;
        break;
    }

    // copy in the paths after the last pointer
    ppPath = thisCam->paPaths;
    pSrc = (char*)&ppPath[(cntDir + 1)];
    memcpy( pSrc, pPathBuf, cbSrc);

    // construct the list of pointers
    while (*pSrc) {
        *ppPath++ = pSrc;
        pSrc = strchr( pSrc, 0) + 1;
    }
    *ppPath = 0;

    // sort the list;  when done, the list will be in the same
    // order it would have been if we had searched recursively
    qsort( thisCam->paPaths, cntDir, sizeof(char*), MsdSortPaths);
    pcgd->rc = 0;
    printf( "%ld directories (%ld bytes) - %ld files\n",
            cntDir, cbSrc, pcgd->cntItems);

} while (0);

    if (pFindBuf)
        free( pFindBuf);
    if (pPathBuf)
        free( pPathBuf);

    if (fMutex)
        DosReleaseMutexSem( pcgd->thisCam->hmtxCam);

    if (!WinPostMsg( pcgd->hReply, CAMMSG_GETDATA, (MP)pcgd, 0))
        printf( "MsdGetDataThread - WinPostMsg failed\n");

    return;
}
#endif
/****************************************************************************/
/****************************************************************************/

// create a list of source directories with "\DCIM" and "\MISC";
// also, get a count of all files within them

void        MsdGetDataThread( void * camgetdataptr)

{
    CAMGetData *    pcgd;
    BOOL            fMutex = FALSE;
    CAMPtr          thisCam;
    ULONG           ulCnt;
    ULONG           cntDir;
    ULONG           cbSrc;
    ULONG           cbPathBuf = CAM_PATHBUFSIZE;
    ULONG           offs;
    ULONG           ulAttr;
    HDIR            hDir = HDIR_CREATE;
    PFILEFINDBUF3   pFF;
    char *          pFindBuf = 0;
    char *      	pPathBuf = 0;
    char *      	pPathStart;
    char **         ppPath;
    char *          pSrc;
    char *          pDst;
    char            szFind[CCHMAXPATH];

do {
    pcgd  = (CAMGetData*)camgetdataptr;
    thisCam = pcgd->thisCam;

    // use of a mutex with mass storage devices is probably silly
    pcgd->rc = DosRequestMutexSem( pcgd->thisCam->hmtxCam, CAM_MUTEXWAIT);
    if (pcgd->rc)
        break;
    fMutex = TRUE;

    // confirm we actually have a drive - the 'drive' field
    // is formatted as "letter-colon-backslash-null"
    if (!pcgd->thisCam->drive[0]) {
        printf( "MsdGetDataThread - invalid drive\n");
        pcgd->rc = CAMERR_MISCELLANEOUS;
        break;
    }

    // alloc buffers for DosFindFirst/Next & for the found directories
    pFindBuf = malloc( CAM_FINDBUFSIZE);
    pPathBuf = malloc( cbPathBuf);
    if (!pFindBuf || !pPathBuf) {
        pcgd->rc = CAMERR_MALLOC;
        break;
    }

    pSrc = pPathBuf;
    pDst = pPathBuf;

    // copy in the root directory;  for the root, we don't want any
    // files, just folders;  at the end of the first iteration,
    // ulAttr will be changed to fetch everything;  when done,
    // pPathStart will be used to exclude the root directory from
    // the path list
    strcpy( pDst, pcgd->thisCam->drive);
    pDst = strchr( pDst, 0) + 1;
    *pDst = 0;
    pPathStart = pDst;
    ulAttr = CAM_DIRONLYATTR;

    cntDir = 0;
    pcgd->cntItems = 0;

    // search for subdirectories by starting at the beginning of the list
    // and adding new entries at the end;  by stepping through the list,
    // we can get every subdirectory with having to use recursion
    while (*pSrc) {

        // construct a search string from the current entry
        cbSrc = strlen( pSrc);
        memcpy( szFind, pSrc, cbSrc);
        szFind[cbSrc] = '*';
        szFind[cbSrc+1] = 0;

        // get the first batch of entries;  this fetches directories
        // we can add them to the list, and files so we can get a count
        ulCnt = ~0;
        pcgd->rc = DosFindFirst( szFind, &hDir, ulAttr, pFindBuf,
                                 CAM_FINDBUFSIZE, &ulCnt, FIL_STANDARD);
        if (pcgd->rc == ERROR_GEN_FAILURE) {
            printf( "MsdGetDataThread - DosFindFirst - first attempt - rc= 0x%lx\n", pcgd->rc);
            DosSleep( 100);
            pcgd->rc = DosFindFirst( szFind, &hDir, ulAttr, pFindBuf,
                                     CAM_FINDBUFSIZE, &ulCnt, FIL_STANDARD);
        }
        
        if (pcgd->rc) {
            hDir = HDIR_CREATE;
            if (pcgd->rc != ERROR_NO_MORE_FILES) {
                printf( "DosFindFirst - rc= 0x%lx\n", pcgd->rc);
                break;
            }
        }

        // each time DosFindFirst/Next returns OK,
        // point at the beginning of the find buf
        while (!pcgd->rc) {
            pFF = (PFILEFINDBUF3)pFindBuf;
            offs = 0;

            // step through each entry until we reach
            // the end of the linked list
            do {
                pFF = (PFILEFINDBUF3)((char*)pFF + offs);

                // if this isn't a directory, add to the
                // file count, then continue
                if (!(pFF->attrFile & FILE_DIRECTORY)) {
                    pcgd->cntItems++;
                    continue;
                }

                // ignore the entries for current & parent directories
                if (!memcmp( pFF->achName, ".", 2) || !memcmp( pFF->achName, "..", 3))
                    continue;

                cntDir++;

                // if we run out of space in the path buffer,
                // reallocate it & adjust the pointers to it
                if (&pDst[CCHMAXPATH] >= &pPathBuf[cbPathBuf]) {
                    ULONG   offSrc = pSrc - pPathBuf;
                    ULONG   offDst = pDst - pPathBuf;

                    cbPathBuf += CAM_PATHBUFSIZE;
                    pPathBuf = realloc( pPathBuf, cbPathBuf);
                    pSrc = pPathBuf + offSrc;
                    pDst = pPathBuf + offDst;
                }

                // construct a f/q path from the source directory &
                // the found directory, add it to the end of the list,
                // and terminate with a backslash & two nulls
                memcpy( pDst, pSrc, cbSrc);
                pDst += cbSrc;
                memcpy( pDst, pFF->achName, pFF->cchName);
                pDst += pFF->cchName;
                memcpy( pDst, "\\\0", 3);
                pDst += 2;

            } while ((offs = pFF->oNextEntryOffset) != 0);

            // see if there are more files & directories
            ulCnt = ~0;
            pcgd->rc =  DosFindNext( hDir, pFindBuf, CAM_FINDBUFSIZE, &ulCnt);
            if (pcgd->rc == ERROR_GEN_FAILURE) {
                printf( "MsdGetDataThread - DosFindNext - first attempt - rc= 0x%lx\n", pcgd->rc);
                DosSleep( 100);
                pcgd->rc =  DosFindNext( hDir, pFindBuf, CAM_FINDBUFSIZE, &ulCnt);
            }
        }

        // if DosFindFirst/Next returns anything but zero, it trashes
        // hDir and attempts to reuse the previously valid handle fail;
        // consequently, we have to explicitly close hDir & get a new
        // handle for each new directory
        if (hDir != HDIR_CREATE) {
            if (DosFindClose( hDir))
                printf( "DosFindClose\n");
            hDir = HDIR_CREATE;
        }

        if (pcgd->rc && pcgd->rc != ERROR_NO_MORE_FILES) {
            printf( "DosFindNext - rc= 0x%lx\n", pcgd->rc);
            break;
        }

        // after the first iteration, we want to fetch everything
        ulAttr = CAM_ANYATTR;

        // point at the next entry in the list;  if that
        // proves to be a null, the loop will exit
        pSrc += cbSrc + 1;
    }

    if (pcgd->rc && pcgd->rc != ERROR_NO_MORE_FILES)
        break;

    // allocate a buffer large enough to hold a list of pointers
    // to the paths, along with the paths themselves
    cbSrc = (pDst - pPathStart) + 1;
    thisCam->paPaths = (char**)malloc( cbSrc + (cntDir + 1) * sizeof(char*));
    if (!thisCam->paPaths) {
        printf( "malloc failed for thisCam->paPaths\n");
        pcgd->rc = CAMERR_MALLOC;
        break;
    }

    // copy in the paths after the last pointer
    ppPath = thisCam->paPaths;
    pSrc = (char*)&ppPath[(cntDir + 1)];
    memcpy( pSrc, pPathStart, cbSrc);

    // construct the list of pointers
    while (*pSrc) {
        *ppPath++ = pSrc;
        pSrc = strchr( pSrc, 0) + 1;
    }
    *ppPath = 0;

    // sort the list;  when done, the list will be in the same
    // order it would have been if we had searched recursively
    qsort( thisCam->paPaths, cntDir, sizeof(char*), MsdSortPaths);
    pcgd->rc = 0;
    printf( "%ld directories (%ld bytes) - %ld files\n",
            cntDir, cbSrc, pcgd->cntItems);

} while (0);

    if (pFindBuf)
        free( pFindBuf);
    if (pPathBuf)
        free( pPathBuf);

    if (fMutex)
        DosReleaseMutexSem( pcgd->thisCam->hmtxCam);

    if (!WinPostMsg( pcgd->hReply, CAMMSG_GETDATA, (MP)pcgd, 0))
        printf( "MsdGetDataThread - WinPostMsg failed\n");

    return;
}

/****************************************************************************/
/****************************************************************************/

// replacing the backslashes with '\n' ensures that 'foo\bar' will sort
// lower that 'foo-bar' - otherwise, we'd get 'foo', 'foo-bar', 'foo\bar'

int         MsdSortPaths( const void *key, const void *element)
{
    char *  ptr;
    int     result;

    for (ptr = *((char**)key); *ptr; ptr++)
        if (*ptr == '\\')
            *ptr = '\n';

    for (ptr = *((char**)element); *ptr; ptr++)
        if (*ptr == '\\')
            *ptr = '\n';

    result = stricmp( *((char**)key), *((char**)element));

    for (ptr = *((char**)key); *ptr; ptr++)
        if (*ptr == '\n')
            *ptr = '\\';

    for (ptr = *((char**)element); *ptr; ptr++)
        if (*ptr == '\n')
            *ptr = '\\';

    return (result);
}

/****************************************************************************/

// use the path list to get a list of all files;
// this handles both the GetList and SyncList functions

void        MsdListThread( void * camgetdataptr)

{
    BOOL            fMutex = FALSE;
    ULONG           rc;
    ULONG           ulCnt;
    ULONG           offs;
    ULONG           ndxSync = 0;
    HDIR            hDir = HDIR_CREATE;
    PFILEFINDBUF3   pFF;
    CAMGetData *    pcgd;
    CAMRecPtr       pcr;
    char **         ppPath;
    char *          pFindBuf = 0;
    CAMObjectInfoPtr   pObjInfo = 0;
    char            szFind[CCHMAXPATH];

do {
    pcgd    = (CAMGetData*)camgetdataptr;
    ppPath  = pcgd->thisCam->paPaths;
    pcr     = pcgd->pcrFirst;

    pcgd->rc = DosRequestMutexSem( pcgd->thisCam->hmtxCam, CAM_MUTEXWAIT);
    if (pcgd->rc)
        break;
    fMutex = TRUE;

    // alloc a find buffer & an extended objectinfo struct
    // with enough room at its end to hold a f/q path
    pFindBuf = malloc( CAM_FINDBUFSIZE);
    pObjInfo = (CAMObjectInfoPtr)malloc( sizeof(CAMObjectInfo) + CCHMAXPATH);
    if (!pFindBuf || !pObjInfo) {
        pcgd->rc = CAMERR_MALLOC;
        break;
    }

    // if this is a sync, update cnr records to point at the new path list
    if (pcgd->paPathsSync)
        MsdSyncPaths( pcgd);

    // step through the path list until it hits a pointer to a null string
    while (*ppPath) {
        // contruct a search string
        strcpy( szFind, *ppPath);
        strcat( szFind, "*");
        ulCnt = ~0;

        rc = DosFindFirst( szFind, &hDir, CAM_FILEATTR, pFindBuf,
                           CAM_FINDBUFSIZE, &ulCnt, FIL_STANDARD);
        if (rc == ERROR_GEN_FAILURE) {
            printf( "MsdListThread - DosFindFirst - first attempt - rc= 0x%lx\n", rc);
            DosSleep( 100);
            rc = DosFindFirst( szFind, &hDir, CAM_FILEATTR, pFindBuf,
                               CAM_FINDBUFSIZE, &ulCnt, FIL_STANDARD);
        }
        if (rc) {
            hDir = HDIR_CREATE;
            if (rc != ERROR_NO_MORE_FILES) {
                printf( "MsdListThread - DosFindFirst - rc= 0x%lx\n", rc);
                pcgd->rc = rc;
                break;
            }
        }

        // while DosFindFirst/Next return OK,
        // point at the start of the find buffer
        while (!rc) {
            pFF = (PFILEFINDBUF3)pFindBuf;
            offs = 0;

            // for each entry in the find buf...
            do {
                pFF = (PFILEFINDBUF3)((char*)pFF + offs);

                // if this is a SyncList call, see if this entry's
                // f/q name matches a container record's;  if so, continue
                if (pcgd->fSync &&
                    MsdSyncRecord( pcgd, &ndxSync, *ppPath, pFF->achName))
                    continue;

                if (!pcr) {
                    rc = CAMERR_NEEDRECORDS;
                    break;
                }

                // fill in the objectinfo struct, transfer its data
                // to a container record, the do some bookkeeping
                MsdGetObjInfo( pFF, pObjInfo, *ppPath);
                SetData( pcr, 0, pObjInfo);
                pcgd->cntRecords++;
                pcgd->pcrLast = pcr;
                pcr = (CAMRecPtr)pcr->mrc.preccNextRecord;

            } while ((offs = pFF->oNextEntryOffset) != 0);

            if (rc)
                break;

            // look for more files
            ulCnt = ~0;
            rc = DosFindNext( hDir, pFindBuf, CAM_FINDBUFSIZE, &ulCnt);
            if (rc == ERROR_GEN_FAILURE) {
                printf( "MsdListThread - DosFindNext - first attempt - rc= 0x%lx\n", rc);
                DosSleep( 100);
                rc = DosFindNext( hDir, pFindBuf, CAM_FINDBUFSIZE, &ulCnt);
            }
        }

        // close the current search
        if (hDir != HDIR_CREATE) {
            DosFindClose( hDir);
            hDir = HDIR_CREATE;
        }

        // bail out if there was an error
        if (rc && rc != ERROR_NO_MORE_FILES) {
            if (rc == CAMERR_NEEDRECORDS)
                printf( "MsdListThread - no more records!\n");
            else
                printf( "MsdListThread - DosFindNext - rc= 0x%lx\n", rc);
            pcgd->rc = rc;
            break;
        }

        // advance to the next path on the list
        ppPath++;
    }

} while (0);

    if (pFindBuf)
        free( pFindBuf);
    if (pObjInfo)
        free( pObjInfo);
    if (pcgd->paPathsSync)
        free( pcgd->paPathsSync);

    if (fMutex)
        DosReleaseMutexSem( pcgd->thisCam->hmtxCam);

    if (!WinPostMsg( pcgd->hReply,
                    (pcgd->fSync ? CAMMSG_SYNCLIST : CAMMSG_GETLIST),
                    (MP)pcgd, 0))
        printf( "MsdListThread - WinPostMsg failed\n");


    return;
}

/****************************************************************************/

// try to find a container record with a f/q name that matches
// the current file;  pndxSync helps avoid searching through
// entries at the top of the list that have already been matched

BOOL        MsdSyncRecord( CAMGetData * pcgd, PULONG pndxSync,
                           char * pPath, char * pName)

{
    BOOL        fRtn = FALSE;
    ULONG       ctr;
    CAMRecPtr * ppcr;

    ppcr = pcgd->ppcrSync;
    for (ctr = *pndxSync; ctr < pcgd->cntSync; ctr++) {
        if (ppcr[ctr] == 0)
            continue;

        if (pPath == ppcr[ctr]->path &&
            !strcmp( pName, (ppcr[ctr]->orgname ?
                     ppcr[ctr]->orgname : ppcr[ctr]->title))) {
            if (ctr == *pndxSync)
                (*pndxSync)++;
            ppcr[ctr] = 0;
            fRtn = TRUE;
            break;
        }
    }

    return (fRtn);
}

/****************************************************************************/

void        MsdSyncPaths( CAMGetData * pcgd)

{
    ULONG       ctr;
    ULONG       ctrStart;
    char **     ppOld;
    char **     ppNew;
    char **     ppNewStart;
    CAMRecPtr * ppcr;

    ppNewStart = pcgd->thisCam->paPaths;
    ctrStart = 0;
    ppcr = pcgd->ppcrSync;

    // step through the old list of paths
    for (ppOld = pcgd->paPathsSync; *ppOld; ppOld++) {

        // see if there's an entry in the new list that matches
        for (ppNew = ppNewStart; *ppNew; ppNew++) {
            if (strcmp( *ppNew, *ppOld))
                continue;

            if (ppNew == ppNewStart)
                ppNewStart++;

            // if there's a match, find container records with a reference
            // to the old path & replace it with a pointer to the new one
            for (ctr = ctrStart; ctr < pcgd->cntSync; ctr++)
                if (ppcr[ctr]->path == *ppOld) {
                    ppcr[ctr]->path = *ppNew;
                    if (ctr == ctrStart)
                        ctrStart++;
                }

            break;
        }
    }

    return;
}

/****************************************************************************/

// fill in an objectinfo struct using info the the find buf;
// for JPEGs, call FileGetJPEGInfo() to get image info

void        MsdGetObjInfo( PFILEFINDBUF3 pFF, CAMObjectInfoPtr pObjInfo,
                           char * pPath)

{
    struct tm   dt;
    CAMJpgInfo  cji;
    char        szPath[CCHMAXPATH];

do {
    memset( pObjInfo, 0, sizeof(CAMObjectInfo) + CCHMAXPATH);

    pObjInfo->camType = CAM_TYPE_MSD;
    pObjInfo->FilePath = pPath;
    pObjInfo->ObjectCompressedSize = pFF->cbFile;
    if (pFF->attrFile & FILE_READONLY)
        pObjInfo->ProtectionStatus = 1;
    if (pFF->attrFile & (FILE_SYSTEM | FILE_HIDDEN))
        pObjInfo->ProtectionStatus |= 2;

    dt.tm_year = pFF->fdateLastWrite.year + 80;
    dt.tm_mon  = pFF->fdateLastWrite.month - 1;
    dt.tm_mday = pFF->fdateLastWrite.day;
    dt.tm_hour = pFF->ftimeLastWrite.hours;
    dt.tm_min  = pFF->ftimeLastWrite.minutes;
    dt.tm_sec  = pFF->ftimeLastWrite.twosecs;
    pObjInfo->CaptureDate = mktime( &dt);

    pObjInfo->Filename = (char*)&pObjInfo[1];
    strcpy( pObjInfo->Filename, pFF->achName);

    pObjInfo->ObjectFormat = FileGetFormat( pObjInfo->Filename);
    if (pObjInfo->ObjectFormat != PTP_OFC_JFIF)
        break;

    strcpy( szPath, pPath);
    strcat( szPath, pObjInfo->Filename);
    if (FileGetJPEGInfo( &cji, szPath, pFF->cbFile))
        break;

    if (cji.fmtImage == CAMJ_EXIF)
        pObjInfo->ObjectFormat = PTP_OFC_EXIF_JPEG;

    pObjInfo->ImagePixWidth  = cji.xImage;
    pObjInfo->ImagePixHeight = cji.yImage;
    pObjInfo->Rotation = cji.ulRot;

    // if there's no thumbnail but the image is thumbnail-sized,
    // use the entire file as the thumbnail
    if (!cji.offsThumb) {
        if (pObjInfo->ImagePixWidth != 160 ||
            pObjInfo->ImagePixHeight != 120)
            break;

        pObjInfo->ThumbFormat = pObjInfo->ObjectFormat;
        pObjInfo->ThumbPixWidth = pObjInfo->ImagePixWidth;
        pObjInfo->ThumbPixHeight = pObjInfo->ImagePixHeight;
        pObjInfo->ThumbCompressedSize = pObjInfo->ObjectCompressedSize;
        pObjInfo->ThumbOffset = 0;
        break;
    }

    if (cji.fmtThumb == CAMJ_JPEG)
        pObjInfo->ThumbFormat = PTP_OFC_JFIF;
    else
    if (cji.fmtThumb == CAMJ_RGB24)
        pObjInfo->ThumbFormat = PTP_OFC_BMP;
    else
        break;

    pObjInfo->ThumbPixWidth = cji.xThumb;
    pObjInfo->ThumbPixHeight = cji.yThumb;
    pObjInfo->ThumbCompressedSize = cji.cbThumb;
    pObjInfo->ThumbOffset = cji.offsThumb;

} while (0);

    return;
}

/****************************************************************************/
// Miscellanea - all but SetData() operate on the primary thread
/****************************************************************************/

// transfer data from an objectinfo struct to a container record

void        SetData( CAMRecPtr pcr, ULONG ulHndl, CAMObjectInfoPtr pObjInfo)

{
    struct tm *     ptm;
    char            szText[32];

    // mark & status
    pcr->mark = GetStatStuff()->hBlkIco;
    pcr->status = 0;

    if (pObjInfo->ProtectionStatus)
        pcr->status |= STAT_PROTECTED;
    if (pObjInfo->ProtectionStatus > 1)
        pcr->status |= STAT_PROTECTED2;

    // number & handle
    pcr->nbr = 0;
    pcr->hndl = ulHndl;

    // title
    pcr->title = pcr->sztitle;
    strcpy( pcr->sztitle, pObjInfo->Filename);
    pcr->orgname = 0;

    // group
    pcr->grpptr = GetDefaultGroup( &pcr->group);

    // image format
    pcr->fmtnbr = pObjInfo->ObjectFormat;
    if (!pcr->fmtnbr)
        pcr->fmt = szDashes;
    else {
        pcr->fmt = GetFormatName( pcr->fmtnbr);
        if (!pcr->fmt)
            pcr->fmt = GetFormatNameFromExt( pcr->title);
    }

    // image XY
    pcr->picx = pObjInfo->ImagePixWidth;
    pcr->picy = pObjInfo->ImagePixHeight;
    pcr->picxy = pcr->szpicxy;
    if (!pcr->picx && !pcr->picy)
        strcpy( pcr->szpicxy, szDashes);
    else
        if (sprintf( szText, "%dx%d", pObjInfo->ImagePixWidth,
                     pObjInfo->ImagePixHeight) <= 11)
            strcpy( pcr->szpicxy, szText);
        else
            strcpy( pcr->szpicxy, pszCOL_TooBig);

    // thumb format
    pcr->tnfmtnbr = pObjInfo->ThumbFormat;
    if (!pcr->tnfmtnbr)
        pcr->tnfmt = szDashes;
    else {
        pcr->tnfmt = GetFormatName( pcr->tnfmtnbr);
        if (!pcr->tnfmt)
            pcr->tnfmt = GetUnknownString();
    }

    // thumb XY
    pcr->tnx = pObjInfo->ThumbPixWidth;
    pcr->tny = pObjInfo->ThumbPixHeight;
    pcr->tnxy = pcr->sztnxy;
    if (!pcr->tnx && !pcr->tny)
        strcpy( pcr->sztnxy, szDashes);
    else
        if (sprintf( szText, "%dx%d", pObjInfo->ThumbPixWidth,
                     pObjInfo->ThumbPixHeight) <= 11)
            strcpy( pcr->sztnxy, szText);
        else
            strcpy( pcr->sztnxy, pszCOL_TooBig);

    // thumb size
    pcr->tnsize = pObjInfo->ThumbCompressedSize;

    // image size
    pcr->size = pObjInfo->ObjectCompressedSize;

    ptm = localtime( &pObjInfo->CaptureDate);

    // day
    pcr->day = aszDay[ptm->tm_wday];

    // date
    pcr->date.day   = (UCHAR)ptm->tm_mday;
    pcr->date.month = (UCHAR)ptm->tm_mon+1;
    pcr->date.year  = (USHORT)ptm->tm_year+1900;

    // time
    pcr->time.hours   = ptm->tm_hour;
    pcr->time.minutes = ptm->tm_min;
    pcr->time.seconds = ptm->tm_sec;

    // icon view info - the pszIcon field of the (MINI)RECORDCORE
    // has to have something or the container won't allocate space
    // for a title when in icon view;  the icon or bitmap field can
    // be empty because the container is ownerdraw in icon view
    pcr->mrc.pszIcon = pcr->sztitle;
    pcr->bmp = 0;

    // data that's unique to MSD files
    pcr->type = pObjInfo->camType;
    if (pcr->type == CAM_TYPE_MSD) {
        pcr->path   = pObjInfo->FilePath;
        pcr->rot    = pObjInfo->Rotation;
        pcr->tnoffs = pObjInfo->ThumbOffset;
    }

    return;
}

/****************************************************************************/
/****************************************************************************/

ULONG       ListRecords( HWND hCnr, CAMRecPtr ** pppcr)

{
    BOOL            fRtn = FALSE;
    ULONG           ctr = 0;
    CAMRecPtr       pcr;
    CAMRecPtr *     ppcr;
    char *          pErr = "";
    CNRINFO         ci;

do {
    *pppcr = 0;

    if (!WinSendMsg( hCnr, CM_QUERYCNRINFO, (MP)&ci, (MP)sizeof(CNRINFO))) {
        pErr = "CM_QUERYCNRINFO";
        break;
    }

    if (!ci.cRecords) {
        fRtn = TRUE;
        break;
    }

    ctr = ci.cRecords * sizeof(CAMRecPtr);
    *pppcr = (CAMRecPtr*)malloc( ctr);
    if (!*pppcr) {
        pErr = "malloc";
        break;
    }

    memset( *pppcr, 0, ctr);
    ppcr = *pppcr;

    ctr = 0;
    pcr = 0;
    NextRec( hCnr, &pcr);
    while (pcr && ctr < ci.cRecords) {
        *ppcr = pcr;
        ppcr++;
        ctr++;
        NextRec( hCnr, &pcr);
    }

    if (ctr < ci.cRecords)
        pErr = "CM_QUERYRECORD";
    else
    if (pcr)
        pErr = "record overflow";
    else
        fRtn = TRUE;

} while (0);

    if (!fRtn) {
        printf( "GetRecordList - %s\n", pErr);
        if (*pppcr) {
            free( *pppcr);
            *pppcr = 0;
        }
        ctr = 0;
    }

    return (ctr);
}

/****************************************************************************/

void        NextRec( HWND hCnr, CAMRecPtr * ppcrIn)

{
    *ppcrIn = (CAMRecPtr)WinSendMsg( hCnr, CM_QUERYRECORD, (MP)*ppcrIn,
               MPFROM2SHORT( (*ppcrIn ? CMA_NEXT : CMA_FIRST), CMA_ITEMORDER));

    if (*ppcrIn == (CAMRecPtr)-1)
        *ppcrIn = 0;

    return;
}

/****************************************************************************/

void        FreeLinkedRecords( HWND hCnr, CAMRecPtr pcr)

{
    CAMRecPtr   pcrSav;

    while (pcr) {
        pcrSav = (CAMRecPtr)pcr->mrc.preccNextRecord;
        if (!WinSendMsg( hCnr, CM_FREERECORD, (MP)&pcr, (MP)1))
            printf( "FreeLinkedRecords -  CM_FREERECORD\n");
        pcr = pcrSav;
    }

    return;
}

/****************************************************************************/

void        CalcCnrTotals( HWND hCnr)

{
    ULONG       ctr;
    ULONG       cnt;
    CAMRecPtr * ppcr = 0;
    STATSTUFF * pss;

do {
    // init the counters
    pss = ClearStatStuff();

    cnt = ListRecords( hCnr, &ppcr);
    if (!cnt)
        break;

    qsort( ppcr, cnt, sizeof(CAMRecPtr), SortObjects);

    for (ctr = 0; ctr < cnt; ctr++) {
        pss->ulTotSize += ppcr[ctr]->size;
        pss->ulTotCnt++;
        ppcr[ctr]->nbr = pss->ulTotCnt;

        if (STAT_NOTSAV(ppcr[ctr]->status)) {
            pss->ulSavCnt++;
            pss->ulSavSize += ppcr[ctr]->size;
        }
        else
        if (STAT_ISDEL(ppcr[ctr]->status)) {
            pss->ulDelCnt++;
            pss->ulDelSize += ppcr[ctr]->size;
        }
    }

} while (0);

    if (ppcr)
        free( ppcr);

    return;
}

/****************************************************************************/

int         SortObjects( const void *key, const void *element)

{
    char *  pszTitle1;
    char *  pszTitle2;
    char *  p1;
    char *  p2;
    int     result;

    pszTitle1 = (*(CAMRecPtr*)key)->orgname ?
                    (*(CAMRecPtr*)key)->orgname : (*(CAMRecPtr*)key)->title;
    pszTitle2 = (*(CAMRecPtr*)element)->orgname ?
                    (*(CAMRecPtr*)element)->orgname : (*(CAMRecPtr*)element)->title;

    p1 = strchr( pszTitle1, '.');
    p2 = strchr( pszTitle2, '.');

    if (p1 && p2 && &p1[-4] > pszTitle1 && &p2[-4] > pszTitle2) {
        result = memcmp( &p1[-4], &p2[-4], 4);
        if (result)
            return (result);
    }

    return (stricmp( pszTitle1, pszTitle2));
}

/****************************************************************************/
/****************************************************************************/

// The list of file handles generated by Dave M's camera had invalid entries
// at the beginning, followed by valid ones.  The invalid ones could be
// identified because their numbers were out of sequence.  This works around
// the issue by starting at the end of the list and ensuring that each
// preceeding entry has a lower handle number.  When it finds an out-of-
// sequence entry, it assumes that everything from that point back to the
// beginning is invalid and adjusts the list pointer & count accordingly.

void        AdjustList( CAMObjectHandlePtr pHandles)

{
    ULONG               ctr;
    ULONG               last;
    PULONG              pHndl;

    ctr = pHandles->cntHandles;
    pHndl = (ULONG*)pHandles->Handles;
    last = ~0;

    while (ctr) {
        ctr--;
        if (pHndl[ctr] >= last) {
            ctr++;
            break;
        }
        last = pHndl[ctr];
    }

    if (ctr) {
        printf( "AdjustList - ignoring first %d of %d files\n",
                (int)ctr, (int)pHandles->cntHandles);
        pHandles->Handles = (uint32_t*)&pHndl[ctr];
        pHandles->cntHandles -= ctr;
    }

    return;
}

/****************************************************************************/

void        CloseLoadingDlg( HWND hwnd)

{
    HWND    hDlg = GetHwnd( IDD_LOADING);

    if (hDlg) {
        WindowClosed( IDD_LOADING);
        WinDestroyWindow( hDlg);
    }

    return;
}

/****************************************************************************/

