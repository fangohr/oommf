/* FILE: output.h                 -*-Mode: c++-*-
 *
 * Output classes.  Each class providing output should embed an
 * instance of an output class for each output provided.
 *
 * NOTE: This code looks pretty fragile, because there is a lot
 *  of coupling between the output classes and their owner class.
 *  Maybe we can figure out a cleaner, more robust solution in
 *  the future.
 */

#ifndef _OXS_OUTPUT
#define _OXS_OUTPUT

#include <string>

#include "oc.h"

#include "simstate.h"

OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported
OC_USE_STRING;

/* End includes */

class Oxs_Director;   // Forward declarations
class Oxs_SimState;

class Oxs_Output {
private:
  Oxs_Director* director;  // Director instance this Oxs_Output
  /// instance is registered with.

  String owner_name;
  String output_name;
  String output_type;  // "scalar", "vector field", or "scalar field"
  String output_units;

  // The CacheRequestIncrement routine puts requests in (positive
  // argument) or out (negative argument) to have the cache updated with
  // new results. This allows output requests to be grouped for
  // efficiency. Such requests are applied to subsequent UpdateCache
  // requests; UpdateCache() will only update the cache if the request
  // count is bigger than zero. UpdateCache() does not adjust the cache
  // request count---cache requests are persistent, remaining until
  // expressed removed by a subsequent CacheRequestIncrement with a
  // negative increment.
  //   The output object automatically manages the cache. In particular,
  // the cache control may release the cache when cache_request_count
  // goes to zero.
  OC_UINT4m cache_request_count;

  OC_INT4m priority; // Output evaluation priority.  Smaller
  /// is higher priority.  Default priority is 0.  This
  /// can only be set from Oxs_Output::Register function,
  /// because it should remain constant throughout registration
  /// lifetime---the Oxsii and Boxsi scripts build an ordered
  /// output list by calling Oxs_Director::ListOutputObjects
  /// immediately after loading a problem, and use that list
  /// throughout the lifetime of the problem.

  Oxs_Output(const Oxs_Output&) = delete;
  Oxs_Output& operator=(const Oxs_Output&) = delete;

protected:
  OC_BOOL GetMifOption(const char* label,String& value) const;

  const char* GetProblemName() const;

  void GetBaseNames(vector<String>& label_value_names) const {
    label_value_names.clear();
    label_value_names.emplace_back("owner");
    label_value_names.emplace_back(owner_name);
    label_value_names.emplace_back("name");
    label_value_names.emplace_back(output_name);
    label_value_names.emplace_back("type");
    label_value_names.emplace_back(output_type);
    label_value_names.emplace_back("units");
    label_value_names.emplace_back(output_units);
  }

  void Setup(const char* owner_name_,
             const char* output_name_,
             const char* output_type_,
             const char* output_units_);
  // Called from public child Setup members, to initialize private
  // data.  Also resets cache_request_count to 0.  A previous fifth
  // optional parameter "cache_support" has been deprecated since
  // 2021-03-18 (commit 110419aca0d2).

  Oxs_Director* GetDirector() const { return director; }

  void Register(Oxs_Director* director_,OC_INT4m priority_);
  // Registers with director.  Automatic deregistration is set up
  // in ~Oxs_Output.

  void Deregister();

public:
  Oxs_Output();
  virtual ~Oxs_Output(); // Deregisters

  virtual void GetNames(vector<String>& label_value_names) const =0;
  String OwnerName()  const { return owner_name;  }
  String OutputName() const { return output_name; }
  String LongName() const;
  String OutputType() const { return output_type; }
  String OutputUnits() const { return output_units; }
  virtual int SetOutputFormat(const String& format) =0;
  virtual String GetOutputFormat() const =0;

  virtual int Output(const Oxs_SimState*,Tcl_Interp*,int argc,
                     const char** argv) =0;

  // Cache request control.
  // Note 1: The controlling cache_request_count variable gets reset
  //  each time Setup is called.  So CacheRequestIncrement should be
  //  called only _after_ Setup, and cache request needs must be
  //  re-requested any time Setup is re-called.
  // Note 2: CacheRequestIncrement() may be overridden by a child class,
  //  for example to release cache buffers when cache_request_count goes
  //  to zero.
  void virtual CacheRequestIncrement(OC_INT4m incr) {
    cache_request_count += incr;
  }
  OC_UINT4m GetCacheRequestCount() const { return cache_request_count; }

  OC_INT4m Priority() const { return priority; }

  // Output unit kludge added to support "user output" initialization
  // protocol.
  String GetUnits() const { return output_units; }
  void ChangeUnits(const String& new_units) {
    // Kludge added to support user output initialization protocol.
    output_units = new_units;
  }

};

#endif // _OXS_OUTPUT
