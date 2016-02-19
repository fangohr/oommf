/* FILE: atlas.cc                 -*-Mode: c++-*-
 *
 * Atlas class, derived from OXS extension class.
 *
 */

#include "atlas.h"

/* End includes */


// Constructors
Oxs_Atlas::Oxs_Atlas
( const char* name,     // Child instance id
  Oxs_Director* newdtr  // App director
  ) : Oxs_Ext(name,newdtr)
{}

Oxs_Atlas::Oxs_Atlas
( const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr    // MIF block argument string
  ) : Oxs_Ext(name,newdtr,argstr)
{}

void Oxs_Atlas::GetRegionList(vector<String>& regions) const
{
  regions.clear();
  const OC_INDEX count = GetRegionCount();
  for(OC_INDEX i=0;i<count;++i) {
    String tmp;
    if(GetRegionName(i,tmp)) regions.push_back(tmp);
  }
}



