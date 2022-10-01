/* FILE: xf_stt.cc         -*-Mode: c++-*-
 *
 * Spin-transfer torque (STT), derived from Oxs_Energy class,
 * specified from a Tcl proc.
 *
 */

#include <cfloat>
#include <string>

#include "oc.h"
#include "nb.h"
#include "xf_stt.h"

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Xf_STT);

/* End includes */


// Constructor
Xf_STT::Xf_STT(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    mesh_id(0), number_of_stages(0)
{
  // Process arguments
  number_of_stages = GetUIntInitValue("stage_count",0);
  if(HasInitValue("P")) {
    OXS_GET_INIT_EXT_OBJECT("P",Oxs_ScalarField,P_fixed_init);
  } else {
    if(HasInitValue("P_fixed") && HasInitValue("P_free")) {
      OXS_GET_INIT_EXT_OBJECT("P_fixed",Oxs_ScalarField,P_fixed_init);
      OXS_GET_INIT_EXT_OBJECT("P_free",Oxs_ScalarField,P_free_init);
    } else {
      // Default setting: create a uniform scalar field
      // object with value 0.4.  See mjd's NOTES III,
      // 13-Aug-2004, p 196.
      Oxs_Ext* foo=MakeNew("Oxs_UniformScalarField",director,"value 0.4");
      P_fixed_init.SetAsOwner(dynamic_cast<Oxs_ScalarField*>(foo));
    }
  }
  if(P_free_init.GetPtr()==NULL) {
    P_free_init.SetAsNonOwner(P_fixed_init.GetPtr());
    /// This is a little dangerous, because P_fixed_init
    /// has no way to know when the Oxs_ScalarField object
    /// is it pointing to is deleted.  But we should be
    /// okay as long as we don't delete P_free_init
    /// except as part of Xf_ThermSpinXferEvolve instance
    /// destruction, because P_fixed_init won't access
    /// its pointer in that destructor.  Moreover,
    /// if P_free_init points to an Oxs_Ext object
    /// outside this instance (i.e., P_free_init
    /// doesn't "own" the Oxs_ScalarField object
    /// either), then we are guaranteed by Oxs_Director
    /// problem destruction that this instance will
    /// be destroyed before the referenced Oxs_Ext
    /// object.
  }

  if(HasInitValue("Lambda")) {
    OXS_GET_INIT_EXT_OBJECT("Lambda",Oxs_ScalarField,Lambda_fixed_init);
  } else {
    if(HasInitValue("Lambda_fixed") && HasInitValue("Lambda_free")) {
      OXS_GET_INIT_EXT_OBJECT("Lambda_fixed",
			      Oxs_ScalarField,Lambda_fixed_init);
      OXS_GET_INIT_EXT_OBJECT("Lambda_free",
			      Oxs_ScalarField,Lambda_free_init);
    } else {
      // Default setting: create a uniform scalar field
      // object with value 2.0.  See mjd's NOTES III,
      // 13-Aug-2004, p 196.
      Oxs_Ext* foo=MakeNew("Oxs_UniformScalarField",director,"value 2.0");
      Lambda_fixed_init.SetAsOwner(dynamic_cast<Oxs_ScalarField*>(foo));
    }
  }
  if(Lambda_free_init.GetPtr()==NULL) {
    Lambda_free_init.SetAsNonOwner(Lambda_fixed_init.GetPtr());
    /// Dangerous, as noted above.
  }

  if(HasInitValue("eps_prime")) {
    OXS_GET_INIT_EXT_OBJECT("eps_prime",Oxs_ScalarField,eps_prime_init);
  } else {
    // Default is zero
    Oxs_Ext* foo=MakeNew("Oxs_UniformScalarField",director,"value 0.0");
    eps_prime_init.SetAsOwner(dynamic_cast<Oxs_ScalarField*>(foo));
  }

  // Direction of current flow.  This is *opposite* the direction
  // of electron flow.
  J_direction = JD_INVALID;
  String J_direction_str = GetStringInitValue("J_direction","-z");
  Oxs_ToLower(J_direction_str);
  if(J_direction_str.compare("+x")==0) {
    J_direction = JD_X_POS;
  } else  if(J_direction_str.compare("-x")==0) {
    J_direction = JD_X_NEG;
  } else  if(J_direction_str.compare("+y")==0) {
    J_direction = JD_Y_POS;
  } else  if(J_direction_str.compare("-y")==0) {
    J_direction = JD_Y_NEG;
  } else  if(J_direction_str.compare("+z")==0) {
    J_direction = JD_Z_POS;
  } else  if(J_direction_str.compare("-z")==0) {
    J_direction = JD_Z_NEG;
  }
  if(J_direction == JD_INVALID) {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
		       " \"J_direction\" value must be one of"
		       " +x, -x, +y, -y, +z, -z");
  }

  // Base magnitude of the current flow.  Positive values mean
  // current flow is in the direction specified by J_direction.
  // Negative values mean actual current flow is in direction
  // opposite J_direction.
  OXS_GET_INIT_EXT_OBJECT("J",Oxs_ScalarField,J_init);    // Required

  // Stage-varying multiplier for J.  Note sign convention, as
  // outlined in previous stanza.
  if(HasInitValue("J_profile")) {
    has_J_profile = 1;
    String cmdoptreq = GetStringInitValue("J_profile_args",
					  "stage stage_time total_time");
    J_profile_opts.push_back(Nb_TclCommandLineOption("stage",1));
    J_profile_opts.push_back(Nb_TclCommandLineOption("stage_time",1));
    J_profile_opts.push_back(Nb_TclCommandLineOption("total_time",1));
    J_profile_cmd.SetBaseCommand(InstanceName(),
				 director->GetMifInterp(),
				 GetStringInitValue("J_profile"),
				 Nb_ParseTclCommandLineRequest(InstanceName(),
							       J_profile_opts,
							       cmdoptreq));
  }

  // Use m to modify mp in-flight?  Propagation direction is computed
  // using direction directly opposite J_direction (i.e., in the nominal
  // electron flow direction), without considering J or J_profile.
  mp_propagate = GetIntInitValue("propagate_mp",0);

  // "mp" is required input, but is actually used iff mp_propagate is 0.
  OXS_GET_INIT_EXT_OBJECT("mp",Oxs_VectorField,mp_init);

  // NOTE: Currently only coded up for J_direction = JD_X_*.

  // Use bog standard 2-pt estimate for dm/dx, or 4-pt?
  // NOTE: 4-pt currently only coded up for J_direction = JD_X_*.
  fourpt_derivative = GetIntInitValue("fourpt_derivative",0);


  VerifyAllInitArgsUsed();

  // Set up outputs
  ave_J_output.Setup(this,InstanceName(),"average J","A/m^2",0,
     &Xf_STT::UpdateSpinTorqueOutputs);
  Jmp_output.Setup(this,InstanceName(),"J*mp","A/m^2",0,
     &Xf_STT::UpdateSpinTorqueOutputs);

  // Register outputs
  ave_J_output.Register(director,-5);
  Jmp_output.Register(director,-5);
}

Xf_STT::~Xf_STT()
{}

OC_BOOL Xf_STT::Init()
{
  mesh_id = 0;
  // Free memory used by spin-transfer polarization field
  mp.Release();
  beta_eps_prime.Release();
  B.Release();
  A.Release();
  beta_q_minus.Release();
  beta_q_plus.Release();
  mesh_id = 0;
  return Oxs_Energy::Init();
}

void
Xf_STT::StageRequestCount
(unsigned int& min,
 unsigned int& max) const
{
  if(number_of_stages==0) {
    min=0; max=UINT_MAX;
  } else {
    min = max = number_of_stages;
  }
}

void Xf_STT::UpdateMeshArrays(const Oxs_Mesh* mesh) const
{
  mesh_id = 0; // Mark update in progress

  const Oxs_RectangularMesh* rmesh
    = dynamic_cast<const Oxs_RectangularMesh*>(mesh);
  if(rmesh==NULL) {
    String msg="Import mesh to Xf_STT::UpdateMeshArrays"
      " is not an Oxs_RectangularMesh object.";
    throw Oxs_ExtError(msg.c_str());
  }

  OC_REAL8m work_thickness = rmesh->EdgeLengthZ();
  if(J_direction == JD_X_POS || J_direction == JD_X_NEG) {
    work_thickness = rmesh->EdgeLengthX();
  } else if(J_direction == JD_Y_POS || J_direction == JD_Y_NEG) {
    work_thickness = rmesh->EdgeLengthY();
  }
  const OC_REAL8m cell_thickness = work_thickness;

  const OC_REAL8m hbar = 1.0545717e-34;
  const OC_REAL8m e_charge = 1.6021765e-19;
  OC_REAL8m beta_factor = fabs(hbar/(MU0*e_charge));
  if(cell_thickness>0.) beta_factor /= cell_thickness;
  else                  beta_factor = 0.0; // Safety

  Lambda_fixed_init->FillMeshValue(mesh,beta_q_plus);
  Lambda_free_init->FillMeshValue(mesh,beta_q_minus);
  J_init->FillMeshValue(mesh,A); // Use A as temp space
  eps_prime_init->FillMeshValue(mesh,beta_eps_prime);

  B.AdjustSize(mesh);

  const OC_INDEX size = mesh->Size();
  OC_INDEX i;
  for(i=0;i<size;i++) {
    OC_REAL8m beta = A[i] * beta_factor; // A[i] holds J(i)
    /// This is actually (beta/gamma)*d*Ms

    OC_REAL8m fixed_temp = beta_q_plus[i];
    if(fixed_temp<1.0) {
      char buf[1024];
      Oc_Snprintf(buf,sizeof(buf),"Xf_STT::UpdateMeshArrays:"
		  " Invalid value for Lambda_fixed detected at"
		  " mesh node %u: %g (should be >=1.0)",i,
		  static_cast<double>(fixed_temp));
      throw Oxs_ExtError(this,buf);
    }

    OC_REAL8m free_temp = beta_q_minus[i];
    if(free_temp<1.0 || (free_temp==1.0 && fixed_temp>1.0)) {
      char buf[1024];
      Oc_Snprintf(buf,sizeof(buf),"Xf_STT::UpdateMeshArrays:"
		  " Invalid value for Lambda_free detected at"
		  " mesh node %u: %g (should be >1.0)",i,
		  static_cast<double>(free_temp));
      throw Oxs_ExtError(this,buf);
    }

    fixed_temp *= fixed_temp;
    free_temp *= free_temp;

    OC_REAL8m fixed_factor_plus = sqrt(fixed_temp+1.0);
    OC_REAL8m free_factor_plus = sqrt(free_temp+1.0);
    OC_REAL8m plus_ratio = free_factor_plus/fixed_factor_plus;

    // Handle the case where Lambda_free = Lambda_fixed = 1.0;
    OC_REAL8m fixed_factor_minus=0.0;
    OC_REAL8m free_factor_minus=0.0;
    OC_REAL8m minus_ratio = 1.0;
    if(fixed_temp>1.0) fixed_factor_minus = sqrt(fixed_temp-1.0);
    if(free_temp>1.0) {
      free_factor_minus = sqrt(free_temp-1.0);
      minus_ratio = fixed_factor_minus/free_factor_minus;
    }

    beta_eps_prime[i] *= beta;
    B[i] = fixed_factor_minus*free_factor_minus;
    A[i] = fixed_factor_plus*free_factor_plus;
    beta_q_plus[i] = 0.5*beta*fixed_temp*plus_ratio;
    beta_q_minus[i] = 0.5*beta*free_temp*minus_ratio;
  }
  P_fixed_init->MultMeshValue(mesh,beta_q_plus);
  P_free_init->MultMeshValue(mesh,beta_q_minus);
  for(i=0;i<size;i++) {
    OC_REAL8m fixed_temp = beta_q_plus[i];
    OC_REAL8m free_temp = beta_q_minus[i];
    beta_q_plus[i]  = fixed_temp+free_temp;
    beta_q_minus[i] = fixed_temp-free_temp;
  }

  mp_init->FillMeshValue(mesh,mp);
  for(i=0;i<size;i++) mp[i].MakeUnit(); // These are suppose
  /// to be unit vectors!

  mesh_id = mesh->Id();
}

OC_REAL8m Xf_STT::EvaluateJProfileScript
(OC_UINT4m stage,
 OC_REAL8m stage_time,
 OC_REAL8m total_time) const
{
  if(!has_J_profile) return 1.0;

  int index;
  if((index = J_profile_opts[0].position)>=0) { // stage
    J_profile_cmd.SetCommandArg(index,stage);
  }
  if((index = J_profile_opts[1].position)>=0) { // stage_time
    J_profile_cmd.SetCommandArg(index,stage_time);
  }
  if((index = J_profile_opts[2].position)>=0) { // total_time
    J_profile_cmd.SetCommandArg(index,total_time);
  }

  J_profile_cmd.SaveInterpResult();
  J_profile_cmd.Eval();
  if(J_profile_cmd.GetResultListSize()!=1) {
    String msg
      = String("Return script value is not a single scalar: ")
      + J_profile_cmd.GetWholeResult();
    J_profile_cmd.RestoreInterpResult();
    throw Oxs_ExtError(this,msg.c_str());
  }
  OC_REAL8m result;
  J_profile_cmd.GetResultListItem(0,result);
  J_profile_cmd.RestoreInterpResult();

  return result;
}

void
Xf_STT::Propagate_mp
(const Oxs_Mesh* mesh,
 const Oxs_MeshValue<ThreeVector>& spin) const
{
  if(!mp_propagate) return; // Nothing to do.

  if(!mp.CheckMesh(mesh)) {
    throw Oxs_ExtError("mp array not properly initialized"
		       " in Xf_STT::Propagate_mp");
  }

  const Oxs_RectangularMesh* rmesh
    = dynamic_cast<const Oxs_RectangularMesh*>(mesh);
  if(rmesh==NULL) {
    String msg="Import mesh to Xf_STT::Propagate_mp"
      " is not an Oxs_RectangularMesh object.";
    throw Oxs_ExtError(msg.c_str());
  }


  const OC_INDEX iend=rmesh->DimX();
  const OC_INDEX jend=rmesh->DimY();
  const OC_INDEX kend=rmesh->DimZ();
  if(J_direction == JD_X_NEG || J_direction == JD_X_POS) {
    OC_REAL8m multiplier = 0.5;
    if (J_direction == JD_X_NEG) multiplier *= -1;
    OC_INDEX base = rmesh->Index(0,0,0);
    for(OC_INDEX k=0;k<kend;++k) {
      for(OC_INDEX j=0;j<jend;++j) {

	/* One cell layer -> no spatial change */
	if (iend == 1) {
	    mp[base].SetMag(0.0);
	    ++base;
	    continue;
	}

	/* Free boundary condition */
	mp[base] = spin[base + 1];
	mp[base] -= spin[base];
	mp[base] *= 2 * multiplier;
	++base;

	if (iend > 4 && fourpt_derivative) {
	    mp[base] = spin[base + 1];
	    mp[base] -= spin[base - 1];
	    mp[base] *= multiplier;
	    ++base;
	    for(OC_INDEX i=2;i<iend-2;++i) {
		mp[base] = spin[base + 1] - spin[base - 1];
		mp[base] *= 8.0;
		ThreeVector diff_outer = spin[base + 2] - spin[base - 2];
		mp[base] -= diff_outer;
		mp[base] *= (multiplier/6.);
		++base;
	    }
	    mp[base] = spin[base + 1];
	    mp[base] -= spin[base - 1];
	    mp[base] *= multiplier;
	    ++base;
	} else if (iend > 2) {
	    for(OC_INDEX i=1;i<iend-1;++i) {
		mp[base] = spin[base + 1];
		mp[base] -= spin[base - 1];
		mp[base] *= multiplier;
		++base;
	    }
	}

	/* Free boundary condition */
	mp[base] = spin[base];
	mp[base] -= spin[base - 1];
	mp[base] *= 2 * multiplier;
	++base;
      }
    }
  } else if(J_direction == JD_Y_NEG || J_direction == JD_Y_POS) {
    const OC_INT4m offset = rmesh->DimX();
    OC_REAL8m multiplier = 0.5;
    if (J_direction == JD_Y_NEG) multiplier *= -1;
    OC_INDEX base = rmesh->Index(0,0,0);
    for(OC_INDEX k=0;k<kend;++k) {
      if (jend == 1) {
	/* One cell layer -> no spatial change */
	for(OC_INDEX i=0;i<iend;++i) {
	  mp[base].SetMag(0.0);
	  ++base;
	}
	continue;
      }

      /* Free boundary condition */
      for(OC_INDEX i=0;i<iend;++i) {
	mp[base] = spin[base + offset];
	mp[base] -= spin[base];
	mp[base] *= 2 * multiplier;
	++base;
      }

      if (jend > 4 && fourpt_derivative) {
	mp[base] = spin[base + offset];
	mp[base] -= spin[base - offset];
	mp[base] *= multiplier;
	++base;
	for(OC_INDEX j=2;j<jend-2;++j) {
	  for(OC_INDEX i=0;i<iend;++i) {
	    mp[base] = spin[base + offset] - spin[base - offset];
	    mp[base] *= 8.0;
	    ThreeVector diff_outer = spin[base + 2*offset] - spin[base - 2*offset];
	    mp[base] -= diff_outer;
	    mp[base] *= (multiplier/6.);
	    ++base;
	  }
	}
	mp[base] = spin[base + offset];
	mp[base] -= spin[base - offset];
	mp[base] *= multiplier;
	++base;
      } else if (jend > 2) {
	for(OC_INDEX j=1;j<jend-1;++j) {
	  for(OC_INDEX i=0;i<iend;++i) {
	    mp[base] = spin[base + offset];
	    mp[base] -= spin[base - offset];
	    mp[base] *= multiplier;
	    ++base;
	  }
	}
      }

      /* Free boundary condition */
      for(OC_INDEX i=0;i<iend;++i) {
	mp[base] = spin[base];
	mp[base] -= spin[base - offset];
	mp[base] *= 2 * multiplier;
	++base;
      }
    }
  } else if(J_direction == JD_Z_NEG || J_direction == JD_Z_POS) {
    const OC_INT4m offset = rmesh->DimX()*rmesh->DimY();
    OC_REAL8m multiplier = 0.5;
    if (J_direction == JD_Z_NEG) multiplier *= -1;
    OC_INDEX base = rmesh->Index(0,0,0);

    if (kend == 1) {
      /* One cell layer -> no spatial change */
      for(OC_INDEX j=0;j<jend;++j) {
	for(OC_INDEX i=0;i<iend;++i) {
	  mp[base].SetMag(0.0);
	  ++base;
	}
      }
      return;
    }

    /* Free boundary condition */
    for (OC_INDEX j=0; j<jend;++j) {
      for(OC_INDEX i=0;i<iend;++i) {
	mp[base] = spin[base + offset];
	mp[base] -= spin[base];
	mp[base] *= 2 * multiplier;
	++base;
      }
    }

    if (kend > 4 && fourpt_derivative) {
      for(OC_INDEX j=0;j<jend;++j) {
	for(OC_INDEX i=0;i<iend;++i) {
	  mp[base] = spin[base + offset];
	  mp[base] -= spin[base - offset];
	  mp[base] *= multiplier;
	  ++base;
	}
      }
      for(OC_INDEX k=2;k<kend-2;++k) {
	for(OC_INDEX j=0;j<jend;++j) {
	  for(OC_INDEX i=0;i<iend;++i) {
	    mp[base] = spin[base + offset] - spin[base - offset];
	    mp[base] *= 8.0;
	    ThreeVector diff_outer = spin[base + 2*offset] - spin[base - 2*offset];
	    mp[base] -= diff_outer;
	    mp[base] *= (multiplier/6.);
	    ++base;
	  }
	}
      }
      for(OC_INDEX j=0;j<jend;++j) {
	for(OC_INDEX i=0;i<iend;++i) {
	  mp[base] = spin[base + offset];
	  mp[base] -= spin[base - offset];
	  mp[base] *= multiplier;
	  ++base;
	}
      }
    } else if (kend > 2) {
      for(OC_INDEX k=1;k<kend-1;++k) {
	for(OC_INDEX j=0;j<jend;++j) {
	  for(OC_INDEX i=0;i<iend;++i) {
	    mp[base] = spin[base + offset];
	    mp[base] -= spin[base - offset];
	    mp[base] *= multiplier;
	    ++base;
	  }
	}
      }
    }

    /* Free boundary condition */
    for (OC_INDEX j=0; j<jend;++j) {
      for(OC_INDEX i=0;i<iend;++i) {
	mp[base] = spin[base];
	mp[base] -= spin[base - offset];
	mp[base] *= 2 * multiplier;
	++base;
      }
    }
  } else {
    throw Oxs_ExtError("Invalid propagate_mp value,"
		       " in Xf_STT::Propagate_mp");
  }

}

void Xf_STT::UpdateSpinTorqueOutputs(const Oxs_SimState& state)
{
  const Oxs_Mesh* mesh = state.mesh;
  const OC_INDEX size = mesh->Size();
  OC_INDEX i;

  if(mesh_id != mesh->Id()) {
    // This is either the first pass through, or else mesh
    // has changed.
    UpdateMeshArrays(mesh);
  }

  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
  // const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;

  OC_REAL8m Jmult = EvaluateJProfileScript(state.stage_number,
                          state.stage_elapsed_time,
                          state.stage_start_time+state.stage_elapsed_time);

  if(Jmp_output.GetCacheRequestCount()>0) {
    Jmp_output.cache.state_id = 0;

    stmpA.AdjustSize(mesh);
    J_init->FillMeshValue(mesh,stmpA);

    Oxs_MeshValue<ThreeVector>& jmp = Jmp_output.cache.value;
    jmp.AdjustSize(mesh);

    Propagate_mp(mesh,spin);

    for(i=0;i<size;i++) {
      if(Ms[i]==0) {
        jmp[i].Set(0.0,0.0,0.0);
      } else {
        jmp[i] = mp[i];
        jmp[i] *= Jmult*stmpA[i];
      }
    }

    Jmp_output.cache.state_id = state.Id();
  }

  if(ave_J_output.GetCacheRequestCount()>0) {
    ave_J = 0.0;
    for(i=0;i<size;i++) {
      ave_J += A[i];
    }
    if(size>0) ave_J /= size;
    ave_J_output.cache.value=ave_J*Jmult;
    ave_J_output.cache.state_id = state.Id();
  }

}

void Xf_STT::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  OC_INDEX size = state.mesh->Size();
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  // const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
  const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);
  const Oxs_Mesh* mesh = state.mesh;

  OC_INDEX i;
  if(size<1) return; // Nothing to do!

  if(mesh_id != state.mesh->Id()) {
    // This is either the first pass through, or else mesh
    // has changed.
    mesh_id = 0;
    UpdateMeshArrays(mesh);
    mesh_id = state.mesh->Id();
  }

  // Adjust mp array as requested.  This is a nop if mp_propagate
  // is false.
  Propagate_mp(mesh,spin);

  const OC_REAL8m Jmult = EvaluateJProfileScript(state.stage_number,
						 state.stage_elapsed_time,
						 state.stage_start_time+state.stage_elapsed_time);

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;

  for(i=0;i<size;i++) {
    if(Ms_inverse[i]==0) {
      field[i].Set(0.0,0.0,0.0);
      energy[i] = 0.0;
    } else {
      ThreeVector p = mp[i];
      ThreeVector m = spin[i];
      ThreeVector mxp = m; mxp ^= p;

      OC_REAL8m Bdot = B[i];
      if(!mp_propagate) {
	Bdot *= (m*p);
      }
      /// If mp_propagate is true, then mp[i] is actually something like
      /// (m[i+1]-m[i-1])/2, so m*p is in no sense cos(theta_m,p).
      /// Instead, we just take theta=0 so cos(theta_m_p)=1.  As an
      /// alternative, one could use sqrt(1-p*p), although this
      /// isn't right either if theta>pi/2.

      OC_REAL8m Aplus = A[i] + Bdot;
      OC_REAL8m Aminus = A[i] - Bdot;

      OC_REAL8m betaeps = beta_q_plus[i]/Aplus + beta_q_minus[i]/Aminus;
      OC_REAL8m betaepsp = beta_eps_prime[i];
      betaeps  *= Jmult*Ms_inverse[i];
      betaepsp *= Jmult*Ms_inverse[i];

      field[i] = betaeps*mxp + betaepsp*p;
      energy[i] = 0.0;
    }
  }
  oed.pE_pt = 0.0;
}
