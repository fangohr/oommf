/* FILE: ocfpu.h                    -*-Mode: c++-*-
 *
 *	Floating point unit control
 *
 * NB: This class supports x86/x86_64 hardware.
 *     It is currently inert for anything else.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 */

#ifndef _OCFPU
#define _OCFPU

#ifndef OC_USE_X87
# define OC_USE_X87 0
#endif
#ifndef OC_USE_SSE
# define OC_USE_SSE 0
#endif

#include "autobuf.h"

/* End includes */     /* Optional directive to pimake */

class Oc_FpuControlData {
 public:
  // Note: Each thread has a separate set of FPU control registers,
  //       so there shouldn't be any thread safety issues
  void ReadData();  // Fills instance data from thread FPU control registers
  void WriteData(); // Sets FPU control registers from instance data
  void GetDataString(Oc_AutoBuf& buf);

  static void MaskExceptions(); // Disables floating-point exceptions.
  /// This allows for rolling infs and NaNs.

  // Enable/disable flush subnormals to zero and subnormals are zero
  static void SetFlushToZero();
  static void SetNoFlushToZero();

  // Note: The static members MaskExceptions(), SetFlushToZero() and
  //       SetNoFlushToZero() do not affect instance data set via
  //       ReadData() (obviously).  Typical usage pattern is
  //        Oc_FpuControlData foo;
  //        foo.ReadData();  // Save current FPU flag state
  //        Oc_FpuControlData::SetNoFlushToZero();
  //        ... code requiring gradual underflow ...
  //        foo.WriteData(); // Reset FPU flag state

 private:

#if OC_USE_X87
  OC_UINT2 x87_data;
  static void Setx87ControlWord (OC_UINT2 mode);
  static OC_UINT2 Getx87ControlWord();
#endif

#if OC_USE_SSE
  OC_UINT2 sse_data;
  static void SetSSEControlWord (OC_UINT2 mode);
  static OC_UINT2 GetSSEControlWord();
#endif
};

#endif // _OCFPU
