/* FILE: rungekuttaevolve.cc                 -*-Mode: c++-*-
 *
 * Concrete evolver class, using Runge-Kutta steps
 *
 */

#include <float.h>
#include <string>

#include "nb.h"
#include "director.h"
#include "timedriver.h"
#include "simstate.h"
#include "rectangularmesh.h"   // For Zhang damping
#include "rungekuttaevolve.h"
#include "key.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

#if OOMMF_THREADS
# include "oxsthread.h"
#endif // OOMMF_THREADS

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_RungeKuttaEvolve);

/* End includes */

//#define NOAH

#ifdef NOAH
void ShowDouble(char* buf,const void* addr)
{
  const double* dptr = static_cast<const double*>(addr);
  const unsigned char* cptr = static_cast<const unsigned char*>(addr);
  Oc_Snprintf(buf,100,"Addr %p: %25.16e :",addr,*dptr);
  for(int i=0;i<sizeof(double);++i) {
    Oc_Snprintf(buf+strlen(buf),100," %02x",cptr[i]);
  }
  Oc_Snprintf(buf+strlen(buf),100,"\n");
}
#endif

/////////////////////////////////////////////////////////////////////////
///////// DEBUGGING DEBUGGING DEBUGGING DEBUGGING DEBUGGING /////////////
#define DEBUG_DIFF_DUMP 0
#if !DEBUG_DIFF_DUMP
void SpinDiff(const char*,const Oxs_SimState&) {}
void VecDiff(const char*,const Oxs_MeshValue<ThreeVector>&) {}
void RealDiff(const char*,const Oxs_MeshValue<OC_REAL8m>&) {}
#else
void SpinDiff(const char* label,const Oxs_SimState& state) {
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const OC_INDEX size = spin.Size();
  if(size<2) return;
  ThreeVector ref = spin[0];
  OC_REAL8m maxdiffsq = 0.0;
  for(OC_INDEX i=1; i<size; ++i) {
    ThreeVector tmp = spin[i];
    tmp -= ref;
    OC_REAL8m diffsq = tmp.MagSq();
    if(diffsq>maxdiffsq) {
      maxdiffsq = diffsq;
    }
  }
  fprintf(stderr,"%s: Max diff in state %u/%u = %g\n",
          label,
          state.iteration_count,state.Id(),sqrt(maxdiffsq));
}

void VecDiff(const char* label,const Oxs_MeshValue<ThreeVector>& arr) {
  const OC_INDEX size = arr.Size();
  if(size<2) return;
  ThreeVector ref = arr[0];
  OC_REAL8m maxdiffsq = 0.0;
  for(OC_INDEX i=1; i<size; ++i) {
    ThreeVector tmp = arr[i];
    tmp -= ref;
    OC_REAL8m diffsq = tmp.MagSq();
    if(diffsq>maxdiffsq) {
      maxdiffsq = diffsq;
    }
  }
  fprintf(stderr,"%s: Max diff in arr = %g\n",
          label,sqrt(maxdiffsq));
}

void RealDiff(const char* label,const Oxs_MeshValue<OC_REAL8m>& arr) {
  const OC_INDEX size = arr.Size();
  if(size<2) return;
  OC_REAL8m ref = arr[0];
  OC_REAL8m maxdiff = 0.0;
  for(OC_INDEX i=1; i<size; ++i) {
    OC_REAL8m tmp = arr[i];
    tmp -= ref;
    OC_REAL8m diff = fabs(tmp);
    if(diff>maxdiff) {
      maxdiff = diff;
    }
  }
  fprintf(stderr,"%s: Max diff in arr = %g\n",
          label,maxdiff);
}
#endif // DEBUG_DIFF_DUMP

///////// DEBUGGING DEBUGGING DEBUGGING DEBUGGING DEBUGGING /////////////
/////////////////////////////////////////////////////////////////////////


#if OOMMF_THREADS

////////////////////////////////////////////////////////////////////////
///////// THREAD A  THREAD A  THREAD A  THREAD A  THREAD A /////////////
////////////////////////////////////////////////////////////////////////

class _Oxs_RKEvolve_Step_ThreadA : public Oxs_ThreadRunObj {
public:
  // Note: The job basket is static, so only one "set" (tree) of this
  // class may be run at any one time.
  static Oxs_JobControl<OC_REAL8m> job_basket;

  // Imports
  const Oxs_Mesh* mesh_ptr;
  const Oxs_MeshValue<OC_REAL8m>* Ms_ptr;
  const Oxs_MeshValue<OC_REAL8m>* gamma_ptr;
  const Oxs_MeshValue<OC_REAL8m>* alpha_ptr;
  const Oxs_MeshValue<ThreeVector>* mxH_ptr;
  const Oxs_MeshValue<ThreeVector>* spin_ptr;

  // Exports
  Oxs_MeshValue<ThreeVector>* dm_dt_ptr;
  /// Note: mxH and dm_dt may be same Oxs_MeshValue objects.
  vector<OC_REAL8m> thread_dE_dt;
  vector<OC_REAL8m> thread_max_dm_dt;

  // More imports
  OC_BOOL do_precess;

  _Oxs_RKEvolve_Step_ThreadA(int thread_count,
                             const Oxs_StripedArray<OC_REAL8m>* arrblock)
    : mesh_ptr(0), Ms_ptr(0), gamma_ptr(0), alpha_ptr(0), mxH_ptr(0),
      spin_ptr(0), dm_dt_ptr(0),
      thread_dE_dt(thread_count), // Default initialize to 0.0's
      thread_max_dm_dt(thread_count), // Default initialize to 0.0's
      do_precess(1) {
    job_basket.Init(thread_count,arrblock);
  }

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_RKEvolve_Step_ThreadA::job_basket;

void _Oxs_RKEvolve_Step_ThreadA::Cmd(int threadnumber,
                                     void* /* data */)
{ // Imports
  const Oxs_Mesh& mesh = *mesh_ptr;
  const Oxs_MeshValue<OC_REAL8m>& Ms    = *Ms_ptr;
  const Oxs_MeshValue<OC_REAL8m>& gamma = *gamma_ptr;
  const Oxs_MeshValue<OC_REAL8m>& alpha = *alpha_ptr;
  const Oxs_MeshValue<ThreeVector>& mxH = *mxH_ptr;
  const Oxs_MeshValue<ThreeVector>& spin = *spin_ptr;
  const OC_BOOL precess = do_precess;  // Make local copy

  // Exports
  Oxs_MeshValue<ThreeVector>& dm_dt = *dm_dt_ptr;
  /// Note: mxH and dm_dt may be same Oxs_MeshValue objects.
  OC_REAL8m dE_dt_sum = 0.0;  // Local, work variables
  OC_REAL8m max_dm_dt = 0.0;

  // Support variables
  OC_REAL8m max_dm_dt_sq = 0.0;

  while(1) {
    OC_INDEX jstart,jstop;
    job_basket.GetJob(threadnumber,jstart,jstop);
    if(jstart>=jstop) break; // No more jobs

    OC_INDEX j;

    for(j=jstart;j<jstop;++j) {
      if(Ms[j]==0) {
        dm_dt[j].Set(0.0,0.0,0.0);
      } else {
        OC_REAL8m coef1 = gamma[j];
        OC_REAL8m coef2 = -1*alpha[j]*coef1;
        if(!precess) coef1 = 0.0;
        
        ThreeVector scratch1 = mxH[j];
        ThreeVector scratch2 = mxH[j];
        // Note: mxH may be same as dm_dt

        scratch1 *= coef1;   // coef1 == 0 if do_precess if false
        OC_REAL8m mxH_sq = scratch2.MagSq();
        OC_REAL8m dm_dt_sq = mxH_sq*(coef1*coef1+coef2*coef2);
        if(dm_dt_sq>max_dm_dt_sq) {
          max_dm_dt_sq = dm_dt_sq;
        }
        dE_dt_sum += mxH_sq * Ms[j] * mesh.Volume(j) * coef2;
        scratch2 ^= spin[j]; // ((mxH)xm)
        scratch2 *= coef2;  // -alpha.gamma((mxH)xm) = alpha.gamma(mx(mxH))
        dm_dt[j] = scratch1 + scratch2;
      }
    }
  }
  max_dm_dt = sqrt(max_dm_dt_sq);  // Note: This result may slightly
  /// differ from max_i |dm_dt[i]| by rounding errors.

  // Export results into thread-specific storage
  thread_max_dm_dt[threadnumber] = max_dm_dt;
  thread_dE_dt[threadnumber]     = dE_dt_sum;
}

////////////////////////////////////////////////////////////////////////
///////// THREAD B  THREAD B  THREAD B  THREAD B  THREAD B /////////////
////////////////////////////////////////////////////////////////////////
class _Oxs_RKEvolve_RKFBase54_ThreadB : public Oxs_ThreadRunObj {
public:
  // Note: The job basket is static, so only one "set" (tree) of this
  // class may be run at any one time.
  static Oxs_JobControl<OC_REAL8m> job_basket;

  // Imports
  const Oxs_Mesh* mesh_ptr;
  const Oxs_MeshValue<OC_REAL8m>* Ms_ptr;
  const Oxs_MeshValue<OC_REAL8m>* gamma_ptr;
  const Oxs_MeshValue<OC_REAL8m>* alpha_ptr;
  const Oxs_MeshValue<ThreeVector>* mxH_ptr;
  const Oxs_MeshValue<ThreeVector>* base_spin_ptr;
  const vector<OC_REAL8m>* b_ptr;       // State advancement
  const vector<const Oxs_MeshValue<ThreeVector>*>* A_ptr; // State advancement

  // Exports
  Oxs_MeshValue<ThreeVector>* dm_dt_ptr;
  /// Note: mxH and dm_dt may be same Oxs_MeshValue objects.
  Oxs_MeshValue<ThreeVector>* work_spin_ptr; // Import and export
  Oxs_MeshValue<ThreeVector>* kn_ptr; // State advancement; kn = b*A
  vector<OC_REAL8m> thread_dE_dt;
  vector<OC_REAL8m> thread_max_dm_dt;
  vector<OC_REAL8m> thread_norm_error;  // State advancement
  // Also sets spins in work_state to base_state.spin + mstep*(b*A)

  // More imports
  OC_REAL8m mstep; // State advancement
  OC_REAL8m b_dm_dt; // State advancement

  OC_BOOL do_precess;

  _Oxs_RKEvolve_RKFBase54_ThreadB()
    : mesh_ptr(0), Ms_ptr(0), gamma_ptr(0), alpha_ptr(0), mxH_ptr(0),
      base_spin_ptr(0), b_ptr(0), A_ptr(0),
      dm_dt_ptr(0), work_spin_ptr(0), kn_ptr(0),
      mstep(0.0), b_dm_dt(0.0), do_precess(1) {}

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_RKEvolve_RKFBase54_ThreadB::job_basket;

void _Oxs_RKEvolve_RKFBase54_ThreadB::Cmd(int threadnumber,
                                         void* /* data */)
{
  // Imports
  const Oxs_Mesh& mesh = *mesh_ptr;
  const Oxs_MeshValue<OC_REAL8m>& Ms    = *Ms_ptr;
  const Oxs_MeshValue<OC_REAL8m>& gamma = *gamma_ptr;
  const Oxs_MeshValue<OC_REAL8m>& alpha = *alpha_ptr;
  const Oxs_MeshValue<ThreeVector>& mxH = *mxH_ptr;
  const Oxs_MeshValue<ThreeVector>& base_spin = *base_spin_ptr;
  const vector<OC_REAL8m>& b = *b_ptr;
  const vector<const Oxs_MeshValue<ThreeVector>*>& A = *A_ptr;

  const size_t term_count = b.size();

  // Exports
  Oxs_MeshValue<ThreeVector>& dm_dt = *dm_dt_ptr;
  Oxs_MeshValue<ThreeVector>& work_spin = *work_spin_ptr;
  /// Note: mxH and dm_dt may be same Oxs_MeshValue objects.
  Oxs_MeshValue<ThreeVector>& kn = *kn_ptr;

  OC_REAL8m work_dE_dt = 0.0;
  OC_REAL8m work_max_dm_dt = 0.0;
  OC_REAL8m work_norm_error = 0.0;

  // Local copies
  const OC_REAL8m work_mstep = mstep;
  const OC_REAL8m work_b_dm_dt = b_dm_dt;
  const OC_BOOL work_do_precess = do_precess;

  // Support variables
  OC_REAL8m max_dm_dt_sq = 0.0;

  OC_REAL8m min_normsq0  = DBL_MAX;
  OC_REAL8m max_normsq0  = 0.0;

  OC_REAL8m min_normsq1  = DBL_MAX;
  OC_REAL8m max_normsq1  = 0.0;

  while(1) {
    OC_INDEX jstart,jstop;
    job_basket.GetJob(threadnumber,jstart,jstop);
    if(jstart>=jstop) break; // No more jobs

    OC_INDEX j;

    for(j=jstart; j < jstart + (jstop-jstart)%2 ; ++j) {
      ThreeVector dm_dt_result(0,0,0);
      if(Ms[j]!=0.0) {
        OC_REAL8m coef1 = gamma[j];
        OC_REAL8m coef2 = -1*alpha[j]*coef1;
        if(!work_do_precess) coef1 = 0.0;
        
        ThreeVector scratch1 = mxH[j];
        ThreeVector scratch2 = mxH[j];
        // Note: mxH may be same as dm_dt

        scratch1 *= coef1;   // coef1 == 0 if do_precess if false
        OC_REAL8m mxH_sq = scratch2.MagSq();
        OC_REAL8m dm_dt_sq = mxH_sq*(coef1*coef1+coef2*coef2);
        if(dm_dt_sq>max_dm_dt_sq) {
          max_dm_dt_sq = dm_dt_sq;
        }
        work_dE_dt += mxH_sq * Ms[j] * mesh.Volume(j) * coef2;
        scratch2 ^= work_spin[j]; // ((mxH)xm)
        scratch2 *= coef2;  // -alpha.gamma((mxH)xm) = alpha.gamma(mx(mxH))
        dm_dt_result = scratch1 + scratch2;
      }
      dm_dt[j] = dm_dt_result;

      // State advance.  Match the form used in the pair-processing
      // block below so that rounding results don't depend on whether
      // or not the job scheduler exercises this branch.
      ThreeVector tempsum(0,0,0);
      for(size_t k=0;k<term_count;++k) {
        const OC_REAL8m bk = b[k];
        tempsum.Accum(bk,(*(A[k]))[j]);
      }
      tempsum.Accum(work_b_dm_dt,dm_dt_result);
      ThreeVector wspin = base_spin[j];
      wspin.Accum(work_mstep,tempsum);

      kn[j]   = tempsum;
      OC_REAL8m magsq = wspin.MakeUnit();
      work_spin[j]   = wspin;

      if(magsq<min_normsq0) min_normsq0=magsq;
      if(magsq>max_normsq0) max_normsq0=magsq;
    }

    for(;j<jstop;j+=2) {
      ThreeVector dm_dt_result0(0,0,0);
      if(Ms[j]!=0.0) {
        OC_REAL8m coef1 = gamma[j];
        OC_REAL8m coef2 = -1*alpha[j]*coef1;
        if(!work_do_precess) coef1 = 0.0;
        
        ThreeVector scratch1 = mxH[j];
        ThreeVector scratch2 = mxH[j];
        // Note: mxH may be same as dm_dt

        scratch1 *= coef1;   // coef1 == 0 if do_precess if false
        OC_REAL8m mxH_sq = scratch2.MagSq();
        OC_REAL8m dm_dt_sq = mxH_sq*(coef1*coef1+coef2*coef2);
        if(dm_dt_sq>max_dm_dt_sq) {
          max_dm_dt_sq = dm_dt_sq;
        }
        work_dE_dt += mxH_sq * Ms[j] * mesh.Volume(j) * coef2;
        scratch2 ^= work_spin[j]; // ((mxH)xm)
        scratch2 *= coef2;  // -alpha.gamma((mxH)xm) = alpha.gamma(mx(mxH))
        dm_dt_result0 = scratch1 + scratch2;
      }

      ThreeVector dm_dt_result1(0,0,0);
      if(Ms[j+1]!=0.0) {
        OC_REAL8m coef1 = gamma[j+1];
        OC_REAL8m coef2 = -1*alpha[j+1]*coef1;
        if(!work_do_precess) coef1 = 0.0;
        
        ThreeVector scratch1 = mxH[j+1];
        ThreeVector scratch2 = mxH[j+1];
        // Note: mxH may be same as dm_dt

        scratch1 *= coef1;   // coef1 == 0 if do_precess if false
        OC_REAL8m mxH_sq = scratch2.MagSq();
        OC_REAL8m dm_dt_sq = mxH_sq*(coef1*coef1+coef2*coef2);
        if(dm_dt_sq>max_dm_dt_sq) {
          max_dm_dt_sq = dm_dt_sq;
        }
        work_dE_dt += mxH_sq * Ms[j+1] * mesh.Volume(j+1) * coef2;
        scratch2 ^= work_spin[j+1]; // ((mxH)xm)
        scratch2 *= coef2;  // -alpha.gamma((mxH)xm) = alpha.gamma(mx(mxH))
        dm_dt_result1 = scratch1 + scratch2;
      }

      // State advance
      ThreeVector tempsum0(0,0,0);
      ThreeVector tempsum1(0,0,0);
      for(size_t k=0;k<term_count;++k) {
        const OC_REAL8m bk = b[k];
        tempsum0.Accum(bk,(*(A[k]))[j]);
        tempsum1.Accum(bk,(*(A[k]))[j+1]);
      }
      ThreeVector wspin0 = base_spin[j];
      ThreeVector wspin1 = base_spin[j+1];
      tempsum0.Accum(work_b_dm_dt,dm_dt_result0);
      tempsum1.Accum(work_b_dm_dt,dm_dt_result1);

      wspin0.Accum(work_mstep,tempsum0);
      wspin1.Accum(work_mstep,tempsum1);

      kn[j]   = tempsum0;
      kn[j+1] = tempsum1;

      dm_dt[j]   = dm_dt_result0;
      dm_dt[j+1] = dm_dt_result1;

      OC_REAL8m magsq0 = wspin0.MakeUnit();
      if(magsq0<min_normsq0) min_normsq0=magsq0;
      if(magsq0>max_normsq0) max_normsq0=magsq0;

      OC_REAL8m magsq1 = wspin1.MakeUnit();
      if(magsq1<min_normsq1) min_normsq1=magsq1;
      if(magsq1>max_normsq1) max_normsq1=magsq1;

      work_spin[j]   = wspin0;
      work_spin[j+1] = wspin1;
    }
  }

  // Thread-level exports
  if(min_normsq0 <= max_normsq0) {
    // At least one "j" was processed
    if(min_normsq1<min_normsq0) min_normsq0=min_normsq1;
    if(max_normsq1>max_normsq0) max_normsq0=max_normsq1;
    work_max_dm_dt = sqrt(max_dm_dt_sq);  // Note: This result may slightly
    /// differ from max_j |dm_dt[j]| by rounding errors.
    work_norm_error = OC_MAX(sqrt(max_normsq0)-1.0,1.0 - sqrt(min_normsq0));
  }

  thread_dE_dt[threadnumber] = work_dE_dt;
  thread_max_dm_dt[threadnumber] = work_max_dm_dt;
  thread_norm_error[threadnumber] = work_norm_error;
VecDiff("FINI THREAD B spin",work_spin);
VecDiff("             dm_dt",dm_dt);
VecDiff("                kn",kn);
}

void Oxs_RungeKuttaEvolve::Initialize_Threaded_Calculate_dm_dt
(const Oxs_SimState& base_state, // Import
 Oxs_SimState& work_state,       // Import and export
 const Oxs_MeshValue<ThreeVector>& mxH,
 Oxs_MeshValue<ThreeVector>& dm_dt,
 _Oxs_RKEvolve_RKFBase54_ThreadB& thread_data,
 OC_REAL8m mstep,
 OC_REAL8m b_dm_dt,
 const vector<OC_REAL8m>& b,
 vector<const Oxs_MeshValue<ThreeVector>*>& A,
 Oxs_MeshValue<ThreeVector>& kn) // export = b*A
{
  const Oxs_Mesh* mesh = base_state.mesh;
  const OC_INDEX vecsize = mesh->Size();
  const int thread_count = Oc_GetMaxThreadCount();

  // Fill out alpha and gamma meshvalue arrays, as necessary.
  if(mesh_id != mesh->Id() || !gamma.CheckMesh(mesh)
     || !alpha.CheckMesh(mesh)) {
    UpdateMeshArrays(mesh);
  }

  SpinDiff("INIT THREAD B",base_state);
  RealDiff(" gamma",gamma); /**/
  RealDiff(" alpha",alpha); /**/
  VecDiff(" mxH",mxH); /**/

  thread_data.mesh_ptr = mesh;
  thread_data.Ms_ptr = base_state.Ms;
  thread_data.gamma_ptr = &gamma;
  thread_data.alpha_ptr = &alpha;
  thread_data.mxH_ptr = &mxH;
  thread_data.base_spin_ptr = &base_state.spin;
  thread_data.b_ptr = &b;
  thread_data.A_ptr = &A;
  thread_data.dm_dt_ptr = &dm_dt;
  thread_data.work_spin_ptr = &work_state.spin;
  thread_data.kn_ptr = &kn;
  thread_data.mstep = mstep;
  thread_data.b_dm_dt = b_dm_dt;
  thread_data.do_precess = do_precess;

  thread_data.job_basket.Init(thread_count,work_state.Ms->GetArrayBlock());

  thread_data.thread_dE_dt.clear();
  thread_data.thread_dE_dt.resize(thread_count,0.0);

  thread_data.thread_max_dm_dt.clear();
  thread_data.thread_max_dm_dt.resize(thread_count,0.0);

  thread_data.thread_norm_error.clear();
  thread_data.thread_norm_error.resize(thread_count,0.0);

  // Adjust state code
  work_state.ClearDerivedData();
  const size_t term_count = b.size();
  if(term_count != A.size()) {
    OXS_THROW(Oxs_BadParameter,"Failure in"
              " Oxs_RungeKuttaEvolve::Initialize_Threaded_Calculate_dm_dt:"
              " imports b and A are different lengths.");

  }
  if(term_count == 0) {
    OXS_THROW(Oxs_BadParameter,"Failure in"
              " Oxs_RungeKuttaEvolve::Initialize_Threaded_Calculate_dm_dt:"
              " imports b and A are empty.");

  }

  if(vecsize != A[0]->Size()) {
    OXS_THROW(Oxs_BadParameter,"Failure in"
              " Oxs_RungeKuttaEvolve::Initialize_Threaded_Calculate_dm_dt:"
              " imports A[] and mesh are different lengths.");

  }

  kn.AdjustSize(mesh);
}

void Oxs_RungeKuttaEvolve::Finalize_Threaded_Calculate_dm_dt
(const Oxs_SimState& base_state, // Import
 const _Oxs_RKEvolve_RKFBase54_ThreadB& thread_data, // Import
 OC_REAL8m pE_pt,             // Import
 OC_REAL8m hstep,             // Import
 Oxs_SimState& work_state, // Export
 OC_REAL8m& max_dm_dt,        // Export
 OC_REAL8m& dE_dt,            // Export
 OC_REAL8m& min_timestep_export,    // Export
 OC_REAL8m& norm_error)       // Export
{
  const OC_INDEX thread_count
    = static_cast<OC_INDEX>(thread_data.thread_dE_dt.size());
  assert(thread_count>=0);

  dE_dt = thread_data.thread_dE_dt[0];
  max_dm_dt = thread_data.thread_max_dm_dt[0];
  norm_error = thread_data.thread_norm_error[0];
  for(OC_INDEX i=1;i<thread_count;++i) {
    dE_dt += thread_data.thread_dE_dt[i];
    if(thread_data.thread_max_dm_dt[i]>max_dm_dt) {
      max_dm_dt = thread_data.thread_max_dm_dt[i];
    }
    if(thread_data.thread_norm_error[i]>norm_error) {
      norm_error = thread_data.thread_norm_error[i];
    }
  }
  dE_dt = -1 * MU0 * dE_dt + pE_pt;
  /// The first term is (partial E/partial M)*dM/dt, the
  /// second term is (partial E/partial t)*dt/dt.  Note that,
  /// provided Ms_[i]>=0, that by constructions dE_dt_sum above
  /// is always non-negative, so dE_dt_ can only be made positive
  /// by positive pE_pt_.

  // Get bound on smallest stepsize that would actually
  // change spin new_max_dm_dt_index:
  min_timestep_export = PositiveTimestepBound(max_dm_dt);

  // Finalize state adjustments
  work_state.last_timestep=hstep;

  // Adjust time and iteration fields in new_state
  if(base_state.stage_number != work_state.stage_number) {
    // New stage
    work_state.stage_start_time = base_state.stage_start_time
                                + base_state.stage_elapsed_time;
    work_state.stage_elapsed_time = work_state.last_timestep;
  } else {
    work_state.stage_start_time = base_state.stage_start_time;
    work_state.stage_elapsed_time = base_state.stage_elapsed_time
                                  + work_state.last_timestep;
  }
  // Don't touch iteration counts. (?!)  The problem is that one call
  // to Oxs_RungeKuttaEvolve::Step() takes 2 half-steps, and it is the
  // result from these half-steps that are used as the export state.
  // If we increment the iteration count each time through here, then
  // the iteration count goes up by 2 for each call to Step().  So
  // instead, we leave iteration count at whatever value was filled
  // in by the Oxs_RungeKuttaEvolve::NegotiateTimeStep() method.
}

////////////////////////////////////////////////////////////////////////
///////// THREAD C  THREAD C  THREAD C  THREAD C  THREAD C /////////////
////////////////////////////////////////////////////////////////////////
class _Oxs_RKEvolve_RKFBase54_ThreadC : public Oxs_ThreadRunObj {
public:
  // Note: The job basket is static, so only one "set" (tree) of this
  // class may be run at any one time.
  static Oxs_JobControl<OC_REAL8m> job_basket;

  // Imports
  const Oxs_Mesh* mesh_ptr;
  const Oxs_MeshValue<OC_REAL8m>* Ms_ptr;
  const Oxs_MeshValue<OC_REAL8m>* gamma_ptr;
  const Oxs_MeshValue<OC_REAL8m>* alpha_ptr;
  const Oxs_MeshValue<ThreeVector>* mxH_ptr;
  const Oxs_MeshValue<ThreeVector>* base_spin_ptr;
  const vector<OC_REAL8m>* b1_ptr;      // State advancement
  const vector<OC_REAL8m>* b2_ptr;      // State advancement
  const vector<const Oxs_MeshValue<ThreeVector>*>* A_ptr; // State advancement

  // Exports
  Oxs_MeshValue<ThreeVector>* dm_dt_ptr;
  /// Note: mxH and dm_dt may be same Oxs_MeshValue objects.
  Oxs_MeshValue<ThreeVector>* work_spin_ptr; // Import and export
  Oxs_MeshValue<ThreeVector>* kn_ptr; // State advancement; kn = b2*A
  vector<OC_REAL8m> thread_dE_dt;
  vector<OC_REAL8m> thread_max_dm_dt;
  vector<OC_REAL8m> thread_norm_error;  // State advancement
  // Also sets spins in work_state to base_state.spin + mstep*(b1*A)

  // More imports
  OC_REAL8m mstep; // State advancement
  OC_REAL8m b1_dm_dt;  // State advancement
  OC_REAL8m b2_dm_dt; // State advancement

  OC_BOOL do_precess;

  _Oxs_RKEvolve_RKFBase54_ThreadC()
    : mesh_ptr(0), Ms_ptr(0), gamma_ptr(0), alpha_ptr(0), mxH_ptr(0),
      base_spin_ptr(0), b1_ptr(0), b2_ptr(0), A_ptr(0),
      dm_dt_ptr(0), work_spin_ptr(0), kn_ptr(0),
      mstep(0.0), b1_dm_dt(0.0), b2_dm_dt(0.0),
      do_precess(1) {}

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_RKEvolve_RKFBase54_ThreadC::job_basket;

void _Oxs_RKEvolve_RKFBase54_ThreadC::Cmd(int threadnumber,
                                         void* /* data */)
{
  // Imports
  const Oxs_Mesh& mesh = *mesh_ptr;
  const Oxs_MeshValue<OC_REAL8m>& Ms    = *Ms_ptr;
  const Oxs_MeshValue<OC_REAL8m>& gamma = *gamma_ptr;
  const Oxs_MeshValue<OC_REAL8m>& alpha = *alpha_ptr;
  const Oxs_MeshValue<ThreeVector>& mxH = *mxH_ptr;
  const Oxs_MeshValue<ThreeVector>& base_spin = *base_spin_ptr;
  const vector<OC_REAL8m>& b1 = *b1_ptr;
  const vector<OC_REAL8m>& b2 = *b2_ptr;
  const vector<const Oxs_MeshValue<ThreeVector>*>& A = *A_ptr;

  const size_t term_count = b1.size();

  // Exports
  Oxs_MeshValue<ThreeVector>& dm_dt = *dm_dt_ptr;
  Oxs_MeshValue<ThreeVector>& work_spin = *work_spin_ptr;
  /// Note: mxH and dm_dt may be same Oxs_MeshValue objects.
  Oxs_MeshValue<ThreeVector>& kn = *kn_ptr;

  OC_REAL8m work_dE_dt = 0.0;
  OC_REAL8m work_max_dm_dt = 0.0;
  OC_REAL8m work_norm_error = 0.0;

  // Local copies
  const OC_REAL8m work_mstep = mstep;
  const OC_REAL8m work_b1_dm_dt = b1_dm_dt;
  const OC_REAL8m work_b2_dm_dt = b2_dm_dt;
  const OC_BOOL work_do_precess = do_precess;


  // Support variables
  OC_REAL8m max_dm_dt_sq = 0.0;

  OC_REAL8m min_normsq  = DBL_MAX;
  OC_REAL8m max_normsq  = 0.0;

  while(1) {
    OC_INDEX jstart,jstop;
    job_basket.GetJob(threadnumber,jstart,jstop);
    if(jstart>=jstop) break; // No more jobs

    OC_INDEX j;

    for(j=jstart; j<jstop ; ++j) {
      ThreeVector dm_dt_result(0,0,0);
      if(Ms[j]!=0.0) {
        OC_REAL8m coef1 = gamma[j];
        OC_REAL8m coef2 = -1*alpha[j]*coef1;
        if(!work_do_precess) coef1 = 0.0;
        
        ThreeVector scratch1 = mxH[j];
        ThreeVector scratch2 = mxH[j];
        // Note: mxH may be same as dm_dt

        scratch1 *= coef1;   // coef1 == 0 if do_precess if false
        OC_REAL8m mxH_sq = scratch2.MagSq();
        OC_REAL8m dm_dt_sq = mxH_sq*(coef1*coef1+coef2*coef2);
        if(dm_dt_sq>max_dm_dt_sq) {
          max_dm_dt_sq = dm_dt_sq;
        }
        work_dE_dt += mxH_sq * Ms[j] * mesh.Volume(j) * coef2;
        scratch2 ^= work_spin[j]; // ((mxH)xm)
        scratch2 *= coef2;  // -alpha.gamma((mxH)xm) = alpha.gamma(mx(mxH))
        dm_dt_result = scratch1 + scratch2;
      }

      // State advance
#if 1
      ThreeVector tempspin1(0,0,0);
      ThreeVector tempspin2(0,0,0);
      for(size_t k=0;k<term_count;++k) {
        const ThreeVector& tempvec = (*(A[k]))[j];
        tempspin2.Accum(b2[k],tempvec);
        tempspin1.Accum(b1[k],tempvec);
      }
      tempspin1.Accum(work_b1_dm_dt,dm_dt_result);
      tempspin1 *= work_mstep;
      tempspin1 += base_spin[j];
      OC_REAL8m magsq = tempspin1.MakeUnit();
      work_spin[j] = tempspin1;

      dm_dt[j] = dm_dt_result;
      tempspin2.Accum(work_b2_dm_dt,dm_dt_result);
      kn[j] = tempspin2;
#else
      Nb_Xpfloat ts1x,ts1y,ts1z;
      Nb_Xpfloat ts2x,ts2y,ts2z;
      for(size_t k=0;k<term_count;++k) {
        const ThreeVector& tempvec = (*(A[k]))[j];
	ts1x += b1[k]*tempvec.x;
	ts1y += b1[k]*tempvec.y;
	ts1z += b1[k]*tempvec.z;
	ts2x += b2[k]*tempvec.x;
	ts2y += b2[k]*tempvec.y;
	ts2z += b2[k]*tempvec.z;
      }
      ts1x += b1_dm_dt*dm_dt_result.x;
      ts1y += b1_dm_dt*dm_dt_result.y;
      ts1z += b1_dm_dt*dm_dt_result.z;
      ts1x *= mstep;
      ts1y *= mstep;
      ts1z *= mstep;
      ts1x += base_spin[j].x;
      ts1y += base_spin[j].y;
      ts1z += base_spin[j].z;
      ThreeVector tempspin1(ts1x.GetValue(),ts1y.GetValue(),ts1z.GetValue());
      OC_REAL8m magsq = tempspin1.MakeUnit();
      work_spin[j] = tempspin1;

      dm_dt[j] = dm_dt_result;

      ts2x += b2_dm_dt*dm_dt_result.x;
      ts2y += b2_dm_dt*dm_dt_result.y;
      ts2z += b2_dm_dt*dm_dt_result.z;
      kn[j].Set(ts2x.GetValue(),ts2y.GetValue(),ts2z.GetValue());
#endif

      if(magsq<min_normsq) min_normsq=magsq;
      if(magsq>max_normsq) max_normsq=magsq;
    }
  }

  // Thread-level exports
  if(min_normsq <= max_normsq) {
    // At least one "j" was processed
    work_max_dm_dt = sqrt(max_dm_dt_sq);  // Note: This result may slightly
    /// differ from max_j |dm_dt[j]| by rounding errors.
    work_norm_error = OC_MAX(sqrt(max_normsq)-1.0,1.0-sqrt(min_normsq));
  }

  thread_dE_dt[threadnumber] = work_dE_dt;
  thread_max_dm_dt[threadnumber] = work_max_dm_dt;
  thread_norm_error[threadnumber] = work_norm_error;
VecDiff("FINI THREAD C spin",work_spin);
VecDiff("             dm_dt",dm_dt);
VecDiff("                kn",kn);
}

void Oxs_RungeKuttaEvolve::Initialize_Threaded_Calculate_dm_dt
(const Oxs_SimState& base_state, // Import
 Oxs_SimState& work_state,       // Import and export
 const Oxs_MeshValue<ThreeVector>& mxH,
 Oxs_MeshValue<ThreeVector>& dm_dt,
 _Oxs_RKEvolve_RKFBase54_ThreadC& thread_data,
 OC_REAL8m mstep,
 OC_REAL8m b1_dm_dt,
 OC_REAL8m b2_dm_dt,
 const vector<OC_REAL8m>& b1,
 const vector<OC_REAL8m>& b2,
 vector<const Oxs_MeshValue<ThreeVector>*>& A,
 Oxs_MeshValue<ThreeVector>& kn)
{
  const Oxs_Mesh* mesh = base_state.mesh;
  const OC_INDEX vecsize = mesh->Size();
 const int thread_count = Oc_GetMaxThreadCount();

  // Fill out alpha and gamma meshvalue arrays, as necessary.
  if(mesh_id != mesh->Id() || !gamma.CheckMesh(mesh)
     || !alpha.CheckMesh(mesh)) {
    UpdateMeshArrays(mesh);
  }

  thread_data.mesh_ptr = mesh;
  thread_data.Ms_ptr = base_state.Ms;
  thread_data.gamma_ptr = &gamma;
  thread_data.alpha_ptr = &alpha;
  thread_data.mxH_ptr = &mxH;
  thread_data.base_spin_ptr = &base_state.spin;
  thread_data.b1_ptr = &b1;
  thread_data.b2_ptr = &b2;
  thread_data.A_ptr = &A;
  thread_data.dm_dt_ptr = &dm_dt;
  thread_data.work_spin_ptr = &work_state.spin;
  thread_data.kn_ptr = &kn;
  thread_data.mstep = mstep;
  thread_data.b1_dm_dt = b1_dm_dt;
  thread_data.b2_dm_dt = b2_dm_dt;
  thread_data.do_precess = do_precess;

  thread_data.job_basket.Init(thread_count,work_state.Ms->GetArrayBlock());

  thread_data.thread_dE_dt.clear();
  thread_data.thread_dE_dt.resize(thread_count,0.0);

  thread_data.thread_max_dm_dt.clear();
  thread_data.thread_max_dm_dt.resize(thread_count,0.0);

  thread_data.thread_norm_error.clear();
  thread_data.thread_norm_error.resize(thread_count,0.0);

  // Adjust state code
  work_state.ClearDerivedData();
  const size_t term_count = b1.size();
  if(term_count != b2.size()) {
    OXS_THROW(Oxs_BadParameter,"Failure in"
              " Oxs_RungeKuttaEvolve::Initialize_Threaded_Calculate_dm_dt:"
              " imports b1 and b2 are different lengths.");

  }
  if(term_count != A.size()) {
    OXS_THROW(Oxs_BadParameter,"Failure in"
              " Oxs_RungeKuttaEvolve::Initialize_Threaded_Calculate_dm_dt:"
              " imports b1 and A are different lengths.");

  }
  if(term_count == 0) {
    OXS_THROW(Oxs_BadParameter,"Failure in"
              " Oxs_RungeKuttaEvolve::Initialize_Threaded_Calculate_dm_dt:"
              " imports b and A are empty.");

  }

  if(vecsize != A[0]->Size()) {
    OXS_THROW(Oxs_BadParameter,"Failure in"
              " Oxs_RungeKuttaEvolve::Initialize_Threaded_Calculate_dm_dt:"
              " imports A[] and mesh are different lengths.");

  }

  kn.AdjustSize(mesh);
}

void Oxs_RungeKuttaEvolve::Finalize_Threaded_Calculate_dm_dt
(const Oxs_SimState& base_state, // Import
 const _Oxs_RKEvolve_RKFBase54_ThreadC& thread_data, // Import
 OC_REAL8m pE_pt,             // Import
 OC_REAL8m hstep,             // Import
 Oxs_SimState& work_state, // Export
 OC_REAL8m& max_dm_dt,        // Export
 OC_REAL8m& dE_dt,            // Export
 OC_REAL8m& min_timestep_export,    // Export
 OC_REAL8m& norm_error)       // Export
{
  const OC_INDEX thread_count
    = static_cast<OC_INDEX>(thread_data.thread_dE_dt.size());
  assert(thread_count>=0);

  dE_dt = thread_data.thread_dE_dt[0];
  max_dm_dt = thread_data.thread_max_dm_dt[0];
  norm_error = thread_data.thread_norm_error[0];
  for(OC_INDEX i=1;i<thread_count;++i) {
    dE_dt += thread_data.thread_dE_dt[i];
    if(thread_data.thread_max_dm_dt[i]>max_dm_dt) {
      max_dm_dt = thread_data.thread_max_dm_dt[i];
    }
    if(thread_data.thread_norm_error[i]>norm_error) {
      norm_error = thread_data.thread_norm_error[i];
    }
  }
  dE_dt = -1 * MU0 * dE_dt + pE_pt;
  /// The first term is (partial E/partial M)*dM/dt, the
  /// second term is (partial E/partial t)*dt/dt.  Note that,
  /// provided Ms_[i]>=0, that by constructions dE_dt_sum above
  /// is always non-negative, so dE_dt_ can only be made positive
  /// by positive pE_pt_.

  // Get bound on smallest stepsize that would actually
  // change spin new_max_dm_dt_index:
  min_timestep_export = PositiveTimestepBound(max_dm_dt);

  // Finalize state adjustments
  work_state.last_timestep=hstep;

  // Adjust time and iteration fields in new_state
  if(base_state.stage_number != work_state.stage_number) {
    // New stage
    work_state.stage_start_time = base_state.stage_start_time
                                + base_state.stage_elapsed_time;
    work_state.stage_elapsed_time = work_state.last_timestep;
  } else {
    work_state.stage_start_time = base_state.stage_start_time;
    work_state.stage_elapsed_time = base_state.stage_elapsed_time
                                  + work_state.last_timestep;
  }
  // Don't touch iteration counts. (?!)  The problem is that one call
  // to Oxs_RungeKuttaEvolve::Step() takes 2 half-steps, and it is the
  // result from these half-steps that are used as the export state.
  // If we increment the iteration count each time through here, then
  // the iteration count goes up by 2 for each call to Step().  So
  // instead, we leave iteration count at whatever value was filled
  // in by the Oxs_RungeKuttaEvolve::NegotiateTimeStep() method.
}

////////////////////////////////////////////////////////////////////////
///////// THREAD D  THREAD D  THREAD D  THREAD D  THREAD D /////////////
////////////////////////////////////////////////////////////////////////
class _Oxs_RKEvolve_RKFBase54_ThreadD : public Oxs_ThreadRunObj {
public:
  // Note: The job basket is static, so only one "set" (tree) of this
  // class may be run at any one time.
  static Oxs_JobControl<OC_REAL8m> job_basket;

  // Imports
  const Oxs_Mesh* mesh_ptr;
  const Oxs_MeshValue<OC_REAL8m>* Ms_ptr;
  const Oxs_MeshValue<OC_REAL8m>* gamma_ptr;
  const Oxs_MeshValue<OC_REAL8m>* alpha_ptr;
  const Oxs_MeshValue<ThreeVector>* mxH_ptr;
  const Oxs_MeshValue<ThreeVector>* work_spin_ptr;
  const Oxs_MeshValue<ThreeVector>* dD13456_ptr;

  // Exports
  Oxs_MeshValue<ThreeVector>* dm_dt_ptr;
  /// Note: mxH and dm_dt may be same Oxs_MeshValue objects.
  vector<OC_REAL8m> thread_dE_dt;
  vector<OC_REAL8m> thread_max_dm_dt;
  vector<OC_REAL8m> thread_max_dD_magsq;

  // More imports
  OC_REAL8m dc7;

  OC_BOOL do_precess;

  _Oxs_RKEvolve_RKFBase54_ThreadD()
    : mesh_ptr(0),Ms_ptr(0), gamma_ptr(0), alpha_ptr(0), mxH_ptr(0),
      work_spin_ptr(0), dD13456_ptr(0), dm_dt_ptr(0),
      dc7(0.0), do_precess(1) {}

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_RKEvolve_RKFBase54_ThreadD::job_basket;

void _Oxs_RKEvolve_RKFBase54_ThreadD::Cmd(int threadnumber,
                                         void* /* data */)
{ // This routine computes max_dD_magsq.  This involves computing
  // dm_dt at each mesh point, but that result is not stored.

  // Imports
  const Oxs_Mesh& mesh = *mesh_ptr;
  const Oxs_MeshValue<OC_REAL8m>& Ms    = *Ms_ptr;
  const Oxs_MeshValue<OC_REAL8m>& gamma = *gamma_ptr;
  const Oxs_MeshValue<OC_REAL8m>& alpha = *alpha_ptr;
  const Oxs_MeshValue<ThreeVector>& mxH = *mxH_ptr;
  const Oxs_MeshValue<ThreeVector>& work_spin = *work_spin_ptr;
  const Oxs_MeshValue<ThreeVector>& dD13456 = *dD13456_ptr;

  // Exports
  Oxs_MeshValue<ThreeVector>& dm_dt = *dm_dt_ptr;
  OC_REAL8m work_dE_dt = 0.0;
  OC_REAL8m work_max_dD_magsq = 0.0;

  // Local copies
  const OC_REAL8m work_dc7 = dc7;
  const OC_BOOL work_do_precess = do_precess;

  // Support variables
  OC_REAL8m max_dm_dt_sq = 0.0;

  while(1) {
    OC_INDEX jstart,jstop;
    job_basket.GetJob(threadnumber,jstart,jstop);
    if(jstart>=jstop) break; // No more jobs

    OC_INDEX j;

    for(j=jstart;j<jstop;++j) {
      ThreeVector dm_dt_result(0,0,0);
      if(Ms[j]!=0.0) {
        OC_REAL8m coef1 = gamma[j];
        OC_REAL8m coef2 = -1*alpha[j]*coef1;
        if(!work_do_precess) coef1 = 0.0;
        
        ThreeVector scratch1 = mxH[j];
        ThreeVector scratch2 = mxH[j];
        // Note: mxH may be same as dm_dt

        scratch1 *= coef1;   // coef1 == 0 if do_precess if false
        OC_REAL8m mxH_sq = scratch2.MagSq();
        OC_REAL8m dm_dt_sq = mxH_sq*(coef1*coef1+coef2*coef2);
        if(dm_dt_sq>max_dm_dt_sq) {
          max_dm_dt_sq = dm_dt_sq;
        }
        work_dE_dt += mxH_sq * Ms[j] * mesh.Volume(j) * coef2;
        scratch2 ^= work_spin[j]; // ((mxH)xm)
        scratch2 *= coef2;  // -alpha.gamma((mxH)xm) = alpha.gamma(mx(mxH))
        dm_dt_result = scratch1 + scratch2;
      }
      ThreeVector dD = dm_dt[j] = dm_dt_result;
      dD *= work_dc7;
      dD += dD13456[j];
      OC_REAL8m dD_magsq = dD.MagSq();
      if(dD_magsq > work_max_dD_magsq) {
        work_max_dD_magsq = dD_magsq;
      }
    }
  }
  // Thread-level exports
  thread_dE_dt[threadnumber] = work_dE_dt;
  thread_max_dm_dt[threadnumber] = sqrt(max_dm_dt_sq);
  thread_max_dD_magsq[threadnumber] = work_max_dD_magsq;
}

void Oxs_RungeKuttaEvolve::Initialize_Threaded_Calculate_dm_dt
(const Oxs_SimState& work_state,       // Import
 const Oxs_MeshValue<ThreeVector>& mxH,
 Oxs_MeshValue<ThreeVector>& dm_dt,
 _Oxs_RKEvolve_RKFBase54_ThreadD& thread_data,
 OC_REAL8m dc7,
 const Oxs_MeshValue<ThreeVector>& dD13456)
{
  const Oxs_Mesh* mesh = work_state.mesh;
  const int thread_count = Oc_GetMaxThreadCount();

  // Fill out alpha and gamma meshvalue arrays, as necessary.
  if(mesh_id != mesh->Id() || !gamma.CheckMesh(mesh)
     || !alpha.CheckMesh(mesh)) {
    UpdateMeshArrays(mesh);
  }

  thread_data.mesh_ptr = mesh;
  thread_data.Ms_ptr = work_state.Ms;
  thread_data.gamma_ptr = &gamma;
  thread_data.alpha_ptr = &alpha;
  thread_data.mxH_ptr = &mxH;
  thread_data.work_spin_ptr = &work_state.spin;
  thread_data.dm_dt_ptr = &dm_dt;
  thread_data.dc7 = dc7;
  thread_data.dD13456_ptr = &dD13456;
  thread_data.do_precess = do_precess;

  thread_data.job_basket.Init(thread_count,work_state.Ms->GetArrayBlock());

  thread_data.thread_dE_dt.clear();
  thread_data.thread_dE_dt.resize(thread_count,0.0);

  thread_data.thread_max_dm_dt.clear();
  thread_data.thread_max_dm_dt.resize(thread_count,0.0);

  thread_data.thread_max_dD_magsq.clear();
  thread_data.thread_max_dD_magsq.resize(thread_count,0.0);
}

void Oxs_RungeKuttaEvolve::Finalize_Threaded_Calculate_dm_dt
(const _Oxs_RKEvolve_RKFBase54_ThreadD& thread_data, // Import
 OC_REAL8m pE_pt,          // Import
 OC_REAL8m& max_dm_dt,      // Export
 OC_REAL8m& dE_dt,          // Export
 OC_REAL8m& min_timestep_export,  // Export
 OC_REAL8m& max_dD_magsq)   // Export
{
  const OC_INDEX thread_count
    = static_cast<OC_INDEX>(thread_data.thread_dE_dt.size());
  assert(thread_count>=0);

  dE_dt = thread_data.thread_dE_dt[0];
  max_dm_dt = thread_data.thread_max_dm_dt[0];
  max_dD_magsq = thread_data.thread_max_dD_magsq[0];
  for(OC_INDEX i=1;i<thread_count;++i) {
    dE_dt += thread_data.thread_dE_dt[i];
    if(thread_data.thread_max_dm_dt[i]>max_dm_dt) {
      max_dm_dt = thread_data.thread_max_dm_dt[i];
    }
    if(thread_data.thread_max_dD_magsq[i]>max_dD_magsq) {
      max_dD_magsq = thread_data.thread_max_dD_magsq[i];
    }
  }
  dE_dt = -1 * MU0 * dE_dt + pE_pt;
  /// The first term is (partial E/partial M)*dM/dt, the
  /// second term is (partial E/partial t)*dt/dt.  Note that,
  /// provided Ms_[i]>=0, that by constructions dE_dt_sum above
  /// is always non-negative, so dE_dt_ can only be made positive
  /// by positive pE_pt_.

  // Get bound on smallest stepsize that would actually
  // change spin new_max_dm_dt_index:
  min_timestep_export = PositiveTimestepBound(max_dm_dt);
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

#endif // OOMMF_THREADS

// Constructor
Oxs_RungeKuttaEvolve::Oxs_RungeKuttaEvolve(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_TimeEvolver(name,newdtr,argstr),
    mesh_id(0),
    max_step_decrease(0.03125), max_step_increase_limit(4.0),
    max_step_increase_adj_ratio(1.9),
    reject_goal(0.05), reject_ratio(0.05),
#if ZHANG_DAMPING
    zhang_damping(0.0),
    mesh_ispan(0),mesh_jspan(0),mesh_kspan(0),
    mesh_delx_inverse(0.0),mesh_dely_inverse(0.0),mesh_delz_inverse(0.0),
#endif // ZHANG_DAMPING
    energy_state_id(0),next_timestep(0.),
    rkstep_ptr(NULL)
{
  // Process arguments
  min_timestep=GetRealInitValue("min_timestep",0.);
  max_timestep=GetRealInitValue("max_timestep",1e-10);
  if(max_timestep<=0.0) {
    char buf[4096];
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid parameter value:"
                " Specified max time step is %g (should be >0.)",
                static_cast<double>(max_timestep));
    throw Oxs_ExtError(this,buf);
  }

  allowed_error_rate = GetRealInitValue("error_rate",1.0);
  if(allowed_error_rate>0.0) {
    allowed_error_rate *= PI*1e9/180.; // Convert from deg/ns to rad/s
  }
  allowed_absolute_step_error
    = GetRealInitValue("absolute_step_error",0.2);
  if(allowed_absolute_step_error>0.0) {
    allowed_absolute_step_error *= PI/180.; // Convert from deg to rad
  }
  allowed_relative_step_error
    = GetRealInitValue("relative_step_error",0.01);

  expected_energy_precision =
    GetRealInitValue("energy_precision",1e-10);

  reject_goal = GetRealInitValue("reject_goal",0.05);
  if(reject_goal<0.) {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
         " \"reject_goal\" value must be non-negative.");
  }

  min_step_headroom = GetRealInitValue("min_step_headroom",0.33);
  if(min_step_headroom<0.) {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
         " \"min_step_headroom\" value must be bigger than 0.");
  }

  max_step_headroom = GetRealInitValue("max_step_headroom",0.95);
  if(max_step_headroom<0.) {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
         " \"max_step_headroom\" value must be bigger than 0.");
  }

  if(min_step_headroom>max_step_headroom) {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
         " \"min_step_headroom\" value must not be larger than"
         " \"max_step_headroom\".");
  }


  if(HasInitValue("alpha")) {
    OXS_GET_INIT_EXT_OBJECT("alpha",Oxs_ScalarField,alpha_init);
  } else {
    alpha_init.SetAsOwner(dynamic_cast<Oxs_ScalarField *>
                          (MakeNew("Oxs_UniformScalarField",director,
                                   "value 0.5")));
  }

#if ZHANG_DAMPING
  zhang_damping = GetRealInitValue("zhang_damping",0.0);
#endif // ZHANG_DAMPING

  // User may specify either gamma_G (Gilbert) or
  // gamma_LL (Landau-Lifshitz).
  gamma_style = GS_INVALID;
  if(HasInitValue("gamma_G") && HasInitValue("gamma_LL")) {
    throw Oxs_ExtError(this,"Invalid Specify block; "
                         "both gamma_G and gamma_LL specified.");
  } else if(HasInitValue("gamma_G")) {
    gamma_style = GS_G;
    OXS_GET_INIT_EXT_OBJECT("gamma_G",Oxs_ScalarField,gamma_init);
  } else if(HasInitValue("gamma_LL")) {
    gamma_style = GS_LL;
    OXS_GET_INIT_EXT_OBJECT("gamma_LL",Oxs_ScalarField,gamma_init);
  } else {
    gamma_style = GS_G;
    gamma_init.SetAsOwner(dynamic_cast<Oxs_ScalarField *>
                          (MakeNew("Oxs_UniformScalarField",director,
                                   "value -2.211e5")));
  }
  allow_signed_gamma = GetIntInitValue("allow_signed_gamma",0);
  do_precess = GetIntInitValue("do_precess",1);

  start_dm = GetRealInitValue("start_dm",0.01);
  start_dm *= PI/180.; // Convert from deg to rad

  start_dt = GetRealInitValue("start_dt",max_timestep/8.);

  if(start_dm<0. && start_dt<0.) {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
                       " at least one of \"start_dm\" and \"start_dt\""
                       "  must be nonnegative.");
  }

  stage_init_step_control = SISC_AUTO;  // Safety
  String stage_start = GetStringInitValue("stage_start","auto");
  Oxs_ToLower(stage_start);
  if(stage_start.compare("start_conditions")==0) {
    stage_init_step_control = SISC_START_COND;
  } else if(stage_start.compare("continuous")==0) {
    stage_init_step_control = SISC_CONTINUOUS;
  } else if(stage_start.compare("auto")==0
            || stage_start.compare("automatic")==0) {
    stage_init_step_control = SISC_AUTO;
  } else {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
                         " \"stage_start\" value must be one of"
                         " start_conditions, continuous, or auto.");
  }

  String method = GetStringInitValue("method","rkf54");
  Oxs_ToLower(method); // Do case insensitive match
  if(method.compare("rk2")==0) {
    rkstep_ptr = &Oxs_RungeKuttaEvolve::TakeRungeKuttaStep2;
  } else if(method.compare("rk2heun")==0) {
    rkstep_ptr = &Oxs_RungeKuttaEvolve::TakeRungeKuttaStep2Heun;
  } else if(method.compare("rk4")==0) {
    rkstep_ptr = &Oxs_RungeKuttaEvolve::TakeRungeKuttaStep4;
  } else if(method.compare("rkf54m")==0) {
    rkstep_ptr = &Oxs_RungeKuttaEvolve::TakeRungeKuttaFehlbergStep54M;
  } else if(method.compare("rkf54s")==0) {
    rkstep_ptr = &Oxs_RungeKuttaEvolve::TakeRungeKuttaFehlbergStep54S;
  } else if(method.compare("rkf54")==0) {
    rkstep_ptr = &Oxs_RungeKuttaEvolve::TakeRungeKuttaFehlbergStep54;
  } else {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
                         " \"method\" value must be one of"
                         " rk2, rk2heun, rk4, rkf54, rkf54m, or rkf54s.");
  }

  // Setup outputs
  max_dm_dt_output.Setup(this,InstanceName(),"Max dm/dt","deg/ns",0,
     &Oxs_RungeKuttaEvolve::UpdateDerivedOutputs);
  dE_dt_output.Setup(this,InstanceName(),"dE/dt","J/s",0,
     &Oxs_RungeKuttaEvolve::UpdateDerivedOutputs);
  delta_E_output.Setup(this,InstanceName(),"Delta E","J",0,
     &Oxs_RungeKuttaEvolve::UpdateDerivedOutputs);
  dm_dt_output.Setup(this,InstanceName(),"dm/dt","rad/s",1,
     &Oxs_RungeKuttaEvolve::UpdateDerivedOutputs);
  mxH_output.Setup(this,InstanceName(),"mxH","A/m",1,
     &Oxs_RungeKuttaEvolve::UpdateDerivedOutputs);

  max_dm_dt_output.Register(director,-5);
  dE_dt_output.Register(director,-5);
  delta_E_output.Register(director,-5);
  dm_dt_output.Register(director,-5);
  mxH_output.Register(director,-5);

  // dm_dt and mxH output caches are used for intermediate storage,
  // so enable caching.
  dm_dt_output.CacheRequestIncrement(1);
  mxH_output.CacheRequestIncrement(1);

  VerifyAllInitArgsUsed();

  // Reserve space for temp_state; see Step() method below
  director->ReserveSimulationStateRequest(1);

#if REPORT_TIME_RKDEVEL
  timer.resize(10);
  timer_counts.resize(10);
#endif
}

OC_BOOL Oxs_RungeKuttaEvolve::Init()
{
  Oxs_TimeEvolver::Init();

#if REPORT_TIME_RKDEVEL
  Oc_TimeVal cpu,wall;
  for(unsigned int ti=0;ti<timer.size();++ti) {
    timer[ti].GetTimes(cpu,wall);
    if(double(wall)>0.0) {
      fprintf(stderr,"               timer %2u ...   %7.2f cpu /%7.2f wall,"
              " (%s/%s)\n",
              ti,double(cpu),double(wall),InstanceName(),
              timer_counts[ti].name.c_str());
      if(timer_counts[ti].pass_count>0) {
        fprintf(stderr,"                \\---> passes = %d,"
                " bytes=%.2f MB, %.2f GB/sec\n",
                timer_counts[ti].pass_count,
                double(timer_counts[ti].bytes)/double(1024*1024),
                double(timer_counts[ti].bytes)
                /(double(1024*1024*1024)*double(wall)));
      }
    }
    timer[ti].Reset(); 
    timer_counts[ti].Reset();
  }
#endif // REPORT_TIME_RKDEVEL

  // Free scratch space allocated by previous problem (if any)
  vtmpA.Release();   vtmpB.Release();
  vtmpC.Release();   vtmpD.Release();

  // Free memory used by LLG gamma and alpha
  mesh_id = 0;
  alpha.Release();
  gamma.Release();

  max_step_increase = max_step_increase_limit;

  // Setup step_headroom and reject_ratio
  step_headroom = max_step_headroom;
  reject_ratio = reject_goal;

  energy_state_id=0;   // Mark as invalid state
  next_timestep=0.;    // Dummy value
  return 1;
}


Oxs_RungeKuttaEvolve::~Oxs_RungeKuttaEvolve()
{
#if REPORT_TIME_RKDEVEL
  Oc_TimeVal cpu,wall;
  for(unsigned int ti=0;ti<timer.size();++ti) {
    timer[ti].GetTimes(cpu,wall);
    if(double(wall)>0.0) {
      fprintf(stderr,"               timer %2u ...   %7.2f cpu /%7.2f wall,"
              " (%s/%s)\n",
              ti,double(cpu),double(wall),InstanceName(),
              timer_counts[ti].name.c_str());
      if(timer_counts[ti].pass_count>0) {
        fprintf(stderr,"                \\---> passes = %d,"
                " bytes=%.2f MB, %.2f GB/sec\n",
                timer_counts[ti].pass_count,
                double(timer_counts[ti].bytes)/double(1024*1024),
                double(timer_counts[ti].bytes)
                /(double(1024*1024*1024)*double(wall)));
      }
    }
 }
#endif // REPORT_TIME_RKDEVEL
}

void Oxs_RungeKuttaEvolve::UpdateMeshArrays(const Oxs_Mesh* mesh)
{
  mesh_id = 0; // Mark update in progress
  const OC_INDEX size = mesh->Size();
  OC_INDEX i;

  alpha_init->FillMeshValue(mesh,alpha);
  gamma_init->FillMeshValue(mesh,gamma);
#ifdef NOAH
  fprintf(stderr,"alpha[  1]=%25.16e\nalpha[256]=%25.16e\n",alpha[1],alpha[256]); /**/
#endif
  if(gamma_style == GS_G) { // Convert to LL form
    for(i=0;i<size;++i) {
      OC_REAL8m cell_alpha = alpha[i];
      gamma[i] /= (1+cell_alpha*cell_alpha);
    }
  }
  if(!allow_signed_gamma) {
    for(i=0;i<size;++i) gamma[i] = -1*fabs(gamma[i]);
  }

#if ZHANG_DAMPING
  if(zhang_damping != 0.0) {
    // Zhang damping takes derivaties of m, and for that we assume a
    // rectangular mesh.  We could work with either periodic or
    // non-periodic meshes, but for first pass just do the non-periodic
    // case.  If this code gets extended to periodic case, then the
    // upcast goes to Oxs_CommonRectangularMesh, and we need to set
    // flags to tell the delm code to handle boundaries appropriately.
    const Oxs_RectangularMesh* rmesh 
      = dynamic_cast<const Oxs_RectangularMesh*>(mesh);
    if(rmesh==NULL) {
      String msg=String("Zhang damping code requires a standard"
                        " rectangular mesh, but mesh object ")
        + String(mesh->InstanceName())
        + String(" is not a rectangular mesh.");
      throw Oxs_ExtError(this,msg);
    }
    mesh_ispan = rmesh->DimX();
    mesh_jspan = rmesh->DimY();
    mesh_kspan = rmesh->DimZ();
    mesh_delx_inverse = 1.0/rmesh->EdgeLengthX();
    mesh_dely_inverse = 1.0/rmesh->EdgeLengthY();
    mesh_delz_inverse = 1.0/rmesh->EdgeLengthZ();
  }
#endif // ZHANG_DAMPING

  mesh_id = mesh->Id();
}

OC_REAL8m
Oxs_RungeKuttaEvolve::PositiveTimestepBound
(OC_REAL8m max_dm_dt)
{ // Computes an estimate on the minimum time needed to
  // change the magnetization state, subject to floating
  // points limits.
  OC_REAL8m min_timestep = DBL_MAX/64.;
  if(max_dm_dt>1 || OC_REAL8_EPSILON<min_timestep*max_dm_dt) {
    min_timestep = OC_REAL8_EPSILON/max_dm_dt;
    // A timestep of size min_timestep will be hopelessly lost
    // in roundoff error.  So increase a bit, based on an empirical
    // fudge factor.  This fudge factor can be tested by running a
    // problem with start_dm = 0.  If the evolver can't climb its
    // way out of the stepsize=0 hole, then this fudge factor is too
    // small.  So far, the most challenging examples have been
    // problems with small cells with nearly aligned spins, e.g., in
    // a remanent state with an external field is applied at t=0.
    // Damping ratio doesn't seem to have much effect, either way.
    min_timestep *= 64;
  } else {
    // Degenerate case: max_dm_dt_ must be exactly or very nearly
    // zero.  Punt.
    min_timestep = 1.0;
  }
  return min_timestep;
}

#if ZHANG_DAMPING
void Oxs_RungeKuttaEvolve::Compute_delm
(const Oxs_SimState& state_,  // Import
 const OC_INDEX mi,           // Import
 const OC_INDEX mj,           // Import
 const OC_INDEX mk,           // Import
 const OC_INDEX i,            // Import
 ThreeVector& D1,             // Export; 1st row of Dab
 ThreeVector& D2,             // Export; 2nd row of Dab
 ThreeVector& D3) const       // Export; 3rd row of Dab
{
  const Oxs_MeshValue<ThreeVector>& spin = state_.spin;
  const Oxs_MeshValue<OC_REAL8m>&     Ms = *(state_.Ms);
  const OC_REAL8m& idelx = mesh_delx_inverse;
  const OC_REAL8m& idely = mesh_dely_inverse;
  const OC_REAL8m& idelz = mesh_delz_inverse;
  const OC_INDEX yoff = mesh_ispan;
  const OC_INDEX zoff = mesh_ispan*mesh_jspan;

  if(Ms[i]==0.0) {
    D1 = D2 = D3.Set(0.0,0.0,0.0);
    return;
  }

  // Compute dm/dx
  ThreeVector dm_dx;
  if(mi+1<mesh_ispan && Ms[i+1]!=0.0) dm_dx  = spin[i+1];
  else                                dm_dx  = spin[i];
  if(mi>0 && Ms[i-1]!=0.0)            dm_dx -= spin[i-1];
  else                                dm_dx -= spin[i];
  dm_dx *= 0.5*idelx;  // The 0.5 factor is correct in all cases;
  /// if mi is at an edge, then the dm_dn=0 boundary constraint is
  /// implemented by reflecting the edge spin across the boundary.  We
  /// might want to add some code here to check boundaries interior to
  /// the simulation, i.e., spots where Ms=0.
  dm_dx ^= spin[i];  // Prepare for Dab computation

  // Compute dm/dy
  ThreeVector dm_dy;
  if(mj+1<mesh_jspan && Ms[i+yoff]!=0.0) dm_dy  = spin[i+yoff];
  else                                   dm_dy  = spin[i];
  if(mj>0 && Ms[i-yoff]!=0.0)            dm_dy -= spin[i-yoff];
  else                                   dm_dy -= spin[i];
  dm_dy *= 0.5*idely;  // The 0.5 factor is correct in all cases, as above.
  dm_dy ^= spin[i];  // Prepare for Dab computation

  // Compute dm/dz
  ThreeVector dm_dz;
  if(mk+1<mesh_kspan && Ms[i+zoff]!=0.0) dm_dz  = spin[i+zoff];
  else                                   dm_dz  = spin[i];
  if(mk>0 && Ms[i-zoff]!=0.0)            dm_dz -= spin[i-zoff];
  else                                   dm_dz -= spin[i];
  dm_dz *= 0.5*idelz;  // The 0.5 factor is correct in all cases, as above.
  dm_dz ^= spin[i];  // Prepare for Dab computation

  D1.Set(dm_dx.x*dm_dx.x + dm_dy.x*dm_dy.x + dm_dz.x*dm_dz.x,
         dm_dx.x*dm_dx.y + dm_dy.x*dm_dy.y + dm_dz.x*dm_dz.y,
         dm_dx.x*dm_dx.z + dm_dy.x*dm_dy.z + dm_dz.x*dm_dz.z);

  D2.Set(D1.y,
         dm_dx.y*dm_dx.y + dm_dy.y*dm_dy.y + dm_dz.y*dm_dz.y,
         dm_dx.y*dm_dx.z + dm_dy.y*dm_dy.z + dm_dz.y*dm_dz.z);

  D3.Set(D1.z,
         D2.z,
         dm_dx.z*dm_dx.z + dm_dy.z*dm_dy.z + dm_dz.z*dm_dz.z);
}
#endif // ZHANG_DAMPING


void Oxs_RungeKuttaEvolve::Calculate_dm_dt
(const Oxs_SimState& state_,
 const Oxs_MeshValue<ThreeVector>& mxH_,
 OC_REAL8m pE_pt_,
 Oxs_MeshValue<ThreeVector>& dm_dt_,
 OC_REAL8m& max_dm_dt_,OC_REAL8m& dE_dt_,OC_REAL8m& min_timestep_export)
{ // Imports: state, mxH_, pE_pt_
  // Exports: dm_dt_, max_dm_dt_, dE_dt_
  // NOTE: dm_dt_ is allowed, and in fact is encouraged,
  //   to be the same as mxH_.  In this case, mxH_ is
  //   overwritten by dm_dt on return.
  const Oxs_Mesh* mesh_ = state_.mesh;
  const Oxs_MeshValue<OC_REAL8m>& Ms_ = *(state_.Ms);
  const Oxs_MeshValue<ThreeVector>& spin_ = state_.spin;
  const OC_INDEX size = mesh_->Size(); // Assume import data are compatible

  // Fill out alpha and gamma meshvalue arrays, as necessary.
  if(mesh_id != mesh_->Id() || !gamma.CheckMesh(mesh_)
     || !alpha.CheckMesh(mesh_)) {
    UpdateMeshArrays(mesh_);
  }

  dm_dt_.AdjustSize(mesh_);  // For case &dm_dt_ != &mxH_

  OC_REAL8m dE_dt_sum=0.0;
  OC_REAL8m max_dm_dt_sq = 0.0;
#if ZHANG_DAMPING
  if(zhang_damping == 0.0) {
#endif // ZHANG_DAMPING
    // Canonical case: no Zhang damping
    OC_INDEX i;
    for(i=0;i<size;i++) {
      if(Ms_[i]==0.0) {
        dm_dt_[i].Set(0,0,0);
      } else {
        OC_REAL8m coef1 = gamma[i];
        OC_REAL8m coef2 = -1*alpha[i]*coef1;
        if(!do_precess) coef1 = 0.0;

        ThreeVector scratch1 = mxH_[i];
        ThreeVector scratch2 = mxH_[i];
        // Note: mxH may be same as dm_dt

        scratch1 *= coef1;   // coef1 == 0 if do_precess if false
        OC_REAL8m mxH_sq = scratch2.MagSq();
        OC_REAL8m dm_dt_sq = mxH_sq*(coef1*coef1+coef2*coef2);
        if(dm_dt_sq>max_dm_dt_sq) {
          max_dm_dt_sq = dm_dt_sq;
        }
        dE_dt_sum += mxH_sq * Ms_[i] * mesh_->Volume(i) * coef2;
        scratch2 ^= spin_[i]; // ((mxH)xm)
        scratch2 *= coef2;  // -alpha.gamma((mxH)xm) = alpha.gamma(mx(mxH))
        dm_dt_[i] = scratch1 + scratch2;
      }
    }
#if ZHANG_DAMPING
  } else {
    // Zhang damping enabled.  In this setting the mesh must be rectangular
    //
    // Zhang damping is a damping term added to LLG that depends
    // directly on the local magnetization configuration.  See
    //
    //   Shufeng Zhang and Steven S.-L. Zhang, "Generalization of the
    //   Landau-Lifshitz-Gilbert equation for conducting
    //   ferromagnetics," Physical Review Letters, 102, 086601 (2009).
    //
    // and also NOTES VI, 1-June-2012, p 40-41.
    ThreeVector D1,D2,D3;
    OC_INDEX i=0;
    for(OC_INDEX mk=0;mk<mesh_kspan;++mk) {
      for(OC_INDEX mj=0;mj<mesh_jspan;++mj) {
        for(OC_INDEX mi=0;mi<mesh_ispan;++mi) {
          if(Ms_[i]==0.0) {
            dm_dt_[i].Set(0,0,0);
          } else {
            OC_REAL8m coef1 = gamma[i];
            OC_REAL8m coef2 = -1*alpha[i]*coef1;
            OC_REAL8m cz = -1*zhang_damping*coef1;
            if(!do_precess) coef1 = 0.0;

            ThreeVector scratch1 = mxH_[i];

            // Zhang damping term
            Compute_delm(state_,mi,mj,mk,i,D1,D2,D3);
            ThreeVector damp_term(cz*(D1*scratch1),
                                  cz*(D2*scratch1),
                                  cz*(D3*scratch1));

            // Normal damping
            damp_term.Accum(coef2,scratch1);
            
            // Energy loss (see NOTES VI, 1-June-2012, p41)
            dE_dt_sum += Ms_[i] * mesh_->Volume(i)
              * (damp_term * scratch1);

            // Compute damping contribution to dm_dt
            damp_term ^= spin_[i];

            // Precession contribution, if any
            scratch1 *= coef1;   // coef1 == 0 if do_precess if false

            dm_dt_[i] = scratch1 + damp_term;

            OC_REAL8m dm_dt_sq = dm_dt_[i].MagSq();
            if(dm_dt_sq>max_dm_dt_sq) {
              max_dm_dt_sq = dm_dt_sq;
            }
          }
          ++i;
        }
      }
    }
  }
#endif // ZHANG_DAMPING
  max_dm_dt_ = sqrt(max_dm_dt_sq);
  dE_dt_ = -1 * MU0 * dE_dt_sum + pE_pt_;
  /// The first term is (partial E/partial M)*dM/dt, the
  /// second term is (partial E/partial t)*dt/dt.  Note that,
  /// provided Ms_[i]>=0, that by constructions dE_dt_sum above
  /// is always non-negative, so dE_dt_ can only be made positive
  /// by positive pE_pt_.

  // Get bound on smallest stepsize that would actually
  // change spin new_max_dm_dt_index:
  min_timestep_export = PositiveTimestepBound(max_dm_dt_);

  return;
}

void Oxs_RungeKuttaEvolve::CheckCache(const Oxs_SimState& cstate)
{
  // Pull cached values out from cstate.
  // If cstate.Id() == energy_state_id, then cstate has been run
  // through either this method or UpdateDerivedOutputs.  Either
  // way, all derived state data should be stored in cstate,
  // except currently the "energy" mesh value array, which is
  // stored independently inside *this.  Eventually that should
  // probably be moved in some fashion into cstate too.
  if(energy_state_id != cstate.Id()) {
    // cached data out-of-date
    UpdateDerivedOutputs(cstate);
  }
  OC_BOOL cache_good = 1;
  OC_REAL8m max_dm_dt,dE_dt,delta_E,pE_pt;
  OC_REAL8m timestep_lower_bound;  // Smallest timestep that can actually
  /// change spin with max_dm_dt (due to OC_REAL8_EPSILON restrictions).
  /// The next timestep is based on the error from the last step.  If
  /// there is no last step (either because this is the first step,
  /// or because the last state handled by this routine is different
  /// from the incoming current_state), then timestep is calculated
  /// so that max_dm_dt * timestep = start_dm.

  cache_good &= cstate.GetDerivedData("Max dm/dt",max_dm_dt);
  cache_good &= cstate.GetDerivedData("dE/dt",dE_dt);
  cache_good &= cstate.GetDerivedData("Delta E",delta_E);
  cache_good &= cstate.GetDerivedData("pE/pt",pE_pt);
  cache_good &= cstate.GetDerivedData("Timestep lower bound",
                                      timestep_lower_bound);
  cache_good &= (energy_state_id == cstate.Id());
  cache_good &= (dm_dt_output.cache.state_id == cstate.Id());

  if(!cache_good) {
    throw Oxs_ExtError(this,
       "Oxs_RungeKuttaEvolve::CheckCache: Invalid data cache.");
  }
}

#if !OOMMF_THREADS
void
Oxs_RungeKuttaEvolve::AdjustState
(OC_REAL8m hstep,
 OC_REAL8m mstep,
 const Oxs_SimState& old_state,
 const Oxs_MeshValue<ThreeVector>& dm_dt,
 Oxs_SimState& new_state,
 OC_REAL8m& norm_error) const
{
  new_state.ClearDerivedData();

  const Oxs_MeshValue<ThreeVector>& old_spin = old_state.spin;
  Oxs_MeshValue<ThreeVector>& new_spin = new_state.spin;
  
  if(!dm_dt.CheckMesh(old_state.mesh)) {
    throw Oxs_ExtError(this,
                         "Oxs_RungeKuttaEvolve::AdjustState:"
                         " Import spin and dm_dt are different sizes.");
  }
  new_spin.AdjustSize(old_state.mesh);
  const OC_INDEX size = old_state.mesh->Size();

  OC_REAL8m min_normsq = DBL_MAX;
  OC_REAL8m max_normsq = 0.0;
  ThreeVector tempspin;
  OC_INDEX i;
  for(i=0;i<size;++i) {
    tempspin = old_spin[i];
    tempspin.Accum(mstep,dm_dt[i]);
    OC_REAL8m magsq = tempspin.MakeUnit();
    new_spin[i] = tempspin;
    if(magsq<min_normsq) min_normsq=magsq;
    if(magsq>max_normsq) max_normsq=magsq;
  }
  norm_error = OC_MAX(sqrt(max_normsq)-1.0,
                      1.0 - sqrt(min_normsq));

  // Adjust time and iteration fields in new_state
  new_state.last_timestep=hstep;
  if(old_state.stage_number != new_state.stage_number) {
    // New stage
    new_state.stage_start_time = old_state.stage_start_time
                                + old_state.stage_elapsed_time;
    new_state.stage_elapsed_time = new_state.last_timestep;
  } else {
    new_state.stage_start_time = old_state.stage_start_time;
    new_state.stage_elapsed_time = old_state.stage_elapsed_time
                                  + new_state.last_timestep;
  }

  // Don't touch iteration counts. (?!)  The problem is that one call
  // to Oxs_RungeKuttaEvolve::Step() takes 2 half-steps, and it is the
  // result from these half-steps that are used as the export state.
  // If we increment the iteration count each time through here, then
  // the iteration count goes up by 2 for each call to Step().  So
  // instead, we leave iteration count at whatever value was filled
  // in by the Oxs_RungeKuttaEvolve::NegotiateTimeStep() method.
}

#else // !OOMMF_THREADS

class _Oxs_RKEvolve_RKFBase54_AdjustState : public Oxs_ThreadRunObj {
public:
  // Note: The job basket is static, so only one "set" (tree) of this
  // class may be run at any one time.
  static Oxs_JobControl<ThreeVector> job_basket;

  const Oxs_MeshValue<ThreeVector>* dm_dt_ptr;    // Import
  const Oxs_MeshValue<ThreeVector>* old_spin_ptr; // Import
  Oxs_MeshValue<ThreeVector>* new_spin_ptr;       // Export
  vector<OC_REAL8m> norm_error;                   // Export
  OC_REAL8m mstep;                                // Import
  
  _Oxs_RKEvolve_RKFBase54_AdjustState(int thread_count,
                        const Oxs_StripedArray<ThreeVector>* arrblock)
          : dm_dt_ptr(0), old_spin_ptr(0), new_spin_ptr(0),
            norm_error(thread_count),mstep(0.0) {
    job_basket.Init(thread_count,arrblock);
  }

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<ThreeVector> _Oxs_RKEvolve_RKFBase54_AdjustState::job_basket;


void _Oxs_RKEvolve_RKFBase54_AdjustState::Cmd(int threadnumber,
                                              void* /* data */)
{ // Imports
  const Oxs_MeshValue<ThreeVector>& dm_dt = *dm_dt_ptr;
  const Oxs_MeshValue<ThreeVector>& old_spin = *old_spin_ptr;

  // Exports
  Oxs_MeshValue<ThreeVector>& new_spin = *new_spin_ptr;

  // Support variables
  OC_REAL8m min_normsq = DBL_MAX;
  OC_REAL8m max_normsq = 0.0;

  OC_REAL8m min_normsq0 = DBL_MAX;
  OC_REAL8m max_normsq0 = 0.0;

  OC_REAL8m min_normsq1 = DBL_MAX;
  OC_REAL8m max_normsq1 = 0.0;

  OC_REAL8m tstep = mstep;  // Local copy

  while(1) {
    OC_INDEX jstart,jstop;
    job_basket.GetJob(threadnumber,jstart,jstop);
    if(jstart>=jstop) break; // No more jobs

    OC_INDEX j;

    for(j=jstart; j < jstart + (jstop-jstart)%2 ; ++j) {
      ThreeVector tempspin = old_spin[j];
      tempspin.Accum(tstep,dm_dt[j]);
      OC_REAL8m magsq = tempspin.MakeUnit();
      if(magsq<min_normsq) min_normsq=magsq;
      if(magsq>max_normsq) max_normsq=magsq;
      new_spin[j] = tempspin;
    }

    for(;j<jstop;j+=2) {
      ThreeVector tempspin0 = old_spin[j];
      tempspin0.Accum(tstep,dm_dt[j]);
      OC_REAL8m magsq0 = tempspin0.MakeUnit();

      ThreeVector tempspin1 = old_spin[j+1];
      tempspin1.Accum(tstep,dm_dt[j+1]);
      OC_REAL8m magsq1 = tempspin1.MakeUnit();

      new_spin[j]   = tempspin0;
      new_spin[j+1] = tempspin1;

      if(magsq0<min_normsq0) min_normsq0=magsq0;
      if(magsq0>max_normsq0) max_normsq0=magsq0;

      if(magsq1<min_normsq1) min_normsq1=magsq1;
      if(magsq1>max_normsq1) max_normsq1=magsq1;
    }
  }

  if(max_normsq<max_normsq0) max_normsq = max_normsq0;
  if(max_normsq<max_normsq1) max_normsq = max_normsq1;
  OC_REAL8m max_norm = sqrt(max_normsq);

  if(min_normsq>min_normsq0) min_normsq = min_normsq0;
  if(min_normsq>min_normsq1) min_normsq = min_normsq1;
  OC_REAL8m min_norm = sqrt(min_normsq);

  norm_error[threadnumber] = OC_MAX(max_norm-1.0,1.0 - min_norm);
}

void
Oxs_RungeKuttaEvolve::AdjustState  // Threaded version
(OC_REAL8m hstep,
 OC_REAL8m mstep,
 const Oxs_SimState& old_state,
 const Oxs_MeshValue<ThreeVector>& dm_dt,
 Oxs_SimState& new_state,
 OC_REAL8m& norm_error) const
{
  new_state.ClearDerivedData();

  const Oxs_MeshValue<ThreeVector>& old_spin = old_state.spin;
  Oxs_MeshValue<ThreeVector>& new_spin = new_state.spin;
  
  if(!dm_dt.CheckMesh(old_state.mesh)) {
    throw Oxs_ExtError(this,
                         "Oxs_RungeKuttaEvolve::AdjustState:"
                         " Import spin and dm_dt are different sizes.");
  }
  new_spin.AdjustSize(old_state.mesh);

  const int thread_count = Oc_GetMaxThreadCount();
  _Oxs_RKEvolve_RKFBase54_AdjustState
    threadAdjustState(thread_count,old_spin.GetArrayBlock());

  threadAdjustState.dm_dt_ptr = &dm_dt;
  threadAdjustState.old_spin_ptr = &old_spin;
  threadAdjustState.new_spin_ptr = &new_spin;
  threadAdjustState.mstep = mstep;

  Oxs_ThreadTree threadtree;
  threadtree.LaunchTree(threadAdjustState,0);

  norm_error = threadAdjustState.norm_error[0];
  for(int i=1;i<thread_count;++i) {
    if(norm_error < threadAdjustState.norm_error[i]) {
      norm_error = threadAdjustState.norm_error[i];
    }
  }

  // Adjust time and iteration fields in new_state
  new_state.last_timestep=hstep;
  if(old_state.stage_number != new_state.stage_number) {
    // New stage
    new_state.stage_start_time = old_state.stage_start_time
                                + old_state.stage_elapsed_time;
    new_state.stage_elapsed_time = new_state.last_timestep;
  } else {
    new_state.stage_start_time = old_state.stage_start_time;
    new_state.stage_elapsed_time = old_state.stage_elapsed_time
                                  + new_state.last_timestep;
  }

  // Don't touch iteration counts. (?!)  The problem is that one call
  // to Oxs_RungeKuttaEvolve::Step() takes 2 half-steps, and it is the
  // result from these half-steps that are used as the export state.
  // If we increment the iteration count each time through here, then
  // the iteration count goes up by 2 for each call to Step().  So
  // instead, we leave iteration count at whatever value was filled
  // in by the Oxs_RungeKuttaEvolve::NegotiateTimeStep() method.
}
#endif // OOMMF_THREADS

void Oxs_RungeKuttaEvolve::UpdateTimeFields
(const Oxs_SimState& cstate,
 Oxs_SimState& nstate,
 OC_REAL8m stepsize) const
{
  nstate.last_timestep=stepsize;
  if(cstate.stage_number != nstate.stage_number) {
    // New stage
    nstate.stage_start_time = cstate.stage_start_time
                              + cstate.stage_elapsed_time;
    nstate.stage_elapsed_time = stepsize;
  } else {
    nstate.stage_start_time = cstate.stage_start_time;
    nstate.stage_elapsed_time = cstate.stage_elapsed_time
                                + stepsize;
  }
}

void Oxs_RungeKuttaEvolve::NegotiateTimeStep
(const Oxs_TimeDriver* driver,
 const Oxs_SimState&  cstate,
 Oxs_SimState& nstate,
 OC_REAL8m stepsize,
 OC_BOOL use_start_cond,
 OC_BOOL& force_step,
 OC_BOOL& driver_set_step) const
{ // This routine negotiates with driver over the proper step size.
  // As a side-effect, also initializes the nstate data structure,
  // where nstate is the "next state".

  // Pull needed cached values out from cstate.
  OC_REAL8m max_dm_dt;
  if(!cstate.GetDerivedData("Max dm/dt",max_dm_dt)) {
    throw Oxs_ExtError(this,
       "Oxs_RungeKuttaEvolve::NegotiateTimeStep: max_dm_dt not cached.");
  }
  OC_REAL8m timestep_lower_bound=0.;  // Smallest timestep that can actually
  /// change spin with max_dm_dt (due to OC_REAL8_EPSILON restrictions).
  if(!cstate.GetDerivedData("Timestep lower bound",
                            timestep_lower_bound)) {
    throw Oxs_ExtError(this,
       "Oxs_RungeKuttaEvolve::NegotiateTimeStep: "
       " timestep_lower_bound not cached.");
  }
  if(timestep_lower_bound<=0.0) {
    throw Oxs_ExtError(this,
       "Oxs_RungeKuttaEvolve::NegotiateTimeStep: "
       " cached timestep_lower_bound value not positive.");
  }

  // The next timestep is based on the error from the last step.  If
  // there is no last step (either because this is the first step,
  // or because the last state handled by this routine is different
  // from the incoming current_state), then timestep is calculated
  // so that max_dm_dt * timestep = start_dm or
  // timestep = start_dt.
  if(use_start_cond || stepsize<=0.0) {
    if(start_dm>=0.0) {
      if(start_dm < sqrt(DBL_MAX/4) * max_dm_dt) {
        stepsize = step_headroom * start_dm / max_dm_dt;
      } else {
        stepsize = sqrt(DBL_MAX/4);
      }
    }
    if(start_dt>=0.0) {
      if(start_dm<0.0 || stepsize>start_dt) {
        stepsize = start_dt;
      }
    }
  }

  // Insure step is not outside requested step bounds
  if(!use_start_cond && stepsize<timestep_lower_bound) {
    // Check for this before max_timestep, so max_timestep can
    // override.  On the one hand, if the timestep is too small to
    // move any spins, then taking the step just advances the
    // simulation time without changing the state; instead what would
    // be small accumulations are just lost.  On the other hand, if
    // the applied field is changing with time, then perhaps all that
    // is needed to get the magnetization moving is to advance the
    // simulation time.  In general, it is hard to tell which is
    // which, so we assume that the user knews what he was doing when
    // he set the max_timestep value (or accepted the default), and it
    // is up to him to notice if the simulation time is advancing
    // without any changes to the magnetization pattern.
    stepsize = timestep_lower_bound;
  }
  if(stepsize>max_timestep) stepsize = max_timestep;
  if(stepsize<min_timestep) stepsize = min_timestep;

  // Negotiate with driver over size of next step
  driver->FillStateMemberData(cstate,nstate);
  UpdateTimeFields(cstate,nstate,stepsize);

  // Update iteration count
  nstate.iteration_count = cstate.iteration_count + 1;
  nstate.stage_iteration_count = cstate.stage_iteration_count + 1;

  // Additional timestep control
  driver->FillStateSupplemental(nstate);

  // Check for forced step.
  // Note: The driver->FillStateSupplemental call may increase the
  //       timestep slightly to match a stage time termination
  //       criteria.  We should tweak the timestep size check
  //       to recognize that changes smaller than a few epsilon
  //       of the stage time are below our effective timescale
  //       resolution.
  force_step = 0;
  OC_REAL8m timestepcheck = nstate.last_timestep
                         - 4*OC_REAL8_EPSILON*nstate.stage_elapsed_time;
  if(timestepcheck<=min_timestep || timestepcheck<=timestep_lower_bound) {
    // Either driver wants to force stepsize, or else step size can't
    // be reduced any further.
    force_step=1;
  }

  // Is driver responsible for stepsize?
  if(nstate.last_timestep == stepsize) driver_set_step = 0;
  else                                 driver_set_step = 1;
}


OC_BOOL
Oxs_RungeKuttaEvolve::CheckError
(OC_REAL8m global_error_order,
 OC_REAL8m error,
 OC_REAL8m stepsize,
 OC_REAL8m reference_stepsize,
 OC_REAL8m max_dm_dt,
 OC_REAL8m& new_stepsize)
{ // Returns 1 if step is good, 0 if error is too large.
  // Export new_stepsize is set to suggested stepsize
  // for next step.
  //
  // new_stepsize is initially filled with a relative stepsize
  // adjustment ratio, e.g., 1.0 means no change in stepsize.
  // At the end of this routine this ratio is multiplied by
  // stepsize to get the actual absolute stepsize.
  //
  // The import stepsize is the size (in seconds) of the actual
  // step.  The new_stepsize is computed from this based on the
  // various error estimates.  An upper bound is placed on the
  // size of new_stepsize relative to the imports stepsize and
  // reference_stepsize.  reference_stepsize has no effect if
  // it is smaller than max_step_increase*stepsize.  It is
  // usually used only in the case where the stepsize was
  // artificially reduced by, for example, a stage stopping
  // criterion.
  //
  // NOTE: This routine assumes the local error order is
  //     global_error_order + 1.
  //
  // NOTE: A copy of this routine lives in eulerevolve.cc.  Updates
  //     should be copied there.

  OC_BOOL good_step = 1;
  OC_BOOL error_checked=0;

  if(allowed_relative_step_error>=0. || allowed_error_rate>=0.) {
    // Determine tighter rate bound.
    OC_REAL8m rate_error = 0.0;
    if(allowed_relative_step_error<0.) {
      rate_error = allowed_error_rate;
    } else if(allowed_error_rate<0.) {
      rate_error = allowed_relative_step_error * max_dm_dt;
    } else {
      rate_error = allowed_relative_step_error * max_dm_dt;
      if(rate_error>allowed_error_rate) {
        rate_error = allowed_error_rate;
      }
    }
    rate_error *= stepsize;

    // Rate check
    if(error>rate_error) {
      good_step = 0;
      new_stepsize = pow(rate_error/error,1.0/global_error_order);
    } else {
      OC_REAL8m ratio = 0.125*DBL_MAX;
      if(error>=1 || rate_error<ratio*error) {
        OC_REAL8m test_ratio = rate_error/error;
        if(test_ratio<ratio) ratio = test_ratio;
      }
      new_stepsize = pow(ratio,1.0/global_error_order);
    }
    error_checked = 1;
  }

  // Absolute error check
  if(allowed_absolute_step_error>=0.0) {
    OC_REAL8m test_stepsize = 0.0;
    OC_REAL8m local_error_order = global_error_order + 1.0;
    if(error>allowed_absolute_step_error) {
      good_step = 0;
      test_stepsize = pow(allowed_absolute_step_error/error,
                          1.0/local_error_order);
    } else {
      OC_REAL8m ratio = 0.125*DBL_MAX;
      if(error>=1 || allowed_absolute_step_error<ratio*error) {
        OC_REAL8m test_ratio = allowed_absolute_step_error/error;
        if(test_ratio<ratio) ratio = test_ratio;
      }
      test_stepsize = pow(ratio,1.0/local_error_order);
    }
    if(!error_checked || test_stepsize<new_stepsize) {
      new_stepsize = test_stepsize;
    }
    error_checked = 1;
  }

  if(error_checked) {
    new_stepsize *= step_headroom;
    if(new_stepsize<max_step_decrease) {
      new_stepsize = max_step_decrease*stepsize;
    } else {
      new_stepsize *= stepsize;
      OC_REAL8m step_bound = stepsize * max_step_increase;
      const OC_REAL8m refrat = 0.85;  // Ad hoc value
      if(stepsize<reference_stepsize*refrat) {
        step_bound = OC_MIN(step_bound,reference_stepsize);
      } else if(stepsize<reference_stepsize) {
        OC_REAL8m ref_bound = reference_stepsize + (max_step_increase-1)
          *(stepsize-reference_stepsize*refrat)/(1-refrat);
        /// If stepsize = reference_stepsize*refrat,
        ///     then ref_bound = reference_stepsize
        /// If stepsize = reference_stepsize,
        ///     then ref_bound = step_bound
        /// Otherwise, linear interpolation on stepsize.
        step_bound = OC_MIN(step_bound,ref_bound);
      }
      if(new_stepsize>step_bound) new_stepsize = step_bound;
    }
  } else {
    new_stepsize = stepsize;
  }

  return good_step;
}

void Oxs_RungeKuttaEvolve::TakeRungeKuttaStep2
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_BOOL& new_energy_and_dmdt_computed)
{ // This routine performs a second order Runge-Kutta step, with
  // error estimation.  The form used is the "modified Euler"
  // method due to Collatz (1960).
  //
  // A general RK2 step involves
  //
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+a.h,m1+a.h.dm_dt1)
  //  m2 = m1 + h.( (1-1/(2a)).dm_dt1 + (1/(2a)).dm_dt2 )
  //
  // where 0<a<=1 is a free parameter.  Taking a=1/2 gives
  //
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+h/2,m1+dm_dt1.h/2)
  //  m2 = m1 + dm_dt2*h + O(h^3)
  //
  // This is the "modified Euler" method from Collatz (1960).
  // Setting a=1 yields
  //
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+h,m1+dm_dt1.h)
  //  m2 = m1 + (dm_dt1 + dm_dt2).h/2 + O(h^3)
  //
  // which is the method of Heun (1900).  For details see
  // J. Stoer and R. Bulirsch, "Introduction to Numerical
  // Analysis," Springer 1993, Section 7.2.1, p438.
  //
  // In the code below, we use the "modified Euler" approach,
  // i.e., select a=1/2.

  const Oxs_SimState* cstate = &(current_state_key.GetReadReference());

  OC_INDEX i;
  const OC_INDEX size = cstate->mesh->Size();

  OC_REAL8m pE_pt,max_dm_dt,dE_dt,timestep_lower_bound,dummy_error;

  // Calculate dm_dt2
  AdjustState(stepsize/2,stepsize/2,*cstate,current_dm_dt,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  // vtmpA holds dm_dt2
  Oxs_SimState* nstate = &(next_state_key.GetWriteReference());
  nstate->ClearDerivedData();
  nstate->spin = cstate->spin;
  nstate->spin.Accumulate(stepsize,vtmpA);

  // Tweak "last_timestep" field in next_state, and adjust other
  // time fields to protect against rounding errors.
  UpdateTimeFields(*cstate,*nstate,stepsize);

  // Normalize spins in nstate, and collect norm error info
  // Normalize m2, including norm error check
  OC_REAL8m min_normsq = DBL_MAX;
  OC_REAL8m max_normsq = 0.0;
  for(i=0;i<size;++i) {
    OC_REAL8m magsq = nstate->spin[i].MakeUnit();
    if(magsq<min_normsq) min_normsq=magsq;
    if(magsq>max_normsq) max_normsq=magsq;
  }
  norm_error = OC_MAX(sqrt(max_normsq)-1.0,
                      1.0 - sqrt(min_normsq));
  const Oxs_SimState* endstate
    = &(next_state_key.GetReadReference()); // Lock down

  // To estimate error, compute dm_dt at end state.
  OC_REAL8m total_E;
  GetEnergyDensity(*endstate,temp_energy,&mxH_output.cache.value,
                   NULL,pE_pt,total_E);
  mxH_output.cache.state_id=endstate->Id();
  Calculate_dm_dt(*endstate,mxH_output.cache.value,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);
  if(!endstate->AddDerivedData("Timestep lower bound",
                                timestep_lower_bound) ||
     !endstate->AddDerivedData("Max dm/dt",max_dm_dt) ||
     !endstate->AddDerivedData("pE/pt",pE_pt) ||
     !endstate->AddDerivedData("Total E",total_E) ||
     !endstate->AddDerivedData("dE/dt",dE_dt)) {
    throw Oxs_ExtError(this,
                         "Oxs_RungeKuttaEvolve::TakeRungeKuttaStep2:"
                         " Programming error; data cache already set.");
  }

  // Best guess at error compares computed endpoint against what
  // we would get if we ran a Heun-type step with dm/dt at this
  // endpoint.
  OC_REAL8m max_err_sq = 0.0;
  for(i=0;i<size;++i) {
    ThreeVector tvec = current_dm_dt[i];
    tvec += vtmpB[i];
    tvec *= 0.5;
    tvec -= vtmpA[i];
    OC_REAL8m err_sq = tvec.MagSq();
    if(err_sq>max_err_sq) max_err_sq = err_sq;
  }
  error_estimate = sqrt(max_err_sq)*stepsize;
  global_error_order = 2.0;

  // Move end dm_dt data into vtmpA, for use by calling routine.
  // Note that end energy is already in temp_energy, as per
  // contract.
  vtmpA.Swap(vtmpB);
  new_energy_and_dmdt_computed = 1;
}

void Oxs_RungeKuttaEvolve::TakeRungeKuttaStep2Heun
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_BOOL& new_energy_and_dmdt_computed)
{ // This routine performs a second order Runge-Kutta step, with
  // error estimation.  The form used in this routine is the "Heun"
  // method.
  //
  // A general RK2 step involves
  //
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+a.h,m1+a.h.dm_dt1)
  //  m2 = m1 + h.( (1-1/(2a)).dm_dt1 + (1/(2a)).dm_dt2 )
  //
  // where 0<a<=1 is a free parameter.  Taking a=1/2 gives
  //
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+h/2,m1+dm_dt1.h/2)
  //  m2 = m1 + dm_dt2*h + O(h^3)
  //
  // This is the "modified Euler" method from Collatz (1960).
  // Setting a=1 yields
  //
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+h,m1+dm_dt1.h)
  //  m2 = m1 + (dm_dt1 + dm_dt2).h/2 + O(h^3)
  //
  // which is the method of Heun (1900).  For details see
  // J. Stoer and R. Bulirsch, "Introduction to Numerical
  // Analysis," Springer 1993, Section 7.2.1, p438.
  //
  // In the code below, we use the Heun approach,
  // i.e., select a=1.

  const Oxs_SimState* cstate = &(current_state_key.GetReadReference());

  OC_INDEX i;
  const OC_INDEX size = cstate->mesh->Size();

  OC_REAL8m pE_pt,max_dm_dt,dE_dt,timestep_lower_bound,dummy_error;

  // Calculate dm_dt2
  AdjustState(stepsize,stepsize,*cstate,current_dm_dt,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  // vtmpA holds dm_dt2
  vtmpC = vtmpA;  // Make a copy to be used in error control

  // Form 0.5*(dm_dt1+dm_dt2)
  vtmpA += current_dm_dt;
  vtmpA *= 0.5;

  // Create new state
  Oxs_SimState* nstate = &(next_state_key.GetWriteReference());
  nstate->ClearDerivedData();
  nstate->spin = cstate->spin;
  nstate->spin.Accumulate(stepsize,vtmpA);

  // Tweak "last_timestep" field in next_state, and adjust other
  // time fields to protect against rounding errors.
  UpdateTimeFields(*cstate,*nstate,stepsize);

  // Normalize spins in nstate, and collect norm error info
  // Normalize m2, including norm error check
  OC_REAL8m min_normsq = DBL_MAX;
  OC_REAL8m max_normsq = 0.0;
  for(i=0;i<size;++i) {
    OC_REAL8m magsq = nstate->spin[i].MakeUnit();
    if(magsq<min_normsq) min_normsq=magsq;
    if(magsq>max_normsq) max_normsq=magsq;
  }
  norm_error = OC_MAX(sqrt(max_normsq)-1.0,
                      1.0 - sqrt(min_normsq));
  const Oxs_SimState* endstate
    = &(next_state_key.GetReadReference()); // Lock down

  // To estimate error, compute dm_dt at end state.
  OC_REAL8m total_E;
  GetEnergyDensity(*endstate,temp_energy,&mxH_output.cache.value,
                   NULL,pE_pt,total_E);
  mxH_output.cache.state_id=endstate->Id();
  Calculate_dm_dt(*endstate,mxH_output.cache.value,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);
  if(!endstate->AddDerivedData("Timestep lower bound",
                                timestep_lower_bound) ||
     !endstate->AddDerivedData("Max dm/dt",max_dm_dt) ||
     !endstate->AddDerivedData("pE/pt",pE_pt) ||
     !endstate->AddDerivedData("Total E",total_E) ||
     !endstate->AddDerivedData("dE/dt",dE_dt)) {
    throw Oxs_ExtError(this,
                         "Oxs_RungeKuttaEvolve::TakeRungeKuttaStep2Heun:"
                         " Programming error; data cache already set.");
  }

  // Best guess at error compares computed endpoint against what
  // we would get if we ran a Heun-type step with dm/dt at this
  // endpoint.
  OC_REAL8m max_err_sq = 0.0;
  for(i=0;i<size;++i) {
    ThreeVector tvec = vtmpC[i] - vtmpB[i];
    OC_REAL8m err_sq = tvec.MagSq();
    if(err_sq>max_err_sq) max_err_sq = err_sq;
  }
  error_estimate = sqrt(max_err_sq)*stepsize/2.;
  global_error_order = 2.0;

  // Move end dm_dt data into vtmpA, for use by calling routine.
  // Note that end energy is already in temp_energy, as per
  // contract.
  vtmpA.Swap(vtmpB);
  new_energy_and_dmdt_computed = 1;
}

#if 0
void Oxs_RungeKuttaEvolve::TakeRungeKuttaStep4
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_BOOL& new_energy_and_dmdt_computed)
{ // This routine performs two successive "classical" Runge-Kutta
  // steps of size stepsize/2, and stores the resulting magnetization
  // state into the next_state export.  Additionally, a single
  // step of size stepsize is performed, and used for estimating
  // the error.  This error is returned in the export error_estimate;
  // This is the largest error detected cellwise, in radians.  The
  // export global_error_order is always set to 4 by this routine.
  // (The local error order is one better, i.e., 5.)  The norm_error
  // export is set to the cellwise maximum deviation from unit norm
  // across all the spins in the final state, before renormalization.

  // A single RK4 step involves
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+h/2,m1+dm_dt1*h/2)
  //  dm_dt3 = dm_dt(t1+h/2,m1+dm_dt2*h/2)
  //  dm_dt4 = dm_dt(t1+h,m1+dm_dt3*h)
  //  m2 = m1 + dm_dt1*h/6 + dm_dt2*h/3 + dm_dt3*h/3 + dm_dt4*h/6 + O(h^5)
  // To improve accuracy, for each step accumulate dm_dt?, where
  // ?=1,2,3,4, into m2 with proper weights, and add in m1 at the end.

  // To calculate dm_dt, we first fill next_state with the proper
  // spin data (e.g., m1+dm_dt2*h/2).  The utility routine AdjustState
  // is applied for this purpose.  Then next_state is locked down,
  // so that it will have a valid state_id, and GetEnergyDensity
  // is called in order to get mxH and related data.  This is then
  // passed to Calculate_dm_dt.  Note that dm_dt is not calculated
  // for the final state.  This is left for the calling routine,
  // which first examines the error estimates to decide whether
  // or not the step will be accepted.  If the step is rejected,
  // then the energy and dm_dt do not need to be calculated.

  // Scratch space usage:
  //   vtmpA is used to store, successively, dm_dt2, dm_dt3 and
  //      dm_dt4.  In calculating dm_dt?, vtmpA is first filled
  //      with mxH by the GetEnergyDensity() routine.
  //      Calculate_dm_dt() allows import mxH and export dm_dt
  //      to be the same MeshValue structure.
  //   vtmpB is used to accumulate the weighted sums of the dm_dt's,
  //      as explained above.  To effect a small efficiency gain,
  //      dm_dt1 is calculated directly into vtmpB, in the same
  //      manner as the other dm_dt's are filled into vtmpA.
  //   tempState is used to hold the middle state when calculating
  //      the two half steps, and the end state for the single
  //      full size step.

  // Any locks arising from temp_state_key will be released when
  // temp_state_key is destroyed on method exit.
  Oxs_SimState* nstate = &(next_state_key.GetWriteReference());
  const Oxs_SimState* cstate = &(current_state_key.GetReadReference());
  Oxs_Key<Oxs_SimState> temp_state_key;
  director->GetNewSimulationState(temp_state_key);
  Oxs_SimState* tstate = &(temp_state_key.GetWriteReference());
  nstate->CloneHeader(*tstate);

  OC_INDEX i;
  const OC_INDEX size = cstate->mesh->Size();

  OC_REAL8m pE_pt,max_dm_dt,dE_dt,timestep_lower_bound;
  OC_REAL8m dummy_error;

  // Do first half step.  Because dm_dt1 is already calculated,
  // we fill dm_dt2 directly into vtmpB.
  AdjustState(stepsize/4,stepsize/4,*cstate,current_dm_dt,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpB,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpB,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);
  // vtmpB currently holds dm_dt2
  AdjustState(stepsize/4,stepsize/4,*cstate,vtmpB,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  vtmpB += vtmpA;
  // vtmpA holds dm_dt3, vtmpB holds dm_dt2 + dm_dt3.
  AdjustState(stepsize/2,stepsize/2,*cstate,vtmpA,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  // vtmpA holds dm_dt4
  vtmpA += current_dm_dt;
  vtmpB.Accumulate(0.5,vtmpA); // Add in 0.5*(dm_dt1+dm_dt4)
  tstate = &(temp_state_key.GetWriteReference());
  tstate->ClearDerivedData();
  tstate->spin = cstate->spin;
  tstate->spin.Accumulate(stepsize/6.,vtmpB);
  // Note: state time index set to lasttime + stepsize/2
  // by dm_dt4 calculation above.  Note that "h" here is
  // stepsize/2, so the weights on dm_dt1, dm_dt2, dm_dt3 and
  // dn_dt4 are (stepsize/2)(1/6, 1/3, 1/3, 1/6), respectively.

  // Normalize spins in tstate
  for(i=0;i<size;++i) tstate->spin[i].MakeUnit();

  // At this point, temp_state holds the middle point.
  // Calculate dm_dt for this state, and store in vtmpB.
  tstate = NULL; // Disable non-const access
  const Oxs_SimState* midstate
    = &(temp_state_key.GetReadReference());
  GetEnergyDensity(*midstate,temp_energy,&vtmpB,NULL,pE_pt);
  Calculate_dm_dt(*midstate,vtmpB,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);

  // Next do second half step.  Store end result in next_state
  AdjustState(stepsize/4,stepsize/4,*midstate,vtmpB,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  // vtmpB currently holds dm_dt1, vtmpA holds dm_dt2
  vtmpB *= 0.5;
  vtmpB += vtmpA;
  AdjustState(stepsize/4,stepsize/4,*midstate,vtmpA,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  vtmpB += vtmpA;
  // vtmpA holds dm_dt3, vtmpB holds dm_dt1/2 + dm_dt2 + dm_dt3.
  AdjustState(stepsize/2,stepsize/2,*midstate,vtmpA,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  // vtmpA holds dm_dt4
  vtmpB.Accumulate(0.5,vtmpA);
  nstate = &(next_state_key.GetWriteReference());
  nstate->ClearDerivedData();
  nstate->spin = midstate->spin;
  nstate->spin.Accumulate(stepsize/6.,vtmpB);
  midstate = NULL; // We're done using midstate

  // Tweak "last_timestep" field in next_state, and adjust other
  // time fields to protect against rounding errors.
  UpdateTimeFields(*cstate,*nstate,stepsize);

  // Normalize spins in nstate, and collect norm error info
  // Normalize m2, including norm error check
  OC_REAL8m min_normsq = DBL_MAX;
  OC_REAL8m max_normsq = 0.0;
  for(i=0;i<size;++i) {
    OC_REAL8m magsq = nstate->spin[i].MakeUnit();
    if(magsq<min_normsq) min_normsq=magsq;
    if(magsq>max_normsq) max_normsq=magsq;
  }
  norm_error = OC_MAX(sqrt(max_normsq)-1.0,
                      1.0 - sqrt(min_normsq));
  nstate = NULL;
  const Oxs_SimState* endstate
    = &(next_state_key.GetReadReference()); // Lock down

  // Repeat now for full step, storing end result into temp_state
  AdjustState(stepsize/2,stepsize/2,*cstate,current_dm_dt,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpB,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpB,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);
  // vtmpB currently holds dm_dt2
  AdjustState(stepsize/2,stepsize/2,*cstate,vtmpB,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  vtmpB += vtmpA;
  // vtmpA holds dm_dt3, vtmpB holds dm_dt2 + dm_dt3.
  AdjustState(stepsize,stepsize,*cstate,vtmpA,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  // vtmpA holds dm_dt4
  vtmpA += current_dm_dt;
  vtmpB.Accumulate(0.5,vtmpA); // Add in 0.5*(dm_dt1+dm_dt4)
  tstate = &(temp_state_key.GetWriteReference());
  tstate->ClearDerivedData();  // Safety
  tstate->spin = cstate->spin;
  tstate->spin.Accumulate(stepsize/3.,vtmpB);
  for(i=0;i<size;++i) tstate->spin[i].MakeUnit(); // Normalize
  tstate = NULL; // Disable write access
  const Oxs_SimState* teststate
    = &(temp_state_key.GetReadReference()); // Lock down

  // Error is max|next_state.spin-temp_state.spin|/15 + O(stepsize^6)
  OC_REAL8m max_error_sq = 0.0;
  for(i=0;i<size;++i) {
    ThreeVector tvec = endstate->spin[i] - teststate->spin[i];
    OC_REAL8m error_sq = tvec.MagSq();
    if(error_sq>max_error_sq) max_error_sq = error_sq;
    // If we want, we can experiment with adding
    // tvec/15 to endstate->spin.  In theory, this changes
    // the error from O(stepsize^5) to O(stepsize^6)
  }
  error_estimate = sqrt(max_error_sq)/15.;
  global_error_order = 4.0;
  new_energy_and_dmdt_computed = 0;
}
#else // 0
void Oxs_RungeKuttaEvolve::TakeRungeKuttaStep4
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_BOOL& new_energy_and_dmdt_computed)
{ // This routine performs two successive "classical" Runge-Kutta
  // steps of size stepsize/2, and stores the resulting magnetization
  // state into the next_state export.  Additionally, a single
  // step of size stepsize is performed, and used for estimating
  // the error.  This error is returned in the export error_estimate;
  // This is the largest error detected cellwise, in radians.  The
  // export global_error_order is always set to 4 by this routine.
  // (The local error order is one better, i.e., 5.)  The norm_error
  // export is set to the cellwise maximum deviation from unit norm
  // across all the spins in the final state, before renormalization.

  // A single RK4 step involves
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+h/2,m1+dm_dt1*h/2)
  //  dm_dt3 = dm_dt(t1+h/2,m1+dm_dt2*h/2)
  //  dm_dt4 = dm_dt(t1+h,m1+dm_dt3*h)
  //  m2 = m1 + dm_dt1*h/6 + dm_dt2*h/3 + dm_dt3*h/3 + dm_dt4*h/6 + O(h^5)
  // To improve accuracy, for each step accumulate dm_dt?, where
  // ?=1,2,3,4, into m2 with proper weights, and add in m1 at the end.

  // To calculate dm_dt, we first fill next_state with the proper
  // spin data (e.g., m1+dm_dt2*h/2).  The utility routine AdjustState
  // is applied for this purpose.  Then next_state is locked down,
  // so that it will have a valid state_id, and GetEnergyDensity
  // is called in order to get mxH and related data.  This is then
  // passed to Calculate_dm_dt.  Note that dm_dt is not calculated
  // for the final state.  This is left for the calling routine,
  // which first examines the error estimates to decide whether
  // or not the step will be accepted.  If the step is rejected,
  // then the energy and dm_dt do not need to be calculated.

  // Scratch space usage:
  //   vtmpA is used to store, successively, dm_dt2, dm_dt3 and
  //      dm_dt4.  In calculating dm_dt?, vtmpA is first filled
  //      with mxH by the GetEnergyDensity() routine.
  //      Calculate_dm_dt() allows import mxH and export dm_dt
  //      to be the same MeshValue structure.
  //   vtmpB is used to accumulate the weighted sums of the dm_dt's,
  //      as explained above.  To effect a small efficiency gain,
  //      dm_dt1 is calculated directly into vtmpB, in the same
  //      manner as the other dm_dt's are filled into vtmpA.
  //   tempState is used to hold the middle state when calculating
  //      the two half steps, and the end state for the single
  //      full size step.

  // Any locks arising from temp_state_key will be released when
  // temp_state_key is destroyed on method exit.
  Oxs_SimState* nstate = &(next_state_key.GetWriteReference());
  const Oxs_SimState* cstate = &(current_state_key.GetReadReference());
  Oxs_Key<Oxs_SimState> temp_state_key;
  director->GetNewSimulationState(temp_state_key);
  Oxs_SimState* tstate = &(temp_state_key.GetWriteReference());
  nstate->CloneHeader(*tstate);

  OC_INDEX i;
  const OC_INDEX size = cstate->mesh->Size();

  OC_REAL8m pE_pt,max_dm_dt,dE_dt,timestep_lower_bound;
  OC_REAL8m dummy_error;

  // Do first half step.  Because dm_dt1 is already calculated,
  // we fill dm_dt2 directly into vtmpB.
  AdjustState(stepsize/4,stepsize/4,*cstate,current_dm_dt,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpB,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpB,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);
  // vtmpB currently holds dm_dt2
  AdjustState(stepsize/4,stepsize/4,*cstate,vtmpB,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  vtmpB += vtmpA;
  // vtmpA holds dm_dt3, vtmpB holds dm_dt2 + dm_dt3.
  AdjustState(stepsize/2,stepsize/2,*cstate,vtmpA,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  // vtmpA holds dm_dt4
  vtmpA += current_dm_dt;
  vtmpB.Accumulate(0.5,vtmpA); // Add in 0.5*(dm_dt1+dm_dt4)
  tstate = &(temp_state_key.GetWriteReference());
  tstate->ClearDerivedData();
  tstate->spin = cstate->spin;
  tstate->spin.Accumulate(stepsize/6.,vtmpB);
  // Note: state time index set to lasttime + stepsize/2
  // by dm_dt4 calculation above.  Note that "h" here is
  // stepsize/2, so the weights on dm_dt1, dm_dt2, dm_dt3 and
  // dn_dt4 are (stepsize/2)(1/6, 1/3, 1/3, 1/6), respectively.

  // Save vtmpB for error estimate
  vtmpB.Swap(vtmpC);

  // Normalize spins in tstate, and collect norm error info.
  OC_REAL8m min_normsq = DBL_MAX;
  OC_REAL8m max_normsq = 0.0;
  for(i=0;i<size;++i) {
    OC_REAL8m magsq = tstate->spin[i].MakeUnit();
    if(magsq<min_normsq) min_normsq=magsq;
    if(magsq>max_normsq) max_normsq=magsq;
  }

  // At this point, temp_state holds the middle point.
  // Calculate dm_dt for this state, and store in vtmpB.
  tstate = NULL; // Disable non-const access
  const Oxs_SimState* midstate
    = &(temp_state_key.GetReadReference());
  GetEnergyDensity(*midstate,temp_energy,&vtmpB,NULL,pE_pt);
  Calculate_dm_dt(*midstate,vtmpB,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);

  // Next do second half step.  Store end result in next_state
  AdjustState(stepsize/4,stepsize/4,*midstate,vtmpB,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  // vtmpB currently holds dm_dt1, vtmpA holds dm_dt2
  vtmpB *= 0.5;
  vtmpB += vtmpA;
  AdjustState(stepsize/4,stepsize/4,*midstate,vtmpA,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  vtmpB += vtmpA;
  // vtmpA holds dm_dt3, vtmpB holds dm_dt1/2 + dm_dt2 + dm_dt3.
  AdjustState(stepsize/2,stepsize/2,*midstate,vtmpA,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  // vtmpA holds dm_dt4
  vtmpB.Accumulate(0.5,vtmpA);
  nstate = &(next_state_key.GetWriteReference());
  nstate->ClearDerivedData();
  nstate->spin = midstate->spin;
  nstate->spin.Accumulate(stepsize/6.,vtmpB);
  midstate = NULL; // We're done using midstate

  // Combine vtmpB with results from first half-step.
  // This is used for error estimation.
  vtmpC += vtmpB;

  // Tweak "last_timestep" field in next_state, and adjust other
  // time fields to protect against rounding errors.
  UpdateTimeFields(*cstate,*nstate,stepsize);

  // Normalize spins in nstate, and collect norm error info.
  for(i=0;i<size;++i) {
    OC_REAL8m magsq = nstate->spin[i].MakeUnit();
    if(magsq<min_normsq) min_normsq=magsq;
    if(magsq>max_normsq) max_normsq=magsq;
  }
  norm_error = OC_MAX(sqrt(max_normsq)-1.0,
                      1.0 - sqrt(min_normsq));
  nstate = NULL;
  next_state_key.GetReadReference(); // Lock down (safety)

  // Repeat now for full step, storing end result into temp_state
  AdjustState(stepsize/2,stepsize/2,*cstate,current_dm_dt,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpB,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpB,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);
  // vtmpB currently holds dm_dt2
  AdjustState(stepsize/2,stepsize/2,*cstate,vtmpB,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  vtmpB += vtmpA;
  // vtmpA holds dm_dt3, vtmpB holds dm_dt2 + dm_dt3.
  AdjustState(stepsize,stepsize,*cstate,vtmpA,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  // vtmpA holds dm_dt4
  vtmpA += current_dm_dt;
  vtmpB.Accumulate(0.5,vtmpA); // Add in 0.5*(dm_dt1+dm_dt4)

  // Estimate error
  OC_REAL8m max_error_sq = 0.0;
  for(i=0;i<size;++i) {
    ThreeVector tvec = vtmpB[i];
    tvec.Accum(-0.5,vtmpC[i]);  // 0.5 because vtmpC holds sum of
                               /// two half-steps.
    OC_REAL8m error_sq = tvec.MagSq();
    if(error_sq>max_error_sq) max_error_sq = error_sq;
  }
  error_estimate = sqrt(max_error_sq)*stepsize/3.;
  /// vtmpB hold 0.5*dm_dt1 + dm_dt2 + dm_dt3 + 0.5*dm_dt4,
  /// but successor state looks like
  ///    m2 = m1 + dm_dt1*h/6 + dm_dt2*h/3 + dm_dt3*h/3 + dm_dt4*h/6 + O(h^5)

  global_error_order = 4.0;
  new_energy_and_dmdt_computed = 0;
}
#endif //0

void Oxs_RungeKuttaEvolve::RungeKuttaFehlbergBase54
(RKF_SubType method,
 OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_BOOL& new_energy_and_dmdt_computed)
{ // Runge-Kutta-Fehlberg routine with combined 4th and 5th
  // order Runge-Kutta steps.  The difference between the
  // two results (4th vs. 5th) is used to estimate the error.
  // The largest detected error detected cellsize is returned
  // in export error_estimate.  The units on this are radians.
  // The export global_error_order is set to 4 by this routine.
  // (The local error order is one better, i.e., 5.)  The norm_error
  // export is set to the cellwise maximum deviation from unit norm
  // across all the spins in the final state, before renormalization.

#if REPORT_TIME_RKDEVEL
timer[1].Start(); /**/
timer[2].Start(); /**/
#endif // REPORT_TIME_RKDEVEL

  // The following coefficients appear in
  //
  //   J. R. Dormand and P. J. Prince, ``A family of embedded
  //   Runge-Kutta formulae,'' J. Comp. Appl. Math., 6, 19--26
  //   (1980).
  //
  // They are also listed in J. Stoer and R. Bulirsch's book,
  // ``Introduction to Numerical Analysis,'' Springer, 2nd edition,
  // Sec. 7.2.5, p 454, but there are a number of errors in the S&B
  // account; the reader is recommended to refer directly to the D&P
  // paper.  A follow-up paper,
  //
  //   J. R. Dormand and P. J. Prince, ``A reconsideration of some 
  //   embedded Runge-Kutta formulae,'' J. Comp. Appl. Math., 15,
  //   203--211 (1986)
  //
  // provides formulae with improved stability and higher order.
  // See also
  //
  //   J. H. Williamson, ``Low-Storage Runge-Kutta Schemes,''
  //   J. Comp. Phys., 35, 48--56 (1980).
  //
  // FORMULAE for RK5(4)7FM:
  //
  //     dm_dt1 = dm_dt(t1,m1)
  //     dm_dt2 = dm_dt(t1+ (1/5)*h, m1+h*k1);
  //     dm_dt3 = dm_dt(t1+(3/10)*h, m1+h*k2);
  //     dm_dt4 = dm_dt(t1+ (4/5)*h, m1+h*k3);
  //     dm_dt5 = dm_dt(t1+ (8/9)*h, m1+h*k4);
  //     dm_dt6 = dm_dt(t1+     1*h, m1+h*k5);
  //     dm_dt7 = dm_dt(t1+     1*h, m1+h*k6);
  //  where
  //     k1 = dm_dt1*1/5
  //     k2 = dm_dt1*3/40       + dm_dt2*9/40
  //     k3 = dm_dt1*44/45      - dm_dt2*56/15      + dm_dt3*32/9
  //     k4 = dm_dt1*19372/6561 - dm_dt2*25360/2187 + dm_dt3*64448/6561
  //               - dm_dt4*212/729
  //     k5 = dm_dt1*9017/3168  - dm_dt2*355/33     + dm_dt3*46732/5247
  //               + dm_dt4*49/176   - dm_dt5*5103/18656
  //     k6 = dm_dt1*35/384     + 0                 + dm_dt3*500/1113
  //               + dm_dt4*125/192  - dm_dt5*2187/6784  + dm_dt6*11/84
  // Then
  //     Da = dm_dt1*35/384     + 0 + dm_dt3*500/1113   + dm_dt4*125/192
  //              - dm_dt5*2187/6784      + dm_dt6*11/84
  //     Db = dm_dt1*5179/57600 + 0 + dm_dt3*7571/16695 + dm_dt4*393/640
  //              - dm_dt5*92097/339200   + dm_dt6*187/2100   + dm_dt7*1/40
  // and
  //     m2a = m1 + h*Da
  //     m2b = m1 + h*Db.
  //
  // where m2a is the 5th order estimate, which is the candidate
  // for the next state, and m2b is the 4th order estimate used
  // only for error estimation/stepsize control.  Note that the
  // 4th order estimate uses more dm/dt evaluations (7) than the
  // 5th order method (6).  This is intentional; the coefficients
  // are selected to minimize error (see D&P paper cited above).
  // The extra calculation costs are minimal, because the 7th dm_dt
  // evaluation is at the candidate next state, it is re-used
  // on the next step unless the step rejected.
  //
  // The error estimate is
  // 
  //     E = |m2b-m2a| = h*|Db-Da| = C*h^6
  //
  // where C is a constant that can be estimated in terms of E.
  // Note that we don't need to know C in order to adjust the
  // stepsize, because stepsize adjustment involves only the
  // ratio of the old stepsize to the new, so C drops out.

  // Scratch space usage:
  //  The import next_state is used for intermediate state
  // storage for all dm_dt computations.  The final computation
  // is for dm_dt7 at m1+h*k6 = m1+h*Da, which is the candidate
  // next state.  (Da=k6; see FSAL note below.)

  // Four temporary arrays, A-D, are used:
  //
  // Step \  Temp
  // Index \ Array:  A         B         C         D
  // ------+---------------------------------------------
  //   1   |      dm_dt2       -         -         -
  //   2   |      dm_dt2      k2         -         -
  //   3   |      dm_dt2     dm_dt3      -         -
  //   4   |      dm_dt2     dm_dt3     k3         -
  //   5   |      dm_dt2     dm_dt3    dm_dt4      -
  //   6   |      dm_dt2     dm_dt3    dm_dt4     k4
  //   7   |      dm_dt2     dm_dt3    dm_dt4    dm_dt5
  //   8   |        k5       dm_dt3    dm_dt4    dm_dt5
  //   9   |      dm_dt6     dm_dt3    dm_dt4    dm_dt5
  //  10   |      k6(3,6)    dD(3,6)   dm_dt4    dm_dt5
  //  11   |      dm_dt7     dD(3,6)   dm_dt4    dm_dt5
  //  12   |      dm_dt7       dD      dm_dt4    dm_dt5
  //
  // Here dD is Db-Da.  We don't need to compute Db directly.
  // The parenthesized numbers, e.g., k6(3,6) indicates
  // a partially formed value.  The total value k6 depends
  // upon dm_dt1, dm_dt3, dm_dt4, dm_dt5, and dm_dt6.  But
  // if we form k6 directly at step 11 in array A, then we
  // lose dm_dt6 which is needed to compute dD.  Rather than
  // use an additional array, we accumulate partial results
  // into arrays A and B for k6 and dD as indicated.
  //   Note that Da = k6.  Further, note that dm_dt7 is
  // dm_dt for the next state candidate.  (This is the 'F',
  // short for 'FSAL' ("first same as last"?) in the method
  // name, RK5(4)7FM.  The 'M' means minimized error norm,
  // 7 is the number of stages, and 5(4) is the main/subsidiary
  // integration formula order.  See the D&P 1986 paper for
  // details and additional references.)

  // Coefficient arrays, a, b, dc, defined by:
  //
  //   dm_dtN = dm_dt(t1+aN*h,m1+h*kN)
  //       kN = \sum_{M=1}^{M=N} dm_dtM*bNM
  //  Db - Da = \sum dm_dtM*dcM
  //
  OC_REAL8m a1,a2,a3,a4; // a5 and a6 are 1.0
  OC_REAL8m b11,b21,b22,b31,b32,b33,b41,b42,b43,b44,
    b51,b52,b53,b54,b55,b61,b63,b64,b65,b66; // b62 is 0.0
  OC_REAL8m dc1,dc3,dc4,dc5,dc6,dc7;  // c[k] = b6k, and c^[2]=c[2]=0.0,
  /// where c are the coeffs for Da, c^ for Db, and dcM = c^[M]-c[M].

  switch(method) {
  case RK547FC:
    /////////////////////////////////////////////////////////////////
    // Coefficients for Dormand & Prince RK5(4)7FC
    a1 = OC_REAL8m(1.)/OC_REAL8m(5.);
    a2 = OC_REAL8m(3.)/OC_REAL8m(10.);
    a3 = OC_REAL8m(6.)/OC_REAL8m(13.);
    a4 = OC_REAL8m(2.)/OC_REAL8m(3.);
    // a5 and a6 are 1.0

    b11 =      OC_REAL8m(1.)/OC_REAL8m(5.);
  
    b21 =      OC_REAL8m(3.)/OC_REAL8m(40.);
    b22 =      OC_REAL8m(9.)/OC_REAL8m(40.);
  
    b31 =    OC_REAL8m(264.)/OC_REAL8m(2197.);
    b32 =    OC_REAL8m(-90.)/OC_REAL8m(2197.);
    b33 =    OC_REAL8m(840.)/OC_REAL8m(2197.);
  
    b41 =    OC_REAL8m(932.)/OC_REAL8m(3645.);
    b42 =    OC_REAL8m(-14.)/OC_REAL8m(27.);
    b43 =   OC_REAL8m(3256.)/OC_REAL8m(5103.);
    b44 =   OC_REAL8m(7436.)/OC_REAL8m(25515.);
  
    b51 =   OC_REAL8m(-367.)/OC_REAL8m(513.);
    b52 =     OC_REAL8m(30.)/OC_REAL8m(19.);
    b53 =   OC_REAL8m(9940.)/OC_REAL8m(5643.);
    b54 = OC_REAL8m(-29575.)/OC_REAL8m(8208.);
    b55 =   OC_REAL8m(6615.)/OC_REAL8m(3344.);
  
    b61 =     OC_REAL8m(35.)/OC_REAL8m(432.);
    b63 =   OC_REAL8m(8500.)/OC_REAL8m(14553.);
    b64 = OC_REAL8m(-28561.)/OC_REAL8m(84672.);
    b65 =    OC_REAL8m(405.)/OC_REAL8m(704.);
    b66 =     OC_REAL8m(19.)/OC_REAL8m(196.);
    // b62 is 0.0

    // Coefs for error calculation (c^[k] - c[k]).
    // Note that c[k] = b6k, and c^[2]=c[2]=0.0
    dc1 =     OC_REAL8m(11.)/OC_REAL8m(108.)    - b61;
    dc3 =   OC_REAL8m(6250.)/OC_REAL8m(14553.)  - b63;
    dc4 =  OC_REAL8m(-2197.)/OC_REAL8m(21168.)  - b64;
    dc5 =     OC_REAL8m(81.)/OC_REAL8m(176.)    - b65;
    dc6 =    OC_REAL8m(171.)/OC_REAL8m(1960.)   - b66;
    dc7 =      OC_REAL8m(1.)/OC_REAL8m(40.);
    break;

  case RK547FM:
    /////////////////////////////////////////////////////////////////
    // Coefficients for Dormand & Prince RK5(4)7FM
    a1 = OC_REAL8m(1.)/OC_REAL8m(5.);
    a2 = OC_REAL8m(3.)/OC_REAL8m(10.);
    a3 = OC_REAL8m(4.)/OC_REAL8m(5.);
    a4 = OC_REAL8m(8.)/OC_REAL8m(9.);
    // a5 and a6 are 1.0

    b11 =      OC_REAL8m(1.)/OC_REAL8m(5.);
  
    b21 =      OC_REAL8m(3.)/OC_REAL8m(40.);
    b22 =      OC_REAL8m(9.)/OC_REAL8m(40.);
  
    b31 =     OC_REAL8m(44.)/OC_REAL8m(45.);
    b32 =    OC_REAL8m(-56.)/OC_REAL8m(15.);
    b33 =     OC_REAL8m(32.)/OC_REAL8m(9.);
  
    b41 =  OC_REAL8m(19372.)/OC_REAL8m(6561.);
    b42 = OC_REAL8m(-25360.)/OC_REAL8m(2187.);
    b43 =  OC_REAL8m(64448.)/OC_REAL8m(6561.);
    b44 =   OC_REAL8m(-212.)/OC_REAL8m(729.);
  
    b51 =   OC_REAL8m(9017.)/OC_REAL8m(3168.);
    b52 =   OC_REAL8m(-355.)/OC_REAL8m(33.);
    b53 =  OC_REAL8m(46732.)/OC_REAL8m(5247.);
    b54 =     OC_REAL8m(49.)/OC_REAL8m(176.);
    b55 =  OC_REAL8m(-5103.)/OC_REAL8m(18656.);
  
    b61 =     OC_REAL8m(35.)/OC_REAL8m(384.);
    b63 =    OC_REAL8m(500.)/OC_REAL8m(1113.);
    b64 =    OC_REAL8m(125.)/OC_REAL8m(192.);
    b65 =  OC_REAL8m(-2187.)/OC_REAL8m(6784.);
    b66 =     OC_REAL8m(11.)/OC_REAL8m(84.);
    // b62 is 0.0

    // Coefs for error calculation (c^[k] - c[k]).
    // Note that c[k] = b6k, and c^[2]=c[2]=0.0
    dc1 =   OC_REAL8m(5179.)/OC_REAL8m(57600.)  - b61;
    dc3 =   OC_REAL8m(7571.)/OC_REAL8m(16695.)  - b63;
    dc4 =    OC_REAL8m(393.)/OC_REAL8m(640.)    - b64;
    dc5 = OC_REAL8m(-92097.)/OC_REAL8m(339200.) - b65;
    dc6 =    OC_REAL8m(187.)/OC_REAL8m(2100.)   - b66;
    dc7 =      OC_REAL8m(1.)/OC_REAL8m(40.);
    break;
  case RK547FS:
    /////////////////////////////////////////////////////////////////
    // Coefficients for Dormand & Prince RK5(4)7FS
    a1 = OC_REAL8m(2.)/OC_REAL8m(9.);
    a2 = OC_REAL8m(1.)/OC_REAL8m(3.);
    a3 = OC_REAL8m(5.)/OC_REAL8m(9.);
    a4 = OC_REAL8m(2.)/OC_REAL8m(3.);
    // a5 and a6 are 1.0

    b11 =      OC_REAL8m(2.)/OC_REAL8m(9.);
  
    b21 =      OC_REAL8m(1.)/OC_REAL8m(12.);
    b22 =      OC_REAL8m(1.)/OC_REAL8m(4.);
  
    b31 =     OC_REAL8m(55.)/OC_REAL8m(324.);
    b32 =    OC_REAL8m(-25.)/OC_REAL8m(108.);
    b33 =     OC_REAL8m(50.)/OC_REAL8m(81.);
  
    b41 =     OC_REAL8m(83.)/OC_REAL8m(330.);
    b42 =    OC_REAL8m(-13.)/OC_REAL8m(22.);
    b43 =     OC_REAL8m(61.)/OC_REAL8m(66.);
    b44 =      OC_REAL8m(9.)/OC_REAL8m(110.);
  
    b51 =    OC_REAL8m(-19.)/OC_REAL8m(28.);
    b52 =      OC_REAL8m(9.)/OC_REAL8m(4.);
    b53 =      OC_REAL8m(1.)/OC_REAL8m(7.);
    b54 =    OC_REAL8m(-27.)/OC_REAL8m(7.);
    b55 =     OC_REAL8m(22.)/OC_REAL8m(7.);
  
    b61 =     OC_REAL8m(19.)/OC_REAL8m(200.);
    b63 =      OC_REAL8m(3.)/OC_REAL8m(5.);
    b64 =   OC_REAL8m(-243.)/OC_REAL8m(400.);
    b65 =     OC_REAL8m(33.)/OC_REAL8m(40.);
    b66 =      OC_REAL8m(7.)/OC_REAL8m(80.);
    // b62 is 0.0

    // Coefs for error calculation (c^[k] - c[k]).
    // Note that c[k] = b6k, and c^[2]=c[2]=0.0
    dc1 =    OC_REAL8m(431.)/OC_REAL8m(5000.)  - b61;
    dc3 =    OC_REAL8m(333.)/OC_REAL8m(500.)   - b63;
    dc4 =  OC_REAL8m(-7857.)/OC_REAL8m(10000.) - b64;
    dc5 =    OC_REAL8m(957.)/OC_REAL8m(1000.)  - b65;
    dc6 =    OC_REAL8m(193.)/OC_REAL8m(2000.)  - b66;
    dc7 =     OC_REAL8m(-1.)/OC_REAL8m(50.);
    break;
  default:
    throw Oxs_ExtError(this,
                 "Oxs_RungeKuttaEvolve::RungeKuttaFehlbergBase54:"
                 " Programming error; Invalid sub-type.");
  }

#ifndef NDEBUG
  // COEFFICIENT CHECKS ////////////////////////////////////////
  // Try to catch some simple typing errors.  Oc_Nop calls below force
  // evaluation in order shown; otherwise, some compiler optimizations
  // reorder sums into form with less accuracy.
#define EPS (8*OC_REAL8_EPSILON)  // 6*OC_REAL8_EPSILON should be enough,
  /// but include a little slack compilers with bad numeric taste.
  if(fabs(b11-a1)>EPS ||
     fabs(b21+b22-a2)>EPS ||
     fabs(b31+b32+b33-a3)>EPS ||
     fabs(b41+b42+b43+b44-a4)>EPS ||
     fabs(Oc_Nop(b51+b52+b53) -1.0 + Oc_Nop(b54+b55))>EPS ||
     fabs(b61+b63+b64+b65+b66-1.0)>EPS) {
    char buf[512];
    Oc_Snprintf(buf,sizeof(buf),
                "Coefficient check failed:\n"
                "1: %g\n2: %g\n3: %g\n4: %g\n5: %g\n6: %g",
                static_cast<double>(b11-a1),
                static_cast<double>(b21+b22-a2),
                static_cast<double>(b31+b32+b33-a3),
                static_cast<double>(b41+b42+b43+b44-a4),
                static_cast<double>(b51+b52+b53+b54+b55-1.0),
                static_cast<double>(b61+b63+b64+b65+b66-1.0));
    throw Oxs_ExtError(this,buf);
  }
#endif // NDEBUG

  const Oxs_SimState* cstate = &(current_state_key.GetReadReference());
SpinDiff("\nPt A",*cstate); /**/

  OC_REAL8m pE_pt,max_dm_dt,dE_dt,timestep_lower_bound;
  OC_REAL8m dummy_error;

#if REPORT_TIME_RKDEVEL
timer[2].Stop(); /**/
++timer_counts[2].pass_count;
 timer_counts[2].name = "RKFB54 setup";
#endif // REPORT_TIME_RKDEVEL

#if !OOMMF_THREADS
  // Step 1
  AdjustState(stepsize*a1,stepsize*b11,*cstate,current_dm_dt,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);

  // Step 2
  vtmpB = current_dm_dt;
  vtmpB *= b21;
  vtmpB.Accumulate(b22,vtmpA);
  AdjustState(stepsize*a2,stepsize,*cstate,vtmpB,
              next_state_key.GetWriteReference(),dummy_error);

  // Step 3
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpB,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpB,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);

  // Step 4
  vtmpC = current_dm_dt;
  vtmpC *= b31;
  vtmpC.Accumulate(b32,vtmpA);
  vtmpC.Accumulate(b33,vtmpB);
  AdjustState(stepsize*a3,stepsize,*cstate,vtmpC,
              next_state_key.GetWriteReference(),dummy_error);

  // Step 5
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpC,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpC,pE_pt,
                  vtmpC,max_dm_dt,dE_dt,timestep_lower_bound);

  // Step 6
  vtmpD = current_dm_dt;
  vtmpD *= b41;
  vtmpD.Accumulate(b42,vtmpA);
  vtmpD.Accumulate(b43,vtmpB);
  vtmpD.Accumulate(b44,vtmpC);
  AdjustState(stepsize*a4,stepsize,*cstate,vtmpD,
              next_state_key.GetWriteReference(),dummy_error);

  // Step 7
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpD,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpD,pE_pt,
                  vtmpD,max_dm_dt,dE_dt,timestep_lower_bound);
  // Array holdings: A=dm_dt2   B=dm_dt3   C=dm_dt4   D=dm_dt5

  // Step 8
  vtmpA *= b52;
  vtmpA.Accumulate(b51,current_dm_dt);
  vtmpA.Accumulate(b53,vtmpB);
  vtmpA.Accumulate(b54,vtmpC);
  vtmpA.Accumulate(b55,vtmpD);
  AdjustState(stepsize,stepsize,*cstate,vtmpA,
              next_state_key.GetWriteReference(),dummy_error); // a5=1.0

  // Step 9
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  // Array holdings: A=dm_dt6   B=dm_dt3   C=dm_dt4   D=dm_dt5

  // Step 10
  OC_INDEX i;
  const OC_INDEX size = cstate->mesh->Size();
  for(i=0;i<size;i++) {
    ThreeVector dm_dt3 = vtmpB[i];
    ThreeVector dm_dt6 = vtmpA[i];
    vtmpA[i] = b63 * dm_dt3  +  b66 * dm_dt6;  // k6(3,6)
    vtmpB[i] = dc3 * dm_dt3  +  dc6 * dm_dt6;  // dD(3,6)
  }
  // Array holdings: A=k6(3,6)   B=dD(3,6)   C=dm_dt4   D=dm_dt5

  // Step 11
  vtmpA.Accumulate(b61,current_dm_dt);   // Note: b62=0.0
  vtmpA.Accumulate(b64,vtmpC);
  vtmpA.Accumulate(b65,vtmpD);
  AdjustState(stepsize,stepsize,*cstate,vtmpA,
              next_state_key.GetWriteReference(),norm_error); // a6=1.0
  const Oxs_SimState& endstate
    = next_state_key.GetReadReference(); // Candidate next state
  OC_REAL8m total_E;
  GetEnergyDensity(endstate,temp_energy,&mxH_output.cache.value,
                   NULL,pE_pt,total_E);
  mxH_output.cache.state_id=endstate.Id();
  Calculate_dm_dt(endstate,mxH_output.cache.value,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);

  if(!endstate.AddDerivedData("Timestep lower bound",
                                timestep_lower_bound) ||
     !endstate.AddDerivedData("Max dm/dt",max_dm_dt) ||
     !endstate.AddDerivedData("pE/pt",pE_pt) ||
     !endstate.AddDerivedData("Total E",total_E) ||
     !endstate.AddDerivedData("dE/dt",dE_dt)) {
    throw Oxs_ExtError(this,
                 "Oxs_RungeKuttaEvolve::RungeKuttaFehlbergBase54:"
                 " Programming error; data cache already set.");
  }
  // Array holdings: A=dm_dt7   B=dD(3,6)   C=dm_dt4   D=dm_dt5

  // Step 12
  OC_REAL8m max_dD_sq=0.0;
  vtmpB.Accumulate(dc1,current_dm_dt);
  vtmpB.Accumulate(dc4,vtmpC);
  vtmpB.Accumulate(dc5,vtmpD);
  vtmpB.Accumulate(dc7,vtmpA);
  // Array holdings: A=dm_dt7   B=dD   C=dm_dt4   D=dm_dt5

  // next_state holds candidate next state, normalized and
  // with proper time field settings; see Step 11.  Note that
  // Step 11 also set norm_error.

  // Error estimate is max|m2a-m2b| = h*max|dD|
  for(i=0;i<size;i++) {
    OC_REAL8m magsq = vtmpB[i].MagSq();
    if(magsq>max_dD_sq) max_dD_sq = magsq;
  }

#else // OOMMF_THREADS
#if REPORT_TIME_RKDEVEL
timer[3].Start(); /**/
#endif // REPORT_TIME_RKDEVEL
  // Steps 1 and 2
  AdjustState(stepsize*a1,stepsize*b11,*cstate,current_dm_dt,
              next_state_key.GetWriteReference(),dummy_error);
#if REPORT_TIME_RKDEVEL
timer[3].Stop(); /**/
++timer_counts[3].pass_count;
 timer_counts[3].bytes += (cstate->mesh->Size())*(9*sizeof(OC_REAL8m));
 timer_counts[3].name = "RKF54 step 1";
#endif // REPORT_TIME_RKDEVEL

SpinDiff("Pt B",next_state_key.GetReadReference()); /**/
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
VecDiff("Pt B1 vtmpA",vtmpA); /**/
RealDiff("Pt B1 temp enery",temp_energy); /**/

#if REPORT_TIME_RKDEVEL
timer[4].Start(); /**/
#endif // REPORT_TIME_RKDEVEL
  {
    // Initialize thread structure
    vector<OC_REAL8m> vb; vector<const Oxs_MeshValue<ThreeVector>*> vs;
    vb.push_back(b21); vs.push_back(&current_dm_dt);
    _Oxs_RKEvolve_RKFBase54_ThreadB thread_data;
    Oxs_SimState& work_state = next_state_key.GetWriteReference();
    Initialize_Threaded_Calculate_dm_dt(*cstate,work_state,
                                        vtmpA,vtmpA,thread_data,
                                        stepsize /* mstep */,
                                        b22 /* b_dm_dt */,
                                        vb,vs,vtmpB);

    // Run threads
    Oxs_ThreadTree threadtree;
    threadtree.LaunchTree(thread_data,0);

    // Collect results
    Finalize_Threaded_Calculate_dm_dt(*cstate,thread_data,pE_pt,
                                      stepsize*a2 /* hstep */,
                                      work_state,
                                      max_dm_dt,dE_dt,
                                      timestep_lower_bound,dummy_error);
  }
#if REPORT_TIME_RKDEVEL
timer[4].Stop(); /**/
++timer_counts[4].pass_count;
 timer_counts[4].bytes += (cstate->mesh->Size())*(3*(7+1)*sizeof(OC_REAL8m));
 timer_counts[4].name = "RKF54 step 2";
#endif // REPORT_TIME_RKDEVEL

  // Steps 3 and 4
SpinDiff("Pt C",next_state_key.GetReadReference()); /**/
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpB,NULL,pE_pt);
VecDiff("Pt C1 vtmpB",vtmpB); /**/
#if REPORT_TIME_RKDEVEL
timer[5].Start(); /**/
#endif // REPORT_TIME_RKDEVEL
  {
    // Initialize thread structure
    vector<OC_REAL8m> vb; vector<const Oxs_MeshValue<ThreeVector>*> vs;
    vb.push_back(b31); vs.push_back(&current_dm_dt);
    vb.push_back(b32); vs.push_back(&vtmpA);
    _Oxs_RKEvolve_RKFBase54_ThreadB thread_data;
    Oxs_SimState& work_state = next_state_key.GetWriteReference();
    Initialize_Threaded_Calculate_dm_dt(*cstate,work_state,
                                        vtmpB,vtmpB,thread_data,
                                        stepsize /* mstep */,
                                        b33 /* b_dm_dt */,
                                        vb,vs,vtmpC);

    // Run threads
    Oxs_ThreadTree threadtree;
    threadtree.LaunchTree(thread_data,0);

    // Collect results
    Finalize_Threaded_Calculate_dm_dt(*cstate,thread_data,pE_pt,
                                      stepsize*a3 /* hstep */,
                                      work_state,
                                      max_dm_dt,dE_dt,
                                      timestep_lower_bound,dummy_error);
  }
#if REPORT_TIME_RKDEVEL
timer[5].Stop(); /**/
++timer_counts[5].pass_count;
 timer_counts[5].bytes += (cstate->mesh->Size())*(3*(7+2)*sizeof(OC_REAL8m));
 timer_counts[5].name = "RKF54 step 4";
#endif // REPORT_TIME_RKDEVEL

  // Steps 5 and 6
SpinDiff("Pt D",next_state_key.GetReadReference()); /**/
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpC,NULL,pE_pt);
#if REPORT_TIME_RKDEVEL
timer[6].Start(); /**/
#endif // REPORT_TIME_RKDEVEL
  {
    // Initialize thread structure
    vector<OC_REAL8m> vb; vector<const Oxs_MeshValue<ThreeVector>*> vs;
    vb.push_back(b41); vs.push_back(&current_dm_dt);
    vb.push_back(b42); vs.push_back(&vtmpA);
    vb.push_back(b43); vs.push_back(&vtmpB);
    _Oxs_RKEvolve_RKFBase54_ThreadB thread_data;
    Oxs_SimState& work_state = next_state_key.GetWriteReference();
    Initialize_Threaded_Calculate_dm_dt(*cstate,work_state,
                                        vtmpC,vtmpC,thread_data,
                                        stepsize /* mstep */,
                                        b44 /* b_dm_dt */,
                                        vb,vs,vtmpD);

    // Run threads
    Oxs_ThreadTree threadtree;
    threadtree.LaunchTree(thread_data,0);

    // Collect results
    Finalize_Threaded_Calculate_dm_dt(*cstate,thread_data,pE_pt,
                                      stepsize*a4 /* hstep */,
                                      work_state,
                                      max_dm_dt,dE_dt,
                                      timestep_lower_bound,dummy_error);
  }
#if REPORT_TIME_RKDEVEL
timer[6].Stop(); /**/
++timer_counts[6].pass_count;
 timer_counts[6].bytes += (cstate->mesh->Size())*(3*(7+3)*sizeof(OC_REAL8m));
 timer_counts[6].name = "RKF54 step 6";
#endif // REPORT_TIME_RKDEVEL

  // Steps 7 and 8
SpinDiff("Pt E",next_state_key.GetReadReference()); /**/
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpD,NULL,pE_pt);
#if REPORT_TIME_RKDEVEL
timer[7].Start(); /**/
#endif // REPORT_TIME_RKDEVEL
  {
    // Initialize thread structure
    vector<OC_REAL8m> vb; vector<const Oxs_MeshValue<ThreeVector>*> vs;
    vb.push_back(b51); vs.push_back(&current_dm_dt);
    vb.push_back(b52); vs.push_back(&vtmpA);
    vb.push_back(b53); vs.push_back(&vtmpB);
    vb.push_back(b54); vs.push_back(&vtmpC);
    _Oxs_RKEvolve_RKFBase54_ThreadB thread_data;
    Oxs_SimState& work_state = next_state_key.GetWriteReference();
    Initialize_Threaded_Calculate_dm_dt(*cstate,work_state,
                                        vtmpD,vtmpD,thread_data,
                                        stepsize /* mstep */,
                                        b55 /* b_dm_dt */,
                                        vb,vs,vtmpA);

    // Run threads
    Oxs_ThreadTree threadtree;
    threadtree.LaunchTree(thread_data,0);

    // Collect results
    Finalize_Threaded_Calculate_dm_dt(*cstate,thread_data,pE_pt,
                                      stepsize,   /* hstep; a5==1 */
                                      work_state,
                                      max_dm_dt,dE_dt,
                                      timestep_lower_bound,dummy_error);
  }
#if REPORT_TIME_RKDEVEL
timer[7].Stop(); /**/
++timer_counts[7].pass_count;
 timer_counts[7].bytes += (cstate->mesh->Size())*(3*(7+4)*sizeof(OC_REAL8m));
 timer_counts[7].name = "RKF54 step 8";
#endif // REPORT_TIME_RKDEVEL

SpinDiff("Pt F",next_state_key.GetReadReference()); /**/
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
VecDiff("Pt F1 vtmpA",vtmpA); /**/
#if REPORT_TIME_RKDEVEL
timer[8].Start(); /**/
#endif // REPORT_TIME_RKDEVEL
  {
    // Initialize thread structure
    vector<OC_REAL8m> vb1;
    vector<OC_REAL8m> vb2;
    vector<const Oxs_MeshValue<ThreeVector>*> vs;
    vb1.push_back(b61); vb2.push_back(dc1); vs.push_back(&current_dm_dt);
    vb1.push_back(b63); vb2.push_back(dc3); vs.push_back(&vtmpB);
    vb1.push_back(b64); vb2.push_back(dc4); vs.push_back(&vtmpC);
    vb1.push_back(b65); vb2.push_back(dc5); vs.push_back(&vtmpD);

    _Oxs_RKEvolve_RKFBase54_ThreadC thread_data;
    Oxs_SimState& work_state = next_state_key.GetWriteReference();
    Initialize_Threaded_Calculate_dm_dt(*cstate,work_state,
                                        vtmpA,vtmpA,thread_data,
                                        stepsize /* mstep */,
                                        b66 /* b1_dm_dt */,
                                        dc6 /* b2_dm_dt */,
                                        vb1,vb2,vs,vtmpB);

    // Run threads
    Oxs_ThreadTree threadtree;
    threadtree.LaunchTree(thread_data,0);

    // Collect results
    Finalize_Threaded_Calculate_dm_dt(*cstate,thread_data,pE_pt,
                                      stepsize,   /* hstep; a6==1 */
                                      work_state,
                                      max_dm_dt,dE_dt,
                                      timestep_lower_bound,norm_error);
    // Array holdings: A=dm_dt6   B=dD(1,3,4,5,6)   C=dm_dt4   D=dm_dt5
  }
  const Oxs_SimState& endstate
    = next_state_key.GetReadReference(); // Candidate next state
#if REPORT_TIME_RKDEVEL
timer[8].Stop(); /**/
++timer_counts[8].pass_count;
 timer_counts[8].bytes += (cstate->mesh->Size())*(3*(7+4)*sizeof(OC_REAL8m));
 timer_counts[8].name = "RKF54 step 9";
#endif // REPORT_TIME_RKDEVEL
  OC_REAL8m total_E;
SpinDiff("Pt G",endstate); /**/
  GetEnergyDensity(endstate,temp_energy,&mxH_output.cache.value,
                   NULL,pE_pt,total_E);
  mxH_output.cache.state_id=endstate.Id();
  OC_REAL8m max_dD_sq=0.0;

#if REPORT_TIME_RKDEVEL
timer[9].Start(); /**/
#endif // REPORT_TIME_RKDEVEL
  {
    // Initialize thread structure
    _Oxs_RKEvolve_RKFBase54_ThreadD thread_data;
    Initialize_Threaded_Calculate_dm_dt(endstate,mxH_output.cache.value,vtmpA,
                                        thread_data,dc7,vtmpB);
    // Run threads
    Oxs_ThreadTree threadtree;
    threadtree.LaunchTree(thread_data,0);

    // Collect results
    Finalize_Threaded_Calculate_dm_dt(thread_data,pE_pt,
                                      max_dm_dt,dE_dt,
                                      timestep_lower_bound,max_dD_sq);
  }
  // Array holdings: A=dm_dt7   B=dD(1,3,4,5,6)   C=dm_dt4   D=dm_dt5
#if REPORT_TIME_RKDEVEL
timer[9].Stop(); /**/
++timer_counts[9].pass_count;
 timer_counts[9].bytes += (cstate->mesh->Size())*(3*5*sizeof(OC_REAL8m));
 timer_counts[9].name = "RKF54 step 10";
#endif // REPORT_TIME_RKDEVEL

  if(!endstate.AddDerivedData("Timestep lower bound",
                                timestep_lower_bound) ||
     !endstate.AddDerivedData("Max dm/dt",max_dm_dt) ||
     !endstate.AddDerivedData("pE/pt",pE_pt) ||
     !endstate.AddDerivedData("Total E",total_E) ||
     !endstate.AddDerivedData("dE/dt",dE_dt)) {
    throw Oxs_ExtError(this,
                 "Oxs_RungeKuttaEvolve::RungeKuttaFehlbergBase54:"
                 " Programming error; data cache already set.");
  }

#endif // OOMMF_THREADS
  error_estimate = stepsize * sqrt(max_dD_sq);
  global_error_order = 5.0;

  new_energy_and_dmdt_computed = 1;

#if REPORT_TIME_RKDEVEL
timer[1].Stop(); /**/
++timer_counts[1].pass_count;
 timer_counts[1].name = "RKFB54 total";
#endif // REPORT_TIME_RKDEVEL

}

void Oxs_RungeKuttaEvolve::TakeRungeKuttaFehlbergStep54
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_BOOL& new_energy_and_dmdt_computed)
{
  RungeKuttaFehlbergBase54(RK547FC,stepsize,
     current_state_key,current_dm_dt,next_state_key,
     error_estimate,global_error_order,norm_error,
     new_energy_and_dmdt_computed);
}

void Oxs_RungeKuttaEvolve::TakeRungeKuttaFehlbergStep54M
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_BOOL& new_energy_and_dmdt_computed)
{
  RungeKuttaFehlbergBase54(RK547FM,stepsize,
     current_state_key,current_dm_dt,next_state_key,
     error_estimate,global_error_order,norm_error,
     new_energy_and_dmdt_computed);
}

void Oxs_RungeKuttaEvolve::TakeRungeKuttaFehlbergStep54S
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_BOOL& new_energy_and_dmdt_computed)
{
  RungeKuttaFehlbergBase54(RK547FS,stepsize,
     current_state_key,current_dm_dt,next_state_key,
     error_estimate,global_error_order,norm_error,
     new_energy_and_dmdt_computed);
}

OC_REAL8m Oxs_RungeKuttaEvolve::MaxDiff
(const Oxs_MeshValue<ThreeVector>& vecA,
 const Oxs_MeshValue<ThreeVector>& vecB)
{
  OC_INDEX size = vecA.Size();
  if(vecB.Size()!=size) {
    throw Oxs_ExtError(this,
                 "Oxs_RungeKuttaEvolve::MaxDiff:"
                 " Import MeshValues incompatible (different lengths).");
  }
  OC_REAL8m max_magsq = 0.0;
  for(OC_INDEX i=0;i<size;i++) {
    ThreeVector vtemp = vecB[i] - vecA[i];
    OC_REAL8m magsq = vtemp.MagSq();
    if(magsq>max_magsq) max_magsq = magsq;
  }
  return sqrt(max_magsq);
}

void Oxs_RungeKuttaEvolve::AdjustStepHeadroom(OC_INT4m step_reject)
{ // step_reject should be 0 or 1, reflecting whether the current
  // step was rejected or not.  This routine updates reject_ratio
  // and adjusts step_headroom appropriately.

  // First adjust reject_ratio, weighing mostly the last
  // thirty or so results.
  reject_ratio = (31*reject_ratio + step_reject)/32.;

  // Adjust step_headroom
  if(reject_ratio>reject_goal && step_reject>0) {
    // Reject ratio too high and getting worse
    step_headroom *= 0.925;
  }
  if(reject_ratio<reject_goal && step_reject<1) {
    // Reject ratio too small and getting smaller
    step_headroom *= 1.075;
  }

  if(step_headroom>max_step_headroom) step_headroom=max_step_headroom;
  if(step_headroom<min_step_headroom) step_headroom=min_step_headroom;
}

////////////////////////////////////////////////////////////////////////
/// Oxs_RungeKuttaEvolve::ComputeEnergyChange  /////////////////////////
///    Threaded and non-threaded versions      /////////////////////////
////////////////////////////////////////////////////////////////////////
#if !OOMMF_THREADS

void Oxs_RungeKuttaEvolve::ComputeEnergyChange
(const Oxs_Mesh* mesh,
 const Oxs_MeshValue<OC_REAL8m>& current_energy,
 const Oxs_MeshValue<OC_REAL8m>& candidate_energy,
 OC_REAL8m& dE,OC_REAL8m& var_dE,OC_REAL8m& total_E)
{ // Computes cellwise difference between energies, and variance.
  // Export total_E is "current" energy (used for stepsize control).
  Nb_Xpfloat dE_xp      = 0.0;
  Nb_Xpfloat var_dE_xp  = 0.0;
  Nb_Xpfloat total_E_xp = 0.0;
  const OC_INDEX size = mesh->Size();
  for(OC_INDEX i=0;i<size;++i) {
    OC_REAL8m vol = mesh->Volume(i);
    OC_REAL8m e = vol*current_energy[i];
    OC_REAL8m new_e = vol*candidate_energy[i];
    total_E_xp += e;
    dE_xp += new_e - e;
    var_dE_xp += new_e*new_e + e*e;
  }
  total_E = total_E_xp.GetValue();
  dE      = dE_xp.GetValue();
  var_dE  = var_dE_xp.GetValue();
}

#else

class _Oxs_RKEvolve_RKFBase54_ComputeEnergyChange : public Oxs_ThreadRunObj {
public:
  // Note: The job basket is static, so only one "set" (tree) of this
  // class may be run at any one time.
  static Oxs_JobControl<OC_REAL8m> job_basket;

  const Oxs_Mesh* mesh;                                  // Import
  const Oxs_MeshValue<OC_REAL8m>* current_energy_ptr;    // Import
  const Oxs_MeshValue<OC_REAL8m>* candidate_energy_ptr;  // Import
  vector<OC_REAL8m> thread_dE;                           // Export
  vector<OC_REAL8m> thread_var_dE;                       // Export
  vector<OC_REAL8m> thread_total_E;                      // Export

  _Oxs_RKEvolve_RKFBase54_ComputeEnergyChange()
    : mesh(0),current_energy_ptr(0),candidate_energy_ptr(0) {}

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_RKEvolve_RKFBase54_ComputeEnergyChange::job_basket;

void _Oxs_RKEvolve_RKFBase54_ComputeEnergyChange::Cmd(int threadnumber,
                                                      void* /* data */)
{ // Imports (local copies)
  const Oxs_Mesh* work_mesh                        = mesh;
  const Oxs_MeshValue<OC_REAL8m>& current_energy   = *current_energy_ptr;
  const Oxs_MeshValue<OC_REAL8m>& candidate_energy = *candidate_energy_ptr;

  Nb_Xpfloat dE0=0.0, var_dE0=0.0, total_E0=0.0;
  Nb_Xpfloat dE1=0.0, var_dE1=0.0, total_E1=0.0;
  Nb_Xpfloat dE2=0.0, var_dE2=0.0, total_E2=0.0;
  Nb_Xpfloat dE3=0.0, var_dE3=0.0, total_E3=0.0;

  while(1) {
    OC_INDEX jstart,jstop;
    job_basket.GetJob(threadnumber,jstart,jstop);
    if(jstart>=jstop) break; // No more jobs

    OC_INDEX j;

    for(j=jstart; j < jstart + (jstop-jstart)%4 ; ++j) {
      OC_REAL8m vol = work_mesh->Volume(j);
      OC_REAL8m e = vol*current_energy[j];
      OC_REAL8m new_e = vol*candidate_energy[j];
      total_E0 += e;
      dE0 += new_e - e;
      var_dE0 += new_e*new_e + e*e;
    }

    for(;j<jstop;j+=4) {
      OC_REAL8m vol0 = work_mesh->Volume(j);
      OC_REAL8m vol1 = work_mesh->Volume(j+1);

      OC_REAL8m e0 = vol0*current_energy[j];
      OC_REAL8m e1 = vol1*current_energy[j+1];

      OC_REAL8m new_e0 = vol0*candidate_energy[j];
      OC_REAL8m new_e1 = vol1*candidate_energy[j+1];

      total_E0 += e0;
      dE0 += new_e0 - e0;
      var_dE0 += new_e0*new_e0 + e0*e0;

      total_E1 += e1;
      dE1 += new_e1 - e1;
      var_dE1 += new_e1*new_e1 + e1*e1;

      OC_REAL8m vol2 = work_mesh->Volume(j+2);
      OC_REAL8m vol3 = work_mesh->Volume(j+3);

      OC_REAL8m e2 = vol2*current_energy[j+2];
      OC_REAL8m e3 = vol3*current_energy[j+3];
      OC_REAL8m new_e2 = vol2*candidate_energy[j+2];
      OC_REAL8m new_e3 = vol3*candidate_energy[j+3];

      total_E2 += e2;
      dE2 += new_e2 - e2;
      var_dE2 += new_e2*new_e2 + e2*e2;

      total_E3 += e3;
      dE3 += new_e3 - e3;
      var_dE3 += new_e3*new_e3 + e3*e3;
    }
  }

  dE0      += dE1 + dE2 + dE3;
  var_dE0  += var_dE1 + var_dE2 + var_dE3;
  total_E0 += total_E1 + total_E2 + total_E3;

  thread_dE[threadnumber] = dE0.GetValue();
  thread_var_dE[threadnumber] = var_dE0.GetValue();
  thread_total_E[threadnumber] = total_E0.GetValue();
}

void
Oxs_RungeKuttaEvolve::ComputeEnergyChange // Threaded version
(const Oxs_Mesh* mesh,
 const Oxs_MeshValue<OC_REAL8m>& current_energy,
 const Oxs_MeshValue<OC_REAL8m>& candidate_energy,
 OC_REAL8m& dE,OC_REAL8m& var_dE,OC_REAL8m& total_E)
{ // Computes cellwise difference between energies, and variance.
  // Export total_E is "current" energy (used for stepsize control).

  const int thread_count = Oc_GetMaxThreadCount();

  _Oxs_RKEvolve_RKFBase54_ComputeEnergyChange thread_data;

  // Initialize thread structure
  thread_data.job_basket.Init(thread_count,
                              current_energy.GetArrayBlock());
  thread_data.mesh = mesh;
  thread_data.current_energy_ptr = &current_energy;
  thread_data.candidate_energy_ptr = &candidate_energy;
  thread_data.thread_dE.clear();
  thread_data.thread_dE.resize(thread_count,0.0);
  thread_data.thread_var_dE.clear();
  thread_data.thread_var_dE.resize(thread_count,0.0);
  thread_data.thread_total_E.clear();
  thread_data.thread_total_E.resize(thread_count,0.0);

  // Run threads
  Oxs_ThreadTree threadtree;
  threadtree.LaunchTree(thread_data,0);

  // Collect results
  dE = thread_data.thread_dE[0];
  var_dE = thread_data.thread_var_dE[0];
  total_E = thread_data.thread_total_E[0];
  for(int i=1;i<thread_count;++i) {
    dE += thread_data.thread_dE[i];
    var_dE += thread_data.thread_var_dE[i];
    total_E += thread_data.thread_total_E[i];
  }
}

#endif


OC_BOOL
Oxs_RungeKuttaEvolve::InitNewStage
(const Oxs_TimeDriver* /* driver */,
 Oxs_ConstKey<Oxs_SimState> state,
 Oxs_ConstKey<Oxs_SimState> prevstate)
{
  // Update derived data in state.
  const Oxs_SimState& cstate = state.GetReadReference();
  const Oxs_SimState* pstate_ptr = prevstate.GetPtr();
  UpdateDerivedOutputs(cstate,pstate_ptr);

  // Note 1: state is a copy-by-value import, so its read lock
  //         will be released on exit.
  // Note 2: pstate_ptr will be NULL if prevstate has
  //         "INVALID" status.

  return 1;
}

OC_BOOL
Oxs_RungeKuttaEvolve::Step(const Oxs_TimeDriver* driver,
                      Oxs_ConstKey<Oxs_SimState> current_state_key,
                      const Oxs_DriverStepInfo& step_info,
                      Oxs_Key<Oxs_SimState>& next_state_key)
{
#if REPORT_TIME
  steponlytime.Start();
#endif // REPORT_TIME

  const OC_REAL8m bad_energy_cut_ratio = 0.75;
  const OC_REAL8m bad_energy_step_increase = 1.3;

  const OC_REAL8m previous_next_timestep = next_timestep;

  const Oxs_SimState& cstate = current_state_key.GetReadReference();

  CheckCache(cstate);

  // Note if start_dm or start_dt is being used
  OC_BOOL start_cond_active=0;
  if(next_timestep<=0.0 ||
     (cstate.stage_iteration_count<1
      && step_info.current_attempt_count==0)) {
    if(cstate.stage_number==0
       || stage_init_step_control == SISC_START_COND) {
      start_cond_active = 1;
    } else if(stage_init_step_control == SISC_CONTINUOUS) {
      start_cond_active = 0;  // Safety
    } else if(stage_init_step_control == SISC_AUTO) {
      // Automatic detection based on energy values across
      // stage boundary.
      OC_REAL8m total_E,E_diff;
      if(cstate.GetDerivedData("Total E",total_E) &&
         cstate.GetDerivedData("Delta E",E_diff)  &&
         fabs(E_diff) <= 256*OC_REAL8_EPSILON*fabs(total_E) ) {
        // The factor of 256 in the preceding line is a fudge factor,
        // selected with no particular justification.
        start_cond_active = 0;  // Continuous case
      } else {
        start_cond_active = 1;  // Assume discontinuous
      }
    } else {
      throw Oxs_ExtError(this,
           "Oxs_RungeKuttaEvolve::Step; Programming error:"
           " unrecognized stage_init_step_control value");
    }
  }

  // Negotiate timestep, and also initialize both next_state and
  // temp_state structures.
  Oxs_SimState* work_state = &(next_state_key.GetWriteReference());
  OC_BOOL force_step=0,driver_set_step=0;
  NegotiateTimeStep(driver,cstate,*work_state,next_timestep,
                    start_cond_active,force_step,driver_set_step);
  OC_REAL8m stepsize = work_state->last_timestep;
  work_state=NULL; // Safety: disable pointer

  // Step
  OC_REAL8m error_estimate,norm_error;
  OC_REAL8m global_error_order;
  OC_BOOL new_energy_and_dmdt_computed;
  OC_BOOL reject_step=0;
  (this->*rkstep_ptr)(stepsize,current_state_key,
                      dm_dt_output.cache.value,
                      next_state_key,
                      error_estimate,global_error_order,norm_error,
                      new_energy_and_dmdt_computed);
  const Oxs_SimState& nstate = next_state_key.GetReadReference();
  driver->FillStateDerivedData(cstate,nstate);

  OC_REAL8m max_dm_dt;
  cstate.GetDerivedData("Max dm/dt",max_dm_dt);
  OC_REAL8m reference_stepsize = stepsize;
  if(driver_set_step) reference_stepsize = previous_next_timestep;
  OC_BOOL good_step = CheckError(global_error_order,error_estimate,
                              stepsize,reference_stepsize,
                              max_dm_dt,next_timestep);
  OC_REAL8m timestep_grit = PositiveTimestepBound(max_dm_dt);

  /// Note: Might want to use average or larger of max_dm_dt
  /// and new_max_dm_dt (computed below.)

  if(!good_step && !force_step) {
    // Bad step; The only justfication to do energy and dm_dt
    // computation would be to get an energy-based stepsize
    // adjustment estimate, which we only need to try if 
    // next_timestep is larger than cut applied by energy
    // rejection code (i.e., bad_energy_cut_ratio).
    if(next_timestep<=stepsize*bad_energy_cut_ratio) {
      AdjustStepHeadroom(1);
#if REPORT_TIME
      steponlytime.Stop();
#endif // REPORT_TIME
      return 0; // Don't bother with energy calculation
    }
    reject_step=1; // Otherwise, mark step rejected and see what energy
    /// info suggests for next stepsize
  }

  if(start_cond_active && !force_step) {
    if(start_dm>=0.0) {
      // Check that no spin has moved by more than start_dm
      OC_REAL8m diff = MaxDiff(cstate.spin,nstate.spin);
      if(diff>start_dm) {
        next_timestep = step_headroom * stepsize * (start_dm/diff);
        if(next_timestep<=stepsize*bad_energy_cut_ratio) {
          AdjustStepHeadroom(1);
#if REPORT_TIME
          steponlytime.Stop();
#endif // REPORT_TIME
          return 0; // Don't bother with energy calculation
        }
        reject_step=1; // Otherwise, mark step rejected and see what energy
        /// info suggests for next stepsize
      }
    }
  }

#ifdef OLDE_CODE
  if(norm_error>0.0005) {
    fprintf(stderr,
            "Iteration %u passed error check; norm_error=%8.5f\n",
            nstate.iteration_count,norm_error);
  } /**/
#endif // OLDE_CODE

  // Energy timestep control:
  //   The relationship between energy error and stepsize appears to be
  // highly nonlinear, so that estimating appropriate stepsize from energy
  // increase is difficult.  Perhaps it is possible to include energy
  // interpolation into RK step routines, but for the present we just
  // reduce the step by a fixed ratio if we detect energy increase beyond
  // that which can be attributed to numerical errors.  Of course, this
  // doesn't take into account the expected energy decrease (which depends
  // on the damping ratio alpha), which is another reason to try to build
  // it into the high order RK step routines.
  OC_REAL8m pE_pt,new_pE_pt=0.;
  cstate.GetDerivedData("pE/pt",pE_pt);
  if(new_energy_and_dmdt_computed) {
    nstate.GetDerivedData("pE/pt",new_pE_pt);
  } else {
    OC_REAL8m new_total_E;
    GetEnergyDensity(nstate,temp_energy,
                     &mxH_output.cache.value,
                     NULL,new_pE_pt,new_total_E);
    mxH_output.cache.state_id=nstate.Id();
    if(!nstate.AddDerivedData("pE/pt",new_pE_pt)) {
      throw Oxs_ExtError(this,
           "Oxs_RungeKuttaEvolve::Step:"
           " Programming error; data cache (pE/pt) already set.");
    }
    if(!nstate.AddDerivedData("Total E",new_total_E)) {
      throw Oxs_ExtError(this,
           "Oxs_RungeKuttaEvolve::Step:"
           " Programming error; data cache (Total E) already set.");
    }
  }

#if REPORT_TIME_RKDEVEL
timer[0].Start(); /**/
#endif // REPORT_TIME_RKDEVEL
  OC_REAL8m dE,var_dE,total_E;
  ComputeEnergyChange(nstate.mesh,energy,temp_energy,dE,var_dE,total_E);
  if(!nstate.AddDerivedData("Delta E",dE)) {
    throw Oxs_ExtError(this,
         "Oxs_RungeKuttaEvolve::Step:"
         " Programming error; data cache (Delta E) already set.");
  }
#if REPORT_TIME_RKDEVEL
timer[0].Stop(); /**/
++timer_counts[0].pass_count;
 timer_counts[0].bytes += (nstate.mesh->Size())*(2*sizeof(OC_REAL8m));
 timer_counts[0].name = "ComputeEnergyChange";
#endif // REPORT_TIME_RKDEVEL

  if(expected_energy_precision>=0.) {
    var_dE *= expected_energy_precision * expected_energy_precision;
    /// Variance, assuming error in each energy[i] term is independent,
    /// uniformly distributed, 0-mean, with range
    ///        +/- expected_energy_precision*energy[i].
    /// It would probably be better to get an error estimate directly
    /// from each energy term.
    OC_REAL8m E_numerror = OC_MAX(fabs(total_E)*expected_energy_precision,
                               2*sqrt(var_dE));
    OC_REAL8m pE_pt_max = OC_MAX(pE_pt,new_pE_pt); // Might want to
    /// change this from a constant to a linear function in timestep.

    OC_REAL8m reject_dE = 2*E_numerror + pE_pt_max * stepsize;
    if(dE>reject_dE) {
      OC_REAL8m teststep = bad_energy_cut_ratio*stepsize;
      if(teststep<next_timestep) {
        next_timestep=teststep;
        max_step_increase = bad_energy_step_increase;
        // Damp the enthusiasm of the RK stepper routine for
        // stepsize growth, for a step or two.
      }
      if(!force_step) reject_step=1;
    }
  }

  // Guarantee that next_timestep is large enough to move
  // at least one spin by an amount perceptible to the
  // floating point representation.
  if(next_timestep<timestep_grit) next_timestep = timestep_grit;

  if(!force_step && reject_step) {
    AdjustStepHeadroom(1);
#if REPORT_TIME
    steponlytime.Stop();
#endif // REPORT_TIME
    return 0;
  }

  // Otherwise, we are accepting the new step.

  // Calculate dm_dt at new point, and fill in cache.
  // This branch is not active when using the RKF54 kernel, but is
  // when using the RK4 kernel.
  if(new_energy_and_dmdt_computed) {
    dm_dt_output.cache.value.Swap(vtmpA);
  } else {
    OC_REAL8m new_max_dm_dt,new_dE_dt,new_timestep_lower_bound;
    if(mxH_output.cache.state_id != nstate.Id()) { // Safety
    throw Oxs_ExtError(this,
         "Oxs_RungeKuttaEvolve::Step:"
         " Programming error; mxH_output cache improperly filled.");
    }
#if !OOMMF_THREADS
    Calculate_dm_dt(nstate,
                    mxH_output.cache.value,new_pE_pt,
                    dm_dt_output.cache.value,new_max_dm_dt,
                    new_dE_dt,new_timestep_lower_bound);
#else // OOMMF_THREADS
    { // Threaded version of Calculate_dm_dt:
      // Fill out alpha and gamma meshvalue arrays, as necessary.
      if(mesh_id != nstate.mesh->Id() || !gamma.CheckMesh(nstate.mesh)
         || !alpha.CheckMesh(nstate.mesh)) {
        UpdateMeshArrays(nstate.mesh);
      }

      // Initialize thread structure
      const int thread_count = Oc_GetMaxThreadCount();
      _Oxs_RKEvolve_Step_ThreadA thread_data(thread_count,
                                             nstate.Ms->GetArrayBlock());
      thread_data.mesh_ptr   = nstate.mesh;
      thread_data.Ms_ptr     = nstate.Ms;
      thread_data.gamma_ptr  = &gamma;
      thread_data.alpha_ptr  = &alpha;
      thread_data.mxH_ptr    = &mxH_output.cache.value;
      thread_data.spin_ptr   = &nstate.spin;
      thread_data.dm_dt_ptr  = &dm_dt_output.cache.value;
      thread_data.do_precess = do_precess;

      // Run threads
      Oxs_ThreadTree threadtree;
      threadtree.LaunchTree(thread_data,0);

      // Collect results
      new_max_dm_dt = thread_data.thread_max_dm_dt[0];
      new_dE_dt = thread_data.thread_dE_dt[0];
      for(int i=1;i<thread_count;++i) {
        if(thread_data.thread_max_dm_dt[i] > new_max_dm_dt) {
          new_max_dm_dt = thread_data.thread_max_dm_dt[i];
        }
        new_dE_dt += thread_data.thread_dE_dt[i];
      }
      new_dE_dt = -1 * MU0 * new_dE_dt + new_pE_pt;
      /// The first term is (partial E/partial M)*dM/dt, the
      /// second term is (partial E/partial t)*dt/dt.  Note that,
      /// provided Ms_[i]>=0, that by constructions dE_dt_sum above
      /// is always non-negative, so dE_dt_ can only be made positive
      /// by positive pE_pt_.

      // Get bound on smallest stepsize that would actually
      // change spin new_max_dm_dt_index:
      new_timestep_lower_bound = PositiveTimestepBound(new_max_dm_dt);
    }
#endif // OOMMF_THREADS
    if(!nstate.AddDerivedData("Timestep lower bound",
                              new_timestep_lower_bound) ||
       !nstate.AddDerivedData("Max dm/dt",new_max_dm_dt) ||
       !nstate.AddDerivedData("dE/dt",new_dE_dt)) {
      throw Oxs_ExtError(this,
                           "Oxs_RungeKuttaEvolve::Step:"
                           " Programming error; data cache already set.");
    }
  }
  dm_dt_output.cache.state_id = nstate.Id();

  energy.Swap(temp_energy);
  energy_state_id = nstate.Id();

  AdjustStepHeadroom(0);
  if(!force_step && max_step_increase<max_step_increase_limit) {
    max_step_increase *= max_step_increase_adj_ratio;
  }
  if(max_step_increase>max_step_increase_limit) {
    max_step_increase = max_step_increase_limit;
  }

#if REPORT_TIME
  steponlytime.Stop();
#endif // REPORT_TIME
  return 1; // Accept step
}

void
Oxs_RungeKuttaEvolve::UpdateDerivedOutputs(const Oxs_SimState& state,
					   const Oxs_SimState* prevstate_ptr)
{ // This routine fills all the Oxs_RungeKuttaEvolve Oxs_ScalarOutput's to
  // the appropriate value based on the import "state", and any of
  // Oxs_VectorOutput's that have CacheRequest enabled are filled.
  // It also makes sure all the expected WOO objects in state are
  // filled.
  max_dm_dt_output.cache.state_id
    = dE_dt_output.cache.state_id
    = delta_E_output.cache.state_id
    = 0;  // Mark change in progress

  OC_REAL8m dummy_value;
  if(!state.GetDerivedData("Max dm/dt",max_dm_dt_output.cache.value) ||
     !state.GetDerivedData("dE/dt",dE_dt_output.cache.value) ||
     !state.GetDerivedData("Delta E",delta_E_output.cache.value) ||
     !state.GetDerivedData("pE/pt",dummy_value) ||
     !state.GetDerivedData("Total E",dummy_value) ||
     !state.GetDerivedData("Timestep lower bound",dummy_value) ||
     (dm_dt_output.GetCacheRequestCount()>0
      && dm_dt_output.cache.state_id != state.Id()) ||
     (mxH_output.GetCacheRequestCount()>0
      && mxH_output.cache.state_id != state.Id())) {

    // Missing at least some data, so calculate from scratch

    // Check ahead for trouble computing Delta E:
    if(!state.GetDerivedData("Delta E",dummy_value)
       && state.previous_state_id != 0
       && prevstate_ptr!=NULL
       && state.previous_state_id == prevstate_ptr->Id()) {
      OC_REAL8m old_E;
      if(!prevstate_ptr->GetDerivedData("Total E",old_E)) {
	// Previous state doesn't have stored Total E.  Compute it
	// now.
	OC_REAL8m old_pE_pt;
	GetEnergyDensity(*prevstate_ptr,energy,NULL,NULL,old_pE_pt,old_E);
	prevstate_ptr->AddDerivedData("Total E",old_E);
      }
    }

    // Calculate H and mxH outputs
    Oxs_MeshValue<ThreeVector>& mxH = mxH_output.cache.value;
    OC_REAL8m pE_pt,total_E;
    GetEnergyDensity(state,energy,&mxH,NULL,pE_pt,total_E);
    energy_state_id=state.Id();
    mxH_output.cache.state_id=state.Id();
    if(!state.GetDerivedData("pE/pt",dummy_value)) {
      state.AddDerivedData("pE/pt",pE_pt);
    }
    if(!state.GetDerivedData("Total E",dummy_value)) {
      state.AddDerivedData("Total E",total_E);
    }

    // Calculate dm/dt, Max dm/dt and dE/dt
    Oxs_MeshValue<ThreeVector>& dm_dt
      = dm_dt_output.cache.value;
    dm_dt_output.cache.state_id=0;
    OC_REAL8m timestep_lower_bound;
    Calculate_dm_dt(state,mxH,pE_pt,dm_dt,
                    max_dm_dt_output.cache.value,
                    dE_dt_output.cache.value,timestep_lower_bound);
    dm_dt_output.cache.state_id=state.Id();

    if(!state.GetDerivedData("Max dm/dt",dummy_value)) {
      state.AddDerivedData("Max dm/dt",max_dm_dt_output.cache.value);
    }

    if(!state.GetDerivedData("dE/dt",dummy_value)) {
      state.AddDerivedData("dE/dt",dE_dt_output.cache.value);
    }

    if(!state.GetDerivedData("Timestep lower bound",dummy_value)) {
      state.AddDerivedData("Timestep lower bound",
                           timestep_lower_bound);
    }

    if(!state.GetDerivedData("Delta E",dummy_value)) {
      if(state.previous_state_id == 0) {
        // No previous state
        dummy_value = 0.0;
      } else if(prevstate_ptr!=NULL
                && state.previous_state_id == prevstate_ptr->Id()) {
        OC_REAL8m old_E;
        if(!prevstate_ptr->GetDerivedData("Total E",old_E)) {
          throw Oxs_ExtError(this,
                             "Oxs_RungeKuttaEvolve::UpdateDerivedOutputs:"
                             " \"Total E\" not set in previous state.");
        }
        dummy_value = total_E - old_E; // This is less accurate than adding
        /// up the cell-by-cell differences (as is used in the main code),
        /// but w/o the entire energy map for prevstate this is the best
        /// we can do.
      } else {
        throw Oxs_ExtError(this,
           "Oxs_RungeKuttaEvolve::UpdateDerivedOutputs:"
           " Can't derive Delta E from single state.");
      }
      state.AddDerivedData("Delta E",dummy_value);
    }
    delta_E_output.cache.value=dummy_value;

  }

  max_dm_dt_output.cache.value*=(180e-9/PI);
  /// Convert from radians/second to deg/ns

  max_dm_dt_output.cache.state_id
    = dE_dt_output.cache.state_id
    = delta_E_output.cache.state_id
    = state.Id();
}
