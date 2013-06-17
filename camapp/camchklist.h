/****************************************************************************/
// camchklist.h
//
//  - header for the checklist control
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

#ifndef _CAMCHKLIST_H_
#define _CAMCHKLIST_H_

#define INCL_PM
#include <os2.h>

/****************************************************************************/

typedef struct  _CLITEM {
    LONG    ndx;
    BOOL    chk;
    ULONG   hndl;
    char *  str;
} CLITEM;

#define CLN_CLICKED     1001
#define CLN_DESELECTED  1002
#define CLN_SELECTED    1003

#define CLM_SELECTITEM  (WM_USER + 50)
#define CLM_INSERTITEM  (WM_USER + 51)
#define CLM_REMOVEITEM  (WM_USER + 52)
#define CLM_QUERYITEM   (WM_USER + 53)
#define CLM_SETITEM     (WM_USER + 54)
#define CLM_MOVEITEM    (WM_USER + 55)
#define CLM_QUERYCHECK  (WM_USER + 56)
#define CLM_SETCHECK    (WM_USER + 57)
#define CLM_REFRESHITEM (WM_USER + 58)

#define CLF_LAST        -1
#define CLF_SELECTED    -2

#define CLF_UNCHECKED   0
#define CLF_CHECKED     1
#define CLF_CHKDISABLED 0x10000
#define CLF_CHKHIDDEN   0x20000
#define CLF_CHKMASK     0x30001

/****************************************************************************/

BOOL            CL_Init( HWND hCL, char * pszCnrTitle,
                         char * pszChkTitle, char * pszStrTitle);

#endif  // _CAMCHKLIST_H_

/****************************************************************************/

