/****************************************************************************/
// camjpg.c
//
//  - routines to rotate a JPEG while saving it
//
//  Much of the source code in this file was taken from libjpeg.
//  - JpgSaveRotatedJPEG() was taken almost verbatim from jpegtran.c;
//  - the JXxxRot_* routines are derived from similar code in
//    jdatasrc.c and jdatadst.c.
//  As required by the license for libjpeg:
//
//  This software is based in part on the work of the Independent JPEG Group.
//      Copyright (C) 1994-1997, Thomas G. Lane.
//  For conditions of distribution and use, see the accompanying
//  readme-jpg.txt file.
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
 *  Thomas G. Lane, Independent JPEG Group - Copyright (C) 1994-1997
 *
 * ***** END LICENSE BLOCK ***** */

/****************************************************************************/
/****************************************************************************/

// from jpegtran.c
#include "cdjpeg.h"         // Common decls for cjpeg/djpeg applications
#include "transupp.h"       // Support routines for jpegtran
#include "jversion.h"       // for version message

// from jdatadst.c & jdatasrc.c
#include "jinclude.h"
#include "jpeglib.h"
#include "jerror.h"

// Cam includes
#include <setjmp.h>
#include "camusb.h"
#define INCL_DOS
#include <os2.h>
#include "cammisc.h"

/****************************************************************************/
/****************************************************************************/

typedef struct _CAMJpg {
    CAMSaveRot *    csr;
    uint32_t        hMsdFile;               // MSD input file handle
    JOCTET *        pInbuf;                 // start of buffer
    uint32_t        cbInbuf;                // size of buffer
    uint32_t    	cbToGet;                // bytes to fetch
    JOCTET *        pOutbuf;                // start of buffer
    uint32_t        cbOutbuf;               // size of buffer
    jmp_buf         jmpbuf;                 // setjmp buffer
} CAMJpg;

#define OUTPUT_BUF_SIZE     0x100000


typedef struct _CAMJThumbInt {
    CAMJThumb *     cjt;
    uint32_t        rtn;
    uint32_t        eoiCtr;
    JOCTET          eoi[4];
    jmp_buf         jmpbuf;
} CAMJThumbInt;

/****************************************************************************/

uint32_t        JpgSaveRotatedJPEG( CAMSaveRot * csr);

void            JPtpRot_stdio_src( j_decompress_ptr cinfo);
void            JPtpRot_init_source( j_decompress_ptr cinfo);
boolean         JPtpRot_fill_input_buffer( j_decompress_ptr cinfo);
void            JPtpRot_skip_input_data( j_decompress_ptr cinfo, long num_bytes);
void            JPtpRot_term_source( j_decompress_ptr cinfo);
uint32_t        JPtpRot_read_input( j_decompress_ptr cinfo);

void            JMsdRot_stdio_src( j_decompress_ptr cinfo);
void            JMsdRot_init_source( j_decompress_ptr cinfo);
boolean         JMsdRot_fill_input_buffer( j_decompress_ptr cinfo);
void            JMsdRot_skip_input_data( j_decompress_ptr cinfo, long num_bytes);
void            JMsdRot_term_source( j_decompress_ptr cinfo);
uint32_t        JMsdRot_read_input( j_decompress_ptr cinfo);

void            JSavRot_stdio_dest( j_compress_ptr cinfo);
void            JSavRot_init_destination( j_compress_ptr cinfo);
boolean         JSavRot_empty_output_buffer( j_compress_ptr cinfo);
void            JSavRot_term_destination( j_compress_ptr cinfo);
void            JSavRot_error_exit (j_common_ptr cinfo);
void            JSavRot_abort( j_common_ptr cinfo);

uint32_t        JpgDecodeThumbnail( CAMJThumb * cjt);
void            JThumb_stdio_src( j_decompress_ptr cinfo);
void            JThumb_init_source( j_decompress_ptr cinfo);
boolean         JThumb_fill_input_buffer( j_decompress_ptr cinfo);
void            JThumb_skip_input_data( j_decompress_ptr cinfo, long num_bytes);
void            JThumb_term_source( j_decompress_ptr cinfo);
void            JThumb_error_exit( j_common_ptr cinfo);
void            JThumb_abort( j_common_ptr cinfo);

/****************************************************************************/
/****************************************************************************/

uint32_t    JpgSaveRotatedJPEG( CAMSaveRot * csr)

{
    struct jpeg_decompress_struct   srcinfo;
    struct jpeg_compress_struct     dstinfo;
    struct jpeg_error_mgr           jsrcerr;
    struct jpeg_error_mgr           jdsterr;
    jpeg_transform_info             transformoption;
    jvirt_barray_ptr *              src_coef_arrays;
    jvirt_barray_ptr *              dst_coef_arrays;
    CAMJpg                          cjpg;

do {
    memset( &cjpg, 0, sizeof(CAMJpg));
    cjpg.csr = csr;
    if (setjmp( cjpg.jmpbuf))
        break;

    // Initialize the JPEG decompression object with custom error handling.
    srcinfo.client_data = &cjpg;
    srcinfo.err = jpeg_std_error( &jsrcerr);
    jsrcerr.error_exit = &JSavRot_error_exit;
    jpeg_create_decompress( &srcinfo);

    // Initialize the JPEG compression object with custom error handling.
    dstinfo.client_data = &cjpg;
    dstinfo.err = jpeg_std_error( &jdsterr);
    jdsterr.error_exit = &JSavRot_error_exit;
    jpeg_create_compress( &dstinfo);

    jsrcerr.trace_level = jdsterr.trace_level;
    srcinfo.mem->max_memory_to_use = dstinfo.mem->max_memory_to_use;

    // Specify data source for decompression
    if (csr->pszObject)
        JMsdRot_stdio_src( &srcinfo);
    else
        JPtpRot_stdio_src( &srcinfo);

    // Enable saving of extra markers that we want to copy
    jcopy_markers_setup( &srcinfo, JCOPYOPT_ALL);

    // Read file header
    jpeg_read_header( &srcinfo, TRUE);

    // set transform options
    transformoption.trim = FALSE;
    transformoption.force_grayscale = FALSE;
    if (csr->ulRotate == 90)
        transformoption.transform = JXFORM_ROT_90;
    else
    if (csr->ulRotate == 180)
        transformoption.transform = JXFORM_ROT_180;
    else
        transformoption.transform = JXFORM_ROT_270;

    // Any space needed by a transform option must be requested before
    // jpeg_read_coefficients so that memory allocation will be done right.
    jtransform_request_workspace( &srcinfo, &transformoption);

    // Read source file as DCT coefficients
    src_coef_arrays = jpeg_read_coefficients( &srcinfo);

    // Initialize destination compression parameters from source values
    jpeg_copy_critical_parameters( &srcinfo, &dstinfo);

    // Adjust destination parameters if required by transform options;
    // also find out which set of coefficient arrays will hold the output.
    dst_coef_arrays = jtransform_adjust_parameters( &srcinfo, &dstinfo,
                                    src_coef_arrays, &transformoption);

    // Specify data destination for compression
    JSavRot_stdio_dest( &dstinfo);
  
    // Start compressor (note no image data is actually written here)
    jpeg_write_coefficients( &dstinfo, dst_coef_arrays);
  
    // Copy to the output file any extra markers that we want to preserve
    jcopy_markers_execute( &srcinfo, &dstinfo, JCOPYOPT_ALL);
  
    // Execute image transformation
    jtransform_execute_transformation( &srcinfo, &dstinfo,
                                       src_coef_arrays, &transformoption);
  
    // Finish compression and release memory
    jpeg_finish_compress( &dstinfo);
    (void) jpeg_finish_decompress( &srcinfo);

} while (0);

    jpeg_destroy_compress( &dstinfo);
    jpeg_destroy_decompress( &srcinfo);

    if (cjpg.pInbuf)
        DosFreeMem( cjpg.pInbuf);
    if (cjpg.pOutbuf)
        DosFreeMem( cjpg.pOutbuf);

    // All done
    return (csr->rtn);
}

/****************************************************************************/
/****************************************************************************/

void        JPtpRot_stdio_src( j_decompress_ptr cinfo)

{
    uint32_t        rc;
    uint32_t        cnt;
    CAMJpg *        cjpg = (CAMJpg *)cinfo->client_data;
    CAMSaveRot *    csr = cjpg->csr;
    CAMReadData     crd;

do {
    if (*csr->pStop) {
        csr->rtn = CAMERR_REQCANCELLED;
        break;
    }

    csr->rtn = ReadObjectBegin( csr->thisCam->hCam, csr->hObject, &cjpg->cbToGet, &crd);
    if (csr->rtn != PTP_RC_OK)
        break;

    if (cjpg->cbToGet <= sizeof(crd.data)) {
        csr->rtn = ReadObjectEnd( csr->thisCam->hCam);
        if (csr->rtn != PTP_RC_OK)
            break;
        cjpg->cbInbuf = cjpg->cbToGet;
        cnt = cjpg->cbToGet;
        cjpg->cbToGet = 0;
    }
    else {
        cjpg->cbInbuf = (cjpg->cbToGet < 0x100000) ? cjpg->cbToGet : 0x100000;
        cnt = sizeof(crd.data);
        cjpg->cbToGet -= cnt;
    }

    rc = DosAllocMem( (void*)&cjpg->pInbuf, cjpg->cbInbuf,
                      PAG_COMMIT | PAG_READ | PAG_WRITE);
    if (rc) {
        camcli_error( "JPtpRot_stdio_src - DosAllocMem  rc= 0x%04x", rc);
        cjpg->pInbuf = 0;
        csr->rtn = CAMERR_MALLOC;
        break;
    }

    memcpy( cjpg->pInbuf, crd.data, cnt);

    cinfo->src = (struct jpeg_source_mgr *) (*cinfo->mem->alloc_small)
                 ((j_common_ptr) cinfo, JPOOL_PERMANENT, SIZEOF(struct jpeg_source_mgr));

    cinfo->src->init_source = JPtpRot_init_source;
    cinfo->src->fill_input_buffer = JPtpRot_fill_input_buffer;
    cinfo->src->skip_input_data = JPtpRot_skip_input_data;
    cinfo->src->resync_to_restart = jpeg_resync_to_restart; // use default method
    cinfo->src->term_source = JPtpRot_term_source;
    cinfo->src->bytes_in_buffer = cnt;
    cinfo->src->next_input_byte = cjpg->pInbuf;

} while (0);

    if (csr->rtn != PTP_RC_OK)
        JSavRot_abort((j_common_ptr)cinfo);

    return;
}

/****************************************************************************/

// Initialize source - called by jpeg_read_header
// before any data is actually read.

void        JPtpRot_init_source( j_decompress_ptr cinfo)
{
    return;
}

/****************************************************************************/

// Fill the input buffer - called whenever buffer is emptied.

boolean     JPtpRot_fill_input_buffer( j_decompress_ptr cinfo)

{
    uint32_t        cnt;

    cnt = JPtpRot_read_input( cinfo);
    if (!cnt)
        JSavRot_abort((j_common_ptr)cinfo);

    cinfo->src->next_input_byte = ((CAMJpg *)cinfo->client_data)->pInbuf;
    cinfo->src->bytes_in_buffer = cnt;

    return (TRUE);
}

/****************************************************************************/

// Skip data --- used to skip over a potentially large amount of
// uninteresting data (such as an APPn marker).

void        JPtpRot_skip_input_data( j_decompress_ptr cinfo, long num_bytes)

{
    if (num_bytes > 0) {
        while (num_bytes > (long)cinfo->src->bytes_in_buffer) {
            num_bytes -= (long)cinfo->src->bytes_in_buffer;
            JPtpRot_fill_input_buffer( cinfo);
        }
        cinfo->src->next_input_byte += (size_t) num_bytes;
        cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
    }

    return;
}

/****************************************************************************/

// Terminate source - called by jpeg_finish_decompress
// after all data has been read.

void        JPtpRot_term_source( j_decompress_ptr cinfo)

{
    CAMJpg *        cjpg = (CAMJpg *)cinfo->client_data;
    CAMSaveRot *    csr = cjpg->csr;

    while (JPtpRot_read_input( cinfo))
        if (csr->rtn != PTP_RC_OK)
            JSavRot_abort((j_common_ptr)cinfo);

    return;
}

/****************************************************************************/

uint32_t    JPtpRot_read_input( j_decompress_ptr cinfo)
{

    uint32_t        ulRtn = 0;
    uint32_t        cnt;
    CAMJpg *        cjpg = (CAMJpg *)cinfo->client_data;
    CAMSaveRot *    csr = cjpg->csr;

do {
    if (!cjpg->cbToGet)
        break;

    if (*csr->pStop) {
        csr->rtn = CancelRequest( csr->thisCam->hCam);
        if (!csr->rtn)
            csr->rtn = CAMERR_REQCANCELLED;
        break;
    }

    if (cjpg->cbToGet <= cjpg->cbInbuf) {
        cnt = cjpg->cbToGet;
        cnt++;
    }
    else
        cnt = cjpg->cbInbuf;

    csr->rtn = ReadObjectData( csr->thisCam->hCam, &cnt, cjpg->pInbuf);
    if (csr->rtn != PTP_RC_OK)
        break;

    cjpg->cbToGet -= cnt;
    if (cjpg->cbToGet == 0) {
        csr->rtn = ReadObjectEnd( csr->thisCam->hCam);
        if (csr->rtn != PTP_RC_OK)
            break;
    }

    ulRtn = cnt;

} while (0);

    return (ulRtn);
}

/****************************************************************************/
/****************************************************************************/

void        JMsdRot_stdio_src( j_decompress_ptr cinfo)

{
    ULONG           rc;
    ULONG           cnt;
    CAMJpg *        cjpg = (CAMJpg *)cinfo->client_data;
    CAMSaveRot *    csr = cjpg->csr;

do {
    if (*csr->pStop) {
        csr->rtn = CAMERR_REQCANCELLED;
        break;
    }

    rc = DosOpen( csr->pszObject, (PULONG)&cjpg->hMsdFile, &cnt, 0, 0,
                  (OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS),
                  (OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_SEQUENTIAL |
                   OPEN_SHARE_DENYWRITE | OPEN_ACCESS_READONLY), 0);
    if (rc) {
        printf( "JMsdRot_stdio_src - DosOpen - rc= 0x%lx\n", rc);
        csr->rtn = CAMERR_MISCELLANEOUS;
        break;
    }

    rc = DosSetFilePtr( cjpg->hMsdFile, 0, FILE_END, (PULONG)&cjpg->cbToGet);
    if (!rc)
        rc = DosSetFilePtr( cjpg->hMsdFile, 0, FILE_BEGIN, (PULONG)&cnt);
    if (rc) {
        printf( "JMsdRot_stdio_src - DosSetFilePtr - rc= 0x%lx\n", rc);
        csr->rtn = CAMERR_MISCELLANEOUS;
        break;
    }

    cjpg->cbInbuf = (cjpg->cbToGet < 0x100000) ? cjpg->cbToGet : 0x100000;

    rc = DosAllocMem( (void*)&cjpg->pInbuf, cjpg->cbInbuf,
                      PAG_COMMIT | PAG_READ | PAG_WRITE);
    if (rc) {
        printf( "JMsdRot_stdio_src - DosAllocMem  rc= 0x%lx\n", rc);
        cjpg->pInbuf = 0;
        csr->rtn = CAMERR_MALLOC;
        break;
    }

    cinfo->src = (struct jpeg_source_mgr *) (*cinfo->mem->alloc_small)
                 ((j_common_ptr) cinfo, JPOOL_PERMANENT, SIZEOF(struct jpeg_source_mgr));

    cinfo->src->init_source = JMsdRot_init_source;
    cinfo->src->fill_input_buffer = JMsdRot_fill_input_buffer;
    cinfo->src->skip_input_data = JMsdRot_skip_input_data;
    cinfo->src->resync_to_restart = jpeg_resync_to_restart; // use default method
    cinfo->src->term_source = JMsdRot_term_source;
    cinfo->src->bytes_in_buffer = 0;
    cinfo->src->next_input_byte = cjpg->pInbuf;

} while (0);

    if (csr->rtn != PTP_RC_OK)
        JSavRot_abort((j_common_ptr)cinfo);

    return;
}

/****************************************************************************/

// Initialize source - called by jpeg_read_header
// before any data is actually read.

void        JMsdRot_init_source( j_decompress_ptr cinfo)
{
    return;
}

/****************************************************************************/

// Fill the input buffer - called whenever buffer is emptied.

boolean     JMsdRot_fill_input_buffer( j_decompress_ptr cinfo)

{
    uint32_t        cnt;

    cnt = JMsdRot_read_input( cinfo);
    if (!cnt)
        JSavRot_abort((j_common_ptr)cinfo);

    cinfo->src->next_input_byte = ((CAMJpg *)cinfo->client_data)->pInbuf;
    cinfo->src->bytes_in_buffer = cnt;

    return (TRUE);
}

/****************************************************************************/

// Skip data --- used to skip over a potentially large amount of
// uninteresting data (such as an APPn marker).

void        JMsdRot_skip_input_data( j_decompress_ptr cinfo, long num_bytes)

{
    if (num_bytes > 0) {
        while (num_bytes > (long) cinfo->src->bytes_in_buffer) {
            num_bytes -= (long) cinfo->src->bytes_in_buffer;
            (void) JMsdRot_fill_input_buffer( cinfo);
        }
        cinfo->src->next_input_byte += (size_t) num_bytes;
        cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
    }

    return;
}

/****************************************************************************/

// Terminate source - called by jpeg_finish_decompress
// after all data has been read.

void        JMsdRot_term_source( j_decompress_ptr cinfo)

{
    ULONG           rc;
    CAMJpg *        cjpg = (CAMJpg *)cinfo->client_data;

    if (cjpg->hMsdFile) {
        rc = DosClose( cjpg->hMsdFile);
        if (rc)
            printf( "JMsdRot_term_source - DosClose - rc= 0x%lx\n", rc);
        cjpg->hMsdFile = 0;
    }

    return;
}

/****************************************************************************/

uint32_t    JMsdRot_read_input( j_decompress_ptr cinfo)

{
    uint32_t        ulRtn = 0;
    uint32_t        cnt;
    CAMJpg *        cjpg = (CAMJpg *)cinfo->client_data;
    CAMSaveRot *    csr = cjpg->csr;

do {
    if (!cjpg->cbToGet)
        break;

    if (*csr->pStop) {
        csr->rtn = CAMERR_REQCANCELLED;
        break;
    }

    if (cjpg->cbToGet <= cjpg->cbInbuf)
        cnt = cjpg->cbToGet;
    else
        cnt = cjpg->cbInbuf;

    ulRtn = FileReadFile( cjpg->hMsdFile, cjpg->pInbuf, cnt, csr->pszObject);
    if (!ulRtn) {
        csr->rtn = CAMERR_NEEDDATA;
        break;
    }

    cjpg->cbToGet -= ulRtn;

} while (0);

    return (ulRtn);
}

/****************************************************************************/
/****************************************************************************/

// Prepare for output to a file.

void        JSavRot_stdio_dest( j_compress_ptr cinfo)

{
    uint32_t        rc;
    CAMJpg *        cjpg = (CAMJpg *)cinfo->client_data;
    CAMSaveRot *    csr = cjpg->csr;

do {
    rc = DosAllocMem( (void*)&cjpg->pOutbuf, OUTPUT_BUF_SIZE,
                      PAG_COMMIT | PAG_READ | PAG_WRITE);
    if (rc) {
        camcli_error( "JSavRot_stdio_dest - DosAllocMem  rc= 0x%04x", csr->rtn);
        cjpg->pOutbuf = 0;
        csr->rtn = CAMERR_MALLOC;
        break;
    }

    cinfo->dest = (struct jpeg_destination_mgr *) (*cinfo->mem->alloc_small)
                  ((j_common_ptr) cinfo, JPOOL_PERMANENT, SIZEOF(struct jpeg_destination_mgr));

    cinfo->dest->init_destination = JSavRot_init_destination;
    cinfo->dest->empty_output_buffer = JSavRot_empty_output_buffer;
    cinfo->dest->term_destination = JSavRot_term_destination;
    cinfo->dest->next_output_byte = cjpg->pOutbuf;
    cinfo->dest->free_in_buffer = OUTPUT_BUF_SIZE;

} while (0);

    if (csr->rtn != PTP_RC_OK) 
        JSavRot_abort((j_common_ptr)cinfo);

    return;
}

/****************************************************************************/

// Initialize destination --- called by jpeg_start_compress
// before any data is actually written.

void        JSavRot_init_destination( j_compress_ptr cinfo)
{
    return;
}

/****************************************************************************/

// Empty the output buffer - called whenever buffer fills up.

boolean     JSavRot_empty_output_buffer( j_compress_ptr cinfo)
{
    uint32_t        rc;
    uint32_t        wrote;
    CAMJpg *        cjpg = (CAMJpg *)cinfo->client_data;
    CAMSaveRot *    csr = cjpg->csr;

    rc = DosWrite( csr->hFile, cjpg->pOutbuf, OUTPUT_BUF_SIZE, (PULONG)&wrote);
    if (rc) {
        camcli_error( "JSavRot_empty_output_buffer - DosWrite  rc= 0x%04x", rc);
        csr->rtn = CAMERR_WRITEFAILED;
        JSavRot_abort((j_common_ptr)cinfo);
    }

    cinfo->dest->next_output_byte = cjpg->pOutbuf;
    cinfo->dest->free_in_buffer = OUTPUT_BUF_SIZE;

    return TRUE;
}

/****************************************************************************/

// Terminate destination --- called by jpeg_finish_compress
// after all data has been written.

void        JSavRot_term_destination( j_compress_ptr cinfo)
{
    uint32_t        rc;
    uint32_t    	wrote;
    uint32_t    	cnt;
    CAMJpg *        cjpg = (CAMJpg *)cinfo->client_data;
    CAMSaveRot *    csr = cjpg->csr;

    cnt = OUTPUT_BUF_SIZE - cinfo->dest->free_in_buffer;

    // Write any data remaining in the buffer
    if (cnt > 0) {
        rc = DosWrite( csr->hFile, cjpg->pOutbuf, cnt, (PULONG)&wrote);
        if (rc) {
            camcli_error( "term_destination - DosWrite  rc= 0x%04x", rc);
            csr->rtn = CAMERR_WRITEFAILED;
            JSavRot_abort((j_common_ptr)cinfo);
        }
    }

    return;
}

/****************************************************************************/
/****************************************************************************/

// Error exit handler: must not return to caller.

void        JSavRot_error_exit (j_common_ptr cinfo)

{
    CAMJpg *    cjpg = (CAMJpg *)cinfo->client_data;

    printf( "JSavRot_error_exit called\n");

    // Always display the message
    (*cinfo->err->output_message) (cinfo);

    cjpg->csr->rtn = CAMERR_LIBJPEG;
    JSavRot_abort( cinfo);

    exit( EXIT_FAILURE);
}

/****************************************************************************/

void        JSavRot_abort( j_common_ptr cinfo)

{
    CAMJpg *    cjpg = (CAMJpg *)cinfo->client_data;

    printf( "JSavRot_abort called\n");

    if (cjpg->csr->hObject) {
        if (cjpg->cbToGet)
            CancelRequest( cjpg->csr->thisCam->hCam);
    }
    else
        if (cjpg->hMsdFile) {
            ULONG   rc;

            rc = DosClose( cjpg->hMsdFile);
            if (rc)
                printf( "JSavRot_abort - DosClose - rc= 0x%lx\n", rc);
            cjpg->hMsdFile = 0;
        }

    longjmp( cjpg->jmpbuf, 1);

    exit( EXIT_FAILURE);
}

/****************************************************************************/
/****************************************************************************/

uint32_t    JpgDecodeThumbnail( CAMJThumb * cjt)

{
    uint32_t                        cbRow;
    uint32_t                        cntRows;
    uint32_t                        ctr;
    JSAMPARRAY                      buffer;
    char *                          ptr;
    struct jpeg_decompress_struct   srcinfo;
    struct jpeg_error_mgr           jsrcerr;
    CAMJThumbInt                    cjpg;

do {
    memset( &cjpg, 0, sizeof(CAMJThumbInt));
    memset( cjpg.eoi, M_EOI, sizeof(cjpg.eoi));
    cjpg.cjt = cjt;
    if (setjmp( cjpg.jmpbuf))
        break;

    // Initialize the JPEG decompression object with custom error handling.
    srcinfo.client_data = &cjpg;
    srcinfo.err = jpeg_std_error( &jsrcerr);
    jsrcerr.error_exit = &JThumb_error_exit;
    jpeg_create_decompress( &srcinfo);

    // Specify data source for decompression
    JThumb_stdio_src( &srcinfo);

    // Read file header then start decompression
    jpeg_read_header( &srcinfo, TRUE);
    jpeg_start_decompress( &srcinfo);

    cbRow = (srcinfo.output_width * srcinfo.output_components + 3) & ~3;
    cntRows = srcinfo.rec_outbuf_height;
    buffer = (*srcinfo.mem->alloc_sarray)
		      ( (j_common_ptr)&srcinfo, JPOOL_IMAGE, cbRow, cntRows);

    cjt->cxDst = srcinfo.output_width;
    cjt->cyDst = srcinfo.output_height;
    cjt->cbPix = srcinfo.output_components;
    cjt->cbDst = cbRow * srcinfo.output_height;
    cjt->pDst = malloc( cjt->cbDst);
    if (!cjt->pDst) {
        cjpg.rtn = CAMERR_MALLOC;
        break;
    }

    ptr = &cjt->pDst[cjt->cbDst - cbRow];

    while (srcinfo.output_scanline < srcinfo.output_height) {
        jpeg_read_scanlines( &srcinfo, buffer, cntRows);
        for (ctr = 0; ctr < cntRows; ctr++, ptr -= cbRow)
            memcpy( ptr, buffer[ctr], cbRow);
    }

    // Finish decompression and release memory
    jpeg_finish_decompress( &srcinfo);

} while (0);

    jpeg_destroy_decompress( &srcinfo);

    if (cjpg.rtn && cjt->pDst) {
        free( cjt->pDst);
        cjt->pDst = 0;
    }

    // All done
    return (cjpg.rtn);
}

/****************************************************************************/

// Setup data source manager.  For thumbnails, we already have the
// entire image, so most of the methods are no-ops.  We give the library
// what we have & if it needs more, we'll abort.

void        JThumb_stdio_src( j_decompress_ptr cinfo)

{
    CAMJThumbInt *  cjpg = (CAMJThumbInt *)cinfo->client_data;

    cinfo->src = (struct jpeg_source_mgr *) (*cinfo->mem->alloc_small)
                 ((j_common_ptr) cinfo, JPOOL_PERMANENT, SIZEOF(struct jpeg_source_mgr));

    cinfo->src->init_source = JThumb_init_source;
    cinfo->src->fill_input_buffer = JThumb_fill_input_buffer;
    cinfo->src->skip_input_data = JThumb_skip_input_data;
    cinfo->src->resync_to_restart = jpeg_resync_to_restart; // use default method
    cinfo->src->term_source = JThumb_term_source;
    cinfo->src->bytes_in_buffer = cjpg->cjt->cbSrc;
    cinfo->src->next_input_byte = cjpg->cjt->pSrc;

    return;
}

/****************************************************************************/

// Initialize source - called by jpeg_read_header before
// any data is actually read.  For thumbnails, it's a no-op.

void        JThumb_init_source( j_decompress_ptr cinfo)
{
    return;
}

/****************************************************************************/

// Fill the input buffer - called whenever buffer is emptied.
// This should never be called because we started out with a
// complete image.  If it's called once, return some EOIs in
// the hope that the library will stop processing.  If it's
// called again, abort.

boolean     JThumb_fill_input_buffer( j_decompress_ptr cinfo)

{
    CAMJThumbInt *  cjpg = (CAMJThumbInt *)cinfo->client_data;

    if (cjpg->eoiCtr) {
        printf( "2nd call to JThumb_fill_input_buffer - aborting\n");
        cjpg->rtn = CAMERR_NEEDDATA;
        JThumb_abort( (j_common_ptr)cinfo);
    }

    printf( "1st call to JThumb_fill_input_buffer - returning EOI\n");
    cinfo->src->bytes_in_buffer = sizeof(cjpg->eoi);
    cinfo->src->next_input_byte = cjpg->eoi;
    cjpg->eoiCtr++;

    return (TRUE);
}

/****************************************************************************/

// Skip data --- used to skip over a potentially large amount of
// uninteresting data (such as an APPn marker).

void        JThumb_skip_input_data( j_decompress_ptr cinfo, long num_bytes)

{
    if (num_bytes > 0) {
        while (num_bytes > (long) cinfo->src->bytes_in_buffer) {
            num_bytes -= (long) cinfo->src->bytes_in_buffer;
            JThumb_fill_input_buffer( cinfo);
        }
        cinfo->src->next_input_byte += (size_t)num_bytes;
        cinfo->src->bytes_in_buffer -= (size_t)num_bytes;
    }

    return;
}

/****************************************************************************/

// Terminate source - called by jpeg_finish_decompress after
// all data has been read.  For thumbnails, it's a no-op.

void        JThumb_term_source( j_decompress_ptr cinfo)

{
    return;
}

/****************************************************************************/

// Error exit handler: must not return to caller.

void        JThumb_error_exit( j_common_ptr cinfo)

{
    CAMJThumbInt *  cjpg = (CAMJThumbInt *)cinfo->client_data;

    printf( "JThumb_error_exit called\n");

    // Always display the message
    (*cinfo->err->output_message)( cinfo);

    cjpg->rtn = CAMERR_LIBJPEG;
    JThumb_abort( cinfo);

    exit( EXIT_FAILURE);
}

/****************************************************************************/

void        JThumb_abort( j_common_ptr cinfo)

{
    CAMJThumbInt *  cjpg = (CAMJThumbInt *)cinfo->client_data;

    printf( "JThumb_abort called\n");
    longjmp( cjpg->jmpbuf, 1);

    exit( EXIT_FAILURE);
}

/****************************************************************************/

