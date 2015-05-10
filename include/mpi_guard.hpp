#ifndef MPI_GUARD_H
#define MPI_GUARD_H

#include "mpi_mutex.hpp"

class mpi_guard {
	private:
		mpi_mutex *m;
	public:
		mpi_guard(mpi_mutex *_m);
		~mpi_guard();
};

#endif
