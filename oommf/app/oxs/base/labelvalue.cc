/* FILE: labelvalue.cc                 -*-Mode: c++-*-
 *
 * Concrete label-value class, derived from OXS extension class.
 * A wrapper around an STL map object, using the OXS initialization
 * paradigm.
 *
 */

#include "labelvalue.h"

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_LabelValue);

/* End includes */


// Constructor
Oxs_LabelValue::Oxs_LabelValue(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Ext(name,newdtr,argstr)
{
  // Uses parent "init_strings" map to hold all label-value pairs.
  // Note the absence of a VerifyAllInitArgsUsed() call. <g>
}

void Oxs_LabelValue::GetLabels(vector<String>& labels) const
{
  labels.clear();
  map<String,String>::const_iterator it = init_strings.begin();
  while(it != init_strings.end()) {
    labels.push_back(it->first);
    ++it;
  }
}

OC_BOOL
Oxs_LabelValue::HasLabel(const String& label) const
{ // Returns 1 if label is a valid key into init_strings, 0 otherwise.
  return HasInitValue(label);
}

String
Oxs_LabelValue::GetValue(const String& label) const
{ // Throws an exception if "label" is not a key in init_strings.
  String value;
  if(!FindInitValue(label,value)) {
    String msg
      = String("Label \"") + label
      + String("\" not found.");
    throw Oxs_ExtError(this,msg.c_str());
  }
  return value;
}

void
Oxs_LabelValue::SetLabel
(const String& label,
 const String& value)
{ // Unconditionally sets/adds the pair label,value to init_strings.
  init_strings[label] = value;
}

OC_BOOL
Oxs_LabelValue::InitializeLabel
(const String& label,
 const String& value)
{ // If 'label' is not in init_strings, then adds it with associated
  // value 'value', and returns 1.  If 'label' is already in
  // init_strings, then the value is unchanged and 0 is returned.
  if(HasLabel(label)) return 0;
  SetLabel(label,value);
  return 1;
}
