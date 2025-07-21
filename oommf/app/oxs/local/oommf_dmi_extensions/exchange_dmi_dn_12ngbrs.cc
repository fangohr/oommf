/* FILE: ExchangeAndDMI_dn_12ngbrs.cc
 *
 * Exchange and Dzyaloshinskii-Moriya field and energy calculation.
 *
 * DMI is defined for the Dn crystallographic class [1, 2]:
 *
 * $w_\text{dmi} = D1 * ( L_{xz}^{(y)} +  L_{zy}^{(x)} ) + D2 * L_{xy}^{(z)}
 * 
 * Note that this has opposite signs than the DMI reported in [3]
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
 * [3] Ado et al. PRB 101, 161403(R) (2020)
 *
 */

#include <string>
#include "atlas.h"
#include "director.h"
#include "energy.h" // Needed to make MSVC++ 5 happy
#include "exchange_dmi_dn_12ngbrs.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "nb.h"
#include "rectangularmesh.h"
#include "simstate.h"
#include "threevector.h"

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_ExchangeAndDMI_Dn_12ngbrs);

/* End includes */

// Constructor
Oxs_ExchangeAndDMI_Dn_12ngbrs::Oxs_ExchangeAndDMI_Dn_12ngbrs(
    const char *name,     // Child instance id
    Oxs_Director *newdtr, // App director
    const char *argstr)   // MIF input block parameters
    : Oxs_Energy(name, newdtr, argstr), A_size(0), D1(0), D2(0), Aex(0),
      xperiodic(0), yperiodic(0), zperiodic(0), mesh_id(0) {
  // Process arguments
  OXS_GET_INIT_EXT_OBJECT("atlas", Oxs_Atlas, atlas);
  atlaskey.Set(atlas.GetPtr());
  /// Dependency lock is held until *this is deleted.

  // Check for optional default_D parameter; default is 0.
  // TODO: make parameters Scalar fields
  // D1 = GetRealInitValue("D1", 0.0);

  OC_BOOL has = HasInitValue("D1");
  if (!has) {
    throw Oxs_ExtError(this, "DMI value D1 not specified");
  } else {
    D1 = GetRealInitValue("D1");
  }

  has = HasInitValue("D2");
  if (!has) {
    throw Oxs_ExtError(this, "DMI value D2 not specified");
  } else {
    D2 = GetRealInitValue("D2");
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
  // The Oxs_CommonRectangularMesh is a common interface for both, which contains
  // the xdim ... dimensions of the rectangular mesh
  // (we only support these rect meshes for the 12 ngbr DMI class)
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

  printf("mesh size = %d \n", size);
  printf("mesh dx = %d \n", mesh->DimX());
  printf("mesh dy = %d \n", mesh->DimY());
  printf("mesh dz = %d \n", mesh->DimZ());
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

  VerifyAllInitArgsUsed();
}

Oxs_ExchangeAndDMI_Dn_12ngbrs::~Oxs_ExchangeAndDMI_Dn_12ngbrs() {
  // if (A_size > 0 && D != NULL && Aex != NULL) {
  //   delete[] D1[0];
  //   delete[] D1;
  //   delete[] D2[0];
  //   delete[] D2;
  //   delete[] Aex[0];
  //   delete[] Aex;
  // }
}

OC_BOOL Oxs_ExchangeAndDMI_Dn_12ngbrs::Init() {
  mesh_id = 0;
  region_id.Release();
  return Oxs_Energy::Init();
}

void Oxs_ExchangeAndDMI_Dn_12ngbrs::GetEnergy(const Oxs_SimState &state,
                                              Oxs_EnergyData &oed) const {

  const Oxs_MeshValue<ThreeVector> &spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m> &Ms_inverse = *(state.Ms_inverse);

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m> &energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector> &field = *oed.field_buffer;

  // --------------------------------------------------------------------------

  OC_REAL8m mu0Inv = 1 / MU0;
  OC_REAL8m fdsign = 1.;
  ThreeVector ZeroVector(0., 0., 0.);
  ThreeVector SpinM2, SpinM1, SpinP1, SpinP2;

  for (OC_INDEX z = 0; z < zdim; z++) {
    for (OC_INDEX y = 0; y < ydim; y++) {
      for (OC_INDEX x = 0; x < xdim; x++) {

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

        if (ydim > 4) {

          OC_REAL8m dmz_dy, dmx_dy;
          ThreeVector d2my_dy2(0., 0., 0.);

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

          // If there is no material in the nearest neighbor (single spin hole), use 3-point stencil
          if ((Msi_M1 == 0.0 && Msi_M2 != 0.0) || (Msi_P1 == 0.0 && Msi_P2 != 0.0)) {
            dmz_dy = wgty * 0.5 * (SpinP1.z - SpinM1.z);
            dmx_dy = wgty * 0.5 * (SpinP1.x - SpinM1.x);
            d2my_dy2 = wgty * wgty * (SpinP1 - 2. * spin[i] + SpinM1);
          } else {
            dmz_dy = wgty * (1. / 12) * (SpinM2.z - 8. * SpinM1.z + 8. * SpinP1.z - SpinP2.z);
            dmx_dy = wgty * (1. / 12) * (SpinM2.x - 8. * SpinM1.x + 8. * SpinP1.x - SpinP2.x);
            d2my_dy2 = wgty * wgty * (1. / 12) * (-1. * SpinM2 + 16. * SpinM1 - 30. * spin[i] + 16. * SpinP1 - 1. * SpinP2);
          }

          sum += 2.0 * Aex * d2my_dy2;
          // DMI field
          sum.x += -2 * D1 * (dmz_dy);
          sum.z += 2 * D1 * (dmx_dy);
        }

        if (xdim > 4) {

          OC_REAL8m dmz_dx = 0.0, dmy_dx = 0.0;
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

          // If there is no material in the nearest neighbor, use 3-point stencil
          if ((Msi_M1 == 0.0 && Msi_M2 != 0.0) || (Msi_P1 == 0.0 && Msi_P2 != 0.0)) {
            dmz_dx = wgtx * 0.5 * (SpinP1.z - SpinM1.z);
            dmy_dx = wgtx * 0.5 * (SpinP1.y - SpinM1.y);
            d2mx_dx2 = wgtx * wgtx * (SpinP1 - 2. * spin[i] + SpinM1);
          } else {
            dmz_dx = wgtx * (1. / 12) * (SpinM2.z - 8. * SpinM1.z + 8. * SpinP1.z - SpinP2.z);
            dmy_dx = wgtx * (1. / 12) * (SpinM2.y - 8. * SpinM1.y + 8. * SpinP1.y - SpinP2.y);
            d2mx_dx2 = wgtx * wgtx * (1. / 12) * (-1. * SpinM2 + 16. * SpinM1 - 30. * spin[i] + 16. * SpinP1 - 1. * SpinP2);
          }

          sum += 2.0 * Aex * d2mx_dx2;
          // DMI field
          sum.y += 2 * D1 * (dmz_dx);
          sum.z += -2 * D1 * (dmy_dx);
        }

        // TODO: allow smaller samples in the other directions
        if (zdim > 4) {
          OC_REAL8m dmy_dz, dmx_dz;
          ThreeVector d2mz_dz2;

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

          // If there is no material in the nearest neighbor (single spin hole), use 3-point stencil
          if ((Msi_M1 == 0.0 && Msi_M2 != 0.0) || (Msi_P1 == 0.0 && Msi_P2 != 0.0)) {
            dmx_dz = wgtz * 0.5 * (SpinP1.x - SpinM1.x);
            dmy_dz = wgtz * 0.5 * (SpinP1.y - SpinM1.y);
            d2mz_dz2 = wgtz * wgtz * (SpinP1 - 2. * spin[i] + SpinM1);
          } else {
            dmy_dz = wgtz * (1. / 12) * (SpinM2.y - 8. * SpinM1.y + 8. * SpinP1.y - SpinP2.y);
            dmx_dz = wgtz * (1. / 12) * (SpinM2.x - 8. * SpinM1.x + 8. * SpinP1.x - SpinP2.x);
            d2mz_dz2 = wgtz * wgtz * (1. / 12) * (-1. * SpinM2 + 16. * SpinM1 - 30. * spin[i] + 16. * SpinP1 - 1. * SpinP2);
          }

          sum += 2.0 * Aex * d2mz_dz2;
          // DMI:
          sum.x += 2 * D2 * (dmy_dz);
          sum.y += 2 * D2 * (-dmx_dz);

          // Otherwise use 6-ngbr stencil
        } else if (zdim <= 4) {
          OC_REAL8m dmy_dz, dmx_dz;
          ThreeVector d2mz_dz2;

          dmy_dz = 0.5 * wgtz * (SpinP1.y - SpinM1.y);
          dmx_dz = 0.5 * wgtz * (SpinP1.x - SpinM1.x);
          d2mz_dz2 = wgtz * wgtz * (SpinM1 - 2.0 * spin[i] + SpinP1);

          sum += 2.0 * Aex * d2mz_dz2;
          // DMI:
          sum.x += 2 * D2 * (dmy_dz);
          sum.y += 2 * D2 * (-dmx_dz);
        }

        field[i] = (mu0Inv * Msii) * sum;
        energy[i] = -0.5 * (sum * base);
      }
    }
  }
}
