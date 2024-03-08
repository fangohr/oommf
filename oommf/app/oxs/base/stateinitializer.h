/* FILE: stateinitializer.h                 -*-Mode: c++-*-
 *
 * Abstract Oxs_StateInitializer class; supports user-specified
 * Oxs_SimState initialization.
 *
 */

#ifndef _OXS_STATEINITIALIZER
#define _OXS_STATEINITIALIZER

#include "oc.h"
#include "ext.h"

/* End includes */

OC_USE_STRING;
class Oxs_SimState; // Forward reference

class Oxs_StateInitializer : public Oxs_Ext {
 public:
  Oxs_StateInitializer
  ( const char* name,        // Child instance id
    Oxs_Director* newdtr,    // App director
    const char* argstr       // Args
    ) : Oxs_Ext(name,newdtr,argstr) {}
  virtual ~Oxs_StateInitializer() {}

  virtual void InitState(Oxs_SimState& new_state) =0;
  /// Note: This routine is called by Oxs_Driver each time a new
  /// Oxs_SimState is created, starting with the simulation initial
  /// state. (Note: For the first state new_state.previous_state_id
  /// will be 0.)

 private:
  // Disable implicit constructors and assignment operator
  Oxs_StateInitializer() = delete;
  Oxs_StateInitializer(const Oxs_StateInitializer&) = delete;
  Oxs_StateInitializer& operator=(const Oxs_StateInitializer&) = delete;
};

#endif // _OXS_STATEINITIALIZER
