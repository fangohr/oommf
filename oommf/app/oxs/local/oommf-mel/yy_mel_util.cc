/* FILE: yy_mel_util.cc                 -*-Mode: c++-*-
 *
 * OOMMF magnetoelastic coupling extension module.
 * yy_mel_util.* contain YY_MELField class , which is to be used by other
 * YY_*MEL classes
 *
 * Release ver. 1.0.1 (2015-03-03)
 * 
 */

#include "yy_mel_util.h"

#include "mesh.h"
#include "rectangularmesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "vectorfield.h"
#include "util.h"

#ifdef YY_DEBUG
#include <iostream>
#endif

/* End includes */

YY_MELField::YY_MELField():
  displacement_valid(0), strain_valid(0), 
  MELCoef1_valid(0), MELCoef2_valid(0), pE_pt_buf(0)
{
}

void YY_MELField::Release()
{
  u_cache.Release();
  e_diag_cache.Release();
  e_offdiag_cache.Release();
  u.Release();
  e_diag.Release();
  e_offdiag.Release();
  du.Release();
  de_diag.Release();
  de_offdiag.Release();
  MELCoef1.Release();
  MELCoef2.Release();
}

void YY_MELField::SetMELCoef(const Oxs_SimState& state,
    const Oxs_OwnedPointer<Oxs_ScalarField>& MELCoef1_init,
    const Oxs_OwnedPointer<Oxs_ScalarField>& MELCoef2_init)
{
  MELCoef1_init->FillMeshValue(state.mesh,MELCoef1);
  MELCoef2_init->FillMeshValue(state.mesh,MELCoef2);
  MELCoef1_valid = 1;
  MELCoef2_valid = 1;
}

void YY_MELField::SetDisplacement(const Oxs_SimState& state,
    const Oxs_OwnedPointer<Oxs_VectorField>& u_init)
{
  // Check mesh size
  const OC_INDEX size = state.mesh->Size();
  if(size<1) return;

  u_init->FillMeshValue(state.mesh,u_cache);
  u = u_cache;

  // Initialize du/dt with zeros.
  const ThreeVector zeros(0,0,0);
  du.AdjustSize(state.mesh);
  du=zeros;
  displacement_valid = 1;

  CalculateStrain(state);
  // Update the strain cache.
  e_diag_cache = e_diag;
  e_offdiag_cache = e_offdiag;
  strain_valid = 1;

#ifdef YY_DEBUG
  DisplayValues(state,4,6,0,2,0,2);
#endif
}

void YY_MELField::SetStrain(const Oxs_SimState& state,
    const Oxs_OwnedPointer<Oxs_VectorField>& e_diag_init,
    const Oxs_OwnedPointer<Oxs_VectorField>& e_offdiag_init)
{
  if(state.mesh->Size()<1) return;

  e_diag_init->FillMeshValue(state.mesh,e_diag_cache);
  e_offdiag_init->FillMeshValue(state.mesh,e_offdiag_cache);
  e_diag = e_diag_cache;
  e_offdiag=e_offdiag_cache;

  // Initialize de/dt with zeros.
  const ThreeVector zeros(0,0,0);
  de_diag.AdjustSize(state.mesh);
  de_offdiag.AdjustSize(state.mesh);
  de_diag=zeros;
  de_offdiag=zeros;

  displacement_valid = 0;
  strain_valid = 1;

#ifdef YY_DEBUG
  DisplayValues(state,4,6,0,2,0,2);
#endif
}

void YY_MELField::CalculateStrain(const Oxs_SimState& state)
{
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_RectangularMesh* mesh =
    dynamic_cast<const Oxs_RectangularMesh*>(state.mesh);
  const OC_INDEX xdim = mesh->DimX();
  const OC_INDEX ydim = mesh->DimY();
  const OC_INDEX zdim = mesh->DimZ();
  const OC_INDEX xydim = xdim*ydim;
  const OC_REAL8m idelx = 1./mesh->EdgeLengthX();
  const OC_REAL8m idely = 1./mesh->EdgeLengthX();
  const OC_REAL8m idelz = 1./mesh->EdgeLengthX();

  e_diag.AdjustSize(mesh);
  e_offdiag.AdjustSize(mesh);
  de_diag.AdjustSize(mesh);
  de_offdiag.AdjustSize(mesh);

  // Compute du/dx
  // The following calculation assumes that the displacement is only
  // defined in the region where Ms != 0.
  for(OC_INDEX z=0; z<zdim; z++) {
    for(OC_INDEX y=0; y<ydim; y++) {
      for(OC_INDEX x=0; x<xdim; x++) {
        OC_INDEX i = mesh->Index(x,y,z);  // Get base index
        ThreeVector du_dx, ddu_dx;  // ddu_dx = d(du/dx)/dt
        ThreeVector du_dy, ddu_dy;
        ThreeVector du_dz, ddu_dz;
        if(Ms[i]==0.0) {
          e_diag[i].Set(0.0, 0.0, 0.0);
          e_offdiag[i].Set(0.0, 0.0, 0.0);
          de_diag[i].Set(0.0, 0.0, 0.0);
          de_offdiag[i].Set(0.0, 0.0, 0.0);
          continue;
        }

        if(x<xdim-1 && Ms[i+1]!=0.0) { du_dx  = u[i+1]; ddu_dx  = du[i+1]; }
        else                         { du_dx  = u[i];   ddu_dx  = du[i];   }
        if(x>0 && Ms[i-1]!=0.0)      { du_dx -= u[i-1]; ddu_dx -= du[i-1]; }
        else                         { du_dx -= u[i];   ddu_dx -= du[i];   }
        if(x<xdim-1 && Ms[i+1]!=0.0 && x>0 && Ms[i-1]!=0.0) {
          du_dx *= 0.5*idelx; ddu_dx *= 0.5*idelx;
        } else {
          du_dx *= idelx; ddu_dx *= idelx;
        }

        if(y<ydim-1 && Ms[i+xdim]!=0.0) { du_dy  = u[i+xdim]; ddu_dy  = du[i+xdim]; }
        else                            { du_dy  = u[i];      ddu_dy  = du[i];      }
        if(y>0 && Ms[i-xdim]!=0.0)      { du_dy -= u[i-xdim]; ddu_dy -= du[i-xdim]; }
        else                            { du_dy -= u[i];      ddu_dy -= du[i];      }
        if(y<ydim-1 && Ms[i+xdim]!=0.0 && y>0 && Ms[i-xdim]!=0.0) {
          du_dy *= 0.5*idely; ddu_dy *= 0.5*idely;
        } else {
          du_dy *= idely; ddu_dy *= idely;
        }

        if(z<zdim-1 && Ms[i+xydim]!=0.0) { du_dz  = u[i+xydim]; ddu_dz  = du[i+xydim]; }
        else                             { du_dz  = u[i];       ddu_dz  = du[i];       }
        if(z>0 && Ms[i-xydim]!=0.0)      { du_dz -= u[i-xydim]; ddu_dz -= du[i-xydim]; }
        else                             { du_dz -= u[i];       ddu_dz -= du[i];       }
        if(z<zdim-1 && Ms[i+xydim]!=0.0 && z>0 && Ms[i-xydim]!=0.0) {
          du_dz *= 0.5*idelz; ddu_dz *= 0.5*idelz;
        } else {
          du_dz *= idelz; ddu_dz *= idelz;
        }

        e_diag[i].Set(du_dx.x,du_dy.y,du_dz.z);
        e_offdiag[i].Set(
          0.5*(du_dz.y+du_dy.z),
          0.5*(du_dx.z+du_dz.x),
          0.5*(du_dy.x+du_dx.y)
        );
        de_diag[i].Set(ddu_dx.x,ddu_dy.y,ddu_dz.z);
        de_offdiag[i].Set(
          0.5*(ddu_dz.y+ddu_dy.z),
          0.5*(ddu_dx.z+ddu_dz.x),
          0.5*(ddu_dy.x+ddu_dx.y)
        );
      }
    }
  }
}

void YY_MELField::TransformDisplacement(
    const Oxs_SimState& state,
    ThreeVector& row1, ThreeVector& row2, ThreeVector& row3,
    ThreeVector& drow1, ThreeVector& drow2, ThreeVector& drow3)
{
  const OC_INDEX size = state.mesh->Size();
  if(size<1) return;
  if(!displacement_valid) return;

  for(OC_INDEX i=0; i<size; i++) {
    const ThreeVector& v = u_cache[i];
    u[i].Set(row1*v,row2*v,row3*v);
    du[i].Set(drow1*v,drow2*v,drow3*v);
  }
  CalculateStrain(state);

#ifdef YY_DEBUG
  //std::cerr<<"displacement u_cache, u, du/dt"<<endl;
  //std::cerr<<u_cache[20].x<<"  "<<u[20].x<<"  "<<du[20].x<<endl;
  //std::cerr<<u_cache[20].y<<"  "<<u[20].y<<"  "<<du[20].y<<endl;
  //std::cerr<<u_cache[20].z<<"  "<<u[20].z<<"  "<<du[20].z<<endl;
  //std::cerr<<"strain, de/dt"<<endl;
  //std::cerr<<e_diag[20].x<<" "<<e_offdiag[20].z<<" "<<e_offdiag[20].y<<"  ";
  //std::cerr<<de_diag[20].x<<" "<<de_offdiag[20].z<<" "<<de_offdiag[20].y<<endl;
  //std::cerr<<e_offdiag[20].z<<" "<<e_diag[20].y<<" "<<e_offdiag[20].x<<"  ";
  //std::cerr<<de_offdiag[20].z<<" "<<de_diag[20].y<<" "<<de_offdiag[20].x<<endl;
  //std::cerr<<e_offdiag[20].y<<" "<<e_offdiag[20].x<<" "<<e_diag[20].z<<"  ";
  //std::cerr<<de_offdiag[20].y<<" "<<de_offdiag[20].x<<" "<<de_diag[20].z<<endl;
#endif
}

void YY_MELField::TransformStrain(
    const Oxs_SimState& state,
    ThreeVector& row1, ThreeVector& row2, ThreeVector& row3,
    ThreeVector& drow1, ThreeVector& drow2, ThreeVector& drow3)
{
  const OC_INDEX size = state.mesh->Size();
  if(size<1) return;
  if(!strain_valid) return;

  // Transformation by matrix multiplication T*e*transpose(T), where
  //     / row1 \
  // T = | row2 |
  //     \ row3 /
  // and e is the strain matrix.
  ThreeVector trow1, trow2, trow3;  // Temporary matrix (T*e), row 1 through 3.
  ThreeVector dtrow1, dtrow2, dtrow3;  // Time derivative of the above.
  for(OC_INDEX i=0; i<size; i++) {
    const ThreeVector& vd = e_diag_cache[i];
    const ThreeVector& vo = e_offdiag_cache[i];
    // Calculate T*e
    trow1.Set(
        row1.x*vd.x + row1.y*vo.z + row1.z*vo.y,
        row1.x*vo.z + row1.y*vd.y + row1.z*vo.x,
        row1.x*vo.y + row1.y*vo.x + row1.z*vd.z);
    trow2.Set(
        row2.x*vd.x + row2.y*vo.z + row2.z*vo.y,
        row2.x*vo.z + row2.y*vd.y + row2.z*vo.x,
        row2.x*vo.y + row2.y*vo.x + row2.z*vd.z);
    trow3.Set(
        row3.x*vd.x + row3.y*vo.z + row3.z*vo.y,
        row3.x*vo.z + row3.y*vd.y + row3.z*vo.x,
        row3.x*vo.y + row3.y*vo.x + row3.z*vd.z);

    // Calculate (T*e)*transpose(T)
    e_diag[i].Set(
        trow1*row1,
        trow2*row2,
        trow3*row3);
    e_offdiag[i].Set(
        trow2*row3,
        trow1*row3,
        trow1*row2);

    // Calculate d(T*e)/dt
    dtrow1.Set(
        drow1.x*vd.x + drow1.y*vo.z + drow1.z*vo.y,
        drow1.x*vo.z + drow1.y*vd.y + drow1.z*vo.x,
        drow1.x*vo.y + drow1.y*vo.x + drow1.z*vd.z);
    dtrow2.Set(
        drow2.x*vd.x + drow2.y*vo.z + drow2.z*vo.y,
        drow2.x*vo.z + drow2.y*vd.y + drow2.z*vo.x,
        drow2.x*vo.y + drow2.y*vo.x + drow2.z*vd.z);
    dtrow3.Set(
        drow3.x*vd.x + drow3.y*vo.z + drow3.z*vo.y,
        drow3.x*vo.z + drow3.y*vd.y + drow3.z*vo.x,
        drow3.x*vo.y + drow3.y*vo.x + drow3.z*vd.z);

    // Calculate d((T*e)*transpose(T))/dt
    de_diag[i].Set(
        dtrow1*row1 + trow1*drow1,
        dtrow2*row2 + trow2*drow2,
        dtrow3*row3 + trow3*drow3);
    de_offdiag[i].Set(
        dtrow2*row3 + trow2*drow3,
        dtrow1*row3 + trow1*drow3,
        dtrow1*row2 + trow1*drow2);
  }

#ifdef YY_DEBUG
  //std::cerr<<"strain, de/dt"<<endl;
  //std::cerr<<e_diag_cache[20].x<<" "<<e_offdiag_cache[20].z<<" "<<e_offdiag_cache[20].y<<"  ";
  //std::cerr<<e_diag[20].x<<" "<<e_offdiag[20].z<<" "<<e_offdiag[20].y<<"  ";
  //std::cerr<<de_diag[20].x<<" "<<de_offdiag[20].z<<" "<<de_offdiag[20].y<<endl;
  //std::cerr<<e_offdiag_cache[20].z<<" "<<e_diag_cache[20].y<<" "<<e_offdiag_cache[20].x<<"  ";
  //std::cerr<<e_offdiag[20].z<<" "<<e_diag[20].y<<" "<<e_offdiag[20].x<<"  ";
  //std::cerr<<de_offdiag[20].z<<" "<<de_diag[20].y<<" "<<de_offdiag[20].x<<endl;
  //std::cerr<<e_offdiag_cache[20].y<<" "<<e_offdiag_cache[20].x<<" "<<e_diag_cache[20].z<<"  ";
  //std::cerr<<e_offdiag[20].y<<" "<<e_offdiag[20].x<<" "<<e_diag[20].z<<"  ";
  //std::cerr<<de_offdiag[20].y<<" "<<de_offdiag[20].x<<" "<<de_diag[20].z<<endl;
#endif
}

void YY_MELField::Interpolate(
  const Oxs_SimState& state,
  OC_REAL8m working_stage_stopping_time,
  YY_MELField& MELField1,
  YY_MELField& MELField2)
{
  OC_REAL8m coef0, coef1, coef2, coef3;
  OC_REAL8m dcoef0, dcoef1, dcoef2, dcoef3;
  ThreeVector row11, row12, row13;
  ThreeVector drow11, drow12, drow13;
  ThreeVector row21, row22, row23;
  ThreeVector drow21, drow22, drow23;

  OC_REAL8m t = state.stage_elapsed_time / working_stage_stopping_time;
  coef1 = (1-t);
  coef2 = t;
  dcoef1 = -1/working_stage_stopping_time;
  dcoef2 = 1/working_stage_stopping_time;
  row11.Set(coef1,0,0);
  row12.Set(0,coef1,0);
  row13.Set(0,0,coef1);
  drow11.Set(dcoef1,0,0);
  drow12.Set(0,dcoef1,0);
  drow13.Set(0,0,dcoef1);
  row21.Set(coef2,0,0);
  row22.Set(0,coef2,0);
  row23.Set(0,0,coef2);
  drow21.Set(dcoef2,0,0);
  drow22.Set(0,dcoef2,0);
  drow23.Set(0,0,dcoef2);

  MELCoef1 = MELField1.MELCoef1;
  MELCoef2 = MELField1.MELCoef2;
  MELCoef1_valid = MELField1.MELCoef1_valid;
  MELCoef2_valid = MELField1.MELCoef2_valid;
  max_field = MELField1.max_field;
  pE_pt_buf = MELField1.pE_pt_buf;
  if(MELField1.displacement_valid) {
    MELField1.TransformDisplacement(state, row11, row12, row13, drow11, drow12, drow13);
    MELField2.TransformDisplacement(state, row21, row22, row23, drow21, drow22, drow23);
    strain_valid = false;
    u        = MELField1.u;
    u       += MELField2.u;
    u_cache  = MELField1.u_cache;
    u_cache += MELField2.u_cache;
    du       = MELField1.du;
    du      += MELField2.du;
    CalculateStrain(state);
    strain_valid = true;
  } else if(MELField1.strain_valid) {
    MELField1.TransformStrain(state, row11, row12, row13, drow11, drow12, drow13);
    MELField2.TransformStrain(state, row21, row22, row23, drow21, drow22, drow23);
    displacement_valid = false;
    e_diag           = MELField1.e_diag;
    e_diag          += MELField2.e_diag;
    e_diag_cache     = MELField1.e_diag_cache;
    e_diag_cache    += MELField2.e_diag_cache;
    de_diag          = MELField1.de_diag;
    de_diag         += MELField2.de_diag;
    e_offdiag        = MELField1.e_offdiag;
    e_offdiag       += MELField2.e_offdiag;
    e_offdiag_cache = MELField1.e_offdiag_cache;
    e_offdiag_cache += MELField2.e_offdiag_cache;
    de_offdiag       = MELField1.de_offdiag;
    de_offdiag      += MELField2.de_offdiag;
  }
}

void YY_MELField::CalculateMELField(
  const Oxs_SimState& state,
  OC_REAL8m hmult,
  Oxs_MeshValue<ThreeVector>& field_buf,
  Oxs_MeshValue<OC_REAL8m>& energy_buf) const
{
  const OC_INDEX size = state.mesh->Size();
  if(size<1) return;

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
  const Oxs_MeshValue<OC_REAL8m>& Msi = *(state.Ms_inverse);
  const Oxs_RectangularMesh* mesh =
    dynamic_cast<const Oxs_RectangularMesh*>(state.mesh);
  const OC_INDEX xdim = mesh->DimX();
  const OC_INDEX ydim = mesh->DimX();
  const OC_INDEX zdim = mesh->DimX();
  const OC_INDEX xydim = xdim*ydim;

  // Compute MEL field and energy
  Oxs_MeshValue<ThreeVector> temp_field;
  temp_field.AdjustSize(mesh);
  for(OC_INDEX i=0; i<size; i++) {
    // field_buf[i]*e_diag[i] returns a dot product. Don't use it.
    field_buf[i].x  = spin[i].x*e_diag[i].x;
    field_buf[i].y  = spin[i].y*e_diag[i].y;
    field_buf[i].z  = spin[i].z*e_diag[i].z;
    field_buf[i] *= -1/MU0*2*Msi[i]*MELCoef1[i];
    temp_field[i].x = spin[i].y*e_offdiag[i].z+spin[i].z*e_offdiag[i].y;
    temp_field[i].y = spin[i].x*e_offdiag[i].z+spin[i].z*e_offdiag[i].x;
    temp_field[i].z = spin[i].x*e_offdiag[i].y+spin[i].y*e_offdiag[i].x;
    temp_field[i] *= -1/MU0*2*Msi[i]*MELCoef2[i];
    field_buf[i] += temp_field[i];
  }
  if(hmult != 1.0) field_buf *= hmult;

  // H-field
  max_field.Set(0.,0.,0.);
  if(size>0) {
    OC_INDEX max_i = 0;
    OC_REAL8m max_magsq = field_buf[OC_INDEX(0)].MagSq();
    for(OC_INDEX i=1; i<size; i++) {
      OC_REAL8m magsq = field_buf[i].MagSq();
      if(magsq>max_magsq) {
        max_magsq = magsq;
        max_i = i;
      }
    }
    max_field = field_buf[max_i];
  }

  // Energy density
  OC_REAL8m pE_pt_sum=0;
  for(OC_INDEX i=0; i<size; i++) {
    ThreeVector dH, dH2;
    dH.x  = spin[i].x*de_diag[i].x;
    dH.y  = spin[i].y*de_diag[i].y;
    dH.z  = spin[i].z*de_diag[i].z;
    dH *= -1/MU0*2*Msi[i]*MELCoef1[i];
    dH2.x = spin[i].y*de_offdiag[i].z+spin[i].z*de_offdiag[i].y;
    dH2.y = spin[i].x*de_offdiag[i].z+spin[i].z*de_offdiag[i].x;
    dH2.z = spin[i].x*de_offdiag[i].y+spin[i].y*de_offdiag[i].x;
    dH2 *= -1/MU0*2*Msi[i]*MELCoef2[i];
    dH += dH2;
    ThreeVector temp = (-0.5*MU0*Ms[i])*spin[i];
    energy_buf[i] = field_buf[i]*temp;
    pE_pt_sum += mesh->Volume(i)*(dH*temp);
  }
#ifdef YY_DEBUG
  std::cerr<<"pE_pt: "<<pE_pt_sum<<endl;
#endif
  pE_pt_buf = pE_pt_sum;
}

#ifdef YY_DEBUG
void YY_MELField::DisplayValues(
    const Oxs_SimState& state,
    OC_INDEX xmin, OC_INDEX xmax,
    OC_INDEX ymin, OC_INDEX ymax,
    OC_INDEX zmin, OC_INDEX zmax) const
{
  const Oxs_RectangularMesh* mesh =
    static_cast<const Oxs_RectangularMesh*>(state.mesh);
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  // Indices
  fprintf(stderr,"Indices:\n");
  for(OC_INDEX y=ymin; y<ymax+1; y++) {
    for(OC_INDEX z=zmin; z<zmax+1; z++) {
      for(OC_INDEX x=xmin; x<xmax+1; x++) {
        OC_INDEX i = mesh->Index(x,y,z);
        fprintf(stderr,"%ld %ld %ld %ld ",x,y,z,i);
      }
      fprintf(stderr,"| ");
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr,"\n");

  // Ms
  fprintf(stderr,"Ms:\n");
  for(OC_INDEX y=ymin; y<ymax+1; y++) {
    for(OC_INDEX z=zmin; z<zmax+1; z++) {
      for(OC_INDEX x=xmin; x<xmax+1; x++) {
        OC_INDEX i = mesh->Index(x,y,z);
        fprintf(stderr,"%e ",Ms[i]);
      }
      fprintf(stderr,"| ");
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr,"\n");

  if(displacement_valid) {
    // u.x
    fprintf(stderr,"u.x:\n");
    for(OC_INDEX y=ymin; y<ymax+1; y++) {
      for(OC_INDEX z=zmin; z<zmax+1; z++) {
        for(OC_INDEX x=xmin; x<xmax+1; x++) {
          OC_INDEX i = mesh->Index(x,y,z);
          fprintf(stderr,"%e ",u[i].x);
        }
        fprintf(stderr,"| ");
      }
      fprintf(stderr,"\n");
    }
    fprintf(stderr,"\n");

    // u.y
    fprintf(stderr,"u.y:\n");
    for(OC_INDEX y=ymin; y<ymax+1; y++) {
      for(OC_INDEX z=zmin; z<zmax+1; z++) {
        for(OC_INDEX x=xmin; x<xmax+1; x++) {
          OC_INDEX i = mesh->Index(x,y,z);
          fprintf(stderr,"%e ",u[i].y);
        }
        fprintf(stderr,"| ");
      }
      fprintf(stderr,"\n");
    }
    fprintf(stderr,"\n");

    // u.z
    fprintf(stderr,"u.z:\n");
    for(OC_INDEX y=ymin; y<ymax+1; y++) {
      for(OC_INDEX z=zmin; z<zmax+1; z++) {
        for(OC_INDEX x=xmin; x<xmax+1; x++) {
          OC_INDEX i = mesh->Index(x,y,z);
          fprintf(stderr,"%e ",u[i].z);
        }
        fprintf(stderr,"| ");
      }
      fprintf(stderr,"\n");
    }
    fprintf(stderr,"\n");
  }

  // e_diag.x
  fprintf(stderr,"e_diag.x:\n");
  for(OC_INDEX y=ymin; y<ymax+1; y++) {
    for(OC_INDEX z=zmin; z<zmax+1; z++) {
      for(OC_INDEX x=xmin; x<xmax+1; x++) {
        OC_INDEX i = mesh->Index(x,y,z);
        fprintf(stderr,"%e ",e_diag[i].x);
      }
      fprintf(stderr,"| ");
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr,"\n");

  // e_diag.y
  fprintf(stderr,"e_diag.y:\n");
  for(OC_INDEX y=ymin; y<ymax+1; y++) {
    for(OC_INDEX z=zmin; z<zmax+1; z++) {
      for(OC_INDEX x=xmin; x<xmax+1; x++) {
        OC_INDEX i = mesh->Index(x,y,z);
        fprintf(stderr,"%e ",e_diag[i].y);
      }
      fprintf(stderr,"| ");
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr,"\n");

  // e_diag.z
  fprintf(stderr,"e_diag.z:\n");
  for(OC_INDEX y=ymin; y<ymax+1; y++) {
    for(OC_INDEX z=zmin; z<zmax+1; z++) {
      for(OC_INDEX x=xmin; x<xmax+1; x++) {
        OC_INDEX i = mesh->Index(x,y,z);
        fprintf(stderr,"%e ",e_diag[i].z);
      }
      fprintf(stderr,"| ");
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr,"\n");

  // e_offdiag.x
  fprintf(stderr,"e_offdiag.x:\n");
  for(OC_INDEX y=ymin; y<ymax+1; y++) {
    for(OC_INDEX z=zmin; z<zmax+1; z++) {
      for(OC_INDEX x=xmin; x<xmax+1; x++) {
        OC_INDEX i = mesh->Index(x,y,z);
        fprintf(stderr,"%e ",e_offdiag[i].x);
      }
      fprintf(stderr,"| ");
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr,"\n");

  // e_offdiag.y
  fprintf(stderr,"e_offdiag.y:\n");
  for(OC_INDEX y=ymin; y<ymax+1; y++) {
    for(OC_INDEX z=zmin; z<zmax+1; z++) {
      for(OC_INDEX x=xmin; x<xmax+1; x++) {
        OC_INDEX i = mesh->Index(x,y,z);
        fprintf(stderr,"%e ",e_offdiag[i].y);
      }
      fprintf(stderr,"| ");
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr,"\n");

  // e_offdiag.z
  fprintf(stderr,"e_offdiag.z:\n");
  for(OC_INDEX y=ymin; y<ymax+1; y++) {
    for(OC_INDEX z=zmin; z<zmax+1; z++) {
      for(OC_INDEX x=xmin; x<xmax+1; x++) {
        OC_INDEX i = mesh->Index(x,y,z);
        fprintf(stderr,"%e ",e_offdiag[i].z);
      }
      fprintf(stderr,"| ");
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr,"\n");

}
#endif  // YY_DEBUG
