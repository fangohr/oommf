/* FILE: ocfpu.cc                    -*-Mode: c++-*-
 *
 *	Floating point unit control
 *
 * NB: This class supports x86/x86_64 hardware.
 *     It is currently inert for anything else.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 */

#include "messages.h"  // Oc_Snprintf
#include "ocfpu.h"

#if OC_USE_SSE
#include <xmmintrin.h>  // SSE (version "1")
#endif

/* End includes */     /* Optional directive to pimake */

/*
 * x87 (FPU on x86) control code.  This code is x86 and compiler
 * specific.  At present this code block is only active if the macro
 * OC_USE_X87 is defined, in which case the fpu control word reading and
 * setting functions are needed to propagate the the fpu control word
 * from parent to child threads.  This block could be enabled more
 * generally if other uses are found.
 *
 * x87 FPU control word reference:
 *  Name  Bits    Description
 *  IM     0      Invalid operation mask
 *  DM     1      Denormalized operand mask
 *  ZM     2      Zero divide mask
 *  OM     3      Overflow mask
 *  UM     4      Underflow mask
 *  PM     5      Precision mask  (handles precision loss in operations)
 *         6      Unused
 *  IEM    7      Interrupt enable mask
 *  PC     8-9    Precision control
 *  RC    10-11   Rounding control
 *  IC     12     Infinity control (ignored in 387 and later)
 *        13-15   Unused
 *
 *  Precision control:
 *    00 = 24 bits (Mantissa for 4-byte float) 
 *    01 = Invalid
 *    10 = 53 bits (Mantissa for 8-byte float)
 *    11 = 64 bits (Mantissa for 10-byte float)
 *  
 *  Rounding control:
 *    00 = Round to nearest or even (default)
 *    01 = Round to -infinity
 *    10 = Round to +infinity
 *    11 = Truncate (round to zero)
 *
 * Some compilers (notably Borland C++) by default enable floating
 * point exceptions.  If you don't want the code to abort on floating
 * point overflow, etc., then mask these exceptions with code like
 *
 *   OC_UINT2 mode = Getx87ControlWord();
 *   mode |= 0x3f;
 *   Setx87ControlWord(mode);
 *
 */
#if OC_USE_X87

# if defined(__GNUC__) || defined(__PGIC__)
// __GNUC__ is GNU C/C++, __PGIC__ is Portland group C/C++
// Intel C++ also flows here, via __GNUC__.
#  if !(defined(__i386__) || defined(__x86_64__))
#   error OC_USE_X87 only valid on x86 systems
#  endif
void Oc_FpuControlData::Setx87ControlWord (OC_UINT2 mode)
{
  __asm__("fldcw %0" : : "m"(mode));
}
OC_UINT2 Oc_FpuControlData::Getx87ControlWord()
{
  OC_UINT2 mode;
  __asm__("fstcw %0" : "=m"(mode));
  return mode;
}

# elif defined(_WIN32)
#  if !(defined(_M_IX86) || defined(_M_AMD64) || defined(_M_X64))
#   error OC_USE_X87 only valid on x86 systems
#  endif
// The following is the Visual C++ convention; many Windows compilers
// accept this.  Separate code blocks for any exceptions should be
// place above.
void Oc_FpuControlData::Setx87ControlWord (OC_UINT2 mode)
{
  _control87((unsigned int)mode,0xFFFF); // Declared in <float.h>
}
OC_UINT2 Oc_FpuControlData::Getx87ControlWord()
{
  return static_cast<OC_UINT2>(_control87(0,0));  // Declared in <float.h>
}

# else
// If all else fails, try the gcc assembler convention.  If your
// compiler fails here then write a separate block for that compiler.
// (Or, if you are really desperate, write dummy routines that do
// nothing; at present (Sep-2013) this code is only used to propagate
// the control word to child threads on Windows.)
void Oc_FpuControlData::Setx87ControlWord (OC_UINT2 mode)
{
  __asm__("fldcw %0" : : "m"(mode));
}
OC_UINT2 Oc_FpuControlData::Getx87ControlWord()
{
  OC_UINT2 mode;
  __asm__("fstcw %0" : "=m"(mode));
  return mode;
}
# endif // x87 compiler check
#endif // x87 control word


/* SSE Control word description:
 *
 * Mnemonic   Bit Location             Description
 *   FZ         bit 15                Flush To Zero
 *   R+         bit 14                Round Positive
 *   R-         bit 13                Round Negative
 *   RZ         bits 13 and 14        Round To Zero
 *   RN         bits 13 and 14 are 0  Round To Nearest
 *   PM         bit 12                Precision Mask
 *
 *   UM         bit 11                Underflow Mask
 *   OM         bit 10                Overflow Mask
 *   ZM         bit 9                 Divide By Zero Mask
 *   DM         bit 8                 Denormal Mask
 *
 *   IM         bit 7                 Invalid Operation Mask
 *   DAZ        bit 6                 Denormals Are Zero
 *   PE         bit 5                 Precision Flag
 *   UE         bit 4                 Underflow Flag
 *
 *   OE         bit 3                 Overflow Flag
 *   ZE         bit 2                 Divide By Zero Flag
 *   DE         bit 1                 Denormal Flag
 *   IE         bit 0                 Invalid Operation Flag
 *
 * NOTE: "It is written" that in the early iterations of SSE, bit 6 was
 *       reserved, and setting it would cause a general protection
 *       fault.  All the processors I have access to (Sep 2012) support
 *       bit 6 as DAZ and allow it to be set.  Moreover, I don't know
 *       what value for bit 6 is returned on the older variants.
 *       WARNING: The code below tacitly assumes no problems with bit 6.
 */

#if OC_USE_SSE
OC_UINT2 Oc_FpuControlData::GetSSEControlWord()
{
  return static_cast<OC_UINT2>(_mm_getcsr());
}

void Oc_FpuControlData::SetSSEControlWord (OC_UINT2 mode)
{
  _mm_setcsr(mode & 0xFFC0u);  // Mask out "volatile" bits
  // NOTE: Bit 6 (mask 0x0040) controls DAZ.
}
#endif // OC_USE_SSE

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

void Oc_FpuControlData::ReadData()
{
#if OC_USE_X87
  x87_data = Getx87ControlWord();
#endif
#if OC_USE_SSE
  sse_data = GetSSEControlWord();
#endif
}

void Oc_FpuControlData::WriteData()
{
#if OC_USE_X87
  Setx87ControlWord(x87_data);
#endif
#if OC_USE_SSE
  SetSSEControlWord(sse_data);
#endif
}

void Oc_FpuControlData::MaskExceptions()
{
#if OC_USE_X87
  OC_INT2 mode = Getx87ControlWord();
  mode |= 0x3f;
  Setx87ControlWord(mode);
#endif
}

// Enable flush subnormals to zero and subnormals are zero
void Oc_FpuControlData::SetFlushToZero()
{
  // No such control for x87
#if OC_USE_SSE
    OC_UINT2 csr = GetSSEControlWord();
    csr  |= 0x8040u;  // Flush to zero and denormals are zero
    SetSSEControlWord(csr);
#endif
}

// Disable flush subnormals to zero and subnormals are zero
void Oc_FpuControlData::SetNoFlushToZero()
{
  // No such control for x87
#if OC_USE_SSE
    OC_UINT2 csr = GetSSEControlWord();
    csr &= 0x7FBFu;  // No flush to zero and denormals aren't zero
    SetSSEControlWord(csr);
#endif
}

#if OC_USE_X87 ||  OC_USE_SSE
void Oc_FpuControlData::GetDataString(Oc_AutoBuf& buf)
{
# if OC_USE_X87
  Oc_AutoBuf bufx87(16);
  Oc_Snprintf(bufx87,bufx87.GetLength()+1,
              " x87=%04X",(unsigned int)x87_data);
  buf += bufx87;
# endif
# if OC_USE_SSE
  Oc_AutoBuf bufsse(16);
  Oc_Snprintf(bufsse,bufsse.GetLength()+1,
              " sse=%04X",(unsigned int)sse_data);
  buf += bufsse;
# endif
}
#else //  OC_USE_X87 ||  OC_USE_SSE
void Oc_FpuControlData::GetDataString(Oc_AutoBuf&)
{}
#endif // OC_USE_X87 ||  OC_USE_SSE




