/*++

Copyright (c) 2005, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution. The full text of the license may be found at         
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:

    EventLogInfo.c
    
Abstract:

    Module for clarifying the content of the smbios structure element info.

Revision History

--*/

#include "EfiShellLib.h"
#include "PrintInfo.h"
#include "QueryTable.h"
#include "EventLogInfo.h"
#include "Smbios.h"
#include STRING_DEFINES_FILE

VOID
DisplaySELAccessMethod (
  UINT8 Key,
  UINT8 Option
  )
{
  //
  // Print prompt
  //
  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_ACCESS_METHOD), gSmbiosHiiHandle);
  PRINT_INFO_OPTION (Key, Option);

  //
  // Print value info
  //
  switch (Key) {
  case 0:
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_ONE_EIGHT_BIT), gSmbiosHiiHandle);
    break;

  case 1:
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_TWO_EIGHT_BITS), gSmbiosHiiHandle);
    break;

  case 2:
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_ONE_SIXTEEN_BIT), gSmbiosHiiHandle);
    break;

  case 3:
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_MEM_MAPPED_PHYS), gSmbiosHiiHandle);
    break;

  case 4:
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_AVAIL_VIA_GENERAL), gSmbiosHiiHandle);
    break;

  default:
    if (Key <= 0x7f) {
      PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_AVAIL_FOR_FUTURE_ASSIGN), gSmbiosHiiHandle);
    } else {
      PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_BIOS_VENDOR_OEM), gSmbiosHiiHandle);
    }
  }
}

VOID
DisplaySELLogStatus (
  UINT8 Key,
  UINT8 Option
  )
{
  //
  // Print prompt
  //
  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_LOG_STATUS), gSmbiosHiiHandle);
  PRINT_INFO_OPTION (Key, Option);

  //
  // Print value info
  //
  if ((Key & 0x01) != 0) {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_LOG_AREA_VALID), gSmbiosHiiHandle);
  } else {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_LOG_AREA_VALID), gSmbiosHiiHandle);
  }

  if ((Key & 0x02) != 0) {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_LOG_AREA_FULL), gSmbiosHiiHandle);
  } else {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_LOG_AREA_NOT_FULL), gSmbiosHiiHandle);
  }

  if ((Key & 0xFC) != 0) {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_RES_BITS_NOT_ZERO), gSmbiosHiiHandle, Key & 0xFC);
  }
}

VOID
DisplaySysEventLogHeaderFormat (
  UINT8 Key,
  UINT8 Option
  )
{
  //
  // Print prompt
  //
  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_LOG_HEADER_FORMAT), gSmbiosHiiHandle);
  PRINT_INFO_OPTION (Key, Option);

  //
  // Print value info
  //
  if (Key == 0x00) {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_NO_HEADER), gSmbiosHiiHandle);
  } else if (Key == 0x01) {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_TYPE_LOG_HEADER), gSmbiosHiiHandle);
  } else if (Key <= 0x7f) {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_AVAIL_FOR_FUTURE), gSmbiosHiiHandle);
  } else {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_BIOS_VENDOR), gSmbiosHiiHandle);
  }
}

VOID
DisplaySELLogHeaderLen (
  UINT8 Key,
  UINT8 Option
  )
{
  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_LOG_HEADER_LEN), gSmbiosHiiHandle);
  PRINT_INFO_OPTION (Key, Option);

  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_ONE_VAR_D), gSmbiosHiiHandle, Key & 0x7F);

  //
  // The most-significant bit of the field specifies
  // whether (0) or not (1) the record has been read
  //
  if ((Key & 0x80) != 0) {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_THIS_RECORD_READ), gSmbiosHiiHandle);
  } else {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_THIS_RECORD_NOT_READ), gSmbiosHiiHandle);
  }
}

VOID
DisplaySysEventLogHeaderType1 (
  UINT8 *LogHeader
  )
{
  LOG_HEADER_TYPE1_FORMAT *Header;

  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_SYSTEM_EVENT_LOG), gSmbiosHiiHandle);

  //
  // Print Log Header Type1 Format info
  //
  Header = (LOG_HEADER_TYPE1_FORMAT *) (LogHeader);

  PrintToken (
    STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_OEM_RESERVED),
    gSmbiosHiiHandle,
    Header->OEMReserved[0],
    Header->OEMReserved[1],
    Header->OEMReserved[2],
    Header->OEMReserved[3],
    Header->OEMReserved[4]
    );
  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_MULTIPLE_EVENT_TIME), gSmbiosHiiHandle, Header->METW);
  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_MULTIPLE_EVENT_COUNT), gSmbiosHiiHandle, Header->MECI);
  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_PREBOOT_ADDRESS), gSmbiosHiiHandle, Header->CMOSAddress);
  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_PREBOOT_INDEX), gSmbiosHiiHandle, Header->CMOSBitIndex);
  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_CHECKSUM_STARTING_OFF), gSmbiosHiiHandle, Header->StartingOffset);
  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_CHECKSUN_BYTE_COUNT), gSmbiosHiiHandle, Header->ChecksumOffset);
  PrintToken (
    STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_RESERVED),
    gSmbiosHiiHandle,
    Header->OEMReserved[0],
    Header->OEMReserved[1],
    Header->OEMReserved[2]
    );
  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_HEADER_REVISION), gSmbiosHiiHandle, Header->HeaderRevision);
}

VOID
DisplaySysEventLogHeader (
  UINT8 LogHeaderFormat,
  UINT8 *LogHeader
  )
{
  //
  // Print prompt
  //
  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_LOG_HEADER), gSmbiosHiiHandle);

  //
  // Print value info
  //
  if (LogHeaderFormat == 0x00) {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_NO_HEADER), gSmbiosHiiHandle);
  } else if (LogHeaderFormat == 0x01) {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_TYPE_LOG_HEADER), gSmbiosHiiHandle);
    DisplaySysEventLogHeaderType1 (LogHeader);
  } else if (LogHeaderFormat <= 0x7f) {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_AVAIL_FUTURE_ASSIGN), gSmbiosHiiHandle);
  } else {
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_BIOS_VENDOR), gSmbiosHiiHandle);
  }
}

VOID
DisplayElVdfInfo (
  UINT8 ElVdfType,
  UINT8 *VarData
  )
{
  UINT16  *Word;
  UINT32  *Dword;

  //
  // Display Type Name
  //
  DisplaySELVarDataFormatType (ElVdfType, SHOW_DETAIL);

  //
  // Display Type description
  //
  switch (ElVdfType) {
  case 0:
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_NO_STD_FORMAT), gSmbiosHiiHandle);
    break;

  case 1:
    Word = (UINT16 *) (VarData + 1);
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_SMBIOS_STRUCT_ASSOC), gSmbiosHiiHandle);
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_STRUCT_HANDLE), gSmbiosHiiHandle, *Word);
    break;

  case 2:
    Dword = (UINT32 *) (VarData + 1);
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_MULT_EVENT_COUNTER), gSmbiosHiiHandle, *Dword);
    break;

  case 3:
    Word = (UINT16 *) (VarData + 1);
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_SMBIOS_STRUCT_ASSOC), gSmbiosHiiHandle);
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_STRUCT_HANDLE), gSmbiosHiiHandle, *Word);
    //
    // Followed by a multiple-event counter
    //
    Dword = (UINT32 *) (VarData + 1);
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_MULT_EVENT_COUNTER), gSmbiosHiiHandle, *Dword);
    break;

  case 4:
    Dword = (UINT32 *) (VarData + 1);
    DisplayPostResultsBitmapDw1 (*Dword, SHOW_DETAIL);
    Dword++;
    DisplayPostResultsBitmapDw2 (*Dword, SHOW_DETAIL);
    break;

  case 5:
    Dword = (UINT32 *) (VarData + 1);
    DisplaySELSysManagementTypes (*Dword, SHOW_DETAIL);
    break;

  case 6:
    Dword = (UINT32 *) (VarData + 1);
    DisplaySELSysManagementTypes (*Dword, SHOW_DETAIL);
    //
    // Followed by a multiple-event counter
    //
    Dword = (UINT32 *) (VarData + 1);
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_MULT_EVENT_COUNTER), gSmbiosHiiHandle, *Dword);
    break;

  default:
    if (ElVdfType <= 0x7F) {
      PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_UNUSED_AVAIL_FOR_ASSIGN), gSmbiosHiiHandle);
    } else {
      PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_AVAIL_FOR_SYSTEM), gSmbiosHiiHandle);
    }
  }
}

VOID
DisplaySysEventLogData (
  UINT8   *LogData,
  UINT16  LogAreaLength
  )
{
  LOG_RECORD_FORMAT *Log;
  UINT8             ElVdfType;
  //
  // Event Log Variable Data Format Types
  //
  UINTN             Offset;

  //
  // Print prompt
  //
  PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_SYSTEM_EVENT_LOG_2), gSmbiosHiiHandle);

  //
  // Print Log info
  //
  Offset  = 0;
  Log     = (LOG_RECORD_FORMAT *) LogData;
  while (Log->Type != END_OF_LOG && Offset < LogAreaLength) {
    //
    // Get a Event Log Record
    //
    Log = (LOG_RECORD_FORMAT *) (LogData + Offset);

    //
    // Display Event Log Record Information
    //
    DisplaySELVarDataFormatType (Log->Type, SHOW_DETAIL);
    DisplaySELLogHeaderLen (Log->Length, SHOW_DETAIL);

    Offset += Log->Length;

    //
    // Display Log Header Date/Time Fields
    // These fields contain the BCD representation of the date and time
    // (as read from CMOS) of the occurrence of the event
    // So Print as hex and represent decimal
    //
    PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_DATE), gSmbiosHiiHandle);
    if (Log->Year >= 80 && Log->Year <= 99) {
      Print (L"19");
    } else if (Log->Year <= 79) {
      Print (L"20");
    } else {
      PrintToken (STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_ERROR), gSmbiosHiiHandle);
      continue;
    }

    PrintToken (
      STRING_TOKEN (STR_SMBIOSVIEW_EVENTLOGINFO_TIME_SIX_VARS),
      gSmbiosHiiHandle,
      Log->Year,
      Log->Month,
      Log->Day,
      Log->Hour,
      Log->Minute,
      Log->Second
      );

    //
    // Display Variable Data Format
    //
    if (Log->Length <= (sizeof (LOG_RECORD_FORMAT) - 1)) {
      continue;
    }

    ElVdfType = Log->LogVariableData[0];
    DisplayElVdfInfo (ElVdfType, Log->LogVariableData);
  }
}
