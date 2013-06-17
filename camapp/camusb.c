/***************************************************************************/
//  camusb.c
//
//  - routines that access PTP functionality
//  - routines that interface with USBCALLS
//
/***************************************************************************/

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

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "camusb.h"
#include "camlibusb.h"

#define INCL_DOS
#include <os2.h>
#include "usbcalls.h"

#pragma pack(1)

/***************************************************************************/
/***************************************************************************/

#define CAM_TIMEOUT                 4000

// still-image class-specific requests
#define USB_REQ_CANCEL_REQUEST      0x64
#define USB_REQ_DEVICE_RESET        0x66
#define USB_REQ_GET_DEVICE_STATUS   0x67

#define USB_FEATURE_ENDPOINT_HALT   0x00

#define USB_HOST_TO_DEVICE          (0x00 << 7)
#define USB_DEVICE_TO_HOST          (0x01 << 7)

/***************************************************************************/

// generic request
#define CAM_GetStatus(HANDLE, EP, STATUS) \
    UsbCtrlMessage(HANDLE, (USB_DEVICE_TO_HOST|USB_RECIP_ENDPOINT), \
                   USB_REQ_GET_STATUS, 0, EP, \
                   sizeof(STATUS), (char*)(&STATUS), CAM_TIMEOUT)

// generic request
#define CAM_ClearEpStall(HANDLE, EP) \
    UsbCtrlMessage(HANDLE, USB_RECIP_ENDPOINT, USB_REQ_CLEAR_FEATURE, \
                   USB_FEATURE_ENDPOINT_HALT, EP, \
                   0, 0, CAM_TIMEOUT)

// class-specific request
#define CAM_GetDeviceStatus(HANDLE, STATUS) \
    UsbCtrlMessage(HANDLE, \
                   (USB_TYPE_CLASS|USB_DEVICE_TO_HOST|USB_RECIP_INTERFACE), \
                   USB_REQ_GET_DEVICE_STATUS, 0, 0, \
                   sizeof(STATUS), (char*)(&STATUS), CAM_TIMEOUT)

/***************************************************************************/

// created by InitCamera() to hold USB data;
// referenced externally as CAMHandle, a void *

typedef struct _CAMCamera {
    usb_device *        device;
    USBHANDLE           fd;
    int                 interface;
    int                 configuration;
    int                 epIn;
    int                 epOut;
    int                 epInt;
    uint32_t            maxInPkt;
    uint32_t            maxOutPkt;
    uint32_t            session;
    uint32_t            transaction;
} CAMCamera;

typedef CAMCamera *CAMCameraPtr;

/***************************************************************************/
//  Container structures & macros
/***************************************************************************/

// types of containers used to communicate with camera
#define CAMCNRTYPE_UNDEFINED    0
#define CAMCNRTYPE_COMMAND      1
#define CAMCNRTYPE_DATA         2
#define CAMCNRTYPE_RESPONSE     3
#define CAMCNRTYPE_EVENT        4

// some size macros
#define CAMCNR_MAXSIZE          512
#define CAMCNR_HDRSIZE          (sizeof(CAMCnrHdr))
#define CAMCNR_DATASIZE         (sizeof(CAMDataCnr) - CAMCNR_HDRSIZE)
#define CAMCNR_CALCSIZE(x)      (CAMCNR_HDRSIZE + ((x) * sizeof(uint32_t)))

/***************************************************************************/

// used solely to get the length of the header on the other containers
typedef struct _CAMCnrHdr {
    uint32_t    length;
    uint16_t    type;
    uint16_t    opcode;
    uint32_t    transaction;
} CAMCnrHdr;

// passed to WriteCommand() & ReadResponse() to hold control info;
// the 'dummmy' member ensures the struct is not a multiple of any
// standard USB packet size (needed by ReadResponse())
typedef struct _CAMCmdCnr {
    uint32_t    length;
    uint16_t    type;
    uint16_t    opcode;
    uint32_t    transaction;
    uint32_t    param1;
    uint32_t    param2;
    uint32_t    param3;
    uint32_t    param4;
    uint32_t    param5;
    uint32_t    dummy;
} CAMCmdCnr;

typedef CAMCmdCnr *CAMCmdCnrPtr;

// used internally by ReadData() & WriteData() to get & send data
typedef struct _CAMDataCnr {
    uint32_t    length;
    uint16_t    type;
    uint16_t    opcode;
    uint32_t    transaction;
    char        data[CAMCNR_MAXSIZE - CAMCNR_HDRSIZE];
} CAMDataCnr;

typedef struct _CAMCancelReq {
    uint16_t    opcode;
    uint32_t    transaction;
} CAMCancelReq;

// descriptor for max-length USB unicode string
typedef struct _CAMStringDesc {
    uint8_t     cb;
    uint8_t     type;
    uint16_t    uni[127];
} CAMStringDesc;

/***************************************************************************/
/***************************************************************************/


uint32_t        GetUsbDeviceList( CAMDevice * pdevList);
void            FreeUsbDeviceList( CAMDevice devList, CAMDevice devSave);
void            GetCameraList( CAMCameraListPtr pList, CAMDevice devList, uint32_t force);
uint32_t        GetSpecificCamera( CAMUsbInfo * ptr);
uint32_t        GetUsbProductName( CAMDevice device, char * szText);
uint32_t        InitCamera( CAMDevice device, CAMHandle* phCam);
uint32_t        InitCameraInternal( CAMDevice device, CAMHandle* phCam,
                                    uint32_t opensession);
void            GetEndpoints( CAMCameraPtr pCam);
void            TermCamera( CAMHandle * phCam);
uint32_t        UsbStartNotification( CAMUsbInfo * pUsbInfo, uint32_t hevAdd,
                                      uint32_t hevRemove, uint32_t * pulNotifyID);
uint32_t        EndNotification( uint32_t ulNotifyID);
uint32_t        OpenSession( CAMCameraPtr pCam, uint32_t session);
uint32_t        CloseSession( CAMCameraPtr pCam);
uint32_t        GetObject( CAMHandle hCam, uint32_t handle, char** object);
uint32_t        GetThumb( CAMHandle hCam, uint32_t handle, char** object);
uint32_t        DeleteObject( CAMHandle hCam, uint32_t handle, uint32_t ofc);
uint32_t        GetDeviceInfo( CAMHandle hCam, CAMDeviceInfoPtr* ppDevInfo);
uint32_t        GetObjectHandles( CAMHandle hCam, uint32_t storage,
                                  uint32_t objectformatcode,
                                  uint32_t associationOH,
                                  CAMObjectHandlePtr* ppObjHandles);
uint32_t        GetObjectInfo( CAMHandle hCam, uint32_t handle,
                               CAMObjectInfoPtr* ppObjInfo);
uint32_t        GetStorageIds( CAMHandle hCam, CAMStorageIDPtr* ppStorageIDs);
uint32_t        GetStorageInfo( CAMHandle hCam, uint32_t storageid,
                                CAMStorageInfoPtr* ppStorageInfo);
uint32_t        GetPropertyDesc( CAMHandle hCam, uint32_t propcode, 
                                 CAMPropertyDescPtr* ppPropertyDesc);
uint32_t        GetPropertyValue( CAMHandle hCam,
                                  uint32_t propcode, uint32_t datatype,
                                  CAMPropertyValuePtr* ppPropertyValue);
uint32_t        SetPropertyValue( CAMHandle hCam, uint32_t propcode,
                                  CAMPropertyValuePtr pPropertyValue);
uint32_t        SetObjectProtection( CAMHandle hCam, uint32_t handle,
                                     uint32_t protection);
uint32_t        GetPartialObject( CAMHandle hCam, uint32_t handle,
                                  uint32_t offset, uint32_t* count, char** object);
uint32_t        CanonGetPartialObject( CAMHandle hCam, uint32_t handle,
                                       uint32_t offset, uint32_t* count,
                                       uint32_t pos, char** object);
//void            ResetCamera( CAMDevice device);
uint32_t        ClearStall( CAMHandle hCam);
uint32_t        WriteCommand( CAMCameraPtr pCam, CAMCmdCnrPtr pCnr, uint16_t opcode);
uint32_t        ReadData( CAMCameraPtr pCam, uint16_t opcode, char **data);
uint32_t        WriteData( CAMCameraPtr pCam, uint16_t opcode, char* data,
                           uint32_t size);
uint32_t        ReadResponse( CAMCameraPtr pCam, CAMCmdCnrPtr pCnr, uint16_t opcode);
void            camcli_error( const char* format, ...);
uint32_t        ReadObjectBegin( CAMHandle hCam, uint32_t handle,
                                 uint32_t *size, void * cnr);
uint32_t        ReadObjectData( CAMHandle hCam, uint32_t *size, char *data);
uint32_t        ReadObjectEnd( CAMHandle hCam);
uint32_t        CancelRequest( CAMHandle hCam);
uint32_t        CheckDeviceStatus( CAMHandle hCam, uint32_t cntRetry);

/***************************************************************************/
//  Init / Term
/***************************************************************************/

uint32_t    GetUsbDeviceList( CAMDevice * pdevList)

{
    uint32_t        rtn = 0;

    FreeUsbDeviceList( *pdevList, 0);
    *pdevList = 0;
    rtn = usb_find_devices( (usb_device**)pdevList);
    if (rtn) {
        FreeUsbDeviceList( *pdevList, 0);
        *pdevList = 0;
    }

    return rtn;
}

/***************************************************************************/

void        FreeUsbDeviceList( CAMDevice devList, CAMDevice devSave)

{
    usb_device *    dev = (usb_device*)devList;
    usb_device *    devNext;

    while (dev) {
        devNext = dev->next;
        if (dev == (usb_device*)devSave)
            dev->next = 0;
        else
            usb_free_dev( dev);
        dev = devNext;
    }

    return;
}

/***************************************************************************/

void        GetCameraList( CAMCameraListPtr pList, CAMDevice devList,
                           uint32_t force)

{
    usb_device *        dev;
    CAMUsbInfo *        ptr;

    ptr = pList->Cameras;

    for (dev = (usb_device*)devList; dev; dev = dev->next) {
        if (dev->descriptor.bDeviceClass == USB_CLASS_HUB)
            continue;

        if (dev->config->interface->altsetting->bInterfaceClass ==
                                                USB_CLASS_IMAGE) {
            ptr->IsPTP = TRUE;
            pList->cntPTP++;
        }
        else
            if (!force)
                continue;

        ptr->Device = dev;
        ptr->BusName = "1";
        ptr->DeviceNbr = dev->devnum;
        ptr->VendorId = dev->descriptor.idVendor;
        ptr->ProductId = dev->descriptor.idProduct;
        ptr->bcdDevice = dev->descriptor.bcdDevice;
        ptr++;
        pList->cntList++;

        if (pList->cntList >= CAM_MAXCAMERAS)
            break;
    }

    return;
}

/***************************************************************************/

uint32_t    GetSpecificCamera( CAMUsbInfo * ptr)

{
    uint32_t        rtn = CAMERR_NOCAMERAS;
    usb_device *    devlist = 0;
    usb_device *    dev;

    if (GetUsbDeviceList( (CAMDevice*)&devlist))
        return CAMERR_USBENUM;

    for (dev = devlist; dev; dev = dev->next)
        if (ptr->VendorId  == dev->descriptor.idVendor &&
            ptr->ProductId == dev->descriptor.idProduct &&
            ptr->bcdDevice == dev->descriptor.bcdDevice) {
                ptr->Device = dev;
                ptr->BusName = "1";
                ptr->DeviceNbr = dev->devnum;
                rtn = 0;
                break;
            }

    FreeUsbDeviceList( devlist, (rtn ? 0 : dev));

    return rtn;
}

/***************************************************************************/

// this provides the name of a device for the "Select a Camera" dialog

uint32_t    GetUsbProductName( CAMDevice device, char * szText)

{
    uint32_t        fRtn = FALSE;
    uint32_t        rc = 0;
    USBHANDLE       fd = 0;
    uint32_t        ctr;
    uint16_t *      ptr;
    usb_device *    dev = (usb_device*)device;
    char *          pErr = 0;
    CAMStringDesc   csd;

do {
    if (!dev->descriptor.iProduct) {
        pErr = "no descriptor";
        break;
    }

    // unfortunately, the device has to be opened to get the name
    rc = UsbOpen( &fd,
                  (USHORT)dev->descriptor.idVendor,
                  (USHORT)dev->descriptor.idProduct,
                  (USHORT)dev->descriptor.bcdDevice,
                  (USHORT)USB_OPEN_FIRST_UNUSED);

    if (rc) {
        pErr = "UsbOpen";
        break;
    }

    // get the name; '1033' is the language ID for English which is
    // usually the only ID used - in my limited tests, anything worked
    memset( &csd, 0, sizeof(csd));
    rc = UsbCtrlMessage( fd, USB_DEVICE_TO_HOST, USB_REQ_GET_DESCRIPTOR,
                         (USB_DT_STRING << 8) + dev->descriptor.iProduct,
                         1033, sizeof(csd), (char*)&csd, CAM_TIMEOUT);
    if (rc) {
        pErr = "UsbCtrlMessage";
        break;
    }

    // ensure the descriptor looks at least minimally valid
    if (csd.cb < 4 || csd.type != USB_DT_STRING) {
        pErr = "invalid descriptor";
        break;
    }

    // the string is Unicode but since it's in English,
    // we can use a quick & dirty conversion method
    for (ctr = (csd.cb-2)/2, ptr = csd.uni; ctr; ctr--, ptr++)
        if (*ptr > 0xFF)
            *szText++ = '?';
        else
            *szText++ = (BYTE)*ptr;

    *szText = 0;
    fRtn = TRUE;

} while (0);

    if (fd) {
        rc = UsbClose( fd);
        if (rc && !pErr)
            pErr = "UsbClose";
    }

    if (pErr)
        printf( "%s for product name - id= %04x/%04x  rc= 0x%x\n",
                pErr, dev->descriptor.idVendor, dev->descriptor.idProduct, rc);

    return (fRtn);
}

/***************************************************************************/

uint32_t    InitCamera( CAMDevice device, CAMHandle* phCam)

{
    return (InitCameraInternal( device, phCam, 1));
}

/***************************************************************************/

uint32_t    InitCameraInternal( CAMDevice device, CAMHandle* phCam,
                                uint32_t opensession)
{
    uint32_t        rtn = 0;
    CAMCameraPtr    pCam;
    usb_device *    dev = (usb_device*)device;

do {
    *phCam = 0;

    pCam = malloc( sizeof(CAMCamera));
    if (!pCam)
        return CAMERR_MALLOC;

    memset( pCam, 0, sizeof(CAMCamera));
    pCam->fd = -1;
    pCam->interface = -1;
    pCam->configuration = -1;
    pCam->device = dev;

    rtn = UsbOpen( &pCam->fd,
                   (USHORT)dev->descriptor.idVendor,
                   (USHORT)dev->descriptor.idProduct,
                   (USHORT)dev->descriptor.bcdDevice,
                   (USHORT)USB_OPEN_FIRST_UNUSED);

    if (rtn) {
        camcli_error( "unable to open device - id= %04x/%04x  rc= %lx",
                      dev->descriptor.idVendor, 
                      dev->descriptor.idProduct, rtn);
        rtn = CAMERR_OPENDEVICE;
        break;
    }

    pCam->interface = dev->config->interface->altsetting->bInterfaceNumber;
    GetEndpoints( pCam);

    // UsbDeviceSetConfiguration() is a macro that calls UsbCtrlMessage()
    rtn = UsbDeviceSetConfiguration( pCam->fd, 1);
    if (rtn) {
        camcli_error( "unable to set configuration - rc= %lx", rtn);
        rtn = CAMERR_OPENDEVICE;
        break;
    }
    pCam->configuration = 1;

    if (opensession)
        if (OpenSession( pCam, 1)) {
            camcli_error( "unable to open session for device %04x/%04x",
                          dev->descriptor.idVendor, dev->descriptor.idProduct);
            rtn = CAMERR_OPENSESSION;
            break;
        }

} while (0);

    if (rtn)
        TermCamera( (CAMHandle*)&pCam);
    else
        *phCam = pCam;

    return rtn;
}

/***************************************************************************/

void        GetEndpoints( CAMCameraPtr pCam)
{
    uint32_t    ctr;
    uint32_t    cnt;
    usb_endpoint_descriptor *ep;

    ep = pCam->device->config->interface->altsetting->endpoint;
    cnt = pCam->device->config->interface->altsetting->bNumEndpoints;

    for (ctr = 0; ctr < cnt; ctr++) {
        if (ep[ctr].bmAttributes == USB_ENDPOINT_TYPE_BULK) {
            if ((ep[ctr].bEndpointAddress & USB_ENDPOINT_DIR_MASK) ==
                                            USB_ENDPOINT_DIR_MASK) {
                pCam->epIn = ep[ctr].bEndpointAddress;
                pCam->maxInPkt = ep[ctr].wMaxPacketSize;
            }
            else {
                pCam->epOut = ep[ctr].bEndpointAddress;
                pCam->maxOutPkt = ep[ctr].wMaxPacketSize;
            }
        }
        else
        if (ep[ctr].bmAttributes == USB_ENDPOINT_TYPE_INTERRUPT &&
            (ep[ctr].bEndpointAddress & USB_ENDPOINT_DIR_MASK) ==
                                        USB_ENDPOINT_DIR_MASK)
            pCam->epInt = ep[ctr].bEndpointAddress;
    }

    return;
}

/***************************************************************************/

void        TermCamera( CAMHandle * phCam)
{
    uint32_t     rtn;
    CAMCameraPtr pCam = (CAMCameraPtr)*phCam;

    if (pCam) {
        if (pCam->fd >= 0) {
            if (pCam->session)
                if (CloseSession( pCam))
                    camcli_error( "Could not close session.");

            rtn = ClearStall( pCam);
            if (!rtn) {
                rtn = UsbClose( pCam->fd);
                if (rtn)
                    camcli_error(
                        "unable to close device - id= %x/%x  handle= %x  rc= %x",
                        pCam->device->descriptor.idVendor, 
                        pCam->device->descriptor.idProduct,
                        pCam->fd, (int)rtn);
            }
        }

        FreeUsbDeviceList( pCam->device, 0);
        free( pCam);
        *phCam = 0;
    }

    return;
}

/***************************************************************************/
/***************************************************************************/


uint32_t    UsbStartNotification( CAMUsbInfo * pUsbInfo, uint32_t hevAdd,
                                  uint32_t hevRemove, uint32_t * pulNotifyID)

{
    return (UsbRegisterDeviceNotification( (PUSBNOTIFY)pulNotifyID,
                                           (HEV)hevAdd, (HEV)hevRemove, 
                                           pUsbInfo->VendorId,
                                           pUsbInfo->ProductId,
                                           pUsbInfo->bcdDevice));
}

/***************************************************************************/

uint32_t    EndNotification( uint32_t ulNotifyID)

{
    return (UsbDeregisterNotification( (USBNOTIFY)ulNotifyID));
}

/***************************************************************************/
//  Camera Operations
/***************************************************************************/

uint32_t    OpenSession( CAMCameraPtr pCam, uint32_t session)

{
    uint32_t        rtn;
    uint16_t        opcode;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    // session isn't used by other operations, so its use is
    // primarily as a flag to indicate that a session was opened
    pCam->session = 0;

    // when opening a session, transaction must be zero;
    // WriteCommand() will increment this prior to passing the command
    pCam->transaction = (uint32_t)-1;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_OpenSession;
    cnr.length = CAMCNR_CALCSIZE(1);
    cnr.param1 = session;

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK) {
            pCam->session = session;
            rtn = 0;
    }

    return rtn;
}

/***************************************************************************/

uint32_t    CloseSession( CAMCameraPtr pCam)

{
    uint32_t        rtn;
    uint16_t        opcode;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_CloseSession;
    cnr.length = CAMCNR_CALCSIZE(0);

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK) {
            pCam->session = 0;
            rtn = 0;
    }

    return rtn;
}

/***************************************************************************/

uint32_t    GetObject( CAMHandle hCam, uint32_t handle, char** object)

{
    uint32_t        rtn;
    uint16_t        opcode;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_GetObject;
    cnr.length = CAMCNR_CALCSIZE(1);
    cnr.param1 = handle;

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadData( pCam, opcode, object))   == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK &&
        *object)
            rtn = 0;

    return rtn;
}

/***************************************************************************/

uint32_t    GetThumb( CAMHandle hCam, uint32_t handle, char** object)

{
    uint32_t        rtn;
    uint16_t        opcode;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_GetThumb;
    cnr.length = CAMCNR_CALCSIZE(1);
    cnr.param1 = handle;

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadData( pCam, opcode, object))   == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK &&
        *object)
            rtn = 0;

    return rtn;
}

/***************************************************************************/

uint32_t    DeleteObject( CAMHandle hCam, uint32_t handle, uint32_t ofc)

{
    uint32_t        rtn;
    uint16_t        opcode;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    // the docs specify 1 or 2 args; this code used to provide 2,
    // with the 2nd set to zero but this caused errors on a Sony
    // and did nothing to fix can't-delete errors on other cameras
    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_DeleteObject;
    cnr.length = CAMCNR_CALCSIZE(1);
    cnr.param1 = handle;

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK)
            rtn = 0;

    return rtn;
}

/***************************************************************************/

uint32_t    GetDeviceInfo( CAMHandle hCam, CAMDeviceInfoPtr* ppDevInfo)

{
    uint32_t        rtn;
    uint16_t        opcode;
    char *          data = 0;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_GetDeviceInfo;
    cnr.length = CAMCNR_CALCSIZE(0);

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadData( pCam, opcode, &data))    == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK)
            rtn = Build_CamDeviceInfo( data, ppDevInfo);

    free( data);
    return rtn;
}

/***************************************************************************/

uint32_t    GetObjectHandles( CAMHandle hCam, uint32_t storage,
                              uint32_t objectformatcode,
                              uint32_t associationOH,
                              CAMObjectHandlePtr* ppObjHandles)

{
    uint32_t        rtn;
    uint16_t        opcode;
    char *          data = 0;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_GetObjectHandles;
    cnr.length = CAMCNR_CALCSIZE(3);
    cnr.param1 = storage;
    cnr.param2 = objectformatcode;
    cnr.param3 = associationOH;

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadData( pCam, opcode, &data))    == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK)
            rtn = Build_CamObjectHandle( data, ppObjHandles);

    free( data);
    return rtn;
}

/***************************************************************************/

uint32_t    GetObjectInfo( CAMHandle hCam, uint32_t handle,
                           CAMObjectInfoPtr* ppObjInfo)

{
    uint32_t        rtn;
    uint16_t        opcode;
    char *          data = 0;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_GetObjectInfo;
    cnr.length = CAMCNR_CALCSIZE(1);
    cnr.param1 = handle;

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadData( pCam, opcode, &data))    == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK)
            rtn = Build_CamObjectInfo( data, ppObjInfo);

    free( data);
    return rtn;
}

/***************************************************************************/

uint32_t    GetStorageIds( CAMHandle hCam, CAMStorageIDPtr* ppStorageIDs)

{
    uint32_t        rtn;
    uint16_t        opcode;
    char *          data = 0;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_GetStorageIDs;
    cnr.length = CAMCNR_CALCSIZE(0);

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadData( pCam, opcode, &data))    == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK)
            rtn = Build_CamStorageID( data, ppStorageIDs);

    free( data);
    return rtn;
}

/***************************************************************************/

uint32_t    GetStorageInfo( CAMHandle hCam, uint32_t storageid,
                            CAMStorageInfoPtr* ppStorageInfo)

{
    uint32_t        rtn;
    uint16_t        opcode;
    char *          data = 0;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_GetStorageInfo;
    cnr.length = CAMCNR_CALCSIZE(1);
    cnr.param1 = storageid;

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadData( pCam, opcode, &data))    == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK)
            rtn = Build_CamStorageInfo( data, ppStorageInfo);

    free( data);
    return rtn;
}

/***************************************************************************/

uint32_t    GetPropertyDesc( CAMHandle hCam, uint32_t propcode, 
                             CAMPropertyDescPtr* ppPropertyDesc)

{
    uint32_t        rtn;
    uint16_t        opcode;
    char *          data = 0;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_GetDevicePropDesc;
    cnr.length = CAMCNR_CALCSIZE(1);
    cnr.param1 = propcode;

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadData( pCam, opcode, &data))    == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK)
            rtn = Build_CamPropertyDesc( data, ppPropertyDesc);

    free( data);
    return rtn;
}

/***************************************************************************/

uint32_t    GetPropertyValue( CAMHandle hCam,
                              uint32_t propcode, uint32_t datatype,
                              CAMPropertyValuePtr* ppPropertyValue)

{
    uint32_t        rtn;
    uint16_t        opcode;
    char *          data = 0;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_GetDevicePropValue;
    cnr.length = CAMCNR_CALCSIZE(1);
    cnr.param1 = propcode;

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadData( pCam, opcode, &data))    == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK)
            rtn = Build_CamPropertyValue( data, datatype, ppPropertyValue);

    free( data);
    return rtn;
}

/***************************************************************************/

uint32_t    SetPropertyValue( CAMHandle hCam, uint32_t propcode,
                              CAMPropertyValuePtr pPropertyValue)

{
    uint32_t        rtn;
    uint32_t        size;
    uint16_t        opcode;
    char *          data = 0;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_SetDevicePropValue;
    cnr.length = CAMCNR_CALCSIZE(1);
    cnr.param1 = propcode;

    if ((rtn = Build_RawPropertyValue( pPropertyValue, &data, &size)) == 0 &&
        (rtn = WriteCommand( pCam, &cnr, opcode))    == PTP_RC_OK &&
        (rtn = WriteData( pCam, opcode, data, size)) == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode))    == PTP_RC_OK)
            rtn = 0;

    free( data);
    return rtn;
}

/***************************************************************************/

uint32_t    SetObjectProtection( CAMHandle hCam, uint32_t handle,
                                 uint32_t protection)

{
    uint32_t        rtn;
    uint16_t        opcode;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_SetObjectProtection;
    cnr.length = CAMCNR_CALCSIZE(2);
    cnr.param1 = handle;
    cnr.param2 = (protection ? 1 : 0);

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK)
            rtn = 0;

    return rtn;
}

/***************************************************************************/

uint32_t    GetPartialObject( CAMHandle hCam, uint32_t handle,
                              uint32_t offset, uint32_t* count, char** object)

{
    uint32_t        rtn;
    uint16_t        opcode;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_GetPartialObject;
    cnr.length = CAMCNR_CALCSIZE(3);
    cnr.param1 = handle;
    cnr.param2 = offset;
    cnr.param3 = *count;

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadData( pCam, opcode, object))   == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK &&
        *object) {
            *count = cnr.param1;
            rtn = 0;
    }

    return rtn;
}

/***************************************************************************/

uint32_t    CanonGetPartialObject( CAMHandle hCam, uint32_t handle,
                                   uint32_t offset, uint32_t* count,
                                   uint32_t pos, char** object)

{
    uint32_t        rtn;
    uint16_t        opcode;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cnr;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    memset( &cnr, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_CANON_GetPartialObject;
    cnr.length = CAMCNR_CALCSIZE(4);
    cnr.param1 = handle;
    cnr.param2 = offset;
    cnr.param3 = *count;
    cnr.param4 = pos;

    if ((rtn = WriteCommand( pCam, &cnr, opcode)) == PTP_RC_OK &&
        (rtn = ReadData( pCam, opcode, object))   == PTP_RC_OK &&
        (rtn = ReadResponse( pCam, &cnr, opcode)) == PTP_RC_OK &&
        *object) {
            *count = cnr.param1;
            rtn = 0;
    }

    return rtn;
}

/***************************************************************************/
//  USB Device commands
/***************************************************************************/
/*
void        ResetCamera( CAMDevice device)

{
    uint32_t        rtn;
    uint16_t        status[2] = {0,0};
    CAMCameraPtr    pCam = 0;

do {
    if (InitCameraInternal( device, (CAMHandle*)&pCam, 0))
        break;

    rtn = CAM_GetDeviceStatus( pCam->fd, status);
    if (ClearStall( (CAMHandle)pCam))
        break;

    rtn = CAM_GetDeviceStatus( pCam->fd, status);
    if (rtn) 
        camcli_error( "Unable to get device status - rc= %x", (int)rtn);
    else    {
        if (status[1] == PTP_RC_OK) 
            printf( "Device status OK\n");
        else
            camcli_error( "Device status 0x%04X", status[1]);
    }
    
    rtn = UsbCtrlMessage( pCam->fd, (USB_HOST_TO_DEVICE |
                          USB_TYPE_CLASS | USB_RECIP_INTERFACE),
                          USB_REQ_DEVICE_RESET, 0, 0,
                          0, 0, CAM_TIMEOUT);
    if (rtn)
        camcli_error( "Unable to reset device");

    rtn = CAM_GetDeviceStatus( pCam->fd, status);

} while (0);

    TermCamera( (CAMHandle*)&pCam);
    return;
}
*/
/***************************************************************************/

uint32_t    ClearStall( CAMHandle hCam)

{
    uint32_t        rtn;
    uint16_t        status;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;

    if (!pCam)
        return CAMERR_NULLCAMERAPTR;

    // check the in endpoint status
    if (pCam->epIn) {
        status = 0;
        rtn = CAM_GetStatus( pCam->fd, pCam->epIn, status);

        if (rtn >= 0 && status)
            rtn = CAM_ClearEpStall( pCam->fd, pCam->epIn);

        if (rtn < 0)
            camcli_error( "Unable to reset input pipe.");
    }
    
    // check the out endpoint status
    if (pCam->epOut) {
        status = 0;
        rtn = CAM_GetStatus( pCam->fd, pCam->epOut, status);

        if (rtn >= 0 && status)
            rtn = CAM_ClearEpStall( pCam->fd, pCam->epOut);

        if (rtn < 0)
            camcli_error( "Unable to reset output pipe.");
    }
    
    // check the interrupt endpoint status
    if (pCam->epInt) {
        status = 0;
        rtn = CAM_GetStatus( pCam->fd, pCam->epInt, status);

        if (rtn >= 0 && status)
            rtn = CAM_ClearEpStall( pCam->fd, pCam->epInt);

        if (rtn < 0)
            camcli_error( "Unable to reset interrupt pipe.");
    }

    return 0;
}

/***************************************************************************/
//  Camera I/O functions
/***************************************************************************/

uint32_t    WriteCommand( CAMCameraPtr pCam, CAMCmdCnrPtr pCnr, uint16_t opcode)

{
    uint32_t    rtn;

    if (!pCam)
        rtn = CAMERR_NULLCAMERAPTR;
    else {
        pCnr->type        = CAMCNRTYPE_COMMAND;
        pCnr->opcode      = opcode;
        pCnr->transaction = ++(pCam->transaction);

        rtn = UsbBulkWrite( pCam->fd, (UCHAR)pCam->epOut,
                            (UCHAR)pCam->interface, (ULONG)pCnr->length,
                            (char*)pCnr, (ULONG)CAM_TIMEOUT);
    }

    if (rtn) {
        camcli_error(
            "USB write command failed - pCnr= %x  opcode= 0x%04x  size=%d  rc= 0x%04x",
            (int)pCnr, (int)opcode, (int)pCnr->length, (int)rtn);
        rtn = CAMERR_USBFAILURE;
    }
    else
        rtn = PTP_RC_OK;

    return rtn;
}

/***************************************************************************/

uint32_t    ReadData( CAMCameraPtr pCam, uint16_t opcode, char **data)

{
    uint32_t    rtn = PTP_RC_OK;
    uint32_t    rc;
    uint32_t    cnt;
    uint32_t    tot;
    CAMDataCnr  cnr;

do {
    if (!pCam) {
        rtn = CAMERR_NULLCAMERAPTR;
        break;
    }

    memset( &cnr, 0, sizeof(CAMDataCnr));

    tot = sizeof(CAMDataCnr);
    rc = UsbBulkRead( pCam->fd, (UCHAR)pCam->epIn,
                      (UCHAR)pCam->interface, (PULONG)&tot,
                      (char*)&cnr, (ULONG)CAM_TIMEOUT);

//  I wish Froloff would make up his mind about error codes
//    if (rc && rc != USB_ERROR_LESSTRANSFERED && rc != 7004 && rc != 0x7004 && rc != 0x8004) {
    if (rc && rc != USB_ERROR_LESSTRANSFERED &&
        rc != 8004 && rc != 0x8004 && rc != 7004 && rc != 0x7004) {
        rtn = rc;
        break;
    }

    if (cnr.type != CAMCNRTYPE_DATA || tot == 0) {
        rtn = CAMERR_NODATA;
        break;
    }

    if (cnr.opcode != opcode) {
        rtn = cnr.opcode;
        if (rtn != PTP_RC_OK)
            break;
    }

    tot = cnr.length - CAMCNR_HDRSIZE;
    if (*data == 0) {
        *data = malloc( tot);
        memset( *data, 0, tot);
    }

    cnt = (tot > CAMCNR_DATASIZE) ? CAMCNR_DATASIZE : tot;
    tot -= cnt;
    memcpy( *data, cnr.data, cnt);

    if (cnr.length % pCam->maxInPkt == 0)
        tot++;

    if (tot == 0)
        break;

    if (!pCam) {
        rtn = CAMERR_NULLCAMERAPTR;
        break;
    }

    rc = UsbBulkRead( pCam->fd, (UCHAR)pCam->epIn,
                      (UCHAR)pCam->interface, (PULONG)&tot,
                      (char*)&(*data)[cnt], (ULONG)CAM_TIMEOUT);

//  I wish Froloff would make up his mind about error codes
//    if (rc && rc != USB_ERROR_LESSTRANSFERED && rc != 7004 && rc != 0x7004 && rc != 0x8004) {
    if (rc && rc != USB_ERROR_LESSTRANSFERED &&
        rc != 8004 && rc != 0x8004 && rc != 7004 && rc != 0x7004) {
        rtn = rc;
        break;
    }

} while (0);

    if (rtn != PTP_RC_OK) {
        camcli_error( "USB read data failed - code= 0x%04x  rc= 0x%04x",
                     opcode, rtn);
        rtn = CAMERR_USBFAILURE;
    }

    return rtn;
}

/***************************************************************************/

uint32_t    WriteData( CAMCameraPtr pCam, uint16_t opcode, char* data,
                       uint32_t size)

{
    uint32_t    rtn;
    uint32_t    cnt;
    CAMDataCnr  cnr;

    if (!pCam) {
        camcli_error( "USB write data failed - null camera ptr - opcode= 0x%04x",
                      opcode);
        return CAMERR_NULLCAMERAPTR;
    }

    cnr.length      = CAMCNR_HDRSIZE + size;
    cnr.type        = CAMCNRTYPE_DATA;
    cnr.opcode      = opcode;
    cnr.transaction = pCam->transaction;

    cnt = (size > CAMCNR_DATASIZE) ? CAMCNR_DATASIZE : size;
    size -= cnt;
    memcpy( cnr.data, data, cnt);

    rtn = UsbBulkWrite( pCam->fd, (UCHAR)pCam->epOut,
                        (UCHAR)pCam->interface, (ULONG)cnt+CAMCNR_HDRSIZE,
                        (char*)&cnr, (ULONG)CAM_TIMEOUT);

    if (rtn == 0 && size)
        rtn = UsbBulkWrite( pCam->fd, (UCHAR)pCam->epOut,
                            (UCHAR)pCam->interface, (ULONG)size,
                            (char*)&data[cnt], (ULONG)CAM_TIMEOUT);

    if (rtn) {
        camcli_error( "USB write data failed - opcode= 0x%04x  rc= 0x%04x",
                     opcode, rtn);
        rtn = CAMERR_USBFAILURE;
    }
    else
        rtn = PTP_RC_OK;

    return rtn;
}

/***************************************************************************/

uint32_t    ReadResponse( CAMCameraPtr pCam, CAMCmdCnrPtr pCnr, uint16_t opcode)

{
    uint32_t    rtn = PTP_RC_OK;
    uint32_t    rc;
    uint32_t    size;

    if (!pCam) {
        camcli_error( "USB read response - null camera ptr - opcode= 0x%04x",
                      rtn);
        return CAMERR_NULLCAMERAPTR;
    }

    memset( pCnr, 0, sizeof(CAMCmdCnr));

    size = sizeof(CAMCmdCnr);
    rc = UsbBulkRead( pCam->fd, (UCHAR)pCam->epIn,
                      (UCHAR)pCam->interface, (PULONG)&size,
                      (char*)pCnr, (ULONG)CAM_TIMEOUT);

//  I wish Froloff would stop changing the error codes
//    if (rc && rc != USB_ERROR_LESSTRANSFERED && rc != 7004 && rc != 0x7004 && rc != 0x8004)
    if (rc && rc != USB_ERROR_LESSTRANSFERED &&
        rc != 8004 && rc != 0x8004 && rc != 7004 && rc != 0x7004)
        rtn = rc;
    else
    if (size == 0)
        rtn = CAMERR_NODATA;
    else
    if (pCnr->type != CAMCNRTYPE_RESPONSE)
        rtn = CAMERR_NORESPONSE;
    else
    if (pCnr->opcode != opcode)
        rtn = pCnr->opcode;

    if (rtn != PTP_RC_OK) {
        camcli_error( "USB read response failed - opcode= 0x%04x  rc= 0x%04x",
                      (int)opcode, (int)rtn);
        rtn = CAMERR_USBFAILURE;
    }

    return rtn;
}

/***************************************************************************/
/***************************************************************************/

void        camcli_error( const char* format, ...)

{
    va_list args;
    char    buf[256];

    strcpy( buf, "ERROR: ");
    strcat( buf, format);
    strcat( buf, "\n");

    va_start( args, format);
    vfprintf( stdout, buf, args);
    fflush( stdout);
    va_end (args);
}

/***************************************************************************/
//  special routines used by SaveObject() in camcam.c to save files
/***************************************************************************/

// fetch the PTP header & first 500 bytes of data

uint32_t    ReadObjectBegin( CAMHandle hCam, uint32_t handle,
                             uint32_t *size, void * cnr)

{
    uint32_t        rtn = PTP_RC_OK;
    uint32_t        rc;
    uint32_t        tot;
    uint16_t        opcode = 0;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCmdCnr       cmd;
    CAMDataCnr*     pcnr;

do {
    if (!pCam) {
        rtn = CAMERR_NULLCAMERAPTR;
        break;
    }

    memset( &cmd, 0, sizeof(CAMCmdCnr));
    opcode     = PTP_OC_GetObject;
    cmd.length = CAMCNR_CALCSIZE(1);
    cmd.param1 = handle;

    rtn = WriteCommand( pCam, &cmd, opcode);
    if (rtn != PTP_RC_OK)
        break;

    pcnr = (CAMDataCnr*)cnr;

    tot = sizeof(CAMDataCnr);
    memset( pcnr, 0, tot);
    rc = UsbBulkRead( pCam->fd, (UCHAR)pCam->epIn,
                      (UCHAR)pCam->interface, (PULONG)&tot,
                      (char*)pcnr, (ULONG)CAM_TIMEOUT);

//  I wish Froloff would stop changing the error codes
//    if (rc && rc != USB_ERROR_LESSTRANSFERED && rc != 7004 && rc != 0x7004 && rc != 0x8004) {
    if (rc && rc != USB_ERROR_LESSTRANSFERED &&
        rc != 8004 && rc != 0x8004 && rc != 7004 && rc != 0x7004) {
        camcli_error( "ReadObjectBegin - UsbBulkRead  rc= 0x%04x  tot= %d",
                      (int)rc, (int)tot);
        rtn = rc;
        break;
    }

    if (pcnr->type != CAMCNRTYPE_DATA || tot == 0) {
        rtn = CAMERR_NODATA;
        break;
    }

    if (pcnr->opcode != opcode) {
        rtn = pcnr->opcode;
        if (rtn != PTP_RC_OK)
            break;
    }

    *size = pcnr->length - CAMCNR_HDRSIZE;
    if (*size == 0)
        rtn = CAMERR_NODATA;

} while (0);

    if (rtn != PTP_RC_OK) {
        camcli_error( "ReadObjectBegin failed - code= 0x%04x  rc= 0x%04x",
                      opcode, rtn);
        rtn = CAMERR_USBFAILURE;
    }

    return rtn;
}

/***************************************************************************/

// read the remaining data

uint32_t    ReadObjectData( CAMHandle hCam, uint32_t *size, char *data)

{
    uint32_t        rtn = PTP_RC_OK;
    uint32_t    	rc;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;

    if (!pCam) {
        camcli_error( "ReadObjectData - null camera ptr");
        return CAMERR_NULLCAMERAPTR;
    }

    rc = UsbBulkRead( pCam->fd, (UCHAR)pCam->epIn,
                      (UCHAR)pCam->interface, (PULONG)size,
                      data, (ULONG)CAM_TIMEOUT);

//  I wish Froloff would stop changing the error codes
//    if (rc && rc != USB_ERROR_LESSTRANSFERED && rc != 7004 && rc != 0x7004 && rc != 0x8004) {
    if (rc && rc != USB_ERROR_LESSTRANSFERED &&
        rc != 8004 && rc != 0x8004 && rc != 7004 && rc != 0x7004) {
        camcli_error( "ReadObjectData - UsbBulkRead  rc= 0x%04x", (int)rc);
        rtn = CAMERR_USBFAILURE;
    }
    else
    if (*size == 0) {
        camcli_error( "ReadObjectData - UsbBulkRead returned no data");
        rtn = CAMERR_NODATA;
    }

    return rtn;
}

/***************************************************************************/

// finish by reading the PTP response

uint32_t    ReadObjectEnd( CAMHandle hCam)

{
    CAMCmdCnr       cmd;

    if (!hCam) {
        camcli_error( "ReadObjectEnd - null camera ptr");
        return CAMERR_NULLCAMERAPTR;
    }

    return (ReadResponse( (CAMCameraPtr)hCam, &cmd, PTP_OC_GetObject));
}

/***************************************************************************/
/***************************************************************************/

// enables the user to cancel a file-save part way through it

uint32_t    CancelRequest( CAMHandle hCam)

{
    uint32_t        rtn;
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;
    CAMCancelReq    req;

do {
    if (!pCam) {
        camcli_error( "CancelRequest - null camera ptr");
        return CAMERR_NULLCAMERAPTR;
    }

    req.opcode = PTP_EC_CancelTransaction;
    req.transaction = pCam->transaction;
    rtn = UsbCtrlMessage( pCam->fd,
                   (USB_TYPE_CLASS|USB_HOST_TO_DEVICE|USB_RECIP_INTERFACE),
                   USB_REQ_CANCEL_REQUEST, 0, 0,
                   sizeof(req), (char*)(&req), CAM_TIMEOUT);

    if (rtn) {
        camcli_error( "CancelRequest failed - rc= %x", (int)rtn);
        break;
    }

    rtn = CheckDeviceStatus( hCam, 10);

} while (0);

    return rtn;
}

/***************************************************************************/

uint32_t    CheckDeviceStatus( CAMHandle hCam, uint32_t cntRetry)

{
    uint32_t        rtn = 0;
    uint32_t        ctr;
    uint16_t        status[2] = {0,0};
    CAMCameraPtr    pCam = (CAMCameraPtr)hCam;

    if (!pCam) {
        camcli_error( "CheckDeviceStatus - null camera ptr");
        return CAMERR_NULLCAMERAPTR;
    }

    for (ctr = 0; ctr < cntRetry; ctr++) {
        status[1] = 0;
        rtn = CAM_GetDeviceStatus( pCam->fd, status);
        if (rtn) {
            camcli_error( "CAM_GetDeviceStatus failed - rc= %x", (int)rtn);
            break;
        }

        if (status[1] != PTP_RC_DeviceBusy)
            break;

        DosSleep( 31);
    }

    if (status[1] == PTP_RC_DeviceBusy) {
        printf( "CheckDeviceStatus - still busy after %d retries\n", (int)cntRetry);
        rtn = CAMERR_DEVICEBUSY;
    }
    else
    if (status[1] == PTP_RC_TransactionCanceled) {
        printf( "CheckDeviceStatus - transaction cancelled, clearing stall\n");
        ClearStall( hCam);
    }

    return (rtn);
}

/***************************************************************************/
/***************************************************************************/

#pragma pack()

/***************************************************************************/

