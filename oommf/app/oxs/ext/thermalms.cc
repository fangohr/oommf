/* FILE: thermalms.h                        -*-Mode: c++-*-
 *
 * Non-abstract child of Oxs_StateInitializer that adjusts
 * ptwise saturation magnetization (Ms) between steps.
 *
 */

#include <iostream>

#include "thermalms.h"

/* End includes */

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_ThermalMs);


void
Oxs_ThermalMs::InitState(Oxs_SimState& new_state) {
  // Do nothing for now
  std::cout << "Oxs_ThermalMs::InitState call on state "
            << new_state.previous_state_id
            << "/" << new_state.Id()
            << ", iteration " << new_state.iteration_count
            << std::endl;
}
