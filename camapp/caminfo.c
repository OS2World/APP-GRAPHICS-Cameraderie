/****************************************************************************/
// caminfo.c
//
//  - routines that display info about the camera
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
#include "camlibusb.h"

/****************************************************************************/

BOOL            InfoAvailable( void);
MRESULT _System InfoDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
BOOL            ShowGeneralInfo( HWND hwnd);
//void            ShowConfigInfo( CAMPtr camThis);

/****************************************************************************/

static CAMNLS   nlsInfoDlg[] = {
    { IDC_USBTXT,           DLG_Usb },
    { IDC_USBVERTXT,        DLG_Version },
    { IDC_BUSDEVTXT,        DLG_BusDevice },
    { IDC_VENDPRODTXT,      DLG_VendorProduct },
    { IDC_CAMERATXT,        DLG_Camera },
    { IDC_FILEFMTTXT,       DLG_FileFormats },
    { IDC_CAPTURETXT,       DLG_CaptureFormat },
    { IDC_SERNBRTXT,        DLG_SerialNumber },
    { IDC_DEVVERTXT,        DLG_DeviceVersion },
    { 0,                    0 }};

/****************************************************************************/

BOOL        InfoAvailable( void)

{
    CAMPtr  thisCam = (CAMPtr)GetCamPtr();

    return (thisCam && thisCam->camType == CAM_TYPE_USB && !GetHwnd( IDD_INFO));
}

/****************************************************************************/

MRESULT _System InfoDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    switch (msg)
    {
        case WM_COMMAND:
            WinDestroyWindow( hwnd);
            return (0);

        case WM_CONTROL:
            return (0);

        case WM_FOCUSCHANGE:
            if (SHORT1FROMMP(mp2) && !WinIsWindowEnabled( hwnd)) {
                WinPostMsg( WinQueryWindow( hwnd, QW_OWNER),
                            CAMMSG_FOCUSCHANGE, 0, 0);
                return (0);
            }
            break;

        case WM_SYSCOMMAND:
            if (SHORT1FROMMP( mp1) != SC_CLOSE)
                break;
            WinDestroyWindow( hwnd);
            return (0);

        case WM_CLOSE:
            WinDestroyWindow( hwnd);
            return (0);

        case WM_SAVEAPPLICATION:
            WinStoreWindowPos( CAMINI_APP, CAMINI_INFOPOS, hwnd);
            break;

        case WM_DESTROY:
            WindowClosed( IDD_INFO);
            break;

        case WM_INITDLG:
            LoadDlgStrings( hwnd, nlsInfoDlg);
            if (!ShowGeneralInfo( hwnd))
                return ((MRESULT)-1);

            ShowDialog( hwnd, CAMINI_INFOPOS);
            return 0;

    } //end switch (msg)

    return (WinDefDlgProc( hwnd, msg, mp1, mp2));
}

/****************************************************************************/
/***************************************************************************/

BOOL        ShowGeneralInfo( HWND hwnd)

{
    ULONG           code;
    ULONG           ctr;
    HWND            hTemp;
    CAMPtr 	        camThis;
    CAMDeviceInfo * pDevInfo;
    CAMUsbInfo *    pUsbInfo;
    char *          ptr;
    char *          pText;
    char            text[256];

    camThis = (CAMPtr)GetCamPtr();
    if (!camThis)
        return (FALSE);

    pDevInfo = camThis->pDevInfo;
    pUsbInfo = camThis->pUsbInfo;
    if (!pDevInfo || !pUsbInfo)
        return (FALSE);

//    ShowConfigInfo( camThis);

    ptr = strchr( pDevInfo->Model, 0x20);
    if (ptr &&
        strlen( pDevInfo->Manufacturer) >= (ptr - pDevInfo->Model) &&
        !memicmp( pDevInfo->Manufacturer, pDevInfo->Model, (ptr - pDevInfo->Model)))
        pText = pDevInfo->Model;
    else {
        strcpy( text, pDevInfo->Manufacturer);
        strcat( text, " ");
        strcat( text, pDevInfo->Model);
        pText = text;
    }
    WinSetDlgItemText( hwnd, IDC_MAKEMODEL, pText);

    sprintf( text, "%04x", 
             ((usb_device*)pUsbInfo->Device)->descriptor.bcdUSB);
    pText = &text[16];
    if (text[0] != '0')
        *pText++ = text[0];
    *pText++ = text[1];
    *pText++ = '.';
    *pText++ = text[2];
    if (text[3] != '0')
        *pText++ = text[3];
    *pText = 0;
    WinSetDlgItemText( hwnd, IDC_USBVER, &text[16]);

    sprintf( text, "%s - %d", pUsbInfo->BusName, pUsbInfo->DeviceNbr);
    WinSetDlgItemText( hwnd, IDC_BUSDEV, text);

    sprintf( text, "%04X - %04X", pUsbInfo->VendorId, pUsbInfo->ProductId);
    WinSetDlgItemText( hwnd, IDC_VENDPROD, text);

    WinSetDlgItemText( hwnd, IDC_SERNBR, pDevInfo->SerialNumber);

    WinSetDlgItemText( hwnd, IDC_DEVVER, pDevInfo->DeviceVersion);

    hTemp = WinWindowFromID( hwnd, IDC_FILEFMT);
    WinSendMsg( hTemp, LM_DELETEALL, 0, 0);
    if (pDevInfo->cntImageFormats == 0)
        WinSendMsg( hTemp, LM_INSERTITEM, (MP)LIT_SORTASCENDING,
                    GetUnknownString());
    else
        for (ctr = 0; ctr < pDevInfo->cntImageFormats; ctr++) {
            ULONG   ctr2;
            char    capture;
            code = pDevInfo->ImageFormats[ctr];

            for (ctr2 = 0, capture = ' '; ctr2 < pDevInfo->cntCaptureFormats; ctr2++)
                if (pDevInfo->CaptureFormats[ctr2] == code) {
                    capture = '*';
                    break;
                }

            ptr = GetFormatName( code);
            if (!ptr)
                sprintf( text, "[%s - 0x%04X] %c",
                         GetUnknownString(), (int)code, capture);
            else
                sprintf( text, "%s %c", ptr, capture);
            WinSendMsg( hTemp, LM_INSERTITEM, (MP)LIT_SORTASCENDING, (MP)text);
        }
    WinSendMsg( hTemp, LM_SELECTITEM, (MP)0, (MP)TRUE);

    return (TRUE);
}



/***************************************************************************/
/*
void        ShowConfigInfo( CAMPtr camThis)

{
    CAMUsbInfo *    pUsbInfo = camThis->pUsbInfo;
    struct usb_device * pDev = (struct usb_device*)pUsbInfo->Device;
    struct usb_device_descriptor * pd = &pDev->descriptor;

    printf( "bLength= %hhd  bDescriptorType= %hhd  bcdUSB= %hx\n"
            "bDeviceClass= %hhd  bDeviceSubClass= %hhd  bDeviceProtocol= %hhd\n"
            "bMaxPacketSize0= %hhd  idVendor= %hx  idProduct= %hx  bcdDevice= %hx\n",
        pd->bLength, pd->bDescriptorType, pd->bcdUSB, pd->bDeviceClass,
        pd->bDeviceSubClass, pd->bDeviceProtocol, pd->bMaxPacketSize0,
        pd->idVendor, pd->idProduct, pd->bcdDevice);

    return;
}
*/
/***************************************************************************/

