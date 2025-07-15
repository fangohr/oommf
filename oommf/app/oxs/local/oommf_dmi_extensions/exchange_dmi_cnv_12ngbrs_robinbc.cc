/* FILE: ExchangeAndDMI_cnv_12ngbrs.cc
 *
 * Exchange and Dzyaloshinskii-Moriya field and energy calculation.
 *
 * DMI is defined for the Cnv crystallographic class [1, 2]:
 *
 * $w_\text{dmi} = D ( L_{xz}^{(x)} + L_{yz}^{(y)} )
 *
 * The calculation uses a 5-point stencil for both 1st and 2nd order
 * derivatives of m, making a total of 12 neighbours per mesh site.
 * This implementation uses free boundaries, where spins
 * outside the material are set to zero. No information of the boundary
 * condition is given to the system, thus boundary spins are free to find
 * the lowest energy configuration. This leads to the true
 * bcs, although with larger error compared to setting the bcs explicitly.
 *
 * Extension and modification by David Cortes-Ortuno based on the
 * oommf-extension-dmi-cnv class by the ubermag project
 *
 * [1] A. N. Bogdanov and D. A. Yablonskii. Zh. Eksp. Teor. Fiz. 95, 178-182
 * (1989).
 * [2] C. Abert. Micromagnetics and spintronics: models and numerical methods.
 * The European Physical Journal B. 92 (120). 2019.
 *
 */

#include <string>

#include "atlas.h"
#include "director.h"
#include "energy.h" // Needed to make MSVC++ 5 happy
#include "exchange_dmi_cnv_12ngbrs_robinbc.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "nb.h"
#include "rectangularmesh.h"
#include "simstate.h"
#include "threevector.h"

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_ExchangeAndDMI_Cnv_12ngbrs_RobinBC);

/* End includes */

// Constructor
Oxs_ExchangeAndDMI_Cnv_12ngbrs_RobinBC::Oxs_ExchangeAndDMI_Cnv_12ngbrs_RobinBC(
    const char *name,     // Child instance id
    Oxs_Director *newdtr, // App director
    const char *argstr)   // MIF input block parameters
    : Oxs_Energy(name, newdtr, argstr), A_size(0), D(0), Aex(0), xperiodic(0),
      yperiodic(0), zperiodic(0), mesh_id(0) {
  // Process arguments
  OXS_GET_INIT_EXT_OBJECT("atlas", Oxs_Atlas, atlas);
  atlaskey.Set(atlas.GetPtr());
  /// Dependency lock is held until *this is deleted.

  // Check for optional default_D parameter; default is 0.
  OC_REAL8m default_D = GetRealInitValue("default_D", 0.0);
  // Check for optional default_Aex parameter; default is 1.
  OC_REAL8m default_Aex = GetRealInitValue("default_Aex", 1.0);

  // Normal to plane
  // String Dnormal = GetStringInitValue("Dnormal");

  // Allocate A matrix.  Because raw pointers are used, a memory
  // leak will occur if an exception is thrown inside this constructor.
  // A_size -> Atlas size
  A_size = atlas->GetRegionCount();
  if (A_size < 1) {
    String msg = String("Oxs_Atlas object ") + atlas->InstanceName() +
                 String(" must contain at least one region.");

    throw Oxs_Ext::Error(msg.c_str());
  }

  OC_BOOL has = HasInitValue("D");
  if (!has) {
    throw Oxs_ExtError(this, "DMI value D not specified");
  } else {
    D = GetRealInitValue("D");
  }

  has = HasInitValue("Aex");
  if (!has) {
    throw Oxs_ExtError(this, "Exchange value Aex not specified");
  } else {
    Aex = GetRealInitValue("Aex");
  }

  Oxs_OwnedPointer<Oxs_Mesh> mesh_obj; // Mesh basket
  OXS_GET_INIT_EXT_OBJECT("mesh", Oxs_Mesh, mesh_obj);
  OC_INDEX size = mesh_obj->Size();

  // Obtain either rectangular or Periodic mesh object from the simulation mesh_obj
  // The Oxs_CommonRectangularMesh is a common interface fot both which contains
  // the xdim ... dimensions of the rectangular mesh
  // (we only support these for this DMI class)
  const Oxs_CommonRectangularMesh *mesh =
      dynamic_cast<const Oxs_CommonRectangularMesh *>(mesh_obj.GetPtr());
  if (mesh == NULL) {
    String msg = String("Object ") + String(mesh_obj->InstanceName()) +
                 String(" is not a rectangular mesh.");
    throw Oxs_ExtError(this, msg);
  }

  // NOTE: If we look at periodicity here, we are assuming it stays the same in the
  // whole simulation (instead of constantly checking in the GetEnergy method)
  const Oxs_RectangularMesh *rmesh =
      dynamic_cast<const Oxs_RectangularMesh *>(mesh);
  const Oxs_PeriodicRectangularMesh *pmesh =
      dynamic_cast<const Oxs_PeriodicRectangularMesh *>(mesh);
  if (pmesh != NULL) {
    // Rectangular, periodic mesh
    xperiodic = pmesh->IsPeriodicX();
    yperiodic = pmesh->IsPeriodicY();
    zperiodic = pmesh->IsPeriodicZ();
  } else if (rmesh != NULL) {
    xperiodic = 0;
    yperiodic = 0;
    zperiodic = 0;
  } else {
    String msg =
        String("Unknown mesh type: \"") + String(ClassName()) + String("\".");
    throw Oxs_ExtError(this, msg.c_str());
  }

  // Assign mesh dimensions in class variables:
  xdim = mesh->DimX();
  ydim = mesh->DimY();
  zdim = mesh->DimZ();

  xdim = mesh->DimX();
  ydim = mesh->DimY();
  zdim = mesh->DimZ();
  xydim = xdim * ydim;
  xyzdim = xdim * ydim * zdim;
  wgtx = 1.0 / (mesh->EdgeLengthX());
  wgty = 1.0 / (mesh->EdgeLengthY());
  wgtz = 1.0 / (mesh->EdgeLengthZ());
  deltaX = mesh->EdgeLengthX();
  deltaY = mesh->EdgeLengthY();
  deltaZ = mesh->EdgeLengthZ();

  // const Oxs_MeshValue<OC_REAL8m> &Ms_inverse = *(state.Ms_inverse);
  ComputeNeighbors(n_neighbors, nn_neighbors, xdim, ydim, zdim, xperiodic, yperiodic, zperiodic);

  // for (OC_INDEX ii = 0; ii < size; ii++) {
  //   printf("i = %d  ", ii);
  //   for (int iii = 0; iii < 6; iii++) printf("n_%d = %d  ", iii, n_neighbors[6 * ii + iii]);
  //   printf("\n");
  //   for (int iii = 0; iii < 6; iii++) printf("nn_%d = %d  ", iii, nn_neighbors[6 * ii + iii]);
  //   printf("\n");
  // }
  //

  // Definition of inverse D matrices for the estimation of spins exactly at the sample edges
  OC_REAL8m Df, dfactor;

  Df = 0.5 * deltaY * D / Aex;
  dfactor = 1. / (64. + 9 * Df * Df);
  DInv_minusY_Row1.Set(3 / 8, 0., 0.);
  DInv_minusY_Row2.Set(0, 24 * dfactor, -9. * dfactor * Df);
  DInv_minusY_Row3.Set(0., 9 * dfactor * Df, 24 * dfactor);
  //
  DInv_plusY_Row1.Set(-3 / 8, 0., 0.);
  DInv_plusY_Row2.Set(0, -24 * dfactor, -9. * dfactor * Df);
  DInv_plusY_Row3.Set(0., 9 * dfactor * Df, -24 * dfactor);


  Df = 0.5 * deltaX * D / Aex;
  dfactor = 1. / (64. + 9 * Df * Df);
  DInv_minusX_Row1.Set(24. * dfactor, 0., -9 * Df * dfactor); 
  DInv_minusX_Row2.Set(0, 3. / 8, 0);                         
  DInv_minusX_Row3.Set(9 * Df * dfactor, 0., 24 * dfactor);   
  //
  DInv_plusX_Row1.Set(-24. * dfactor, 0., -9 * Df * dfactor); 
  DInv_plusX_Row2.Set(0, -3. / 8, 0);                         
  DInv_plusX_Row3.Set(9 * Df * dfactor, 0., -24 * dfactor);   

  VerifyAllInitArgsUsed();

}

Oxs_ExchangeAndDMI_Cnv_12ngbrs_RobinBC::~Oxs_ExchangeAndDMI_Cnv_12ngbrs_RobinBC() {
  // if (A_size > 0 && D != NULL && Aex != NULL) {
  //   delete[] D[0];
  //   delete[] D;
  //   delete[] Aex[0];
  //   delete[] Aex;
  // }
}

OC_BOOL Oxs_ExchangeAndDMI_Cnv_12ngbrs_RobinBC::Init() {
  mesh_id = 0;
  region_id.Release();
  return Oxs_Energy::Init();
}

void Oxs_ExchangeAndDMI_Cnv_12ngbrs_RobinBC::GetEnergy(const Oxs_SimState &state,
                                                       Oxs_EnergyData &oed) const {
  // NOTE: we skip the mesh checking (assuming the mesh will not change) and add
  // a pointer to the Ms_inverse array in the class variables
  // We can just take the spin/energy arrays as they will change on every step

  const Oxs_MeshValue<ThreeVector> &spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m> &Ms_inverse = *(state.Ms_inverse);

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m> &energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector> &field = *oed.field_buffer;

  OC_REAL8m mu0Inv = 1 / MU0;
  OC_REAL8m fdsign = 1.;
  ThreeVector ZeroVector(0., 0., 0.);
  ThreeVector SpinM2, SpinM1, SpinP1, SpinP2;
  // OC_REAL8m Dpair, Aexpair;
  ThreeVector spinBdryTmp;
  ThreeVector spinBdry;

  for (OC_INDEX z = 0; z < zdim; z++) {
    for (OC_INDEX y = 0; y < ydim; y++) {
      for (OC_INDEX x = 0; x < xdim; x++) {

        // OC_INDEX i = mesh->Index(x, y, z); // Get base linear address
        OC_INDEX i = GetIndex(x, y, z, xdim, ydim, zdim, xperiodic, yperiodic, zperiodic); // Get base linear address
        ThreeVector base = spin[i];
        ThreeVector spinNgbr(0., 0., 0.);
        ThreeVector zu(0., 0., 1.);
        OC_REAL8m Msii = Ms_inverse[i];

        if (Msii == 0.0) {
          energy[i] = 0.0;
          field[i].Set(0., 0., 0.);
          continue;
        }

        // Field sum:
        ThreeVector sum(0., 0., 0.);

        // This condition is not necessary anymore:
        if (ydim > 4) {

          OC_REAL8m dmz_dy, dmy_dy;
          ThreeVector d2my_dy2;

          // NEW:
          OC_INDEX M2 = nn_neighbors[6 * i + 2];
          OC_INDEX M1 = n_neighbors[6 * i + 2];
          OC_INDEX P1 = n_neighbors[6 * i + 3];
          OC_INDEX P2 = nn_neighbors[6 * i + 3];

          OC_REAL8m Msi_M2 = (M2 < 0 ) ?  0.0  : Ms_inverse[M2];
          OC_REAL8m Msi_M1 = (M1 < 0 ) ?  0.0  : Ms_inverse[M1];
          OC_REAL8m Msi_P1 = (P1 < 0 ) ?  0.0  : Ms_inverse[P1];
          OC_REAL8m Msi_P2 = (P2 < 0 ) ?  0.0  : Ms_inverse[P2];

          SpinM2 = (M2 < 0 || Msi_M2 == 0.0) ? ZeroVector : spin[M2];
          SpinM1 = (M1 < 0 || Msi_M1 == 0.0) ? ZeroVector : spin[M1];
          SpinP1 = (P1 < 0 || Msi_P1 == 0.0) ? ZeroVector : spin[P1];
          SpinP2 = (P2 < 0 || Msi_P2 == 0.0) ? ZeroVector : spin[P2];
 
          // if (neumannBC) {
          // }

          // bottom boundary: n = -y
          if ((Msi_M1 == 0.0) || (Msi_M1 != 0.0 && Msi_M2 == 0.0)) {
              // Calculation of spinBdry using matrix with DMI boundary condition:
              spinBdryTmp = 3. * spin[i] + (-1. / 3.) * SpinP1;
              // For two ThreeVector objects, * makes a dot product
              // See: app/oxs/base/threevector.h line 202
              spinBdry = ThreeVector(DInv_minusY_Row1 * spinBdryTmp,
                                     DInv_minusY_Row2 * spinBdryTmp,
                                     DInv_minusY_Row3 * spinBdryTmp);
              spinBdry.MakeUnit();

              if (Msi_M1 == 0.0) {
                // NOTE: we can also use dm_dx and then use dm_dx.z, for example
                dmz_dy = wgty * (1 / 3.) * (-4. * spinBdry.z + 3. * spin[i].z + SpinP1.z);
                dmy_dy = wgty * (1 / 3.) * (-4. * spinBdry.y + 3. * spin[i].y + SpinP1.y);
                d2my_dy2 = wgty * wgty * (1. / 3) * (8. * spinBdry - 12. * spin[i] + 4. * SpinP1);
              } else {
                dmz_dy = wgty * (1 / 6.) * (-2 * SpinM1.z - 3. * spin[i].z + 6. * SpinP1.z - SpinP2.z);
                dmy_dy = wgty * (1 / 6.) * (-2 * SpinM1.y - 3. * spin[i].y + 6. * SpinP1.y - SpinP2.y);
                d2my_dy2 = wgty * wgty * (SpinM1 - 2 * spin[i] + SpinP1);
              }

          // top boundary: n = +y
          } else if ((Msi_P1 == 0.0) || (Msi_P1 != 0.0 && Msi_P2 == 0.0)) {
              // Calculation of spinBdry using matrix with DMI boundary condition:
              spinBdryTmp = -3. * spin[i] + (1. / 3.) * SpinM1;
              spinBdry = ThreeVector(DInv_plusY_Row1 * spinBdryTmp,
                                     DInv_plusY_Row2 * spinBdryTmp,
                                     DInv_plusY_Row3 * spinBdryTmp);
              spinBdry.MakeUnit();

              // printf("Site i = %d  spin boundary x = %.5f  y = %.5f  z = %.5f \n", i, spinBdry.x, spinBdry.y, spinBdry.z);

              if (Msi_P1 == 0.0) {
                // NOTE: we can also use dm_dx and then use dm_dx.z, for example
                dmz_dy = wgty * (1 / 3.) * (4. * spinBdry.z - 3. * spin[i].z - SpinM1.z);
                dmy_dy = wgty * (1 / 3.) * (4. * spinBdry.y - 3. * spin[i].y - SpinM1.y);
                d2my_dy2 = wgty * wgty * (1. / 3) * (8. * spinBdry - 12. * spin[i] + 4. * SpinM1);
              } else {
                dmz_dy = wgty * (1 / 6.) * (2 * SpinP1.z + 3. * spin[i].z - 6. * SpinM1.z + SpinM2.z);
                dmy_dy = wgty * (1 / 6.) * (2 * SpinP1.y + 3. * spin[i].y - 6. * SpinM1.y + SpinM2.y);
                d2my_dy2 = wgty * wgty * (SpinM1 - 2 * spin[i] + SpinP1);
              }
              
          } else {
            // printf("5 point -- Spin i = %d  MsiM2 = %.10f  MsiM1 = %.10f  MsiP1 = %.10f  MsiP2 = %.10f \n", i, Msi_M2, Msi_M1, Msi_P1, Msi_P2);
            dmz_dy = wgty * (1. / 12) * (SpinM2.z - 8. * SpinM1.z + 8. * SpinP1.z - SpinP2.z);
            dmy_dy = wgty * (1. / 12) * (SpinM2.y - 8. * SpinM1.y + 8. * SpinP1.y - SpinP2.y);
            d2my_dy2 = wgty * wgty * (1. / 12) * (-1. * SpinM2 + 16. * SpinM1 - 30. * spin[i] + 16. * SpinP1 - 1. * SpinP2);
          }

          sum += 2.0 * Aex * d2my_dy2;
          // sum += -Dpair * wgty * ((zu ^ uij) ^ spinNgbr);
          // DMI field
          sum.y += 2 * D * (-dmz_dy);
          sum.z += 2 * D * (dmy_dy);
        }

        // This condition is not necessary anymore:
        if (xdim > 4) {

          OC_REAL8m dmz_dx = 0.0, dmx_dx = 0.0;
          ThreeVector d2mx_dx2(0.0, 0.0, 0.0);

          // NEW:
          OC_INDEX M2 = nn_neighbors[6 * i];
          OC_INDEX M1 = n_neighbors[6 * i];
          OC_INDEX P1 = n_neighbors[6 * i + 1];
          OC_INDEX P2 = nn_neighbors[6 * i + 1];

          OC_REAL8m Msi_M2 = (M2 < 0 ) ?  0.0  : Ms_inverse[M2];
          OC_REAL8m Msi_M1 = (M1 < 0 ) ?  0.0  : Ms_inverse[M1];
          OC_REAL8m Msi_P1 = (P1 < 0 ) ?  0.0  : Ms_inverse[P1];
          OC_REAL8m Msi_P2 = (P2 < 0 ) ?  0.0  : Ms_inverse[P2];

          SpinM2 = (Msi_M2 == 0.0) ? ZeroVector : spin[M2];
          SpinM1 = (Msi_M1 == 0.0) ? ZeroVector : spin[M1];
          SpinP1 = (Msi_P1 == 0.0) ? ZeroVector : spin[P1];
          SpinP2 = (Msi_P2 == 0.0) ? ZeroVector : spin[P2];

          // variable = (condition) ? expressionTrue : expressionFalse;
          // TODO: Add condition if Ms = 0.0 (holes)
          // Comparison to exact 0.0 should be ok if we assign array values
          // of no-material as exact 0.0
          //
          // Careful, Ms_inverse is not defined for M2 < 0
          // SpinM2 = (M2 < 0 || Ms_inverse[M2] == 0.0) ? ZeroVector : spin[M2];
          // SpinM1 = (M1 < 0 || Ms_inverse[M1] == 0.0) ? ZeroVector : spin[M1];
          // SpinP1 = (P1 < 0 || Ms_inverse[P1] == 0.0) ? ZeroVector : spin[P1];
          // SpinP2 = (P2 < 0 || Ms_inverse[P2] == 0.0) ? ZeroVector : spin[P2];


          if ((Msi_M1 == 0.0) || (Msi_M1 != 0.0 && Msi_M2 == 0.0)) {
              // Calculation of spinBdry using matrix with DMI boundary condition:
              spinBdryTmp = 3. * spin[i] + (-1. / 3.) * SpinP1;
              // For two ThreeVector objects, * makes a dot product
              // See: app/oxs/base/threevector.h line 202
              spinBdry = ThreeVector(DInv_minusX_Row1 * spinBdryTmp,
                                     DInv_minusX_Row2 * spinBdryTmp,
                                     DInv_minusX_Row3 * spinBdryTmp);
              spinBdry.MakeUnit();

              // printf("Site i = %d  spin boundary x = %.5f  y = %.5f  z = %.5f \n", i, spinBdry.x, spinBdry.y, spinBdry.z);

              if (Msi_M1 == 0.0) {
                // NOTE: we can also use dm_dx and then use dm_dx.z, for example
                dmz_dx = wgtx * (1 / 3.) * (-4. * spinBdry.z + 3. * spin[i].z + SpinP1.z);
                dmx_dx = wgtx * (1 / 3.) * (-4. * spinBdry.x + 3. * spin[i].x + SpinP1.x);
                d2mx_dx2 = wgtx * wgtx * (1. / 3) * (8. * spinBdry - 12. * spin[i] + 4. * SpinP1);
              } else {
                dmz_dx = wgtx * (1 / 6.) * (-2 * SpinM1.z - 3. * spin[i].z + 6. * SpinP1.z - SpinP2.z);
                dmx_dx = wgtx * (1 / 6.) * (-2 * SpinM1.x - 3. * spin[i].x + 6. * SpinP1.x - SpinP2.x);
                d2mx_dx2 = wgtx * wgtx * (SpinM1 - 2 * spin[i] + SpinP1);
              }

            // right boundary
          } else if ((Msi_P1 == 0.0) || (Msi_P1 != 0.0 && Msi_P2 == 0.0)) {
              // Calculation of spinBdry using matrix with DMI boundary condition:
              spinBdryTmp = -3. * spin[i] + (1. / 3.) * SpinM1;
              spinBdry = ThreeVector(DInv_plusX_Row1 * spinBdryTmp,
                                     DInv_plusX_Row2 * spinBdryTmp,
                                     DInv_plusX_Row3 * spinBdryTmp);
              spinBdry.MakeUnit();

              // printf("Site i = %d  spin boundary x = %.5f  y = %.5f  z = %.5f \n", i, spinBdry.x, spinBdry.y, spinBdry.z);

              if (Msi_P1 == 0.0) {
                // NOTE: we can also use dm_dx and then use dm_dx.z, for example
                dmz_dx = wgtx * (1 / 3.) * (4. * spinBdry.z - 3. * spin[i].z - SpinM1.z);
                dmx_dx = wgtx * (1 / 3.) * (4. * spinBdry.x - 3. * spin[i].x - SpinM1.x);
                d2mx_dx2 = wgtx * wgtx * (1. / 3) * (8. * spinBdry - 12. * spin[i] + 4. * SpinM1);
              } else {
                dmz_dx = wgtx * (1 / 6.) * (2 * SpinP1.z + 3. * spin[i].z - 6. * SpinM1.z + SpinM2.z);
                dmx_dx = wgtx * (1 / 6.) * (2 * SpinP1.x + 3. * spin[i].x - 6. * SpinM1.x + SpinM2.x);
                d2mx_dx2 = wgtx * wgtx * (SpinM1 - 2 * spin[i] + SpinP1);
              }
              
          } else {
            // printf("5 point -- Spin i = %d  MsiM2 = %.10f  MsiM1 = %.10f  MsiP1 = %.10f  MsiP2 = %.10f \n", i, Msi_M2, Msi_M1, Msi_P1, Msi_P2);
            dmz_dx = wgtx * (1. / 12) * (SpinM2.z - 8. * SpinM1.z + 8. * SpinP1.z - SpinP2.z);
            dmx_dx = wgtx * (1. / 12) * (SpinM2.x - 8. * SpinM1.x + 8. * SpinP1.x - SpinP2.x);
            d2mx_dx2 = wgtx * wgtx * (1. / 12) * (-1. * SpinM2 + 16. * SpinM1 - 30. * spin[i] + 16. * SpinP1 - 1. * SpinP2);
          }

          sum += 2.0 * Aex * d2mx_dx2;
          // DMI field
          sum.x += 2 * D * (-dmz_dx);
          sum.z += 2 * D * (dmx_dx);
        }

        // printf("Field dmiX = %f dmiZ = %f  ExchX = %f\n", 2 * Dpair * dmz_dx,
        // 2 * Dpair * dmx_dx, 2.0 * Aexpair * wgtx * wgtx * d2mx_dx2.x);

        // Exchange at z-neighbours (interfacial DMI only in xy-plane)
        // NOTE: Neumann conditions are implicit in the exchange formulation if
        // ther eis no DMI
        //
        // If the system has less than 5 sites, use 6-ngbr stencil (2nd order
        // finite diff)
        if (zdim > 1) {

          ThreeVector d2mz_dz2(0.0, 0.0, 0.0);

          // NEW:
          OC_INDEX M2 = nn_neighbors[6 * i + 4];
          OC_INDEX M1 = n_neighbors[6 * i + 4];
          OC_INDEX P1 = n_neighbors[6 * i + 5];
          OC_INDEX P2 = nn_neighbors[6 * i + 5];

          OC_REAL8m Msi_M2 = (M2 < 0 ) ?  0.0  : Ms_inverse[M2];
          OC_REAL8m Msi_M1 = (M1 < 0 ) ?  0.0  : Ms_inverse[M1];
          OC_REAL8m Msi_P1 = (P1 < 0 ) ?  0.0  : Ms_inverse[P1];
          OC_REAL8m Msi_P2 = (P2 < 0 ) ?  0.0  : Ms_inverse[P2];

          SpinM2 = (Msi_M2 == 0.0) ? ZeroVector : spin[M2];
          SpinM1 = (Msi_M1 == 0.0) ? ZeroVector : spin[M1];
          SpinP1 = (Msi_P1 == 0.0) ? ZeroVector : spin[P1];
          SpinP2 = (Msi_P2 == 0.0) ? ZeroVector : spin[P2];


          // If layer is thin in z, or there is no material in the nearest neighbor, use 3-point stencil
          // NOTE: we have to be careful with the Neumann condition if there is only exchange; in this
          // case we should separate the (SpinM1 - spin[i]) and (SpinP1 - spin[i]) contributions
          // per neighbour, which satisfies the condition correctly
          if ((zdim >= 2 && zdim <= 4) || (Msi_M1 == 0.0 && Msi_M2 != 0.0) || (Msi_P1 == 0.0 && Msi_P2 != 0.0)) {
            d2mz_dz2 = wgtz * wgtz * (SpinP1 - 2. * spin[i] + SpinM1);
          } else {
            d2mz_dz2 = wgtz * wgtz * (1. / 12) * (-1. * SpinM2 + 16. * SpinM1 - 30. * spin[i] + 16. * SpinP1 - 1. * SpinP2);
          }

          sum += 2.0 * Aex * d2mz_dz2;

        }

        field[i] = (mu0Inv * Msii) * sum;
        energy[i] = -0.5 * (sum * base);
      }
    }
  }
}
