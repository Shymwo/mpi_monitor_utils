#ifndef MPI_COND_H
#define MPI_COND_H

#include <pthread.h>

class mpi_cond {
	private:
		int tag;
		pthread_cond_t pcond = PTHREAD_COND_INITIALIZER;
	public:
		mpi_cond(int _tag);
		int get_tag();
		pthread_cond_t* get_pthread_cond();
};

#endif
