/*++

Copyright (c) 2005 - 2007, Intel Corporation                                                  
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution. The full text of the license may be found at         
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:

  IO.c

Abstract:

  the IO library function

Revision History

--*/

#include "EfiShellLib.h"

#define PRINT_STRING_LEN        1024
#define PRINT_ITEM_BUFFER_LEN   100
#define PRINT_JOINT_BUFFER_LEN  4

typedef struct {
  BOOLEAN Ascii;
  UINTN   Index;
  union {
    CHAR16  *pw;
    CHAR8   *pc;
  } u;
} POINTER;

typedef struct _pitem {

  POINTER Item;
  CHAR16  *Scratch;
  UINTN   Width;
  UINTN   FieldWidth;
  UINTN   *WidthParse;
  CHAR16  Pad;
  BOOLEAN PadBefore;
  BOOLEAN Comma;
  BOOLEAN Long;
} PRINT_ITEM;

typedef struct _pstate {
  //
  // Input
  //
  POINTER fmt;
  VA_LIST args;

  //
  // Output
  //
  CHAR16  *Buffer;
  CHAR16  *End;
  CHAR16  *Pos;
  UINTN   Len;

  UINTN   Attr;
  UINTN   RestoreAttr;

  UINTN   AttrNorm;
  UINTN   AttrHighlight;
  UINTN   AttrError;
  UINTN   AttrBlueColor;
  UINTN   AttrGreenColor;

  INTN (*Output) (VOID *context, CHAR16 *str);
  INTN (*SetAttr) (VOID *context, UINTN attr);
  VOID          *Context;

  //
  // Current item being formatted
  //
  struct _pitem *Item;
} PRINT_STATE;

typedef struct {
  BOOLEAN PageBreak;
  BOOLEAN AutoWrap;
  UINTN   MaxRow;
  UINTN   MaxColumn;
  INTN    InitRow;
  INTN    Row;
  INTN    Column;
  BOOLEAN OmitPrint;
  BOOLEAN OutputPause;
} PRINT_MODE;

PRINT_MODE mPrintMode;

//
// Internal fucntions
//
UINTN
_Print (
  IN PRINT_STATE     *ps
  );

INTN
_SPrint (
  IN VOID     *Context,
  IN CHAR16   *Buffer
  );

UINTN
_IPrint (
  IN UINTN                            Column,
  IN UINTN                            Row,
  IN EFI_SIMPLE_TEXT_OUT_PROTOCOL     *Out,
  IN CHAR16                           *fmt,
  IN CHAR8                            *fmta,
  IN VA_LIST                          args
  );

VOID
_PoolCatPrint (
  IN CHAR16               *fmt,
  IN VA_LIST              args,
  IN OUT POOL_PRINT       *spc,
  IN INTN
    (
  *Output)
    (
      VOID *context,
      CHAR16 *str
    )
  );

INTN
_PoolPrint (
  IN VOID     *Context,
  IN CHAR16   *Buffer
  );

VOID
PFLUSH (
  IN OUT PRINT_STATE     *ps
  );

VOID
IFlushWithPageBreak (
  IN OUT PRINT_STATE     *ps
  );

VOID
PPUTC (
  IN OUT PRINT_STATE     *ps,
  IN CHAR16              c
  );

CHAR16
PGETC (
  IN POINTER      *p
  );

VOID
PITEM (
  IN OUT PRINT_STATE  *ps
  );

VOID
PSETATTR (
  IN OUT PRINT_STATE    *ps,
  IN UINTN              Attr
  );

VOID
SetOutputPause (
  IN BOOLEAN    Pause
  );

VOID
SetCursorPosition (
  IN EFI_SIMPLE_TEXT_OUT_PROTOCOL     *ConOut,
  IN  UINTN                           Column,
  IN  INTN                            Row,
  IN  UINTN                           LineLength,
  IN  UINTN                           TotalRow,
  IN  CHAR16                          *Str,
  IN  UINTN                           StrPos,
  IN  UINTN                           Len
  );

INTN
_DbgOut (
  IN VOID     *Context,
  IN CHAR16   *Buffer
  );

INTN
_SPrint (
  IN VOID     *Context,
  IN CHAR16   *Buffer
  )
/*++
Routine Description:
  
  Print function
   
Arguments:

  Context  - The Context
  Buffer   - The Buffer
  
Returns:

--*/
{
  INTN        len;
  POOL_PRINT  *spc;
  
  ASSERT (Context != NULL);
  ASSERT (Buffer != NULL);

  spc = Context;
  len = StrLen (Buffer);

  //
  // Is the string is over the max truncate it
  //
  if (spc->len + len > spc->maxlen) {
    len = spc->maxlen - spc->len;
  }
  //
  // Append the new text
  //
  CopyMem (spc->str + spc->len, Buffer, len * sizeof (CHAR16));
  spc->len += len;

  //
  // Null terminate it
  //
  if (spc->len < spc->maxlen) {
    spc->str[spc->len] = 0;
  } else if (spc->maxlen) {
    spc->str[spc->maxlen] = 0;
  }

  return 0;
}

VOID
_PoolCatPrint (
  IN CHAR16               *fmt,
  IN VA_LIST              args,
  IN OUT POOL_PRINT       *spc,
  IN INTN
    (
  *Output)
    (
      VOID *context,
      CHAR16 *str
    )
  )
/*++'

Routine Description:

    Pool print

Arguments:
    fmt    - fmt
    args   - args
    spc    - spc
    Output - Output
    
Returns:

--*/
{
  PRINT_STATE ps;

  SetMem (&ps, sizeof (ps), 0);
  ps.Output   = Output;
  ps.Context  = spc;
  ps.fmt.u.pw = fmt;
  ps.args     = args;
  _Print (&ps);
}

UINTN
SPrint (
  OUT CHAR16    *Str,
  IN UINTN      StrSize,
  IN CHAR16     *fmt,
  ...
  )
/*++

Routine Description:

  Prints a formatted unicode string to a buffer

Arguments:

  Str         - Output buffer to print the formatted string into

  StrSize     - Size of Str.  String is truncated to this size.
                A size of 0 means there is no limit

  fmt         - The format string

Returns:

  String length returned in buffer

--*/
{
  POOL_PRINT  spc;
  VA_LIST     args;

  VA_START (args, fmt);
  spc.str     = Str;
  spc.maxlen  = StrSize / sizeof (CHAR16) - 1;
  spc.len     = 0;

  _PoolCatPrint (fmt, args, &spc, _SPrint);
  return spc.len;
}

UINTN
VSPrint (
  OUT CHAR16  *Str,
  IN UINTN    StrSize,
  IN CHAR16   *fmt,
  IN VA_LIST  vargs
  )
/*++

Routine Description:

    Prints a formatted unicode string to a buffer

Arguments:

    Str         - Output buffer to print the formatted string into
    StrSize     - Size of Str.  String is truncated to this size.
                  A size of 0 means there is no limit
    fmt         - The format string

Returns:

    String length returned in buffer

--*/
{
  POOL_PRINT  spc;

  spc.str     = Str;
  spc.maxlen  = StrSize / sizeof (CHAR16) - 1;
  spc.len     = 0;

  _PoolCatPrint (fmt, vargs, &spc, _SPrint);
  return spc.len;
}

UINTN
Print (
  IN CHAR16     *fmt,
  ...
  )
/*++

Routine Description:

  Prints a formatted unicode string to the default console

Arguments:

  fmt         - Format string

Returns:

  Length of string printed to the console

--*/
{
  VA_LIST args;

  VA_START (args, fmt);
  return _IPrint ((UINTN) -1, (UINTN) -1, ST->ConOut, fmt, NULL, args);
}

CHAR16 *
PoolPrint (
  IN CHAR16             *fmt,
  ...
  )
/*++

Routine Description:

    Prints a formatted unicode string to allocated pool.  The caller
    must free the resulting buffer.

Arguments:

    fmt         - The format string

Returns:

    Allocated buffer with the formatted string printed in it.  
    The caller must free the allocated buffer.   The buffer
    allocation is not packed.

--*/
{
  POOL_PRINT  spc;
  VA_LIST     args;

  ZeroMem (&spc, sizeof (spc));
  VA_START (args, fmt);
  _PoolCatPrint (fmt, args, &spc, _PoolPrint);
  return spc.str;
}

UINTN
PrintToken (
  IN UINT16           Token,
  IN EFI_HII_HANDLE   Handle,
  ...
  )
/*++

Routine Description:

  Prints a formatted unicode string to the default console

Arguments:

  fmt         - Format string
  Token       - The tokens
  Handle      - Handle
Returns:

  Length of string printed to the console

--*/
{
  VA_LIST           args;
  CHAR16            *StringPtr;
  UINTN             StringSize;
  UINTN             Value;
  EFI_STATUS        Status;
#if (EFI_SPECIFICATION_VERSION < 0x0002000A)
  EFI_HII_PROTOCOL  *Hii = NULL;
#endif
  
  StringPtr   = NULL;
  StringSize  = 0x1000;

#if (EFI_SPECIFICATION_VERSION < 0x0002000A)
  //
  // There should only be one HII protocol
  //
  Status = LibLocateProtocol (
            &gEfiHiiProtocolGuid,
            (VOID **) &Hii
            );
  if (EFI_ERROR (Status)) {
    return 0;
  }
#endif
  //
  // Allocate BufferSize amount of memory
  //
  StringPtr = AllocatePool (StringSize);

  if (StringPtr == NULL) {
    return 0;
  }
  //
  // Retrieve string from HII
  //
#if (EFI_SPECIFICATION_VERSION < 0x0002000A)
  Status = Hii->GetString (Hii, Handle, Token, FALSE, NULL, &StringSize, StringPtr);
#else
  Status = LibGetString (Handle, Token, StringPtr, &StringSize);
#endif

  if (EFI_ERROR (Status)) {
    if (Status == EFI_BUFFER_TOO_SMALL) {
      FreePool (StringPtr);
      StringPtr = AllocatePool (StringSize);

      //
      // Retrieve string from HII
      //
#if (EFI_SPECIFICATION_VERSION < 0x0002000A)
      Status = Hii->GetString (Hii, Handle, Token, FALSE, NULL, &StringSize, StringPtr);
#else
      Status = LibGetString (Handle, Token, StringPtr, &StringSize);
#endif

      if (EFI_ERROR (Status)) {
        return 0;
      }
    } else {
      return 0;
    }
  }

  VA_START (args, Handle);
  Value = _IPrint ((UINTN) -1, (UINTN) -1, ST->ConOut, StringPtr, NULL, args);
  FreePool (StringPtr);
  return Value;
}

UINTN
PrintAt (
  IN UINTN      Column,
  IN UINTN      Row,
  IN CHAR16     *fmt,
  ...
  )
/*++

Routine Description:

  Prints a formatted unicode string to the default console, at 
  the supplied cursor position

Arguments:

  fmt         - Format string
  Column, Row - The cursor position to print the string at

Returns:

  Length of string printed to the console

--*/
{
  VA_LIST args;

  VA_START (args, fmt);
  return _IPrint (Column, Row, ST->ConOut, fmt, NULL, args);
}

CHAR16 *
CatPrint (
  IN OUT POOL_PRINT     *Str,
  IN CHAR16             *fmt,
  ...
  )
/*++

Routine Description:

  Concatenates a formatted unicode string to allocated pool.  
  The caller must free the resulting buffer.

Arguments:

  Str         - Tracks the allocated pool, size in use, and 
                amount of pool allocated.

  fmt         - The format string

Returns:

  Allocated buffer with the formatted string printed in it.  
  The caller must free the allocated buffer.   The buffer
  allocation is not packed.

--*/
{
  VA_LIST args;

  VA_START (args, fmt);
  _PoolCatPrint (fmt, args, Str, _PoolPrint);
  return Str->str;
}

UINTN
_IPrint (
  IN UINTN                            Column,
  IN UINTN                            Row,
  IN EFI_SIMPLE_TEXT_OUT_PROTOCOL     *Out,
  IN CHAR16                           *fmt,
  IN CHAR8                            *fmta,
  IN VA_LIST                          args
  )
/*++
Routine Description:

  Display string worker for: Print, PrintAt, IPrint, IPrintAt

Arguments:

    Column - Column
    Row    - Row 
    Out    - Out 
    fmt    - fmt 
    fmta   - fmta
    args   - args

Returns:


--*/
{
  PRINT_STATE ps;
  UINTN       back;

  ASSERT (NULL != Out);

  SetMem (&ps, sizeof (ps), 0);
  ps.Context  = Out;
  ps.Output   = (INTN (*) (VOID *, CHAR16 *)) Out->OutputString;
  ps.SetAttr  = (INTN (*) (VOID *, UINTN)) Out->SetAttribute;
  ASSERT (NULL != Out->Mode);
  ps.Attr           = Out->Mode->Attribute;

  back              = (ps.Attr >> 4) & 0xF;
  ps.AttrNorm       = EFI_TEXT_ATTR (((~back) & 0x07), back);
  ps.AttrHighlight  = EFI_TEXT_ATTR (EFI_WHITE, back);
  ps.AttrError      = EFI_TEXT_ATTR (EFI_YELLOW, back);
  ps.AttrBlueColor  = EFI_TEXT_ATTR (EFI_LIGHTBLUE, back);
  ps.AttrGreenColor = EFI_TEXT_ATTR (EFI_LIGHTGREEN, back);

  if (fmt) {
    ps.fmt.u.pw = fmt;
  } else {
    ps.fmt.Ascii  = TRUE;
    ps.fmt.u.pc   = fmta;
  }

  ps.args = args;

  if (Column != (UINTN) -1) {
    Out->SetCursorPosition (Out, Column, Row);
  }

  return _Print (&ps);
}

UINTN
IPrint (
  IN EFI_SIMPLE_TEXT_OUT_PROTOCOL     *Out,
  IN CHAR16                           *fmt,
  ...
  )
/*++

Routine Description:

    Prints a formatted unicode string to the specified console

Arguments:

    Out         - The console to print the string too

    fmt         - Format string

Returns:

    Length of string printed to the console

--*/
{
  VA_LIST args;

  VA_START (args, fmt);
  return _IPrint ((UINTN) -1, (UINTN) -1, Out, fmt, NULL, args);
}

UINTN
IPrintAt (
  IN EFI_SIMPLE_TEXT_OUT_PROTOCOL       *Out,
  IN UINTN                              Column,
  IN UINTN                              Row,
  IN CHAR16                             *fmt,
  ...
  )
/*++

Routine Description:

    Prints a formatted unicode string to the specified console, at
    the supplied cursor position

Arguments:

    Out         - The console to print the string too

    Column, Row - The cursor position to print the string at

    fmt         - Format string

Returns:

    Length of string printed to the console

--*/
{
  VA_LIST args;

  VA_START (args, fmt);
  return _IPrint (Column, Row, ST->ConOut, fmt, NULL, args);
}

VOID
PFLUSH (
  IN OUT PRINT_STATE     *ps
  )
{
  EFI_INPUT_KEY Key;
  EFI_STATUS    Status;

  *ps->Pos = 0;
  if (((UINTN) ps->Context == (UINTN) ST->ConOut) && mPrintMode.PageBreak) {

    IFlushWithPageBreak (ps);

  } else {

    if (mPrintMode.OutputPause) {

      Status = EFI_NOT_READY;
      while (EFI_ERROR (Status)) {
        Status = ST->ConIn->ReadKeyStroke (ST->ConIn, &Key);
      }

      SetOutputPause (FALSE);
    }

    ps->Output (ps->Context, ps->Buffer);
  }

  CopyMem (
    ((CHAR8 *) (ps->Buffer)) - PRINT_JOINT_BUFFER_LEN,
    ((CHAR8 *) (ps->Pos)) - PRINT_JOINT_BUFFER_LEN,
    PRINT_JOINT_BUFFER_LEN
    );
  ps->Pos = ps->Buffer;
}

UINTN
APrint (
  IN CHAR8      *fmt,
  ...
  )
/*++

Routine Description:

    For those whom really can't deal with unicode, a print
    function that takes an ascii format string

Arguments:

    fmt         - ascii format string

Returns:

    Length of string printed to the console

--*/
{
  VA_LIST args;

  VA_START (args, fmt);
  return _IPrint ((UINTN) -1, (UINTN) -1, ST->ConOut, NULL, fmt, args);
}

void
PSETATTR (
  IN OUT PRINT_STATE    *ps,
  IN UINTN              Attr
  )
{
  ASSERT (ps != NULL);
  PFLUSH (ps);

  ps->RestoreAttr = ps->Attr;
  if (ps->SetAttr) {
    ps->SetAttr (ps->Context, Attr);
  }

  ps->Attr = Attr;
}

void
PPUTC (
  IN OUT PRINT_STATE     *ps,
  IN CHAR16              c
  )
{
  ASSERT (ps != NULL);
  //
  // If Omit print to ConOut, then return.
  //
  if (mPrintMode.OmitPrint && ((UINTN) ps->Context == (UINTN) ST->ConOut)) {
    return ;
  }
  //
  // if this is a newline and carriage return does not exist,
  // add a carriage return
  //
  if (c == '\n' && ps->Pos >= ps->Buffer && (CHAR16) *(ps->Pos - 1) != '\r') {
    PPUTC (ps, '\r');
  }

  *ps->Pos = c;
  ps->Pos += 1;
  ps->Len += 1;

  //
  // if at the end of the buffer, flush it
  //
  if (ps->Pos >= ps->End) {
    PFLUSH (ps);
  }
}

CHAR16
PGETC (
  IN POINTER      *p
  )
{
  CHAR16  c;

  ASSERT (p != NULL);

  c = (CHAR16) (p->Ascii ? p->u.pc[p->Index] : p->u.pw[p->Index]);
  p->Index += 1;

  return c;
}

void
PITEM (
  IN OUT PRINT_STATE  *ps
  )
{
  UINTN       Len;

  UINTN       i;
  PRINT_ITEM  *Item;
  CHAR16      c;

  ASSERT (ps != NULL);

  //
  // Get the length of the item
  //
  Item              = ps->Item;
  Item->Item.Index  = 0;
  while (Item->Item.Index < Item->FieldWidth) {
    c = PGETC (&Item->Item);
    if (!c) {
      Item->Item.Index -= 1;
      break;
    }
  }

  Len = Item->Item.Index;

  //
  // if there is no item field width, use the items width
  //
  if (Item->FieldWidth == (UINTN) -1) {
    Item->FieldWidth = Len;
  }
  //
  // if item is larger then width, update width
  //
  if (Len > Item->Width) {
    Item->Width = Len;
  }
  //
  // if pad field before, add pad char
  //
  if (Item->PadBefore) {
    for (i = Item->Width; i < Item->FieldWidth; i += 1) {
      PPUTC (ps, ' ');
    }
  }
  //
  // pad item
  //
  for (i = Len; i < Item->Width; i++) {
    PPUTC (ps, Item->Pad);
  }
  //
  // add the item
  //
  Item->Item.Index = 0;
  while (Item->Item.Index < Len) {
    PPUTC (ps, PGETC (&Item->Item));
  }
  //
  // If pad at the end, add pad char
  //
  if (!Item->PadBefore) {
    for (i = Item->Width; i < Item->FieldWidth; i += 1) {
      PPUTC (ps, ' ');
    }
  }
}

UINTN
_Print (
  IN PRINT_STATE     *ps
  )
/*++

Routine Description:

  %w.lF   -   w = width
                l = field width
                F = format of arg

  Args F:
    0       -   pad with zeros
    -       -   justify on left (default is on right)
    ,       -   add comma's to field    
    *       -   width provided on stack
    n       -   Set output attribute to normal (for this field only)
    h       -   Set output attribute to highlight (for this field only)
    e       -   Set output attribute to error (for this field only)
    b       -   Set output attribute to blue color (for this field only)
    v       -   Set output attribute to green color (for this field only)
    l       -   Value is 64 bits

    a       -   ascii string
    s       -   unicode string
    X       -   fixed 8 byte value in hex
    x       -   hex value
    d       -   value as decimal    
    c       -   Unicode char
    t       -   EFI time structure
    g       -   Pointer to GUID
    r       -   EFI status code (result code)

    N       -   Set output attribute to normal
    H       -   Set output attribute to highlight
    E       -   Set output attribute to error
    B       -   Set output attribute to blue color
    V       -   Set output attribute to green color
    %       -   Print a %
    
Arguments:

    ps - Ps

Returns:

  Number of charactors written   

--*/
{
  CHAR16      c;
  UINTN       Attr;
  PRINT_ITEM  Item;
  CHAR16      *Buffer;
  EFI_GUID    *TmpGUID;

  ASSERT (ps != NULL);
  //
  // If Omit print to ConOut, then return 0.
  //
  if (mPrintMode.OmitPrint && ((UINTN) ps->Context == (UINTN) ST->ConOut)) {
    return 0;
  }

  Item.Scratch = AllocateZeroPool (sizeof (CHAR16) * PRINT_ITEM_BUFFER_LEN);
  if (NULL == Item.Scratch) {
    return EFI_OUT_OF_RESOURCES;
  }

  Buffer = AllocateZeroPool (sizeof (CHAR16) * PRINT_STRING_LEN);
  if (NULL == Buffer) {
    FreePool (Item.Scratch);
    return EFI_OUT_OF_RESOURCES;
  }

  ps->Len       = 0;
  ps->Buffer    = (CHAR16 *) ((CHAR8 *) Buffer + PRINT_JOINT_BUFFER_LEN);
  ps->Pos       = ps->Buffer;
  ps->End       = Buffer + PRINT_STRING_LEN - 1;
  ps->Item      = &Item;

  ps->fmt.Index = 0;
  c             = PGETC (&ps->fmt);
  while (c) {

    if (c != '%') {
      PPUTC (ps, c);
      c = PGETC (&ps->fmt);
      continue;
    }
    //
    // setup for new item
    //
    Item.FieldWidth = (UINTN) -1;
    Item.Width      = 0;
    Item.WidthParse = &Item.Width;
    Item.Pad        = ' ';
    Item.PadBefore  = TRUE;
    Item.Comma      = FALSE;
    Item.Long       = FALSE;
    Item.Item.Ascii = FALSE;
    Item.Item.u.pw  = NULL;
    ps->RestoreAttr = 0;
    Attr            = 0;

    c               = PGETC (&ps->fmt);
    while (c) {

      switch (c) {

      case '%':
        //
        // %% -> %
        //
        Item.Item.u.pw    = Item.Scratch;
        Item.Item.u.pw[0] = '%';
        Item.Item.u.pw[1] = 0;
        break;

      case '0':
        Item.Pad = '0';
        break;

      case '-':
        Item.PadBefore = FALSE;
        break;

      case ',':
        Item.Comma = TRUE;
        break;

      case '.':
        Item.WidthParse = &Item.FieldWidth;
        break;

      case '*':
        *Item.WidthParse = VA_ARG (ps->args, UINTN);
        break;

      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        *Item.WidthParse = 0;
        do {
          *Item.WidthParse  = *Item.WidthParse * 10 + c - '0';
          c                 = PGETC (&ps->fmt);
        } while (c >= '0' && c <= '9');
        ps->fmt.Index -= 1;
        break;

      case 'a':
        Item.Item.u.pc  = VA_ARG (ps->args, CHAR8 *);
        Item.Item.Ascii = TRUE;
        if (!Item.Item.u.pc) {
          Item.Item.u.pc = "(null)";
        }

        Item.PadBefore = FALSE;
        break;

      case 's':
        Item.Item.u.pw = VA_ARG (ps->args, CHAR16 *);
        if (!Item.Item.u.pw) {
          Item.Item.u.pw = L"(null)";
        }

        Item.PadBefore = FALSE;
        break;

      case 'c':
        Item.Item.u.pw    = Item.Scratch;
        Item.Item.u.pw[0] = (CHAR16) VA_ARG (ps->args, UINTN);
        Item.Item.u.pw[1] = 0;
        break;

      case 'l':
        Item.Long = TRUE;
        break;

      case 'X':
        Item.Width  = Item.Long ? 16 : 8;
        Item.Pad    = '0';

      case 'x':
        Item.Item.u.pw = Item.Scratch;
        ValueToHex (
          Item.Item.u.pw,
          Item.Long ? VA_ARG (ps->args, UINT64) : VA_ARG (ps->args, UINTN)
          );

        break;

      case 'g':
        TmpGUID = VA_ARG (ps->args, EFI_GUID *);
        if (TmpGUID != NULL) {
          Item.Item.u.pw = Item.Scratch;
          GuidToString (Item.Item.u.pw, TmpGUID);
        }
        break;

      case 'd':
        Item.Item.u.pw = Item.Scratch;
        ValueToString (
          Item.Item.u.pw,
          Item.Comma,
          Item.Long ? VA_ARG (ps->args, UINT64) : VA_ARG (ps->args, INTN)
          );
        break;

      case 't':
        Item.Item.u.pw = Item.Scratch;
        TimeToString (Item.Item.u.pw, VA_ARG (ps->args, EFI_TIME *));
        break;

      case 'r':
        Item.Item.u.pw = Item.Scratch;
        StatusToString (Item.Item.u.pw, VA_ARG (ps->args, EFI_STATUS));
        break;

      case 'n':
        PSETATTR (ps, ps->AttrNorm);
        break;

      case 'h':
        PSETATTR (ps, ps->AttrHighlight);
        break;

      case 'b':
        PSETATTR (ps, ps->AttrBlueColor);
        break;

      case 'v':
        PSETATTR (ps, ps->AttrGreenColor);
        break;

      case 'e':
        PSETATTR (ps, ps->AttrError);
        break;

      case 'N':
        Attr = ps->AttrNorm;
        break;

      case 'H':
        Attr = ps->AttrHighlight;
        break;

      case 'E':
        Attr = ps->AttrError;
        break;

      case 'B':
        Attr = ps->AttrBlueColor;
        break;

      case 'V':
        Attr = ps->AttrGreenColor;
        break;

      default:
        Item.Item.u.pw    = Item.Scratch;
        Item.Item.u.pw[0] = '?';
        Item.Item.u.pw[1] = 0;
        break;
      }
      //
      // if we have an Item
      //
      if (Item.Item.u.pw) {
        PITEM (ps);
        break;
      }
      //
      // if we have an Attr set
      //
      if (Attr) {
        PSETATTR (ps, Attr);
        ps->RestoreAttr = 0;
        break;
      }

      c = PGETC (&ps->fmt);
    }

    if (ps->RestoreAttr) {
      PSETATTR (ps, ps->RestoreAttr);
    }

    c = PGETC (&ps->fmt);
  }
  //
  // Flush buffer
  //
  PFLUSH (ps);

  FreePool (Item.Scratch);
  FreePool (Buffer);

  return ps->Len;
}

BOOLEAN
SetPageBreak (
  IN OUT PRINT_STATE     *ps
  )
{
  EFI_INPUT_KEY Key;
  CHAR16        Str[3];

  ASSERT (ps != NULL);

  ps->Output (ps->Context, L"Press ENTER to continue, 'q' to exit:");

  //
  // Wait for user input
  //
  Str[0]  = ' ';
  Str[1]  = 0;
  Str[2]  = 0;
  for (;;) {
    WaitForSingleEvent (ST->ConIn->WaitForKey, 0);
    ST->ConIn->ReadKeyStroke (ST->ConIn, &Key);

    //
    // handle control keys
    //
    if (Key.UnicodeChar == CHAR_NULL) {
      if (Key.ScanCode == SCAN_ESC) {
        ps->Output (ps->Context, L"\r\n");
        mPrintMode.OmitPrint = TRUE;
        break;
      }

      continue;
    }

    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      ps->Output (ps->Context, L"\r\n");
      mPrintMode.Row = mPrintMode.InitRow;
      break;
    }
    //
    // Echo input
    //
    Str[1] = Key.UnicodeChar;
    if (Str[1] == CHAR_BACKSPACE) {
      continue;
    }

    ps->Output (ps->Context, Str);

    if ((Str[1] == L'q') || (Str[1] == L'Q')) {
      mPrintMode.OmitPrint = TRUE;
    } else {
      mPrintMode.OmitPrint = FALSE;
    }

    Str[0] = CHAR_BACKSPACE;
  }

  return mPrintMode.OmitPrint;
}

void
IFlushWithPageBreak (
  IN OUT PRINT_STATE     *ps
  )
{
  CHAR16  *Pos;
  CHAR16  *LineStart;
  CHAR16  LineEndChar;

  ASSERT (ps != NULL);

  Pos       = ps->Buffer;
  LineStart = Pos;
  while ((*Pos != 0) && (Pos < ps->Pos)) {
    if ((*Pos == L'\n') && (*(Pos - 1) == L'\r')) {
      //
      // Output one line
      //
      LineEndChar = *(Pos + 1);
      *(Pos + 1)  = 0;
      ps->Output (ps->Context, LineStart);
      *(Pos + 1) = LineEndChar;
      //
      // restore line end char
      //
      LineStart         = Pos + 1;
      mPrintMode.Column = 0;
      mPrintMode.Row++;
      if (mPrintMode.Row == mPrintMode.MaxRow) {
        if (SetPageBreak (ps)) {
          return ;
        }
      }
    } else {
      if (*Pos == CHAR_BACKSPACE) {
        mPrintMode.Column--;
      } else {
        mPrintMode.Column++;
      }
      //
      // If column is at the end of line, output a new line feed.
      //
      if ((mPrintMode.Column == mPrintMode.MaxColumn) && (*Pos != L'\n') && (*Pos != L'\r')) {

        LineEndChar = *(Pos + 1);
        *(Pos + 1)  = 0;
        ps->Output (ps->Context, LineStart);
        *(Pos + 1) = LineEndChar;
        //
        // restore line end char
        //
        if (mPrintMode.AutoWrap) {
          ps->Output (ps->Context, L"\r\n");
        }

        LineStart         = Pos + 1;
        mPrintMode.Column = 0;
        mPrintMode.Row++;
        if (mPrintMode.Row == mPrintMode.MaxRow) {
          if (SetPageBreak (ps)) {
            return ;
          }
        }
      }
    }

    Pos++;
  }

  if (*LineStart != 0) {
    ps->Output (ps->Context, LineStart);
  }
}

VOID
SetOutputPause (
  IN BOOLEAN    Pause
  )
{
  EFI_TPL Tpl;

  Tpl                     = BS->RaiseTPL (EFI_TPL_NOTIFY);
  mPrintMode.OutputPause  = Pause;
  BS->RestoreTPL (Tpl);
}

INTN
_PoolPrint (
  IN VOID     *Context,
  IN CHAR16   *Buffer
  )
/*++

Routine Description:

    Append string worker for PoolPrint and CatPrint
    
Arguments:
    Context - Context
    Buffer  - Buffer

Returns:

--*/
{
  UINTN       newlen;
  POOL_PRINT  *spc;

  ASSERT (Context != NULL);
  
  spc     = Context;
  newlen  = spc->len + StrLen (Buffer) + 1;

  //
  // Is the string is over the max, grow the buffer
  //
  if (newlen > spc->maxlen) {
    //
    // Grow the pool buffer
    //
    newlen += PRINT_STRING_LEN;
    spc->maxlen = newlen;
    spc->str = ReallocatePool (
                spc->str,
                spc->len * sizeof (CHAR16),
                spc->maxlen * sizeof (CHAR16)
                );

    if (!spc->str) {
      spc->len    = 0;
      spc->maxlen = 0;
    }
  }
  //
  // Append the new text
  //
  return _SPrint (Context, Buffer);
}

VOID
Output (
  IN CHAR16   *Str
  )
/*++

Routine Description:

    Write a string to the console at the current cursor location

Arguments:

    Str - string

Returns:


--*/
{
  ST->ConOut->OutputString (ST->ConOut, Str);
}

VOID
Input (
  IN CHAR16    *Prompt OPTIONAL,
  OUT CHAR16   *InStr,
  IN UINTN     StrLen
  )
/*++

Routine Description:

  Input a string at the current cursor location, for StrLen
  
Arguments:

    Prompt - Prompt
    InStr  - The input string
    StrLen - The string buffer length

Returns:


--*/
{
  IInput (
    ST->ConOut,
    ST->ConIn,
    Prompt,
    InStr,
    StrLen
    );
}

VOID
ConMoveCursorBackward (
  IN     UINTN                   LineLength,
  IN OUT UINTN                   *Column,
  IN OUT UINTN                   *Row
  )
/*++

Routine Description:
  Move the cursor position one character backward.

Arguments:
  LineLength       Length of a line. Get it by calling QueryMode
  Column           Current column of the cursor position
  Row              Current row of the cursor position

Returns:

--*/
{
  ASSERT (Column != NULL);
  ASSERT (Row != NULL);
  //
  // If current column is 0, move to the last column of the previous line,
  // otherwise, just decrement column.
  //
  if (*Column == 0) {
    (*Column) = LineLength - 1;
    //
    //   if (*Row > 0) {
    //
    (*Row)--;
    //
    // }
    //
  } else {
    (*Column)--;
  }
}

VOID
ConMoveCursorForward (
  IN     UINTN                   LineLength,
  IN     UINTN                   TotalRow,
  IN OUT UINTN                   *Column,
  IN OUT UINTN                   *Row
  )
/*++

Routine Description:
  Move the cursor position one character backward.

Arguments:
  LineLength       Length of a line. Get it by calling QueryMode
  TotalRow         Total row of a screen, get by calling QueryMode
  Column           Current column of the cursor position
  Row              Current row of the cursor position

Returns:

--*/
{
  ASSERT (Column != NULL);
  ASSERT (Row != NULL);
  //
  // If current column is at line end, move to the first column of the nest
  // line, otherwise, just increment column.
  //
  (*Column)++;
  if (*Column >= LineLength) {
    (*Column) = 0;
    if ((*Row) < TotalRow - 1) {
      (*Row)++;
    }
  }
}

VOID
IInput (
  IN EFI_SIMPLE_TEXT_OUT_PROTOCOL     * ConOut,
  IN EFI_SIMPLE_TEXT_IN_PROTOCOL      * ConIn,
  IN CHAR16                           *Prompt OPTIONAL,
  OUT CHAR16                          *InStr,
  IN UINTN                            StrLength
  )
/*++

Routine Description:
  Input a string at the current cursor location, for StrLength

Arguments:
  ConOut           Console output protocol
  ConIn            Console input protocol
  Prompt           Prompt string
  InStr            Buffer to hold the input string
  StrLength        Length of the buffer

Returns:

--*/
{
  BOOLEAN       Done;
  UINTN         Column;
  UINTN         Row;
  UINTN         StartColumn;
  UINTN         Update;
  UINTN         Delete;
  UINTN         Len;
  UINTN         StrPos;
  UINTN         Index;
  UINTN         LineLength;
  UINTN         TotalRow;
  UINTN         SkipLength;
  UINTN         OutputLength;
  UINTN         TailRow;
  UINTN         TailColumn;
  EFI_INPUT_KEY Key;
  BOOLEAN       InsertMode;
  
  ASSERT (ConOut != NULL);
  ASSERT (ConIn != NULL);
  ASSERT (InStr != NULL);

  if (Prompt) {
    ConOut->OutputString (ConOut, Prompt);
  }
  //
  // Read a line from the console
  //
  Len           = 0;
  StrPos        = 0;
  OutputLength  = 0;
  Update        = 0;
  Delete        = 0;
  InsertMode    = TRUE;

  //
  // If buffer is not large enough to hold a CHAR16, do nothing.
  //
  if (StrLength < 1) {
    return ;
  }
  //
  // Get the screen setting and the current cursor location
  //
  StartColumn = ConOut->Mode->CursorColumn;
  Column      = StartColumn;
  Row         = ConOut->Mode->CursorRow;
  ConOut->QueryMode (ConOut, ConOut->Mode->Mode, &LineLength, &TotalRow);
  if (LineLength == 0) {
    return ;
  }

  SetMem (InStr, StrLength * sizeof (CHAR16), 0);
  Done = FALSE;
  do {
    //
    // Read a key
    //
    WaitForSingleEvent (ConIn->WaitForKey, 0);
    ConIn->ReadKeyStroke (ConIn, &Key);

    switch (Key.UnicodeChar) {
    case CHAR_CARRIAGE_RETURN:
      //
      // All done, print a newline at the end of the string
      //
      TailRow     = Row + (Len - StrPos + Column) / LineLength;
      TailColumn  = (Len - StrPos + Column) % LineLength;
      Done        = TRUE;
      break;

    case CHAR_BACKSPACE:
      if (StrPos) {
        //
        // If not move back beyond string beginning, move all characters behind
        // the current position one character forward
        //
        StrPos -= 1;
        Update  = StrPos;
        Delete  = 1;
        CopyMem (InStr + StrPos, InStr + StrPos + 1, sizeof (CHAR16) * (Len - StrPos));

        //
        // Adjust the current column and row
        //
        ConMoveCursorBackward (LineLength, &Column, &Row);
      }
      break;

    default:
      if (Key.UnicodeChar >= ' ') {
        //
        // If we are at the buffer's end, drop the key
        //
        if (Len == StrLength - 1 && (InsertMode || StrPos == Len)) {
          break;
        }
        //
        // If in insert mode, move all characters behind the current position
        // one character backward to make space for this character. Then store
        // the character.
        //
        if (InsertMode) {
          for (Index = Len; Index > StrPos; Index -= 1) {
            InStr[Index] = InStr[Index - 1];
          }
        }

        InStr[StrPos] = Key.UnicodeChar;
        Update        = StrPos;

        StrPos += 1;
        OutputLength = 1;
      }
      break;

    case 0:
      switch (Key.ScanCode) {
      case SCAN_DELETE:
        //
        // Move characters behind current position one character forward
        //
        if (Len) {
          Update  = StrPos;
          Delete  = 1;
          CopyMem (InStr + StrPos, InStr + StrPos + 1, sizeof (CHAR16) * (Len - StrPos));
        }
        break;

      case SCAN_LEFT:
        //
        // Adjust current cursor position
        //
        if (StrPos) {
          StrPos -= 1;
          ConMoveCursorBackward (LineLength, &Column, &Row);
        }
        break;

      case SCAN_RIGHT:
        //
        // Adjust current cursor position
        //
        if (StrPos < Len) {
          StrPos += 1;
          ConMoveCursorForward (LineLength, TotalRow, &Column, &Row);
        }
        break;

      case SCAN_HOME:
        //
        // Move current cursor position to the beginning of the command line
        //
        Row -= (StrPos + StartColumn) / LineLength;
        Column  = StartColumn;
        StrPos  = 0;
        break;

      case SCAN_END:
        //
        // Move current cursor position to the end of the command line
        //
        TailRow     = Row + (Len - StrPos + Column) / LineLength;
        TailColumn  = (Len - StrPos + Column) % LineLength;
        Row         = TailRow;
        Column      = TailColumn;
        StrPos      = Len;
        break;

      case SCAN_ESC:
        //
        // Prepare to clear the current command line
        //
        InStr[0]  = 0;
        Update    = 0;
        Delete    = Len;
        Row -= (StrPos + StartColumn) / LineLength;
        Column        = StartColumn;
        OutputLength  = 0;
        break;

      case SCAN_INSERT:
        //
        // Toggle the SEnvInsertMode flag
        //
        InsertMode = (BOOLEAN)!InsertMode;
        break;
      }
    }

    if (Done) {
      break;
    }
    //
    // If we need to update the output do so now
    //
    if (Update != -1) {
      PrintAt (Column, Row, L"%s%.*s", InStr + Update, Delete, L"");
      Len = StrLen (InStr);

      if (Delete) {
        SetMem (InStr + Len, Delete * sizeof (CHAR16), 0x00);
      }

      if (StrPos > Len) {
        StrPos = Len;
      }

      Update = (UINTN) -1;

      //
      // After using print to reflect newly updates, if we're not using
      // BACKSPACE and DELETE, we need to move the cursor position forward,
      // so adjust row and column here.
      //
      if (Key.UnicodeChar != CHAR_BACKSPACE && !(Key.UnicodeChar == 0 && Key.ScanCode == SCAN_DELETE)) {
        //
        // Calulate row and column of the tail of current string
        //
        TailRow     = Row + (Len - StrPos + Column + OutputLength) / LineLength;
        TailColumn  = (Len - StrPos + Column + OutputLength) % LineLength;

        //
        // If the tail of string reaches screen end, screen rolls up, so if
        // Row does not equal TailRow, Row should be decremented
        //
        // (if we are recalling commands using UPPER and DOWN key, and if the
        // old command is too long to fit the screen, TailColumn must be 79.
        //
        if (TailColumn == 0 && TailRow >= TotalRow && (UINTN) Row != TailRow) {
          Row--;
        }
        //
        // Calculate the cursor position after current operation. If cursor
        // reaches line end, update both row and column, otherwise, only
        // column will be changed.
        //
        if (Column + OutputLength >= LineLength) {
          SkipLength = OutputLength - (LineLength - Column);

          Row += SkipLength / LineLength + 1;
          if ((UINTN) Row > TotalRow - 1) {
            Row = TotalRow - 1;
          }

          Column = SkipLength % LineLength;
        } else {
          Column += OutputLength;
        }
      }

      Delete = 0;
    }
    //
    // Set the cursor position for this key
    //
    SetCursorPosition (ConOut, Column, Row, LineLength, TotalRow, InStr, StrPos, Len);
  } while (!Done);

  //
  // Return the data to the caller
  //
  return ;
}

VOID
SetCursorPosition (
  IN EFI_SIMPLE_TEXT_OUT_PROTOCOL     *ConOut,
  IN  UINTN                           Column,
  IN  INTN                            Row,
  IN  UINTN                           LineLength,
  IN  UINTN                           TotalRow,
  IN  CHAR16                          *Str,
  IN  UINTN                           StrPos,
  IN  UINTN                           Len
  )
{
  CHAR16  Backup;

  ASSERT (ConOut != NULL);
  ASSERT (Str != NULL);

  Backup = 0;
  if (Row >= 0) {
    ConOut->SetCursorPosition (ConOut, Column, Row);
    return ;
  }

  if (Len - StrPos > Column * Row) {
    Backup                          = *(Str + StrPos + Column * Row);
    *(Str + StrPos + Column * Row)  = 0;
  }

  PrintAt (0, 0, L"%s", Str + StrPos);
  if (Len - StrPos > Column * Row) {
    *(Str + StrPos + Column * Row) = Backup;
  }

  ConOut->SetCursorPosition (ConOut, 0, 0);
}
//
// //
//
BOOLEAN
LibGetPrintOmit (
  VOID
  )
{
  return mPrintMode.OmitPrint;
}

VOID
LibSetPrintOmit (
  IN BOOLEAN    OmitPrint
  )
{
  EFI_TPL Tpl;

  Tpl                   = BS->RaiseTPL (EFI_TPL_NOTIFY);
  mPrintMode.OmitPrint  = OmitPrint;
  BS->RestoreTPL (Tpl);
}

VOID
LibEnablePageBreak (
  IN INT32      StartRow,
  IN BOOLEAN    AutoWrap
  )
{
  mPrintMode.PageBreak  = TRUE;
  mPrintMode.OmitPrint  = FALSE;
  mPrintMode.InitRow    = StartRow;
  mPrintMode.AutoWrap   = AutoWrap;

  //
  // Query Mode
  //
  ST->ConOut->QueryMode (
                ST->ConOut,
                ST->ConOut->Mode->Mode,
                &mPrintMode.MaxColumn,
                &mPrintMode.MaxRow
                );

  mPrintMode.Row = StartRow;
}

BOOLEAN
LibGetPageBreak (
  VOID
  )
{
  return mPrintMode.PageBreak;
}

#if 0
STATIC
BOOLEAN
GetOutputPause (
  VOID
  )
{
  return mPrintMode.OutputPause;
}
#endif

INTN
DbgPrint (
  IN INTN       mask,
  IN CHAR8      *fmt,
  ...
  )
/*++

Routine Description:

    Prints a formatted unicode string to the default StandardError console

Arguments:

    mask        - Bit mask of debug string.  If a bit is set in the
                  mask that is also set in EFIDebug the string is 
                  printed; otherwise, the string is not printed

    fmt         - Format string

Returns:

    Length of string printed to the StandardError console

--*/
{
  EFI_SIMPLE_TEXT_OUT_PROTOCOL  *DbgOut;
  PRINT_STATE                   ps;
  VA_LIST                       args;
  UINTN                         back;
  UINTN                         attr;
  UINTN                         SavedAttribute;

  UPDATE_DEBUG_MASK ();

  if (!(EFIDebug & mask)) {
    return 0;
  }

  VA_START (args, fmt);
  ZeroMem (&ps, sizeof (ps));

  ps.Output     = _DbgOut;
  ps.fmt.Ascii  = TRUE;
  ps.fmt.u.pc   = fmt;
  ps.args       = args;
  ps.Attr       = EFI_TEXT_ATTR (EFI_LIGHTGRAY, EFI_RED);

  DbgOut        = LibRuntimeDebugOut;

  if (!DbgOut) {
    DbgOut = ST->StdErr;
    if (!DbgOut) {
      DbgOut = ST->ConOut;
    }
  }

  if (DbgOut) {
    ps.Attr     = DbgOut->Mode->Attribute;
    ps.Context  = DbgOut;
    ps.SetAttr  = (INTN (*) (VOID *, UINTN)) DbgOut->SetAttribute;
  }

  SavedAttribute    = ps.Attr;

  back              = (ps.Attr >> 4) & 0xf;
  ps.AttrNorm       = EFI_TEXT_ATTR (EFI_LIGHTGRAY, back);
  ps.AttrHighlight  = EFI_TEXT_ATTR (EFI_WHITE, back);
  ps.AttrError      = EFI_TEXT_ATTR (EFI_YELLOW, back);
  ps.AttrBlueColor  = EFI_TEXT_ATTR (EFI_LIGHTBLUE, back);
  ps.AttrGreenColor = EFI_TEXT_ATTR (EFI_LIGHTGREEN, back);

  attr              = ps.AttrNorm;

  if (mask & EFI_D_WARN) {
    attr = ps.AttrHighlight;
  }

  if (mask & EFI_D_ERROR) {
    attr = ps.AttrError;
  }

  if (ps.SetAttr) {
    ps.Attr = attr;
    ps.SetAttr (ps.Context, attr);
  }

  _Print (&ps);

  //
  // Restore original attributes
  //
  if (ps.SetAttr) {
    ps.SetAttr (ps.Context, SavedAttribute);
  }

  return 0;
}

INTN
_DbgOut (
  IN VOID     *Context,
  IN CHAR16   *Buffer
  )
/*++

Routine Description:

    Append string worker for DbgPrint

Arguments:

    Context - Context
    Buffer  - Buffer

Returns:
 

--*/
{ 
  EFI_SIMPLE_TEXT_OUT_PROTOCOL  *DbgOut;

  DbgOut = Context;
  if (DbgOut) {
    DbgOut->OutputString (DbgOut, Buffer);
  }

  return 0;
}

