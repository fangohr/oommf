/* FILE: atlas.h                 -*-Mode: c++-*-
 *
 * Abstract atlas class, derived from OXS extension class.
 *
 */

#ifndef _OXS_ATLAS
#define _OXS_ATLAS

#include <string>

#include "ext.h"
#include "threevector.h"
#include "util.h"

OC_USE_STRING;

/* End includes */

class Oxs_Atlas:public Oxs_Ext {
private:
  // Oxs_Ext parent can't be copied, so this class can't either.
  // So disable copy constructor and assignment operators by declaring
  // them without providing definitions.
  Oxs_Atlas(const Oxs_Atlas&);
  Oxs_Atlas& operator=(const Oxs_Atlas&);

protected:
  Oxs_Atlas(const char* name,      // Child instance id
	    Oxs_Director* newdtr); // App director
  Oxs_Atlas(const char* name,
	    Oxs_Director* newdtr,
	    const char* argstr);   // MIF block argument string
public:
  // NOTE: All atlases include a special region, with
  // reserve name "universe", which conceptually extends
  // outside the atlas to include the universe <g>.  The
  // id for the universe region is 0.

  virtual ~Oxs_Atlas() {}

  virtual void GetWorldExtents(Oxs_Box &mybox) const =0;
  /// Fills mybox with bounding box for the atlas,
  /// excluding "universe," natch.

  virtual OC_BOOL GetRegionExtents(OC_INDEX id,Oxs_Box &mybox) const =0;
  /// If 0<id<GetRegionCount, then sets mybox to bounding box
  /// for the specified region, and returns 1.  If id is 0,
  /// sets mybox to atlas bounding box, and returns 1.
  /// Otherwise, leaves mybox untouched and returns 0.

  virtual OC_INDEX GetRegionId(const ThreeVector& point) const =0;
  /// Returns the id number for the region containing point.
  /// The return value is 0 if the point is not contained in
  /// the atlas, i.e., belongs to the "universe" region.
  /// The return value is guaranteed to be in the range
  /// [0,GetRegionCount()-1].

  virtual OC_INDEX GetRegionId(const String& name) const =0;
  /// Given a region id string (name), returns
  /// the corresponding region id index.  If
  /// "name" is not included in the atlas, then
  /// -1 is returned.  Note: If name == "universe",
  /// then the return value will be 0.

  virtual OC_BOOL GetRegionName(OC_INDEX id,String& name) const =0;
  /// Given an id number, fills in "name" with
  /// the corresponding region id string.  Returns
  /// 1 on success, 0 if id is invalid.  If id is 0,
  /// then name is set to "universe", and the return
  /// value is 1.

  virtual OC_INDEX GetRegionCount() const =0;
  /// Valid RegionId numbers range from 0 to GetRegionCount() - 1,
  /// inclusive, where 0 is the special "universe" region.

  void GetRegionList(vector<String>& regions) const;

};

#endif // _OXS_ATLAS

