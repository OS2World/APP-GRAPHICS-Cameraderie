/****************************************************************************/
// camfile.c
//
//  - fetch photo data from mass storage devices
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
#define INCL_DOSEXCEPTIONS

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <os2.h>
#include "camcodes.h"
#include "cammisc.h"

/****************************************************************************/

typedef struct _CAMJFind {
    BYTE *  pCur;
    USHORT  cbMarker;
    BYTE    bMarker;
} CAMJFind;


typedef struct _CAMJXRR {
    EXCEPTIONREGISTRATIONRECORD xrr;
    char *      szFile;
    HFILE       hFile;
    LONG        lFilePtr;
    LONG        lEOF;
    BYTE *      pBase;
    BYTE *      pEnd;
    jmp_buf     jmpbuf;
} CAMJXRR;

/****************************************************************************/

#define MAKE4CC(a,b,c,d)    (ULONG)(((BYTE)(a)) + ((BYTE)(b) << 8) + ((BYTE)(c) << 16) + ((UCHAR)(d) << 24))

#define ERRMSG(x)   {pErr = (x); break;}

/****************************************************************************/

ULONG           FileReadFile( HFILE hFile, char * pBuf,
                              ULONG cbRequest, char * path);
void            ShowTime( char * szText, BOOL fEnd);

ULONG           FileGetFormat( char * szFilename);
ULONG           FileGetJPEGInfo( CAMJpgInfo * pji, char * szFile, ULONG cbFile);
ULONG APIENTRY  FileExceptionHandler( PEXCEPTIONREPORTRECORD pReport,
                             struct _EXCEPTIONREGISTRATIONRECORD * pXReg,
                             PCONTEXTRECORD pContext, PVOID pDispatch);
ULONG           FileIdentifyFile( CAMJpgInfo * pji, BYTE * pBase);
ULONG           FileIsJpeg( CAMJFind * pjf);
ULONG           FileFindMarker( CAMJFind * pjf);
ULONG           FileParseApp0( CAMJpgInfo * pji, BYTE * pApp0, ULONG cbApp0);
ULONG           FileParseApp1( CAMJpgInfo * pji, BYTE * pApp1, ULONG cbApp1);
ULONG           FileFindExifThumb( CAMJpgInfo * pji, BYTE * pIfd1,
                                   BYTE * pTiff, BOOL fLE);
ULONG           FileParseJPEGThumb( CAMJpgInfo * pji);
ULONG           FileParseSof( CAMJFind * pjf, PUSHORT pX, PUSHORT pY);

/****************************************************************************/
/****************************************************************************/

// This accommodates flakey flash cards like the one I tested with.
// If the first read attempt fails or returns fewer bytes than
// requested, it tries again after a pause.

ULONG       FileReadFile( HFILE hFile, char * pBuf, ULONG cbRequest, char * path)

{
    ULONG   rc;
    ULONG   ul;
    ULONG   ulRtn;

do {
    ulRtn = 0;
    rc = DosRead( hFile, pBuf, cbRequest, &ulRtn);
    if (!rc && ulRtn == cbRequest)
        break;

    printf( "FileReadFile - first attempt - rc= 0x%lx  request= 0x%lx  read= 0x%lx\n   file= %s\n",
            rc, cbRequest, ulRtn, path);

    DosSleep( 100);

    rc = DosSetFilePtr( hFile, -(ulRtn), FILE_CURRENT, &ul);
    if (rc) {
        printf( "FileReadFile - DosSetFilePtr - rc= 0x%lx\n", rc);
        ulRtn = 0;
        break;
    }

    ulRtn = 0;
    rc = DosRead( hFile, pBuf, cbRequest, &ulRtn);
    if (!rc && ulRtn == cbRequest)
        break;

    printf( "FileReadFile - second attempt - rc= 0x%lx  request= 0x%lx  read= 0x%lx\n",
            rc, cbRequest, ulRtn);

    ulRtn = 0;

} while (0);

    return (ulRtn);

}

/****************************************************************************/
/****************************************************************************/

void        ShowTime( char * szText, BOOL fEnd)

{
    DATETIME        DT;

    DosGetDateTime( &DT);
    printf( "%s %s= %02hhd:%02hhd:%02hhd.%02hhd\n",
            (fEnd ? "end  " : "begin"), szText,
            DT.hours, DT.minutes, DT.seconds, DT.hundredths);

    return;
}

/****************************************************************************/
/****************************************************************************/

// this tries to match extensions to PTP format codes

ULONG       FileGetFormat( char * szFilename)

{
    ULONG       ulRtn = PTP_OFC_Undefined;
    char *      ptr;
    COUNTRYCODE cc = {0,0};
    union CAMJEXT {
                    char    ch[4];
                    ULONG   ul;
                  } ext;

    ptr = strrchr( szFilename, '.');
    if (!ptr)
        return (ulRtn);

    ptr++;
    strncpy( ext.ch, ptr, 4);
    DosMapCase( 4, &cc, ext.ch);

    switch (ext.ul) {

        case MAKE4CC('J','P','G','\0'):
        case MAKE4CC('T','H','M','\0'):
        case MAKE4CC('J','P','E','G'):
            ulRtn = PTP_OFC_JFIF;
            break;

        case MAKE4CC('A','V','I','\0'):
            ulRtn = PTP_OFC_AVI;
            break;

        case MAKE4CC('M','O','V','\0'):
            ulRtn = PTP_OFC_QT;
            break;

        case MAKE4CC('M','P','G','\0'):
        case MAKE4CC('M','P','E','G'):
            ulRtn = PTP_OFC_MPEG;
            break;

        case MAKE4CC('A','S','F','\0'):
            ulRtn = PTP_OFC_ASF;
            break;

        case MAKE4CC('W','A','V','\0'):
            ulRtn = PTP_OFC_WAV;
            break;

        case MAKE4CC('M','P','3','\0'):
            ulRtn = PTP_OFC_MP3;
            break;

        case MAKE4CC('A','I','F','\0'):
        case MAKE4CC('A','I','F','F'):
            ulRtn = PTP_OFC_AIFF;
            break;

        case MAKE4CC('M','R','K','\0'):
            ulRtn = PTP_OFC_DPOF;
            break;

        case MAKE4CC('T','X','T','\0'):
            ulRtn = PTP_OFC_Text;
            break;

        case MAKE4CC('H','T','M','\0'):
        case MAKE4CC('H','T','M','L'):
            ulRtn = PTP_OFC_HTML;
            break;

        case MAKE4CC('T','I','F','\0'):
        case MAKE4CC('T','I','F','F'):
        case MAKE4CC('R','A','W','\0'):
            ulRtn = PTP_OFC_TIFF;
            break;

        case MAKE4CC('B','M','P','\0'):
            ulRtn = PTP_OFC_BMP;
            break;

        case MAKE4CC('G','I','F','\0'):
            ulRtn = PTP_OFC_GIF;
            break;

        case MAKE4CC('P','N','G','\0'):
            ulRtn = PTP_OFC_PNG;
            break;

        case MAKE4CC('F','P','X','\0'):
            ulRtn = PTP_OFC_FlashPix;
            break;

        case MAKE4CC('J','P','2','\0'):
            ulRtn = PTP_OFC_JP2;
            break;

        case MAKE4CC('J','P','X','\0'):
            ulRtn = PTP_OFC_JPX;
            break;

        case MAKE4CC('C','R','W','\0'):     // Canon    .CRW
        case MAKE4CC('C','R','2','\0'):     // Canon    .CR2
        case MAKE4CC('N','E','F','\0'):     // Nikon    .NEF
        case MAKE4CC('O','R','F','\0'):     // Olympus  .ORF
        case MAKE4CC('P','E','F','\0'):     // Pentax   .PEF
        case MAKE4CC('D','C','R','\0'):     // Kodak    .DCR
        case MAKE4CC('K','D','C','\0'):     // Kodak    .KDC
        case MAKE4CC('S','R','F','\0'):     // Sony     .SRF
        case MAKE4CC('M','R','W','\0'):     // Minolta  .MRW
        case MAKE4CC('R','A','F','\0'):     // Fuji     .RAF
        case MAKE4CC('D','N','G','\0'):     // Adobe    .DNG
            ulRtn = PTP_OFC_CAMRAW;
            break;
    }

    return (ulRtn);
}

/****************************************************************************/
/****************************************************************************/

// This scans a JPEG file looking for image info that a PTP camera
// would return in an ObjectInfo struct.  It also returns some
// file-specific info such as the offset of the thumbnail.

ULONG       FileGetJPEGInfo( CAMJpgInfo * pji, char * szFile, ULONG cbFile)

{
    ULONG       rc;
    ULONG       ul;
    ULONG       cbAlloc;
    BOOL        fHandler = FALSE;
    CAMJXRR     reg;

do
{
    memset( &reg, 0, sizeof(CAMJXRR));
    memset( pji, 0, sizeof(CAMJpgInfo));

    rc = DosOpen( szFile, &reg.hFile, &ul, 0, 0,
                  OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                  OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYWRITE |
                  OPEN_ACCESS_READONLY, 0);
    if (rc) {
        printf( "DosOpen - rc= 0x%x\n", (int)rc);
        break;
    }

    // allocate a lot of uncommited memory
    cbAlloc = ((cbFile + 0xfff) & ~0xfff);
    rc = DosAllocMem( (void**)(&reg.pBase), cbAlloc,
                      PAG_READ | PAG_WRITE);
    if (rc) {
        printf( "DosAllocMem - rc= 0x%x\n", (int)rc);
        break;
    }

    // get up to the first 16k of the file - this is enough
    // for my Canon but not enough for a Sony I've tried

    ul = (cbFile >= 0x4000) ? 0x4000 : cbAlloc;
    rc = DosSetMem( reg.pBase, ul, PAG_COMMIT | PAG_DEFAULT);
    if (rc) {
        printf( "FileGetJPEGInfo - DosSetMem - rc= 0x%x\n", (int)rc);
        break;
    }

    ul = (cbFile >= 0x4000) ? 0x4000 : cbFile;
    ul = FileReadFile( reg.hFile, reg.pBase, ul, szFile);
    if (!ul) {
        rc = CAMERR_NEEDDATA;
        break;
    }

    // any access beyond 16k will generate an exception;
    // the handler will commit the page and read the corresponding
    // 4k from the file;  if it encounters a file error, it will
    // longjmp() back to here so we can abort
    reg.lFilePtr += ul;
    reg.lEOF = cbFile;
    reg.pEnd = reg.pBase + cbAlloc;
    reg.szFile = szFile;
    reg.xrr.ExceptionHandler = FileExceptionHandler;

    // since the handler is lower on the stack than the jmpbuf,
    // longjmp() will unwind it before returning, so prevent
    // the cleanup code below from unsetting it
    if (setjmp( reg.jmpbuf)) {
        fHandler = FALSE;
        break;
    }

    rc = DosSetExceptionHandler( &reg.xrr);
    if (rc) {
        printf( "DosSetExceptionHandler - rc= 0x%x\n", (int)rc);
        break;
    }
    fHandler = TRUE;

    // parse the file
    rc = FileIdentifyFile( pji, reg.pBase);
    if (rc) {
        printf( "FileIdentifyFile - rc= 0x%x\n", (int)rc);
        break;
    }

    // change the address of the thumb into an offset into the file
    if (pji->pThumb)
        pji->offsThumb = pji->pThumb - reg.pBase;

} while (0);

    if (fHandler) {
        rc = DosUnsetExceptionHandler( &reg.xrr);
        if (rc)
            printf( "DosUnsetExceptionHandler - rc= 0x%lx\n", rc);
    }

    if (reg.pBase) {
        rc = DosFreeMem( reg.pBase);
        if (rc)
            printf( "DosFreeMem - rc= 0x%x\n", (int)rc);
    }

    if (reg.hFile) {
        rc = DosClose( reg.hFile);
        if (rc)
            printf( "DosClose - rc= 0x%x\n", (int)rc);
    }

    return (0);
}

/****************************************************************************/

ULONG APIENTRY  FileExceptionHandler( PEXCEPTIONREPORTRECORD pReport,
                             struct _EXCEPTIONREGISTRATIONRECORD * pXReg,
                             PCONTEXTRECORD pContext,
                             PVOID pDispatch)

{
    CAMJXRR *   pReg = (CAMJXRR*)pXReg;
    ULONG       ulRtn = XCPT_CONTINUE_SEARCH;
    ULONG       rc = 0;
    ULONG       ulSize;
    ULONG       ulAttr;
    LONG        lOffs;
    char *      pErr = 0;
    PVOID       pMem;

do {
    if (pReport->fHandlerFlags ||
        pReport->ExceptionNum     != XCPT_ACCESS_VIOLATION ||
        pReport->ExceptionAddress == (PVOID)XCPT_DATA_UNKNOWN ||
        pReport->ExceptionInfo[0] != XCPT_READ_ACCESS ||
        pReport->ExceptionInfo[1] == XCPT_DATA_UNKNOWN ||
        pReport->ExceptionInfo[1] <  (ULONG)pReg->pBase ||
        pReport->ExceptionInfo[1] >= (ULONG)pReg->pEnd) {
            printf( "*** Exception not handled - %08lx\n", pReport->ExceptionNum);
            return (ulRtn);
    }

    if (pReg->lFilePtr >= pReg->lEOF)
        ERRMSG( "already at EOF")

    pMem = (PVOID)(pReport->ExceptionInfo[1] & ~0xFFF);
    ulSize = 1;
    ulAttr = 0;

    rc = DosQueryMem( pMem, &ulSize, &ulAttr);
    if (rc)
        ERRMSG( "DosQueryMem")

    if (ulAttr & (PAG_FREE | PAG_COMMIT)) {
        rc = ulAttr;
        ERRMSG( "unexpected memory attributes")
    }

    rc = DosSetMem( pMem, 0x1000, PAG_COMMIT | PAG_DEFAULT);
    if (rc)
        ERRMSG( "DosSetMem")

    lOffs = (BYTE*)pMem - pReg->pBase;
    if (lOffs != pReg->lFilePtr) {
        rc = DosSetFilePtr( pReg->hFile, lOffs, FILE_BEGIN, &pReg->lFilePtr);
        if (rc)
            ERRMSG( "DosSetFilePtr")

        if (lOffs != pReg->lFilePtr) {
            rc = lOffs;
            ERRMSG( "seek beyond EOF")
        }
    }

    ulSize = (pReg->lEOF - pReg->lFilePtr >=  0x1000) ?
                0x1000 :(pReg->lEOF - pReg->lFilePtr);
    ulSize = FileReadFile( pReg->hFile, pMem, ulSize, pReg->szFile);
    if (!ulSize)
        break;

    pReg->lFilePtr += ulSize;

    ulRtn = XCPT_CONTINUE_EXECUTION;
//    printf( "Loaded %08lx, offset %08lx  %s\n",
//            (ULONG)pMem, lOffs, pReg->szFile);

} while (0);

    if (ulRtn != XCPT_CONTINUE_EXECUTION) {
        if (pErr)
            printf( "FileExceptionHandler - %s - rc= 0x%lx\n", pErr, rc);
        printf( "FileExceptionHandler - executing longjmp()\n");
        longjmp( pReg->jmpbuf, 1);
    }

    return (ulRtn);
}

/****************************************************************************/

ULONG       FileIdentifyFile( CAMJpgInfo * pji, BYTE * pBase)

{
    BOOL    	fDone = FALSE;
    ULONG       rc;
    CAMJFind    cjf;

    cjf.pCur = pBase;
    cjf.bMarker = 0;
    cjf.cbMarker = 0;

    rc = FileIsJpeg( &cjf);

    while (!rc && !fDone) {

        cjf.pCur += cjf.cbMarker;

        rc = FileFindMarker( &cjf);
        if (rc)
            break;

        switch (cjf.bMarker) {
            case M_EOI:
            case M_SOS:
                rc = CAMERR_BADFORMAT;
                break;

            case M_APP1:
                if (cjf.cbMarker < 2) {
                    rc = CAMERR_BADFORMAT;
                    break;
                }
                if (pji->fmtImage == CAMJ_EXIF)
                    break;

                rc = FileParseApp1( pji, cjf.pCur + 2, cjf.cbMarker - 2);
                if (rc)
                    break;
                pji->fmtImage = CAMJ_EXIF;
                break;

            case M_APP0:
                if (cjf.cbMarker < 2) {
                    rc = CAMERR_BADFORMAT;
                    break;
                }
                if (pji->fmtImage == CAMJ_JFIF)
                    break;

                rc = FileParseApp0( pji, cjf.pCur + 2, cjf.cbMarker - 2);
                if (!rc)
                    pji->fmtImage = CAMJ_JFIF;
                break;

            case M_SOF0:        // Baseline
            case M_SOF1:        // Extended sequential, Huffman
            case M_SOF2:        // Progressive, Huffman
            {
                USHORT  x, y;

                rc = FileParseSof( &cjf, &x, &y);
                if (!rc) {
                    pji->xImage = x;
                    pji->yImage = y;
                    if (!pji->fmtImage)
                        pji->fmtImage = CAMJ_JPEG;
                    fDone = TRUE;
                }
                break;
            }

            case M_SOF3:
            case M_SOF5:
            case M_SOF6:
            case M_SOF7:
            case M_SOF9:
            case M_SOF10:
            case M_SOF11:
            case M_SOF13:
            case M_SOF14:
            case M_SOF15:
                rc = CAMERR_BADFORMAT;
                break;
        }
    }

    return (rc);
}

/****************************************************************************/

ULONG       FileIsJpeg( CAMJFind * pjf)

{
    ULONG   rc = CAMERR_BADFORMAT;

    if (pjf->pCur[0] == (BYTE)0xff &&
        pjf->pCur[1] == M_SOI) {
        pjf->pCur  += 2;
        rc = 0;
    }

    return (rc);
}

/****************************************************************************/

ULONG       FileFindMarker( CAMJFind * pjf)

{
    ULONG   rc = CAMERR_BADFORMAT;
    ULONG   ctr;

do {
    pjf->bMarker = 0;
    pjf->cbMarker = 0;

    // skip over junk
    ctr = 32;
    while (ctr-- && *pjf->pCur != (BYTE)0xff)
        pjf->pCur++;
    if (!ctr)
        break;

    // skip over multiple ff's
    ctr = 32;
    while (ctr-- && *pjf->pCur == (BYTE)0xff)
        pjf->pCur++;
    if (!ctr)
        break;

    // save the marker
    pjf->bMarker = *pjf->pCur++;

    // calc the length but don't advance pointer beyond it
    pjf->cbMarker = ((uint8_t)pjf->pCur[0] << 8) + (uint8_t)pjf->pCur[1];
    rc = 0;

} while (0);

    return (rc);
}

/****************************************************************************/

// this parses one or two App0 markers in the unlikely event
// the file is in JFIF format rather than Exif

ULONG       FileParseApp0( CAMJpgInfo * pji, BYTE * pApp0, ULONG cbApp0)

{
    ULONG       rc = 0;
    BYTE *      ptr;
    CAMJFind    cjf;

do {
    // confirm this is the JFIF tag
    if (strcmp( pApp0, "JFIF")) {
        rc = CAMERR_BADFORMAT;
        break;
    }

    // see if there's a thumbnail in this tag
    if (pApp0[12] && pApp0[13]) {
        pji->fmtThumb = CAMJ_RGB24;
        pji->xThumb = (BYTE)pApp0[12];
        pji->yThumb = (BYTE)pApp0[13];
        pji->pThumb = &pApp0[14];
        pji->cbThumb = pji->xThumb * pji->yThumb * 3;
        break;
    }

    // see if there's a thumbnail in a JFXX tag
    cjf.pCur = pApp0 + cbApp0;
    if (FileFindMarker( &cjf) ||
        cjf.bMarker != (BYTE)M_APP0 ||
        cjf.cbMarker < 2)
        break;

    ptr = cjf.pCur + 2;
    if (strcmp( ptr, "JFXX"))
        break;

    // JPEG
    if (ptr[5] == 0x10) {
        pji->fmtThumb = CAMJ_JPEG;
        pji->pThumb = &ptr[6];
        pji->cbThumb = cjf.cbMarker - 8;
        rc = FileParseJPEGThumb( pji);
        if (rc) {
            pji->fmtThumb = 0;
            pji->pThumb = 0;
            pji->cbThumb = 0;
        }
        break;
    }
/*
    // palettized RGB - why bother?
    if (ptr[5] == 0x11) {
        pji->fmtThumb = CAMJ_RGB8;
        pji->xThumb = (BYTE)ptr[6];
        pji->yThumb = (BYTE)ptr[7];
        pji->pThumb = &ptr[8];
        pji->cbThumb = pji->xThumb * pji->yThumb + 768;
        break;
    }
*/
    // RGB24
    if (ptr[5] == 0x13) {
        pji->fmtThumb = CAMJ_RGB24;
        pji->xThumb = (BYTE)ptr[6];
        pji->yThumb = (BYTE)ptr[7];
        pji->pThumb = &ptr[8];
        pji->cbThumb = pji->xThumb * pji->yThumb * 3;
        break;
    }

} while (0);

    return (rc);
}

/****************************************************************************/

// this parses the Exif marker;  while we're in the neighborhood,
// it also gets the rotation index (for PTP, we get this just
// before fetching the thumbnail)

ULONG       FileParseApp1( CAMJpgInfo * pji, BYTE * pApp1, ULONG cbApp1)

{
    ULONG       rc = CAMERR_BADFORMAT;
    BOOL        fLE;
    ULONG       offs;
    ULONG       ctr;
    USHORT      RotTag;
    ULONG       cntIfd0;
    BYTE *      pTiff;
    BYTE *      pIfd0;
    BYTE *      ptr = pApp1;

do {
    // confirm this is the Exif tag
    if (memcmp( ptr, "Exif\0", 6))
        break;

    // save the location of the TIFF header
    ptr += 6;
    pTiff = ptr;

    // get the byte order
    if (ptr[0] == 'I' && ptr[1] == 'I')
        fLE = TRUE;
    else
    if (ptr[0] == 'M' && ptr[1] == 'M')
        fLE = FALSE;
    else
        break;

    // this is always '0x2a'
    ptr += 2;
    if (fLE) {
        if (*((uint16_t*)ptr) != 0x002a)
            break;
    }
    else
        if (*((uint16_t*)ptr) != 0x2a00)
            break;

    // get & save the location of IFD0
    ptr += 2;
    if (fLE)
        offs = *((uint32_t*)ptr);
    else
        offs = (((uint8_t)ptr[0] << 24) + ((uint8_t)ptr[1] << 16) +
                ((uint8_t)ptr[2] << 8)  + (uint8_t)ptr[3]);

    if (offs < 8 || offs >= 0x10000)
        break;

    ptr = pIfd0 = pTiff + offs;

    // get the number of tags
    if (fLE)
        cntIfd0 = *((uint16_t*)ptr);
    else
        cntIfd0 = ((uint8_t)ptr[0] << 8) + ptr[1];

    if (fLE)
        RotTag = 0x0112;
    else
        RotTag = 0x1201;
    for (ptr += 2, ctr = cntIfd0; ctr; ptr += 12, ctr--)
        if (*((uint16_t*)ptr) == RotTag) {
            if (fLE)
                pji->ulRot = (uint8_t)ptr[8] + ((uint8_t)ptr[9] << 8);
            else
                pji->ulRot = ((uint8_t)ptr[8] << 8) + (uint8_t)ptr[9];

            // if the orientation isn't between 1 & 8, clear the value
            if (pji->ulRot < 1 || pji->ulRot > 8)
                pji->ulRot = 0;

            break;
        }

    // get offset of IFD1
    ptr = pIfd0 + 2 + (cntIfd0 * 12);
    if (fLE)
        offs = *((uint32_t*)ptr);
    else
        offs = (((uint8_t)ptr[0] << 24) + ((uint8_t)ptr[1] << 16) +
                ((uint8_t)ptr[2] << 8)  +  (uint8_t)ptr[3]);

    if (!offs) {
        rc = 0;
        break;
    }

    if (offs < 8 || offs >= 0x10000)
        break;

    FileFindExifThumb( pji, pTiff + offs, pTiff, fLE);
    rc = 0;

} while (0);

    return (rc);
}

/****************************************************************************/

ULONG       FileFindExifThumb( CAMJpgInfo * pji, BYTE * pIfd1, BYTE * pTiff, BOOL fLE)

{
    ULONG       rc = CAMERR_BADFORMAT;
    ULONG       cntIfd1;
    ULONG       ul;
    USHORT      JifTag;
    USHORT      JifLthTag;
    BYTE *      ptr = pIfd1;

do {
    // get the number of tags
    if (fLE)
        cntIfd1 = *((uint16_t*)ptr);
    else
        cntIfd1 = ((uint8_t)ptr[0] << 8) + ptr[1];

    if (!cntIfd1)
        break;

    ptr += 2;

    if (fLE)
        JifTag = 0x0201;
    else
        JifTag = 0x0102;
    JifLthTag = 0x0202;

    while (cntIfd1--) {

        if (*((uint16_t*)ptr) == JifTag || *((uint16_t*)ptr) == JifLthTag) {
            if (fLE)
                ul = *((uint32_t*)(&ptr[8]));
            else
                ul = (((uint8_t)ptr[8] << 24) + ((uint8_t)ptr[9] << 16) +
                      ((uint8_t)ptr[10] << 8) +  (uint8_t)ptr[11]);

            if (!ul)
                break;

            if (*((uint16_t*)ptr) == JifTag)
                pji->pThumb = pTiff + ul;
            else
                pji->cbThumb = ul;
        }

        if (pji->pThumb && pji->cbThumb) {
            pji->fmtThumb = CAMJ_JPEG;
            rc = FileParseJPEGThumb( pji);
            if (rc) {
                pji->fmtThumb = 0;
                pji->pThumb = 0;
                pji->cbThumb = 0;
            }
            break;
        }

        ptr += 12;
    }

} while (0);

    return (rc);
}

/****************************************************************************/

ULONG       FileParseJPEGThumb( CAMJpgInfo * pji)

{
    ULONG       rc = 0;
    BOOL        fDone = FALSE;
    CAMJFind    cjf;

    cjf.pCur = pji->pThumb;
    cjf.bMarker = 0;
    cjf.cbMarker = 0;

    rc = FileIsJpeg( &cjf);

    while (!rc && !fDone) {

        cjf.pCur += cjf.cbMarker;
        if (cjf.pCur >= &pji->pThumb[pji->cbThumb]) {
            rc = CAMERR_BADFORMAT;
            break;
        }

        rc = FileFindMarker( &cjf);
        if (rc)
            break;

        switch (cjf.bMarker) {
            case M_EOI:
            case M_SOS:
                rc = CAMERR_BADFORMAT;
                break;

            case M_SOF0:        // Baseline
            case M_SOF1:        // Extended sequential, Huffman
            case M_SOF2:        // Progressive, Huffman
            {
                USHORT  x, y;

                rc = FileParseSof( &cjf, &x, &y);
                if (!rc) {
                    pji->xThumb = x;
                    pji->yThumb = y;
                    fDone = TRUE;
                }
                break;
            }

            case M_SOF3:        // unsupported formats
            case M_SOF5:
            case M_SOF6:
            case M_SOF7:
            case M_SOF9:
            case M_SOF10:
            case M_SOF11:
            case M_SOF13:
            case M_SOF14:
            case M_SOF15:
                rc = CAMERR_BADFORMAT;
                break;
        }
    }

    return (rc);
}

/****************************************************************************/

// get the image's dimensions, then confirm that this marker was valid;

ULONG       FileParseSof( CAMJFind * pjf, PUSHORT pX, PUSHORT pY)

{
    ULONG       rc = 0;

    if (pjf->cbMarker < 2)
        rc = CAMERR_BADFORMAT;
    else {
        *pY = ((uint8_t)pjf->pCur[3] << 8) + (uint8_t)pjf->pCur[4];
        *pX = ((uint8_t)pjf->pCur[5] << 8) + (uint8_t)pjf->pCur[6];

        if (!*pY || !*pX ||
            pjf->cbMarker != 3 * ((uint8_t)pjf->pCur[7]) + 8)
            rc = CAMERR_BADFORMAT;
    }

    return (rc);
}

/****************************************************************************/

