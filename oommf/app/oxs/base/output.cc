/* FILE: ouput.cc                -*-Mode: c++-*-
 *
 * Output classes.  Each class providing output should embed an
 * instance of an output class for each output provided.
 *   Most of the real code is in the templated subclasses.  For
 * portability reasons, that code is in the header file, output.h.
 */

#include "director.h"
#include "output.h"

/* End includes */

Oxs_Output::Oxs_Output()
  : director(NULL),
    cache_support(0), cache_request_count(0),
    priority(0)
{}

Oxs_Output::~Oxs_Output()
{ // Deregister output from director
  Deregister();
}

void Oxs_Output::Setup
(const char* owner_name_,
 const char* output_name_,
 const char* output_type_,
 const char* output_units_,
 OC_BOOL cache_support_)
{
  owner_name = owner_name_;
  output_name = output_name_;
  output_type = output_type_;
  output_units = output_units_;
  cache_support = cache_support_;
  cache_request_count = 0;  // Reset to 0.
}

void Oxs_Output::Register
(Oxs_Director* director_,OC_INT4m priority_)
{
  // NOTE: The API guarantees that priority is only allowed
  //  to change across registrations.
  if(director!=NULL && director->IsRegisteredOutput(this)) {
    // Output already registered.  If no changes are requested,
    // then just return.
    // NOTE: If a change is requested, then the deregistration
    //  + registration will change the token associated with
    //  the output.  At present (12/2004) there is no mechanism
    //  to inform users of the old token (in particular at the
    //  Tcl script level) that the token has become invalid,
    //  nor to notify that a new token is available.  At present,
    //  all outputs are setup during problem loading, but these
    //  issues will need to be addressed if the program evolves
    //  to allow more dynamic outputs.
    if(director==director_ && priority==priority_) return;
    director->DeregisterOutput(this);
  }
  director = director_;
  priority = priority_;
  if(director==NULL) {
    OXS_THROW(Oxs_ProgramLogicError,
              "Output object not registered: director not specified.");
  }
  director->RegisterOutput(this);
#ifndef NDEBUG
  if(!director_->IsRegisteredOutput(this)) {
    OXS_THROW(Oxs_ProgramLogicError,
              "Output object not registered.");
  }
#endif // NDEBUG
}

void Oxs_Output::Deregister() {
  if(director!=0) director->DeregisterOutput(this);
  director=0; // Protect against multiple deregistration
}

String Oxs_Output::LongName() const
{
  String longname = String(owner_name) + String(":")
    + String(output_name);
  return longname;
}

// Helper function for templated child constructors.  See the
// "Note" comment attached to the Oxs_ScalarOutput and
// Oxs_VectorFieldOutput constructors for an explanation of
// this initialization nonsense.
OC_BOOL Oxs_Output::GetMifOption(const char* label,String& value) const
{
  if(director==NULL) {
    OXS_THROW(Oxs_ProgramLogicError,"Director not specified.");
  }
  return director->GetMifOption(label,value);
}

// Same "Note" as for Oxs_Output::GetMifOption above.
const char* Oxs_Output::GetProblemName() const {
  if(director==NULL) {
    OXS_THROW(Oxs_ProgramLogicError,"Director not specified.");
  }
  return director->GetProblemName();
}

// Cache flag control
OC_BOOL Oxs_Output::ForceCacheIncrement(OC_INT4m incr)
{
  if(incr>0) cache_request_count+=incr;
  else if(incr<0) {
    OC_UINT4m decr = -1*incr;
    if(cache_request_count < decr) cache_request_count = 0;
    else                           cache_request_count -= decr;
  }
  return 1;
}

