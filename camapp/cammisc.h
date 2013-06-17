/***************************************************************************/
//  cammisc.h
//
//  - stuff that doesn't fit comfortably elsewhere
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

/***************************************************************************/

#ifndef _CAMMISC_H_
#define _CAMMISC_H_

/***************************************************************************/

// camfile & camdata

#define CAMJ_NONE   0
#define CAMJ_JPEG   1
#define CAMJ_JFIF   2
#define CAMJ_EXIF   3
#define CAMJ_RGB8   4
#define CAMJ_RGB24  5


// camfile & camjpg

typedef enum {
  M_SOF0  = 0xc0,
  M_SOF1  = 0xc1,
  M_SOF2  = 0xc2,
  M_SOF3  = 0xc3,
  M_SOF5  = 0xc5,
  M_SOF6  = 0xc6,
  M_SOF7  = 0xc7,
  M_SOF9  = 0xc9,
  M_SOF10 = 0xca,
  M_SOF11 = 0xcb,
  M_SOF13 = 0xcd,
  M_SOF14 = 0xce,
  M_SOF15 = 0xcf,
  M_SOI   = 0xd8,
  M_EOI   = 0xd9,
  M_SOS   = 0xda,
  M_APP0  = 0xe0,
  M_APP1  = 0xe1,
} JPEG_MARKER;


// camdata & camthumb

typedef void FNCTHREAD( void *);
typedef FNCTHREAD *PFNCTHREAD;

/***************************************************************************/

// camfile & camdata

typedef struct _CAMJpgInfo {
    ULONG   ulRot;
    USHORT  fmtImage;
    USHORT  xImage;
    USHORT  yImage;
    USHORT  fmtThumb;
    USHORT  xThumb;
    USHORT  yThumb;
    BYTE *  pThumb;
    ULONG   offsThumb;
    ULONG   cbThumb;
} CAMJpgInfo;

/***************************************************************************/

// camsave & camjpg

typedef struct _CAM * _CAMPtr;

// a grab-bag of stuff passed to camjpg.JpgSaveRotatedJPEG()
typedef struct _CAMSaveRot {
    uint32_t        hObject;
    char *          pszObject;
    uint32_t    	hFile;
    uint32_t        ulRotate;
    _CAMPtr         thisCam;
    uint32_t *      pStop;
    uint32_t        rtn;
} CAMSaveRot;

/***************************************************************************/

// camthumb & camjpg

// stuff passed to camjpg.JpgDecodeThumbnail()
typedef struct _CAMJThumb {
    char *          pSrc;
    uint32_t        cbSrc;
    char *          pDst;
    uint32_t        cbDst;
    uint32_t        cxDst;
    uint32_t        cyDst;
    uint32_t        cbPix;
} CAMJThumb;

/***************************************************************************/

// in camjpg.c
uint32_t        JpgSaveRotatedJPEG( CAMSaveRot * csr);
uint32_t        JpgDecodeThumbnail( CAMJThumb * cjt);

// in camfile.c
ULONG           FileReadFile( HFILE hFile, char * pBuf,
                              ULONG cbRequest, char * path);
void            ShowTime( char * szText, BOOL fEnd);
ULONG           FileGetFormat( char * szFilename);
ULONG           FileGetJPEGInfo( CAMJpgInfo * pji, char * szFile, ULONG cbFile);

/***************************************************************************/
#endif  //_CAMMISC_H_
/***************************************************************************/

