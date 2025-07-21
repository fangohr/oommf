/* FILE: exchange_dmi_cn_12ngbrs.cc
 *
 * Exchange and Dzyaloshinskii-Moriya field and energy calculation.
 *
 * DMI is defined for the S4 crystallographic class [1, 2]:
 *
 * $w_\text{dmi} = D1 ( L_{zx}^{(x)} + L_{yz}^{(y)} ) + D2 ( L_{yz}^{(x)} - L_{zx}^{(y)} )
 *
 * Note that this has the opposite sign than in [2].
 *
 * This extension works both with and without periodic boundary conditions.
 *
 * Extension and modification by David Cortes-Ortuno based on the
 * oommf-extension-dmi-cnv class by the joommf project
 *
 * [1] A. N. Bogdanov and D. A. Yablonskii. Zh. Eksp. Teor. Fiz. 95, 178-182 (1989).
 * [2] Ado et al. PRB 101, 161403(R) (2020)
 */

#include <string>

#include "atlas.h"
#include "director.h"
#include "energy.h" // Needed to make MSVC++ 5 happy
#include "exchange_dmi_s4_12ngbrs.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "nb.h"
#include "rectangularmesh.h"
#include "simstate.h"
#include "threevector.h"

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_ExchangeAndDMI_S4_12ngbrs);

/* End includes */

// Constructor
Oxs_ExchangeAndDMI_S4_12ngbrs::Oxs_ExchangeAndDMI_S4_12ngbrs(
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

Oxs_ExchangeAndDMI_S4_12ngbrs::~Oxs_ExchangeAndDMI_S4_12ngbrs() {
  // if (A_size > 0 && D != NULL && Aex != NULL) {
  //   delete[] D[0];
  //   delete[] D;
  //   delete[] Aex[0];
  //   delete[] Aex;
  // }
}

OC_BOOL Oxs_ExchangeAndDMI_S4_12ngbrs::Init() {
  mesh_id = 0;
  region_id.Release();
  return Oxs_Energy::Init();
}

void Oxs_ExchangeAndDMI_S4_12ngbrs::GetEnergy(const Oxs_SimState &state,
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
  OC_REAL8m Dpair, Aexpair;

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

          OC_REAL8m dmz_dy, dmy_dy, dmx_dy;
          ThreeVector d2my_dy2(0.0, 0.0, 0.0);

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


          // If there is no material in the nearest neighbor (single spin hole), use 3-point stencil
          if ((Msi_M1 == 0.0 && Msi_M2 != 0.0) || (Msi_P1 == 0.0 && Msi_P2 != 0.0)) {
            dmx_dy = wgty * 0.5 * (SpinP1.x - SpinM1.x);
            dmy_dy = wgty * 0.5 * (SpinP1.y - SpinM1.y);
            dmz_dy = wgty * 0.5 * (SpinP1.z - SpinM1.z);
            d2my_dy2 = wgty * wgty * (SpinP1 - 2. * spin[i] + SpinM1);
          } else {
            dmx_dy = wgty * (1. / 12) * (SpinM2.x - 8. * SpinM1.x + 8. * SpinP1.x - SpinP2.x);
            dmy_dy = wgty * (1. / 12) * (SpinM2.y - 8. * SpinM1.y + 8. * SpinP1.y - SpinP2.y);
            dmz_dy = wgty * (1. / 12) * (SpinM2.z - 8. * SpinM1.z + 8. * SpinP1.z - SpinP2.z);
            d2my_dy2 = wgty * wgty * (1. / 12) * (-1. * SpinM2 + 16. * SpinM1 - 30. * spin[i] + 16. * SpinP1 - 1. * SpinP2);
          }

          sum += 2.0 * Aex * d2my_dy2;
          // DMI field
          sum.x += -2 * (D2 * dmz_dy);
          sum.y += 2 * (D1 * dmz_dy);
          sum.z += -2 * (D1 * dmy_dy - D2 * dmx_dy);
        }

        if (xdim > 4) {

          OC_REAL8m dmz_dx, dmx_dx, dmy_dx;
          ThreeVector d2mx_dx2(0.0, 0.0, 0.0);

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
            dmx_dx = wgtx * 0.5 * (SpinP1.x - SpinM1.x);
            dmy_dx = wgtx * 0.5 * (SpinP1.y - SpinM1.y);
            dmz_dx = wgtx * 0.5 * (SpinP1.z - SpinM1.z);
            d2mx_dx2 = wgtx * wgtx * (SpinP1 - 2. * spin[i] + SpinM1);
          } else {
            dmx_dx = wgtx * (1. / 12) * (SpinM2.x - 8. * SpinM1.x + 8. * SpinP1.x - SpinP2.x);
            dmy_dx = wgtx * (1. / 12) * (SpinM2.y - 8. * SpinM1.y + 8. * SpinP1.y - SpinP2.y);
            dmz_dx = wgtx * (1. / 12) * (SpinM2.z - 8. * SpinM1.z + 8. * SpinP1.z - SpinP2.z);
            d2mx_dx2 = wgtx * wgtx * (1. / 12) * (-1. * SpinM2 + 16. * SpinM1 - 30. * spin[i] + 16. * SpinP1 - 1. * SpinP2);
          }

          // dmz_dx = wgtx * ((1. / 12) * SpinM2.z - (8. / 12) * SpinM1.z +
          //                  (8. / 12) * SpinP1.z - (1. / 12) * SpinP2.z);
          // dmx_dx = wgtx * ((1. / 12) * SpinM2.x - (8. / 12) * SpinM1.x +
          //                  (8. / 12) * SpinP1.x - (1. / 12) * SpinP2.x);
          // dmy_dx = wgtx * ((1. / 12) * SpinM2.y - (8. / 12) * SpinM1.y +
          //                  (8. / 12) * SpinP1.y - (1. / 12) * SpinP2.y);

          sum += 2.0 * Aex * d2mx_dx2;
          // DMI field
          sum.x += -2 * (D1 * dmz_dx);
          sum.y += -2 * (D2 * dmz_dx);
          sum.z += 2 * (D1 * dmx_dx + D2 * dmy_dx);
        }

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
