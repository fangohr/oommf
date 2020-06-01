/* FILE: uzeeman.cc            -*-Mode: c++-*-
 *
 * Uniform Zeeman (applied field) energy, derived from Oxs_Energy class.
 *
 */

#include <limits>
#include <string>

#include "oc.h"
#include "nb.h"

#include "mesh.h"
#include "uzeeman.h"

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_UZeeman);

/* End includes */


// Constructor
Oxs_UZeeman::Oxs_UZeeman(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ChunkEnergy(name,newdtr,argstr)
{
  // Process arguments
  // Get scale factor for all Hrange input
  OC_REAL8m hmult = GetRealInitValue("multiplier",1.0);

  // Load Hrange data
  vector<String> hrange;
  FindRequiredInitValue("Hrange",hrange);

  // Build up Happ vector from Hrange data
  OC_UINT4m step_count = 0;
  ThreeVector h0,h1;
  Nb_SplitList range_list;
  for(OC_UINT4m i=0;i<hrange.size();i++) {
    range_list.Split(hrange[i].c_str());
    if(range_list.Count() != 7) {
      char buf[1024];
      Oc_Snprintf(buf,sizeof(buf),
		  "Hrange[%u] value must be a 7 element list"
		  " (actual element count: %d)",
		  i,range_list.Count());
      throw Oxs_ExtError(this,buf);
    }
    OC_BOOL err=0;
    h0.x = Nb_Atof(range_list[0],err);
    if(!err) h0.y = Nb_Atof(range_list[1],err);
    if(!err) h0.z = Nb_Atof(range_list[2],err);
    if(!err) h1.x = Nb_Atof(range_list[3],err);
    if(!err) h1.y = Nb_Atof(range_list[4],err);
    if(!err) h1.z = Nb_Atof(range_list[5],err);
    if(!err) {
      char* cptr;
      long check = strtol(range_list[6],&cptr,10);
      step_count = static_cast<OC_UINT4m>(check);
      if(cptr == range_list[6] || *cptr!='\0' || check<0
         || static_cast<long>(step_count) != check) err=1;
    }
    if(err) {
      char item[1575];  // Safety
      item[1550]='\0';
      Oc_Snprintf(item,sizeof(item),"%.1552s",hrange[i].c_str());
      if(item[1550]!='\0') strcpy(item+1550,"..."); // Overflow
      char buf[2000];
      Oc_Snprintf(buf,sizeof(buf),
		  "Hrange[%u] value is not a 7 element list"
		  " of 6 reals and a non-negative integer: %.1555s",
		  i,item);
      throw Oxs_ExtError(this,buf);
    }
    // Note: There is a potential problem comparing Happ.back() to h0 if
    //  any floating point operations have occured on either.  If those
    //  operations are not exactly the same, there may be differences in
    //  round-off error.  Even if they are written the same in the source
    //  code, if the compiler is given license to re-order floating point
    //  ops, then the results may differ.  Also, there may be differences
    //  between values loaded from memory and those stored in registers.
    //  The code below attempts to work around these issues.
    if(step_count==0 || Happ.empty()) {
      Happ.push_back(h0);
    } else {
      ThreeVector last_h1 = Happ.back();
      if(fabs(last_h1.x-h0.x)>fabs(h0.x)*OC_REAL8_EPSILON*2
	 || fabs(last_h1.y-h0.y)>fabs(h0.y)*OC_REAL8_EPSILON*2
	 || fabs(last_h1.z-h0.z)>fabs(h0.z)*OC_REAL8_EPSILON*2) {
	Happ.push_back(h0);
      }
    }
    for(OC_UINT4m j=1;j<step_count;j++) {
      OC_REAL8m t = static_cast<OC_REAL8m>(j)/static_cast<OC_REAL8m>(step_count);
      Happ.push_back(((1-t)*h0) + (t*h1));
    }
    if(step_count>0) Happ.push_back(h1);
  }
  vector<ThreeVector>::iterator it = Happ.begin();
  while(it!=Happ.end()) {
    *it *= hmult;
    ++it;
  }
  DeleteInitValue("Hrange");

  Bapp_output.Setup(this,InstanceName(),"B","mT",1,
		    &Oxs_UZeeman::Fill__Bapp_output);
  Bappx_output.Setup(this,InstanceName(),"Bx","mT",1,
		     &Oxs_UZeeman::Fill__Bapp_output);
  Bappy_output.Setup(this,InstanceName(),"By","mT",1,
		     &Oxs_UZeeman::Fill__Bapp_output);
  Bappz_output.Setup(this,InstanceName(),"Bz","mT",1,
		     &Oxs_UZeeman::Fill__Bapp_output);

  Bapp_output.Register(director,0);
  Bappx_output.Register(director,0);
  Bappy_output.Register(director,0);
  Bappz_output.Register(director,0);

  VerifyAllInitArgsUsed();
}

Oxs_UZeeman::~Oxs_UZeeman()
{}

OC_BOOL Oxs_UZeeman::Init()
{
  return Oxs_ChunkEnergy::Init();
}

void Oxs_UZeeman::StageRequestCount
(unsigned int& min,
 unsigned int& max) const
{
  min = static_cast<unsigned int>(FieldCount());
  if(min<=1) max = UINT_MAX; // No upper limit
  else       max = min;      // Strict limit
}

ThreeVector Oxs_UZeeman::GetAppliedField(OC_UINT4m stage_number) const
{
  ThreeVector H(0,0,0);
  OC_UINT4m field_count = FieldCount();
  if(field_count>0) {
    if(stage_number>=field_count)  H=Happ[field_count-1];
    else                           H=Happ[stage_number];
  }

  return H;
}

void Oxs_UZeeman::ComputeEnergyChunkInitialize
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>& /* thread_ocedtaux */,
 int /* number_of_threads */) const
{
  const ThreeVector H = GetAppliedField(state.stage_number);
  ocedt.energy_density_error_estimate
    = 4*OC_REAL8m_EPSILON*MU0*state.max_absMs*H.Mag();
}

void Oxs_UZeeman::ComputeEnergyChunk
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,
 OC_INDEX node_stop,
 int /* threadnumber */
 ) const
{
  const OC_INDEX size = state.mesh->Size();
  if(size<1) {
    return;
  }

  const ThreeVector H = GetAppliedField(state.stage_number);

  if(ocedt.H)       {
    for(OC_INDEX i=node_start;i<node_stop;++i) {
      (*ocedt.H)[i] = H;
    }
  }
  if(ocedt.H_accum) {
    for(OC_INDEX i=node_start;i<node_stop;++i) {
      (*ocedt.H_accum)[i] += H;
    }
  }

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  Nb_Xpfloat energy_sum = 0.0;
  OC_REAL8m cell_volume;
  if(state.mesh->HasUniformCellVolumes(cell_volume)) {
    for(OC_INDEX i=node_start;i<node_stop;++i) {
      OC_REAL8m ei,tx,ty,tz;
      OC_REAL8m Msi = Ms[i];
      if(0.0 != Msi) {
        const ThreeVector& m = spin[i];
        tz = m.x*H.y;  // t = m x H
        ty = m.x*H.z;
        tx = m.y*H.z;
        ei =  -MU0*Msi*(m.x*H.x + m.y*H.y + m.z*H.z);
        tz -= m.y*H.x;
        ty  = m.z*H.x - ty;
        tx -= m.z*H.y;
        energy_sum += ei;
        if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
        if(ocedt.mxH_accum) (*ocedt.mxH_accum)[i] += ThreeVector(tx,ty,tz);
      } else {
        ei = tx = ty = tz = 0.0;
      }
      if(ocedt.energy) (*ocedt.energy)[i] = ei;
      if(ocedt.mxH)       (*ocedt.mxH)[i] = ThreeVector(tx,ty,tz);
    }
    energy_sum *= cell_volume;
  } else {
    for(OC_INDEX i=node_start;i<node_stop;++i) {
      OC_REAL8m ei,tx,ty,tz;
      OC_REAL8m Msi = Ms[i];
      if(0.0 != Msi) {
        const ThreeVector& m = spin[i];
        tz = m.x*H.y;  // t = m x H
        ty = m.x*H.z;
        tx = m.y*H.z;
        ei =  -MU0*Msi*(m.x*H.x + m.y*H.y + m.z*H.z);
        tz -= m.y*H.x;
        ty  = m.z*H.x - ty;
        tx -= m.z*H.y;
        energy_sum += ei * state.mesh->Volume(i);
        if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
        if(ocedt.mxH_accum) (*ocedt.mxH_accum)[i] += ThreeVector(tx,ty,tz);
      } else {
        ei = tx = ty = tz = 0.0;
      }
      if(ocedt.energy) (*ocedt.energy)[i] = ei;
      if(ocedt.mxH)       (*ocedt.mxH)[i] = ThreeVector(tx,ty,tz);
    }
  }

  ocedtaux.energy_total_accum += energy_sum;
  // ocedtaux.pE_pt_accum += 0.0;
}

void
Oxs_UZeeman::Fill__Bapp_output(const Oxs_SimState& state)
{
  ThreeVector B = GetAppliedField(state.stage_number);

  B *= MU0*1000; // Report Bapp in mT

  if(Bapp_output.GetCacheRequestCount()>0) {
    Bapp_output.cache.state_id=0;
    Bapp_output.cache.value = sqrt(B.MagSq());
    Bapp_output.cache.state_id=state.Id();
  }

  if(Bappx_output.GetCacheRequestCount()>0) {
    Bappx_output.cache.state_id=0;
    Bappx_output.cache.value = B.x;
    Bappx_output.cache.state_id=state.Id();
  }

  if(Bappy_output.GetCacheRequestCount()>0) {
    Bappy_output.cache.state_id=0;
    Bappy_output.cache.value = B.y;
    Bappy_output.cache.state_id=state.Id();
  }

  if(Bappz_output.GetCacheRequestCount()>0) {
    Bappz_output.cache.state_id=0;
    Bappz_output.cache.value = B.z;
    Bappz_output.cache.state_id=state.Id();
  }

}

// Optional interface for conjugate-gradient evolver.
// For details on this code, see NOTES VI, 21-July-2011, pp 10-11.
OC_INT4m
Oxs_UZeeman::IncrementPreconditioner(PreconditionerData& /* pcd */)
{
  // Nothing to do --- bilinear part of this term is 0.
  return 1;
}
