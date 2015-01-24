/****************************************************************************/
// camnls.h - v1.5
//
//  - resource ids for NLS strings
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

#ifndef _CAMNLS_H_
#define _CAMNLS_H_

/****************************************************************************/

// Menubar
#define MNU_File                0x2000
#define MNU_Open                0x2001
#define MNU_Refresh             0x2002
#define MNU_SaveNow             0x2003
#define MNU_CameraInfo          0x2004
#define MNU_Exit                0x2005
#define MNU_Sort                0x2006
#define MNU_Indicators          0x2007
#define MNU_Done                0x2008
#define MNU_View                0x2009
#define MNU_Details             0x200A
#define MNU_Small               0x200B
#define MNU_Medium              0x200C
#define MNU_Large               0x200D
#define MNU_Reset               0x200E
#define MNU_Rearrange           0x200F
#define MNU_Colors              0x2010
#define MNU_Groups              0x2011
#define MNU_EditGroups          0x2012
#define MNU_Options             0x2013
#define MNU_Rotate              0x2014
#define MNU_RotateOnLoad        0x2015
#define MNU_RotateOnSave        0x2016
#define MNU_Removables          0x2017
#define MNU_MountDrives         0x2018
#define MNU_EjectInfo           0x2019
#define MNU_ShowEjectDlg        0x201A
#define MNU_AlwaysEject         0x201B
#define MNU_NeverEject          0x201C
#define MNU_Restore             0x201D

// Sort Menu
#define MNU_Action              0x2020
#define MNU_Number              0x2021
#define MNU_Title               0x2022
#define MNU_Group               0x2023
#define MNU_Format              0x2024
#define MNU_XY                  0x2025
#define MNU_ThumbFmt            0x2026
#define MNU_ThumbXY             0x2027
#define MNU_ThumbSize           0x2028
#define MNU_Handle              0x2029
#define MNU_Size                0x202A
#define MNU_Day                 0x202B
#define MNU_Date                0x202C
#define MNU_Time                0x202D

// Popup Menus
#define MNU_Save                0x2030
#define MNU_Delete              0x2031
#define MNU_Unmark              0x2032
#define MNU_UndoTitle           0x2033
#define MNU_RotateLeft          0x2034
#define MNU_RotateRight         0x2035
#define MNU_Show                0x2036
#define MNU_SetGroup            0x2037

#define MNU_SaveAll             0x2038
#define MNU_DeleteAll           0x2039
#define MNU_UnmarkAll           0x203A
#define MNU_UndoAllTitles       0x203B

/****************************************************************************/

// Column Headings
#define COL_Mark                0x2040
#define COL_Nbr                 0x2041
#define COL_Title               0x2042
#define COL_Group               0x2043
#define COL_ImageFormat         0x2044
#define COL_ImageXY             0x2045
#define COL_ThumbFormat         0x2046
#define COL_ThumbXY             0x2047
#define COL_ThumbSize           0x2048
#define COL_Handle              0x2049
#define COL_FileSize            0x204A
#define COL_Day                 0x204B
#define COL_Date                0x204C
#define COL_Time                0x204D

// Column Data
#define COL_Sun                 0x204E
#define COL_Mon                 0x204F
#define COL_Tue                 0x2050
#define COL_Wed                 0x2051
#define COL_Thu                 0x2052
#define COL_Fri                 0x2053
#define COL_Sat                 0x2054
#define COL_TooBig              0x2055
#define COL_Unknown             0x2056

// Status Bar
#define STS_Status              0x2057
#define STS_Copyright           0x2058
#define STS_GetList             0x2059
#define STS_RefreshList         0x205A
#define STS_NoFiles             0x205B
#define STS_ListingFiles        0x205C
#define STS_NoThumbnail         0x205D
#define STS_GetThumbErr         0x205E

/****************************************************************************/

// Common Buttons
#define BTN_EditGroups          0x2070
#define BTN_Exit                0x2071
#define BTN_Yes                 0x2072
#define BTN_No                  0x2073
#define BTN_Cancel              0x2074
#define BTN_Undo                0x2075
#define BTN_Apply               0x2076
#define BTN_OK                  0x2077
#define BTN_Retry               0x2078
#define BTN_Skip                0x2079
#define BTN_Stop                0x207A
#define BTN_Close               0x207B
#define BTN_Replace             0x207C
#define BTN_Create              0x207D
#define BTN_Find                0x207E
#define BTN_New                 0x207F
#define BTN_Delete              0x2080
#define BTN_Undelete            0x2081
#define BTN_Default             0x2082
#define BTN_Continue            0x2083
#define BTN_Open                0x2084

// Date Substitution
#define SUB_Year                0x2088
#define SUB_Month               0x2089
#define SUB_Day                 0x208A

/****************************************************************************/

// Error dialogs
#define DLG_Loading             0x2100
#define DLG_ToEndNow            0x2101
#define DLG_NoCameras           0x2102
#define DLG_PlugIn              0x2103
#define DLG_ToEnd               0x2104

// Disconnected dialog
#define DLG_Disconnected1       0x2105
#define DLG_Disconnected2       0x2106
#define DLG_Disconnected3       0x2107
#define DLG_WindowWillClose1    0x2108
#define DLG_WindowWillClose2    0x2109

// No usbresmg.sys dialog (v1.5)
#define DLG_NoUsbRes1           0x210A
#define DLG_NoUsbRes2           0x210B
#define DLG_NoUsbRes3           0x210C
#define DLG_DoNotShow           0x210D

// Select Camera dialog (v1.5)
#define DLG_Select              0x2110
#define MSG_MB                  0x2111
#define MSG_PTPCamera           0x2112
#define MSG_Unidentified        0x2113
#define MSG_NoCamera            0x2114
#define MSG_DisconnectDevice    0x2115
#define MSG_UnableToEject       0x2116

// Removable Drive Error dialog (v1.5)
#define DLG_Removable1          0x2117
#define DLG_Removable2          0x2118
#define DLG_Removable3          0x2119
#define DLG_Removable4          0x211A
#define DLG_Removable5          0x211B

// Eject RemovableDrives dialog (v1.5)
#define DLG_Eject               0x211D
#define MSG_Ejecting            0x211E
#define MSG_ThisMayTake         0x211F

/****************************************************************************/

// Directory dialog
#define DLG_ChooseDir           0x2120
#define DLG_Drive               0x2121
#define DLG_Directory           0x2122
#define DLG_DirDlgMsg1          0x2123
#define DLG_DirDlgMsg2          0x2124

// Colors dialog
#define DLG_View                0x2125
#define DLG_WindowColors        0x2126
#define DLG_HighlightColors     0x2127
#define DLG_GroupColors         0x2128
#define DLG_SeparatorColor      0x2129

// Column dialog strings
#define MSG_Show                0x212A
#define MSG_Column              0x212B

// Info dialog
#define DLG_Usb                 0x2130
#define DLG_Version             0x2131
#define DLG_BusDevice           0x2132
#define DLG_VendorProduct       0x2133
#define DLG_Camera              0x2134
#define DLG_FileFormats         0x2135
#define DLG_CaptureFormat       0x2136
#define DLG_SerialNumber        0x2137
#define DLG_DeviceVersion       0x2138

/****************************************************************************/

// Confirm Save dialog
#define DLG_SaveChanges         0x2160
#define DLG_Save                0x2161
#define DLG_EraseAfterSave      0x2162
#define DLG_Erase               0x2163
#define DLG_EraseLater          0x2164
#define DLG_DontErase           0x2165

// Save Files dialog
#define DLG_ReplaceAll          0x2166

// Save Files strings
#define MSG_Erasing             0x2167
#define MSG_Saving              0x2168
#define MSG_SavedErased         0x2169

// Done Saving dialog
#define MSG_SavedOK             0x216A
#define MSG_SaveEraseOK         0x216B
#define MSG_NotSaved            0x216C
#define MSG_ErasedOK            0x216D
#define MSG_NotErased           0x216E

// Save Files errors
#define MSG_TooLong1            0x2180
#define MSG_TooLong2            0x2181
#define MSG_FileExists1         0x2182
#define MSG_FileExists2         0x2183
#define MSG_PathNotFound1       0x2184
#define MSG_PathNotFound2       0x2185
#define MSG_CouldNotCreate1     0x2186
#define MSG_CouldNotCreate2     0x2187
#define MSG_GetData             0x2188
#define MSG_OpCancelled         0x2189
#define MSG_OpenFile            0x218A
#define MSG_WriteFile           0x218B
#define MSG_ErasePhoto          0x218C
#define MSG_Fatal1              0x218D
#define MSG_Fatal2              0x218E

/****************************************************************************/

// Groups dialog
#define DLG_GroupName           0x21A0
#define DLG_SaveDir             0x21A1
#define DLG_Template            0x21A2
#define DLG_Serial              0x21A3
#define DLG_Digits              0x21A4
#define DLG_InsertsSerial       0x21A5
#define DLG_CopiesFilename      0x21A6
#define DLG_InsertYMD           0x21A7

// Group dialog strings
#define MSG_Deleted             0x21A8
#define MSG_Btn                 0x21A9
#define MSG_Group               0x21AA
#define MSG_NewGroup            0x21AB

// Group dialog errors
#define MSG_ButtonsAssigned     0x21B0
#define MSG_DirIsBlank          0x21B1
#define MSG_DirCreated          0x21B2
#define MSG_PathIsAFile         0x21B3
#define MSG_DirNotCreated       0x21B4
#define MSG_GroupNameBlank      0x21B5
#define MSG_GroupNameInUse      0x21B6
#define MSG_PathDoesNotExist    0x21B7
#define MSG_PathInvalid         0x21B8

// Default Groups
#define GRP_Default             0x21C0
#define GRP_Family              0x21C1
#define GRP_Travel              0x21C2
#define GRP_Nature              0x21C3
#define GRP_Other               0x21C4
#define GRP_FamilyTempl         0x21C5
#define GRP_TravelTempl         0x21C6
#define GRP_NatureTempl         0x21C7
#define GRP_OtherTempl          0x21C8

/****************************************************************************/

// Intro dialog
#define DLG_Welcome             0x21F0
#define DLG_PleaseSelect        0x21F1
#define DLG_IntroMsg1           0x21F2
#define DLG_IntroMsg2           0x21F3

// Startup errors
#define MSG_InitDisplay         0x21F4
#define MSG_InitCamera          0x21F5
#define MSG_ReopenCamera        0x21F6

/****************************************************************************/
#endif  //_CAMNLS_H_
/****************************************************************************/

