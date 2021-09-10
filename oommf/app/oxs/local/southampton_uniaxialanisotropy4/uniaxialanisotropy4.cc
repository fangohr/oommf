/* FILE: uniaxialanisotropy4.cc            -*-Mode: c++-*-
 *
 * Uniaxial Anisotropy, derived from Oxs_Energy class,
 * implementing interface uniaxialanisotropy4.h
 *
 * This class is a modification of the class
 * /oommf/app/oxs/ext/uniaxialanisotropy.cc
 * It is designed for handling higher orders of the 
 * power series of the uniaxial anisotropy
 * The required values are 
 * -scalar 'K1' (for second order power)
 * -scalar 'K2' (for fourth order power),
 * -vector 'axis' indicating the theta = 0 anisotropy direction
 *
 * Juergen Zimmermann, Richard Boardman and Hans Fangohr
 * Computational Engineering and Design Group
 * University of Southampton
 * 
 * file created Wed July 7 2004
 * 
 * file updated Thu April 19 2007: 
 * renaming issues Ced_UniaxialAnisotropy to Southampton_UniaxialAnisotropy4     
 */

#include "oc.h"
#include "nb.h"
#include "threevector.h"
#include "director.h"
#include "simstate.h"
#include "ext.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "rectangularmesh.h"  // For QUAD-style integration
#include "uniaxialanisotropy4.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

// Oxs_Ext registration support
OXS_EXT_REGISTER(Southampton_UniaxialAnisotropy4);

/* End includes */


// Constructor
Southampton_UniaxialAnisotropy4::Southampton_UniaxialAnisotropy4(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr), mesh_id(0),
    integration_method(UNKNOWN_INTEG)
{
  // Process arguments
  OXS_GET_INIT_EXT_OBJECT("K1",Oxs_ScalarField,K1_init);
  OXS_GET_INIT_EXT_OBJECT("K2",Oxs_ScalarField,K2_init);
  OXS_GET_INIT_EXT_OBJECT("axis",Oxs_VectorField,axis_init);

  string integration_request = GetStringInitValue("integration","rect");
  if(integration_request.compare("rect")==0) {
    integration_method = RECT_INTEG;
  } else if(integration_request.compare("quad")==0) {
    integration_method = QUAD_INTEG;
  } else {
    string msg=string("Invalid integration request: ")
      + integration_request
      + string("\n Should be either \"rect\" or \"quad\".");
    throw Oxs_Ext::Error(this,msg.c_str());
  }

  VerifyAllInitArgsUsed();
}

void Southampton_UniaxialAnisotropy4::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;

  OC_INDEX size = state.mesh->Size();
  if(mesh_id != state.mesh->Id()) {
    // This is either the first pass through, or else mesh
    // has changed.
    mesh_id = 0;
    K1_init->FillMeshValue(state.mesh,K1);
    K2_init->FillMeshValue(state.mesh,K2);
    axis_init->FillMeshValue(state.mesh,axis);
    for(OC_INDEX i=0;i<size;i++) {
      // Check that axis is a unit vector:
      const OC_REAL8m eps = 1e-14;
      if(fabs(axis[i].MagSq()-1)>eps) {
	throw Oxs_Ext::Error(this,"Invalid initialization detected:"
			     " Anisotropy axis isn't norm 1");
      }
    }
    mesh_id = state.mesh->Id();
  }

  // calculation of the energy:
  // the anisotropy energy is assumed to be 
  // E = -k1 * (axis*spin)^2 - k2 * (axis*spin)^4
  // where k1, k2 are the second and fourth order anisotropy constants,
  // axis is the direction of the anisotropy (given), and 
  // spin is the directin of the magnetisation
  //
  // the corresponding field is determined by
  // H = - 1/mu_0 dE/dM
  // This results in 
  // H =   ( 2/mu_0 ) k1 ( 1/M_s ) * (axis*spin) * axis
  //     + ( 4/mu_0 ) K2 ( 1/M_s ) * (axis*spin)^3 *axis
  // These formulas are implemented for k1<0/hard axis.
  //
  // For k1>0/easy axis, the formula is transformed for numerical reasons:
  // E = + k1 * (axis x spin)^2 + k2 * (axis x spin)^4 
  // H =   ( 2/mu_0 ) k1 ( 1/M_s ) * |axis*spin| * axis
  //     + ( 4/mu_0 ) K2 ( 1/M_s ) * |axis*spin|^3 *axis


  for(OC_INDEX i=0;i<size;++i) {
    OC_REAL8m k1 = K1[i];
    OC_REAL8m k2 = K2[i];
    OC_REAL8m field_mult1 = (2.0/MU0)*k1*Ms_inverse[i];
    OC_REAL8m field_mult2 = (4.0/MU0)*k2*Ms_inverse[i];
    if(field_mult1==0.0 && field_mult2==0.0) {
      energy[i]=0.0;
      field[i].Set(0.,0.,0.);
      continue;
    }
    if(k1<=0) {
      // Easy plane (hard axis)
      OC_REAL8m dot = axis[i]*spin[i];
      //Use fourier series for anisotropy energy up to 4th order
      field[i] = (field_mult1*dot) * axis[i] + (field_mult2*dot*dot*dot) * axis[i] ;
      energy[i] = -k1*dot*dot -k2*dot*dot*dot*dot; // Easy plane is zero energy

    } else {
      // Easy axis case.  For improved accuracy, we want to report
      // energy as -k*(dot*dot-1), where dot = axis * spin.  But
      // dot*dot-1 suffers from bad loss of precision if spin is
      // nearly parallel to axis.  The are a couple of ways around
      // this.  Recall that both spin and axis are unit vectors.
      // Then from the cross product:
      //            (axis x spin)^2 = 1 - dot*dot
      // The cross product requires 6 mults and 3 adds, and
      // the norm squared takes 3 mult and 2 adds
      //            => 9 mults + 5 adds.
      // Another option is to use
      //            (axis - spin)^2 = 2*(1-dot) 
      //     so  1 - dot*dot = t*(2-t)
      //                where t = 0.5*(axis-spin)^2.
      // The op count here is 
      //            => 5 mults + 6 adds.
      // Another advantage to the second approach is you get 'dot',
      // as opposed to dot*dot, which saves a sqrt if dot is needed.
      // The downside is that if axis and spin are anti-parallel,
      // then you want to use (axis+spin)^2 rather than (axis-spin)^2.
      // Since we don't need a high accuracy 'dot', we just use
      // the cross product formulation.  (I did some single-spin test
      // runs and the performance of the two methods was about the
      // same.) -mjd, 28-Jan-2001
      ThreeVector temp = axis[i];
      OC_REAL8m dot = temp*spin[i];
      //Use fourier series for anisotropy energy up to 4th order
      field[i] = (field_mult1*dot) * temp + (field_mult2*dot*dot*dot) * temp;   //correction JZ 19/04/07
      //field[i] = ( (field_mult1+field_mult2)*dot) * temp - (field_mult2*dot*dot*dot) * temp;   //correction JZ 07/03/05  //correction JZ 19/04/07
      // for easy axis case, the energy is expressed by sin^2 + sin^4:          //correction JZ 07/03/05
      // now we get a mixed product when we calculate the field                 //correction JZ 07/03/05
      // because we have to transfer sin -> cos                                 //correction JZ 07/03/05
      // the mixed product is taken care of by mixing the field_mult1,2         //correction JZ 07/03/05
      // for hard axis case, we start with cos directly, so no mixed product    //correction JZ 07/03/05
      //      field[i] = (field_mult1*dot) * temp + (field_mult2*dot*dot*dot) * temp; //correction JZ 07/03/05
      temp ^= spin[i];
      OC_REAL8m MagSqtemp = temp.MagSq();
      //energy[i] = k1*MagSqtemp + k2*MagSqtemp*MagSqtemp;   //correction JZ 19/04/07
      energy[i] = (k1+2*k2)*MagSqtemp - k2*MagSqtemp*MagSqtemp;   //correction JZ 19/04/07
    }
  }

  // Edge correction if higher-order integration method requested.
  // See mjd's NOTES II, pp 178-181, Aug-2002.
  // NOTE: For short dimension lengths, all cells are modified.
  if(integration_method == QUAD_INTEG) {
    // This code requires a rectangular mesh
    const Oxs_RectangularMesh* mesh
      = dynamic_cast<const Oxs_RectangularMesh*>(state.mesh);
    if(mesh==NULL) {
      throw Oxs_Ext::Error(this,
        "Import mesh to Oxs_UniaxialAnisotropy::GetEnergy()"
        " is not an Oxs_RectangularMesh object.  Quad integration"
	" method requires a rectangular mesh.");
    }
    OC_INDEX xdim = mesh->DimX();
    OC_INDEX ydim = mesh->DimY();
    OC_INDEX zdim = mesh->DimZ();
    OC_INDEX xydim = xdim*ydim;
    OC_INDEX x,y,z;

    // x-axis.  Note special case handling for short lengths.
    if(xdim>=6) {
      for(z=0;z<zdim;z++) for(y=0;y<ydim;y++) {
	OC_INDEX i = mesh->Index(0,y,z); // Get base linear address
	energy[i]   *= 26./24.; // Left face
	energy[i+1] *= 21./24.;
	energy[i+2] *= 25./24.;
	field[i]    *= 26./24.;
	field[i+1]  *= 21./24.;
	field[i+2]  *= 25./24.;

	i += xdim-3;
	energy[i]   *= 25./24.;
	energy[i+1] *= 21./24.;
	energy[i+2] *= 26./24.; // Right face
	field[i]    *= 25./24.;
	field[i+1]  *= 21./24.;
	field[i+2]  *= 26./24.;
      }
    } else if(xdim==5) {
      OC_INDEX i = mesh->Index(0,0,0); // Get base linear address
      for(z=0;z<zdim;z++) for(y=0;y<ydim;y++) {
	energy[i]   *= 26./24.;
	energy[i+1] *= 21./24.;
	energy[i+2] *= 26./24.;
	energy[i+3] *= 21./24.;
	energy[i+4] *= 26./24.;
	field[i]    *= 26./24.;
	field[i+1]  *= 21./24.;
	field[i+2]  *= 26./24.;
	field[i+3]  *= 21./24.;
	field[i+4]  *= 26./24.;
	i += 5;
      }
    } else if(xdim==4) {
      OC_INDEX i = mesh->Index(0,0,0); // Get base linear address
      for(z=0;z<zdim;z++) for(y=0;y<ydim;y++) {
	energy[i]   *= 26./24.;
	energy[i+1] *= 22./24.;
	energy[i+2] *= 22./24.;
	energy[i+3] *= 26./24.;
	field[i]    *= 26./24.;
	field[i+1]  *= 22./24.;
	field[i+2]  *= 22./24.;
	field[i+3]  *= 26./24.;
	i += 4;
      }
    } else if(xdim==3) {
      OC_INDEX i = mesh->Index(0,0,0); // Get base linear address
      for(z=0;z<zdim;z++) for(y=0;y<ydim;y++) {
	energy[i]   *= 27./24.;
	energy[i+1] *= 18./24.;
	energy[i+2] *= 27./24.;
	field[i]    *= 27./24.;
	field[i+1]  *= 18./24.;
	field[i+2]  *= 27./24.;
	i += 3;
      }
    }
    // Quadratic fit requires 3 points, so no higher order method
    // available if xdim<3.
    
    // y-axis.  Note special case handling for short lengths.
    if(ydim>=6) {
      for(z=0;z<zdim;z++) {
	// Front face
	OC_INDEX i = mesh->Index(0,0,z); // Get base linear address
	for(x=0;x<xdim;x++) { // y==0
	  energy[i] *= 26./24.;
	  field[i]  *= 26./24.;
	  ++i;
	} // NB: At end of loop, i wraps around to next x-row.
	for(x=0;x<xdim;x++) { // y==1
	  energy[i] *= 21./24.;
	  field[i]  *= 21./24.;
	  ++i;
	}
	for(x=0;x<xdim;x++) { // y==2
	  energy[i] *= 25./24.;
	  field[i]  *= 25./24.;
	  ++i;
	}
	// Back face
	i = mesh->Index(0,ydim-3,z);
	for(x=0;x<xdim;x++) { // y==ydim-3
	  energy[i] *= 25./24.;
	  field[i]  *= 25./24.;
	  ++i;
	}
	for(x=0;x<xdim;x++) { // y==ydim-2
	  energy[i] *= 21./24.;
	  field[i]  *= 21./24.;
	  ++i;
	}
	for(x=0;x<xdim;x++) { // y==ydim-1
	  energy[i] *= 26./24.;
	  field[i]  *= 26./24.;
	  ++i;
	}
      }
    } else if(ydim==5) {
      OC_INDEX i = mesh->Index(0,0,0); // Get base linear address
      for(z=0;z<zdim;z++) {
	for(x=0;x<xdim;x++) { // y==0
	  energy[i] *= 26./24.;
	  field[i]  *= 26./24.;
	  ++i;
	}
	for(x=0;x<xdim;x++) { // y==1
	  energy[i] *= 21./24.;
	  field[i]  *= 21./24.;
	  ++i;
	}
	for(x=0;x<xdim;x++) { // y==2
	  energy[i] *= 26./24.;
	  field[i]  *= 26./24.;
	  ++i;
	}
	for(x=0;x<xdim;x++) { // y==3
	  energy[i] *= 21./24.;
	  field[i]  *= 21./24.;
	  ++i;
	}
	for(x=0;x<xdim;x++) { // y==4
	  energy[i] *= 26./24.;
	  field[i]  *= 26./24.;
	  ++i;
	}
      }
    } else if(ydim==4) {
      OC_INDEX i = mesh->Index(0,0,0); // Get base linear address
      for(z=0;z<zdim;z++) {
	for(x=0;x<xdim;x++) { // y==0
	  energy[i] *= 26./24.;
	  field[i]  *= 26./24.;
	  ++i;
	}
	for(x=0;x<xdim;x++) { // y==1
	  energy[i] *= 22./24.;
	  field[i]  *= 22./24.;
	  ++i;
	}
	for(x=0;x<xdim;x++) { // y==2
	  energy[i] *= 22./24.;
	  field[i]  *= 22./24.;
	  ++i;
	}
	for(x=0;x<xdim;x++) { // y==3
	  energy[i] *= 26./24.;
	  field[i]  *= 26./24.;
	  ++i;
	}
      }
    } else if(ydim==3) {
      OC_INDEX i = mesh->Index(0,0,0); // Get base linear address
      for(z=0;z<zdim;z++) {
	for(x=0;x<xdim;x++) { // y==0
	  energy[i] *= 27./24.;
	  field[i]  *= 27./24.;
	  ++i;
	}
	for(x=0;x<xdim;x++) { // y==1
	  energy[i] *= 18./24.;
	  field[i]  *= 18./24.;
	  ++i;
	}
	for(x=0;x<xdim;x++) { // y==2
	  energy[i] *= 27./24.;
	  field[i]  *= 27./24.;
	  ++i;
	}
      }
    }
    // Quadratic fit requires 3 points, so no higher order method
    // available if ydim<3.

    
    // z-axis.  Note special case handling for short lengths.
    if(zdim>=6) {
      // Bottom face, z==0
      OC_INDEX i = mesh->Index(0,0,0); // Get base linear address
      while(i<xydim) { // z==0
	energy[i] *= 26./24.;
	field[i]  *= 26./24.;
	++i;
      } // NB: At end of loop, i wraps around to next xy-plane.
      while(i<xydim) { // z==1
	energy[i] *= 21./24.;
	field[i]  *= 21./24.;
	++i;
      }
      while(i<xydim) { // z==2
	energy[i] *= 25./24.;
	field[i]  *= 25./24.;
	++i;
      }
      // Top face, z==0
      i = mesh->Index(0,0,zdim-3);
      while(i<xydim) { // z==zdim-3
	energy[i] *= 25./24.;
	field[i]  *= 25./24.;
	++i;
      }
      while(i<xydim) { // z==zdim-2
	energy[i] *= 21./24.;
	field[i]  *= 21./24.;
	++i;
      }
      while(i<xydim) { // z==zdim-1
	energy[i] *= 26./24.;
	field[i]  *= 26./24.;
	++i;
      }
    } else if(zdim==5) {
      OC_INDEX i = mesh->Index(0,0,0); // Get base linear address
      while(i<xydim) { // z==0; bottom face
	energy[i] *= 26./24.;
	field[i]  *= 26./24.;
	++i;
      }
      while(i<xydim) { // z==1
	energy[i] *= 21./24.;
	field[i]  *= 21./24.;
	++i;
      }
      while(i<xydim) { // z==2
	energy[i] *= 26./24.;
	field[i]  *= 26./24.;
	++i;
      }
      while(i<xydim) { // z==3
	energy[i] *= 21./24.;
	field[i]  *= 21./24.;
	++i;
      }
      while(i<xydim) { // z==4; top face
	energy[i] *= 26./24.;
	field[i]  *= 26./24.;
	++i;
      }
    } else if(zdim==4) {
      OC_INDEX i = mesh->Index(0,0,0); // Get base linear address
      while(i<xydim) { // z==0; bottom face
	energy[i] *= 26./24.;
	field[i]  *= 26./24.;
	++i;
      }
      while(i<xydim) { // z==1
	energy[i] *= 22./24.;
	field[i]  *= 22./24.;
	++i;
      }
      while(i<xydim) { // z==2
	energy[i] *= 22./24.;
	field[i]  *= 22./24.;
	++i;
      }
      while(i<xydim) { // z==3; top face
	energy[i] *= 26./24.;
	field[i]  *= 26./24.;
	++i;
      }
    } else if(zdim==3) {
      OC_INDEX i = mesh->Index(0,0,0); // Get base linear address
      while(i<xydim) { // z==0; bottom face
	energy[i] *= 27./24.;
	field[i]  *= 27./24.;
	++i;
      }
      while(i<xydim) { // z==1
	energy[i] *= 18./24.;
	field[i]  *= 18./24.;
	++i;
      }
      while(i<xydim) { // z==1; top face
	energy[i] *= 27./24.;
	field[i]  *= 27./24.;
	++i;
      }
    }
    // Quadratic fit requires 3 points, so no higher order method
    // available if zdim<3.
  }
}



