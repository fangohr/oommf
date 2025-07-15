#ifndef _OXS_MESH_NEIGHBORS
#define _OXS_MESH_NEIGHBORS
// Functions to calculate neighbors
// Variable types OC_* are defined in oommf/pkg/oc/procs.tcl
#include <vector>

inline OC_INDEX GetIndex(OC_INDEX x, OC_INDEX y, OC_INDEX z,
                         int xdim, int ydim, int zdim,
                         int xperiodic, int yperiodic, int zperiodic) {

  if (xperiodic) {          // if mesh is periodic in x-direction
    if (x < 0) {            // then wrap the left side
      x += xdim;            // to the right
    }
    else if (x >= xdim) {   // and wrap the right side
      x -= xdim;            // to the left
    }
  }

  if (yperiodic) {
    if (y < 0) {
      y += ydim;
    }
    else if (y >= ydim) {
       y -= ydim;
    }
  }

  if (zperiodic) {
    if (z < 0) {
      z += zdim;
    }
    else if (z >= zdim) {
       z -= zdim;
    }
  }

  if (x < 0 or y < 0 or z < 0 or z >= zdim or y >= ydim or x >= xdim) {
    return -1;
  } else {
    return x + y * xdim + z * (xdim * ydim);
  }
};

// TODO: we could just get the mesh parameters passing the mesh object although we would have to
// ehck if we have a periodic or normal mesh
inline void ComputeNeighbors(std::vector<OC_INDEX> &n_neighbors,
                             std::vector<OC_INDEX> &nn_neighbors,
                             int xdim, int ydim, int zdim,
                             int xperiodic, int yperiodic, int zperiodic
                             ) {

  for (OC_INDEX z = 0; z < zdim; z++) {
    for (OC_INDEX y = 0; y < ydim; y++) {
      for (OC_INDEX x = 0; x < xdim; x++) {
        n_neighbors.push_back(GetIndex(x - 1, y,     z    , xdim, ydim, zdim, xperiodic, yperiodic, zperiodic));  // # left
        n_neighbors.push_back(GetIndex(x + 1, y,     z    , xdim, ydim, zdim, xperiodic, yperiodic, zperiodic));  // # right
        n_neighbors.push_back(GetIndex(x,     y - 1, z    , xdim, ydim, zdim, xperiodic, yperiodic, zperiodic));  // # behind
        n_neighbors.push_back(GetIndex(x,     y + 1, z    , xdim, ydim, zdim, xperiodic, yperiodic, zperiodic));  // # in front
        n_neighbors.push_back(GetIndex(x,     y,     z - 1, xdim, ydim, zdim, xperiodic, yperiodic, zperiodic));  // # under
        n_neighbors.push_back(GetIndex(x,     y,     z + 1, xdim, ydim, zdim, xperiodic, yperiodic, zperiodic));  // # over
        // printf("x = %d  y = %d  mesh index i = %d\n",x, y, mesh->Index(x, y, z));

        // Next nearest neighbors
        nn_neighbors.push_back(GetIndex(x - 2, y,     z    , xdim, ydim, zdim, xperiodic, yperiodic, zperiodic));  // -x
        nn_neighbors.push_back(GetIndex(x + 2, y,     z    , xdim, ydim, zdim, xperiodic, yperiodic, zperiodic));  // +x
        nn_neighbors.push_back(GetIndex(x,     y - 2, z    , xdim, ydim, zdim, xperiodic, yperiodic, zperiodic));  // -y
        nn_neighbors.push_back(GetIndex(x,     y + 2, z    , xdim, ydim, zdim, xperiodic, yperiodic, zperiodic));  // +y
        nn_neighbors.push_back(GetIndex(x,     y,     z - 2, xdim, ydim, zdim, xperiodic, yperiodic, zperiodic));  // -z
        nn_neighbors.push_back(GetIndex(x,     y,     z + 2, xdim, ydim, zdim, xperiodic, yperiodic, zperiodic));  // +z

      }
    }
  }
}

#endif // _OXS_MESH_NEIGHBORS
