/***************************************************************************/
//  camusb.h
//
//  - header for USB structures and functions
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

#ifndef _CAMUSB_H_
#define _CAMUSB_H_

/***************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "camcodes.h"

#pragma pack(1)

/***************************************************************************/

#ifndef _CAM_ALIASES
#define _CAM_ALIASES
typedef void *  CAMHandle;      // alias for CAMCameraPtr
typedef void *  CAMDevice;      // alias for struct usb_device *
#endif

/***************************************************************************/

typedef struct _CAMUsbInfo {
    uint32_t    VendorId;
    uint32_t    ProductId;
    uint32_t    bcdDevice;
    CAMDevice   Device;
    char *      BusName;
    uint32_t    DeviceNbr;
    uint32_t    IsPTP;
} CAMUsbInfo;


#define CAM_MAXCAMERAS      4
#define CAM_MAXMSD          7


typedef struct _CAMCameraList {
    uint16_t    fStartup;
    uint16_t    fEjected;
    void **     thisCamPtr;
    uint32_t    cntPTP;
    uint32_t    cntMSD;
    uint32_t    cntList;
    char **     apsz;
    char        achDrives[CAM_MAXMSD+1];
    CAMUsbInfo  Cameras[CAM_MAXCAMERAS];
} CAMCameraList;

typedef CAMCameraList *CAMCameraListPtr;

/***************************************************************************/

typedef struct _CAMDeviceInfo {
    uint32_t    StandardVersion;
    uint32_t    VendorExtensionID;
    uint32_t    VendorExtensionVersion;
    char *      VendorExtensionDesc;
    uint32_t    FunctionalMode;
    uint32_t    cntOperationsSupported;
    uint32_t *  OperationsSupported;
    uint32_t    cntEventsSupported;
    uint32_t *  EventsSupported;
    uint32_t    cntDevicePropertiesSupported;
    uint32_t *  DevicePropertiesSupported;
    uint32_t    cntCaptureFormats;
    uint32_t *  CaptureFormats;
    uint32_t    cntImageFormats;
    uint32_t *  ImageFormats;
    char *      Manufacturer;
    char *      Model;
    char *      DeviceVersion;
    char *      SerialNumber;
} CAMDeviceInfo;

typedef CAMDeviceInfo *CAMDeviceInfoPtr;

/***************************************************************************/

// this contains the essential data for the current camera

typedef struct _CAM {
    uint32_t        camType;        // USB or MSD
    CAMHandle       hCam;           // only camusb.c knows what this is
    CAMHandle       hDisconnect;    // used when camera is disconnected
    CAMUsbInfo *    pUsbInfo;       // struct that identifies this device
    CAMDeviceInfo * pDevInfo;       // details of what the camera can do
    uint32_t        hevTerm;        // used to terminate the Notify thread
    uint32_t        hmtxCam;        // controls access to the camera
    uint32_t        camFlags;       // method to get a partial image
    char            drive[4];       // MSD drive
    char **         paPaths;        // array of ptrs to MSD directories
    char            szName[64];     // title for camera or drive
} CAM;

typedef CAM *CAMPtr;

#define CAM_TYPE_USB            0x10
#define CAM_TYPE_MSD            0x20
#define CAM_TYPE_NUL            0x40

#define CAM_PARTIAL_CANCEL      0
#define CAM_PARTIAL_STANDARD    1
#define CAM_PARTIAL_CANON       2
#define CAM_PARTIAL_MASK        3

#define CAM_MUTEXWAIT           13000

/****************************************************************************/

typedef struct _CAMObjectInfo {
    uint32_t    camType;        // 0x10 = USB; 0x20 = MSD
    char *      FilePath;
    uint32_t    Rotation;
    uint32_t    ThumbOffset;
    uint32_t    StorageID;      // PTP data starts here
    uint32_t    ObjectFormat;
    uint32_t    ProtectionStatus;
    uint32_t    ObjectCompressedSize;
    uint32_t    ThumbFormat;
    uint32_t    ThumbCompressedSize;
    uint32_t    ThumbPixWidth;
    uint32_t    ThumbPixHeight;
    uint32_t    ImagePixWidth;
    uint32_t    ImagePixHeight;
    uint32_t    ImageBitDepth;
    uint32_t    ParentObject;
    uint32_t    AssociationType;
    uint32_t    AssociationDesc;
    uint32_t    SequenceNumber;
    char *      Filename;
    time_t      CaptureDate;
    time_t      ModificationDate;
    char *      Keywords;
} CAMObjectInfo;

typedef CAMObjectInfo *CAMObjectInfoPtr;

/***************************************************************************/

typedef struct _CAMStorageID {
    uint32_t    cntStorageIDs;
    uint32_t *  StorageIDs;
} CAMStorageID;

typedef CAMStorageID *CAMStorageIDPtr;

/***************************************************************************/

typedef struct _CAMStorageInfo {
    uint32_t    StorageType;
    uint32_t    FilesystemType;
    uint32_t    AccessCapability;
    uint64_t    MaxCapacity;
    uint64_t    FreeSpaceInBytes;
    uint32_t    FreeSpaceInImages;
    char *      StorageDescription;
    char *      VolumeLabel;
} CAMStorageInfo;

typedef CAMStorageInfo *CAMStorageInfoPtr;

/***************************************************************************/

typedef struct _CAMObjectHandle {
    uint32_t    cntHandles;
    uint32_t *  Handles;
} CAMObjectHandle;

typedef CAMObjectHandle *CAMObjectHandlePtr;

/***************************************************************************/

typedef struct _CAMPropertyValue {
    uint32_t    DataType;
    uint32_t    cntValue;
    char *      Value;
} CAMPropertyValue;

typedef CAMPropertyValue *CAMPropertyValuePtr;

/***************************************************************************/

typedef struct _CAMPropertyRange {
    uint32_t *  MinimumValue;
    uint32_t *  MaximumValue;
    uint32_t *  StepSize;
} CAMPropertyRange;

typedef CAMPropertyRange *CAMPropertyRangePtr;

typedef struct _CAMPropertyEnum {
    uint32_t    cntSupportedValues;
    uint32_t *  SupportedValues;
} CAMPropertyEnum;

typedef CAMPropertyEnum *CAMPropertyEnumPtr;

typedef struct _CAMPropertyDesc {
    uint32_t    DevicePropertyCode;
    uint32_t    DataType;
    uint32_t    GetSet;
    uint32_t    cntDefaultValue;
    char *      DefaultValue;
    uint32_t    cntCurrentValue;
    char *      CurrentValue;
    uint32_t    FormFlag;
    union {
        CAMPropertyRange  Range;
        CAMPropertyEnum   Enum;
    } Form;
} CAMPropertyDesc;

typedef CAMPropertyDesc *CAMPropertyDescPtr;

#define CAM_PROP_None                       0
#define CAM_PROP_RangeForm                  1
#define CAM_PROP_EnumForm                   2
#define CAM_PROP_NotSupported               0xff00

#define CAM_PROP_ReadOnly                   0
#define CAM_PROP_ReadWrite                  1

/***************************************************************************/
//  Miscellanea
/***************************************************************************/

// This is intended as an optimization based on the original, published
// source code for usbcalls.  Whether it does anything in the current
// version is unknown since it's source hasn't been published.  Anyway...
//
// The buffer usbcalls passes to usbresmg has to be page-aligned.  If the
// app-supplied buffer isn't, usbcalls has to create its own buffer and
// then copy lots of data.  To avoid this, camcam.SaveObject() passes an
// unaligned buffer for the first 500 bytes of data, then uses a large
// buffer for the rest that is guaranteed to be page-aligned.

typedef struct _CAMReadData {
    uint32_t    junk[3];
    char        data[500];
} CAMReadData;

/***************************************************************************/

#define CAM_STORAGE             1
#define CAM_FILESYSTEM          2
#define CAM_STORAGE_ACCESS      3
#define CAM_ASSOCIATION         4
#define CAM_PROTECTION          5
#define CAM_OBJECT_FORMAT       6
#define CAM_DATATYPE            7

/***************************************************************************/

#define CAM_TYPE_UNDEF                      0x0000
#define CAM_TYPE_INT8                       0x0001
#define CAM_TYPE_UINT8                      0x0002
#define CAM_TYPE_INT16                      0x0003
#define CAM_TYPE_UINT16                     0x0004
#define CAM_TYPE_INT32                      0x0005
#define CAM_TYPE_UINT32                     0x0006
#define CAM_TYPE_INT64                      0x0007
#define CAM_TYPE_UINT64                     0x0008
#define CAM_TYPE_INT128                     0x0009
#define CAM_TYPE_UINT128                    0x000A
#define CAM_TYPE_AINT8                      0x4001
#define CAM_TYPE_AUINT8                     0x4002
#define CAM_TYPE_AINT16                     0x4003
#define CAM_TYPE_AUINT16                    0x4004
#define CAM_TYPE_AINT32                     0x4005
#define CAM_TYPE_AUINT32                    0x4006
#define CAM_TYPE_AINT64                     0x4007
#define CAM_TYPE_AUINT64                    0x4008
#define CAM_TYPE_AINT128                    0x4009
#define CAM_TYPE_AUINT128                   0x400A
#define CAM_TYPE_STR                        0xFFFF

/***************************************************************************/

// in camparse.c
uint32_t        Build_CamDeviceInfo( void * pUnpack, CAMDeviceInfoPtr* ppCam);
uint32_t        Build_CamObjectInfo( void * pUnpack, CAMObjectInfoPtr* ppCam);
uint32_t        Build_CamStorageID( void * pUnpack, CAMStorageIDPtr* ppCam);
uint32_t        Build_CamStorageInfo( void * pUnpack, CAMStorageInfoPtr* ppCam);
uint32_t        Build_CamObjectHandle( void * pUnpack, CAMObjectHandlePtr* ppCam);
uint32_t        Build_CamPropertyDesc( void * pUnpack, CAMPropertyDescPtr* ppCam);
uint32_t        Build_CamPropertyValue( void * pUnpack, uint32_t type,
                                        CAMPropertyValuePtr* ppCam);
uint32_t        Build_RawPropertyValue( CAMPropertyValuePtr pCam,
                                        char ** ppPack, uint32_t* pSize);

// in camusb.c
uint32_t        GetUsbDeviceList( CAMDevice * pdevList);
void            FreeUsbDeviceList( CAMDevice devList, CAMDevice devSave);
void            GetCameraList( CAMCameraListPtr pList, CAMDevice devList, uint32_t force);
uint32_t        GetSpecificCamera( CAMUsbInfo * ptr);
uint32_t        GetUsbProductName( CAMDevice device, char * szText);
uint32_t        InitCamera( CAMDevice device, CAMHandle* phCam);
void            TermCamera( CAMHandle * phCam);
uint32_t        UsbStartNotification( CAMUsbInfo * pUsbInfo, uint32_t hevAdd,
                                      uint32_t hevRemove, uint32_t * pulNotifyID);
uint32_t        EndNotification( uint32_t ulNotifyID);
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
uint32_t        SetObjectProtection( CAMHandle hCam, uint32_t handle,
                                     uint32_t protection);
uint32_t        GetPartialObject( CAMHandle hCam, uint32_t handle,
                                  uint32_t offset, uint32_t* count, char** object);
uint32_t        CanonGetPartialObject( CAMHandle hCam, uint32_t handle,
                                       uint32_t offset, uint32_t* count,
                                       uint32_t pos, char** object);
uint32_t        ClearStall( CAMHandle hCam);
void            camcli_error( const char* format, ...);
uint32_t        ReadObjectBegin( CAMHandle hCam, uint32_t handle,
                                 uint32_t *size, void * cnr);
uint32_t        ReadObjectData( CAMHandle hCam, uint32_t *size, char *data);
uint32_t        ReadObjectEnd( CAMHandle hCam);
uint32_t        CancelRequest( CAMHandle hCam);
uint32_t        CheckDeviceStatus( CAMHandle hCam, uint32_t cntRetry);

/***************************************************************************/
#pragma pack()
#endif  //_CAMUSB_H_
/***************************************************************************/

