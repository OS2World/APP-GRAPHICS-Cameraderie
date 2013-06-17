/****************************************************************************/
// camthumb.c
//
//  - fetch thumbnails & Exif info from camera, create thumbnail bitmaps
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

typedef struct _CAMThumb {
    HWND        hReply;
    CAMPtr      thisCam;
    BOOL        fRotate;
    CAMRecPtr   pcr[1];
} CAMThumb;

typedef CAMThumb *CAMThumbPtr;

/****************************************************************************/

void            FetchAllThumbnails( HWND hwnd);
void            FetchOneThumbnail( HWND hwnd, MPARAM mp1, MPARAM mp2);
void            PtpThumbnailThread( void * camthumbptr);
void            PtpGetOrientation( CAMPtr thisCam, CAMRecPtr pcr);
uint32_t        PtpFindOrientation( char * buf, uint32_t count, uint32_t* orientation);
uint32_t        PtpFindExifTiffHdr( char * buf, char * end, char** ppHdr);
uint32_t        PtpFind0thIfd( char * hdr, char * end, char** ppIFD, uint32_t* fLE);
void        	MsdThumbnailThread( void * camthumbptr);
ULONG           MsdGetThumb( CAMRecPtr pcr, char ** ppBuf);
void            SetOrientation( ULONG orientation, CAMRecPtr pcr);
HBITMAP         CreateThumbnailBmp( CAMRecPtr pcr, char * pBuf);

/****************************************************************************/

void        FetchAllThumbnails( HWND hwnd)

{
    BOOL            fRtn = FALSE;
    ULONG           ctr;
    HWND            hCnr;
    PFNCTHREAD      pfn;
    CAMRecPtr       pcr;
    CAMRecPtr *     ppcr;
    CAMThumbPtr     pct = 0;
    char *          pErr = 0;
    CNRINFO         ci;

do {
    hCnr = WinWindowFromID( hwnd, FID_CLIENT);

    if (!WinSendMsg( hCnr, CM_QUERYCNRINFO, (MP)&ci, (MP)sizeof(CNRINFO))) {
        pErr = "CM_QUERYCNRINFO";
        break;
    }

    ctr = ci.cRecords * sizeof(CAMRecPtr) + sizeof(CAMThumb);
    pct = (CAMThumbPtr)malloc( ctr);
    if (!pct) {
        pErr = "malloc";
        break;
    }
    memset( pct, 0, ctr);

    // fill in useful info
    pct->hReply  = hwnd;
    pct->thisCam = (CAMPtr)GetCamPtr();
    pct->fRotate = (QueryRotateOpts() & CAM_ROTATEONLOAD);

    ctr = 0;
    ppcr = pct->pcr;
    pcr = (CAMRecPtr)WinSendMsg( hCnr, CM_QUERYRECORD, (MP)0,
                                 MPFROM2SHORT( CMA_FIRST, CMA_ITEMORDER));

    while (pcr && pcr != (CAMRecPtr)-1 && ctr < ci.cRecords) {
        if (!pcr->bmp && pcr->tnsize) {
            *ppcr++ = pcr;
            ctr++;
        }
        pcr = (CAMRecPtr)WinSendMsg( hCnr, CM_QUERYRECORD, (MP)pcr,
                                     MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER));
    }

    if (!ctr)
        break;

    if (pcr) {
        if (pcr == (CAMRecPtr)-1)
            pErr = "CM_QUERYRECORD";
        else
            pErr = "record overflow";
        break;
    }

    ShowTime( "FetchThumb", FALSE);

    // start the thread
    if (((CAMPtr)GetCamPtr())->camType == CAM_TYPE_MSD)
        pfn = MsdThumbnailThread;
    else
        pfn = PtpThumbnailThread;

    if (_beginthread( pfn, 0, 0xF000, pct) == -1) {
        pErr = "FetchAllThumbnails - _beginthread failed";
        break;
    }

    fRtn = TRUE;

} while (0);

    if (!fRtn) {
        if (pErr)
            printf( "FetchAllThumbnails - %s failed\n", pErr);
        if (pct)
            free( pct);
        UpdateThumbWnd( hwnd);
    }

    return;
}

/****************************************************************************/

void        FetchOneThumbnail( HWND hwnd, MPARAM mp1, MPARAM mp2)

{
    CAMRecPtr   pcr = (CAMRecPtr)mp1;

    if (pcr == (CAMRecPtr)-1) {
        ShowTime( "FetchThumb", TRUE);
        UpdateThumbWnd( hwnd);
        return;
    }

    if (pcr && pcr->bmp) {
        WinSendDlgItemMsg( hwnd, FID_CLIENT, CM_INVALIDATERECORD, (MP)&pcr,
                           MPFROM2SHORT( 1, (CMA_NOREPOSITION | CMA_ERASE)));
        if (pcr->mrc.flRecordAttr & CRA_CURSORED)
            WinPostMsg( hwnd, CAMMSG_SHOWTHUMB, (MP)pcr, 0);
    }

    return;
}

/****************************************************************************/
// everything below runs on a secondary thread
/****************************************************************************/

void        PtpThumbnailThread( void * camthumbptr)

{
    uint32_t        rc = 0;
    uint32_t        errCtr = 0;
    char *          pBuf = 0;
    CAMThumbPtr     pct = (CAMThumbPtr)camthumbptr;
    CAMRecPtr *     ppcr = pct->pcr;
    CAMRecPtr       pcr;

do{
    rc = DosRequestMutexSem( pct->thisCam->hmtxCam, CAM_MUTEXWAIT);
    if (rc)
        break;

    while (*ppcr) {
        pcr = *ppcr;

        // get the thumb from the camera
        rc = GetThumb( pct->thisCam->hCam, pcr->hndl, &pBuf);
        if (rc) {
            printf( "PtpThumbnailThread - GetThumb #1 - handle= %d  rc= 0x%x\n",
                    (int)pcr->hndl, (int)rc);
            if (rc == CAMERR_NULLCAMERAPTR)
                break;

            rc = ClearStall( pct->thisCam->hCam);
            if (rc == CAMERR_NULLCAMERAPTR)
                break;

            rc = GetThumb( pct->thisCam->hCam, pcr->hndl, &pBuf);
            if (rc) {
                printf( "PtpThumbnailThread - GetThumb #2 - handle= %d  rc= 0x%x\n",
                        (int)pcr->hndl, (int)rc);
                if (rc == CAMERR_NULLCAMERAPTR)
                    break;

                ClearStall( pct->thisCam->hCam);
                if (++errCtr > 2)
                    break;
            }
        }

        if (!rc) {
            errCtr = 0;

            // if rotate-on-load is set, get the
            // Exif orientation info from the main image
            if (pct->fRotate &&
                pcr->fmtnbr == PTP_OFC_EXIF_JPEG &&
                pcr->size >= 500)
                    PtpGetOrientation( pct->thisCam, pcr);

            // create the bitmap
            pcr->bmp = CreateThumbnailBmp( pcr, pBuf);
        }

        // free the memory allocated by GetThumb()
        if (pBuf) {
            free( pBuf);
            pBuf = 0;
        }

        // let the main thread know we finished processing this record
        if (pcr->bmp &&
            !WinPostMsg( pct->hReply, CAMMSG_FETCHTHUMBS, (MP)pcr, 0))
            printf( "PtpThumbnailThread - WinPostMsg failed\n");

        ppcr++;
    }

    DosReleaseMutexSem( pct->thisCam->hmtxCam);

} while (0);

    WinPostMsg( pct->hReply, CAMMSG_FETCHTHUMBS, (MP)-1, (MP)rc);

    free( pct);

    return;
}

/***************************************************************************/

// gets the Exif orientation info from the main image, then sets the
// orientation flags on the container record;  it does so by getting
// the first 500 bytes of the image which almost certainly should
// contain the Exif header - if not, too bad

void        PtpGetOrientation( CAMPtr thisCam, CAMRecPtr pcr)

{
    uint32_t        rc = 0;
    uint32_t        cnt;
    uint32_t        orientation;
    CAMReadData     crd;
    char *          pbuf = crd.data;

do {
    cnt = sizeof(crd.data);

    // there are 3 ways to get the first 500 bytes;
    // the one we use depends on what the camera supports
    if (thisCam->camFlags & CAM_PARTIAL_STANDARD)
        rc = GetPartialObject( thisCam->hCam, pcr->hndl, 0,
                               &cnt, &pbuf);
    else
    if (thisCam->camFlags & CAM_PARTIAL_CANON)
        rc = CanonGetPartialObject( thisCam->hCam, pcr->hndl, 0,
                                    &cnt, 0, &pbuf);
    else {
        // this one is ugly, start fetching the entire image,
        // then kill it after the first read (it works, though)
        rc = ReadObjectBegin( thisCam->hCam, pcr->hndl, &cnt, &crd);
        if (rc == PTP_RC_OK)
            rc = CancelRequest( thisCam->hCam);
    }

    // something went wrong
    if (rc || cnt < 500) {
        camcli_error( "GetPartial failed for handle %ld - count= %ld  rc= 0x%lx",
                      pcr->hndl, cnt, rc);
        if (rc)
            ClearStall( thisCam->hCam);
        break;
    }

    // parse the Exif info looking for orientation info
    rc = PtpFindOrientation( crd.data, cnt, &orientation);
    if (rc) {
        camcli_error( "FindOrientation failed for handle %ld - rc= 0x%lx",
                      pcr->hndl, rc);
        break;
    }

    SetOrientation( orientation, pcr);

} while (0);

    return;
}

/****************************************************************************/

uint32_t    PtpFindOrientation( char * buf, uint32_t count, uint32_t* orientation)

{
    uint32_t    ulRtn;
    uint32_t    fLE;
    uint32_t    cntTags;
    char *      pHdr;
    char *      ptr;
    char *      end = &buf[count];

do {
    *orientation = 0;

    ulRtn = PtpFindExifTiffHdr( buf, end, &pHdr);
    if (ulRtn)
        break;

    ulRtn = PtpFind0thIfd( pHdr, end, &ptr, &fLE);
    if (ulRtn)
        break;

    if (&ptr[1] >= end) {
        ulRtn = CAMERR_NEEDDATA;
        break;
    }

    // get the number of tags
    if (fLE)
        cntTags = *((uint16_t*)ptr);
    else
        cntTags = ((uint8_t)ptr[0] << 8) + (uint8_t)ptr[1];
    ptr += 2;

    // go through the array of 12-byte tags looking for the orientation tag
    // (this is supposed to be in ascending order but I don't trust "them")

    ulRtn = CAMERR_NOORIENT;
    while (cntTags && ptr+12 <= end) {
        if ((fLE && *((uint16_t*)ptr) == 0x0112) || *((uint16_t*)ptr) == 0x1201) {
            if (fLE)
                *orientation = (uint8_t)ptr[8] + ((uint8_t)ptr[9] << 8);
            else
                *orientation = ((uint8_t)ptr[8] << 8) + (uint8_t)ptr[9];

            // if the orientation is between 1 & 8, signal success,
            // otherwise, clear the value
            if (*orientation >= 1 && *orientation <= 8)
                ulRtn = 0;
            else
                *orientation = 0;

            break;
        }

        cntTags--;
        ptr += 12;
    }

} while (0);

    return (ulRtn);
}

/****************************************************************************/

uint32_t    PtpFindExifTiffHdr( char * buf, char * end, char** ppHdr)

{
    uint32_t    ulRtn = CAMERR_NEEDDATA;
    uint32_t    lth;
    char        marker;
    char *      ptr = buf;

do {
    // the JPEG tag must be the first two bytes
    if (*ptr++ != (char)0xff ||
        *ptr++ != (char)M_SOI) {
        ulRtn = CAMERR_BADFORMAT;
        break;
    }

    // the Exif tag is _supposed_ to appear immediately after SOI,
    // but then, who actually follows the standards?
    while (ptr < end) {
        // skip over junk
        while (ptr < end && *ptr != (char)0xff)
            ptr++;
        if (ptr >= end)
            break;

        // skip over multiple ff's
        while (ptr < end && *ptr == (char)0xff)
            ptr++;
        if (ptr >= end)
            break;

        // if we've gone too far or don't have any more data, exit
        marker = *ptr++;
        if (&ptr[1] >= end)
            break;
        if (marker == (char)M_EOI || marker == (char)M_SOS) {
            ulRtn = CAMERR_NOEXIF;
            break;
        }

        // get the marker length (which has to be at least 2)
        lth = ((uint8_t)ptr[0] << 8) + (uint8_t)ptr[1];
        if (lth < 2) {
            ulRtn = CAMERR_BADFORMAT;
            break;
        }

        ptr += 2;
        lth -= 2;

        // if this is an APP1 that's minimally long enough
        // and says "Exif", we've found our marker
        if (&ptr[5] >= end)
            break;

        if (marker == (char)M_APP1 && lth >= 6 &&
            !memcmp( ptr, "Exif\0", 6)) {
            ptr += 6;
            ulRtn = 0;
            break;
        }

        // advance to next marker
        ptr += lth;
    }

} while (0);

    *ppHdr = ptr;

    return (ulRtn);
}

/****************************************************************************/

uint32_t    PtpFind0thIfd( char * hdr, char * end, char** ppIFD, uint32_t* fLE)

{
    uint32_t    ulRtn = CAMERR_BADFORMAT;
    uint32_t    offs;
    char *      ptr = hdr;

do {

    // is there enough data to examine the 8-byte TIFF header?
    if (&ptr[7] >= end) {
        ulRtn = CAMERR_NEEDDATA;
        break;
    }

    // get the byte order
    if (ptr[0] == 'I' && ptr[1] == 'I')
        *fLE = TRUE;
    else
    if (ptr[0] == 'M' && ptr[1] == 'M')
        *fLE = FALSE;
    else
        break;

    // this is always '0x2a'
    ptr += 2;
    if (*fLE) {
        if (*((uint16_t*)ptr) != 0x002a)
            break;
    }
    else
        if (*((uint16_t*)ptr) != 0x2a00)
            break;

    // advance to the beginning of the IFD
    ptr += 2;
    if (*fLE)
        offs = *((uint32_t*)ptr);
    else
        offs = (((uint8_t)ptr[0] << 24) + ((uint8_t)ptr[1] << 16) +
                ((uint8_t)ptr[2] << 8)  +  (uint8_t)ptr[3]);

    if (offs < 8 || offs >= 0x10000)
        break;

    ptr += offs - 4;

    if (ptr >= end)
        ulRtn = CAMERR_NEEDDATA;
    else
        ulRtn = 0;

} while (0);

    *ppIFD = ptr;

    return (ulRtn);
}

/****************************************************************************/
/****************************************************************************/

void        MsdThumbnailThread( void * camthumbptr)

{
    uint32_t        rc = 0;
    uint32_t        errCtr = 0;
    char *          pBuf = 0;
    CAMThumbPtr     pct = (CAMThumbPtr)camthumbptr;
    CAMRecPtr *     ppcr = pct->pcr;
    CAMRecPtr       pcr;

do{
    rc = DosRequestMutexSem( pct->thisCam->hmtxCam, CAM_MUTEXWAIT);
    if (rc)
        break;

    while (*ppcr) {
        pcr = *ppcr;
        ppcr++;

        if (!pcr->tnsize)
            continue;

        // get the thumb from the file
        rc = MsdGetThumb( pcr, &pBuf);
        if (rc) {
            if (++errCtr > 2)
                break;
            continue;
        }

        errCtr = 0;

        // if rotate-on-load is set, set the
        // Exif orientation info from the main image
        if (pct->fRotate && pcr->rot)
            SetOrientation( pcr->rot, pcr);

        // create the bitmap
        pcr->bmp = CreateThumbnailBmp( pcr, pBuf);

        // free the memory allocated by GetThumb()
        free( pBuf);
        pBuf = 0;

        // let the main thread know we finished processing this record
        if (pcr->bmp &&
            !WinPostMsg( pct->hReply, CAMMSG_FETCHTHUMBS, (MP)pcr, 0))
            printf( "MsdThumbnailThread - WinPostMsg failed\n");
    }

    DosReleaseMutexSem( pct->thisCam->hmtxCam);

} while (0);

    WinPostMsg( pct->hReply, CAMMSG_FETCHTHUMBS, (MP)-1, (MP)rc);

    if (pBuf)
        free( pBuf);

    free( pct);

    return;
}

/***************************************************************************/

ULONG       MsdGetThumb( CAMRecPtr pcr, char ** ppBuf)

{
    ULONG       rc = 0;
    ULONG       ul;
    HFILE       hFile = 0;
    char        szPath[CCHMAXPATH];

do {
    *ppBuf = malloc( pcr->tnsize);
    if (!*ppBuf) {
        rc = CAMERR_MALLOC;
        break;
    }

    strcpy( szPath, pcr->path);
    strcat( szPath, (pcr->orgname ? pcr->orgname : pcr->title));

    rc = DosOpen( szPath, &hFile, &ul, 0, 0,
                  (OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS),
                  (OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_SEQUENTIAL |
                   OPEN_SHARE_DENYWRITE | OPEN_ACCESS_READONLY), 0);
    if (rc) {
        printf( "MsdGetThumb - DosOpen - rc= 0x%lx\n", rc);
        break;
    }

    rc = DosSetFilePtr( hFile, pcr->tnoffs, FILE_BEGIN, &ul);
    if (rc) {
        printf( "MsdGetThumb - DosSetFilePtr - rc= 0x%lx\n", rc);
        break;
    }
    if (ul != pcr->tnoffs) {
        rc = CAMERR_NEEDDATA;
        printf( "MsdGetThumb - DosSetFilePtr - seek failed\n");
        break;
    }

    ul = FileReadFile( hFile, *ppBuf, pcr->tnsize, szPath);
    if (!ul)
        rc = CAMERR_NEEDDATA;

} while (0);

    if (hFile)
        DosClose( hFile);

    if (rc && *ppBuf) {
        free( *ppBuf);
        *ppBuf = 0;
    }

    return (rc);
}

/****************************************************************************/
/****************************************************************************/

void        SetOrientation( ULONG orientation, CAMRecPtr pcr)

{
    // set the record's orientation flag
    switch (orientation) {
        case 1:
        case 2:
            pcr->status = STAT_SETROT0(pcr->status);
            break;

        case 3:
        case 4:
            pcr->status = STAT_SETROT180(pcr->status);
            break;

        case 5:
        case 6:
            pcr->status = STAT_SETROT270(pcr->status);
            break;

        case 7:
        case 8:
            pcr->status = STAT_SETROT90(pcr->status);
            break;

        default:
            camcli_error( "Invalid orientation flag for handle %ld - orientation= %ld",
                          pcr->hndl, orientation);
            pcr->status = STAT_SETROT0(pcr->status);
            break;
    }

    return;
}

/****************************************************************************/

HBITMAP     CreateThumbnailBmp( CAMRecPtr pcr, char * pBuf)

{
    HBITMAP             hBmp = 0;
    uint32_t            ctr;
    HDC                 hdc = 0;
    HPS                 hps = 0;
    char *              pErr = 0;
    BITMAPINFOHEADER2 * pbmih = 0;
    RGB2 *              pRGB;
    SIZEL               sizl;
    CAMJThumb           cjt;

do {
    // decode the JPEG data from the camera
    memset( &cjt, 0, sizeof(cjt));
    cjt.pSrc = pBuf;
    cjt.cbSrc = pcr->tnsize;
    ctr = JpgDecodeThumbnail( &cjt);
    if (ctr) {
        printf( "JpgDecodeThumbnail - rc= 0x%x\n", (int)ctr);
        break;
    }

    // allocate a BITMAPINFOHEADER2 / BITMAPINFO2
    // suitable for 24-bit color or 8-bit grayscale
    ctr = sizeof(BITMAPINFOHEADER2);
    if (cjt.cbPix == 1)
        ctr += 256 * sizeof(RGB2);
        
    pbmih = (BITMAPINFOHEADER2*)malloc( ctr);
    memset( pbmih, 0, ctr);

    // if grayscale, create a default colormap
    if (cjt.cbPix == 1)
        for (ctr = 0, pRGB = (RGB2*)&pbmih[1]; ctr < 256; ctr++, pRGB++) {
            pRGB->bBlue  = ctr;
            pRGB->bGreen = ctr;
            pRGB->bRed   = ctr;
        }

    // init the BMIH
    pbmih->cbFix         = sizeof(BITMAPINFOHEADER2);
    pbmih->cx            = cjt.cxDst;
    pbmih->cy            = cjt.cyDst;
    pbmih->cPlanes       = 1;
    pbmih->cBitCount     = cjt.cbPix * 8;
    pbmih->ulCompression = BCA_UNCOMP;
    pbmih->cbImage       = cjt.cbDst;

    // create a memory DC
    hdc = DevOpenDC( 0, OD_MEMORY, "*", 0L, 0, 0);
    if (!hdc) {
        pErr = "DevOpenDC";
        break;
    }

    // create a square PS using the longer side so we can
    // reuse the PS if we have to rotate the bitmap
    sizl.cx = ((pbmih->cx >= pbmih->cy) ? pbmih->cx : pbmih->cy);
    sizl.cy = sizl.cx;

    hps = GpiCreatePS( 0, hdc, &sizl, PU_PELS | GPIF_DEFAULT | GPIT_MICRO | GPIA_ASSOC);
    if (!hps) {
        pErr = "GpiCreatePS";
        break;
    }

    // create a bitmap from the data
    hBmp = GpiCreateBitmap( hps, pbmih, CBM_INIT, cjt.pDst, (BITMAPINFO2*)pbmih);
    if (!hBmp) {
        pErr = "GpiCreateBitmap";
        break;
    }

    // if the bitmap should be rotated, replace the original one
    if (STAT_GETROT(pcr->status)) {
        HBITMAP hBmpRot = RotateBitmap( hps, hBmp, pbmih,
                                        STAT_GETROT(pcr->status));
        if (hBmpRot) {
            GpiDeleteBitmap( hBmp);
            hBmp = hBmpRot;
        }
    }

} while (0);

    if (pErr)
        printf( "CreateThumbnailBmp - %s failed\n", pErr);

    if (hps) {
        GpiSetBitmap( hps, 0);
        GpiDestroyPS( hps);
    }
    if (hdc)
        DevCloseDC( hdc);
    if (pbmih)
        free( pbmih);
    if (cjt.pDst)
        free( cjt.pDst);

    return (hBmp);
}

/***************************************************************************/
/****************************************************************************/

