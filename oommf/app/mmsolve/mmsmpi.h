/* FILE: mmsmpi.h                 -*-Mode: c++-*-
 *
 * Base declarations for MPI functionality
 *
 */

#ifdef USE_MPI

#ifndef MMSMPI_H
#define MMSMPI_H

#include <mpi.h>

/* End includes */     /* Optional directive to pimake */

typedef void (*MMS_MPI_ROUTER)();
void Mmsolve_MpiWakeUp(MMS_MPI_ROUTER func);

extern MPI_Datatype MMS_COMPLEX;
extern int mms_mpi_size,mms_mpi_rank;

#endif /* MMSMPI_H */

#endif /* USE_MPI */
