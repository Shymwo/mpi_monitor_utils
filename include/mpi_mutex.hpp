#ifndef MPI_MUTEX_H
#define MPI_MUTEX_H

#include <iostream>
#include <stack>
#include <pthread.h>
#include <unistd.h>
#include <mpich/mpi.h>
#include "mpi_cond.hpp"

enum state {INACTIVE, WAITING, ACTIVE, EXITING};
enum msgtype {REQUEST, ANSWER, SIGNAL, EXIT};

struct mpi_mutex_info {
	int tag;
	int my_clock;
	state my_state;
	int recv_clock;
	int answers_count;
	int exit_count;
	std::stack<int> waiting_list;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER;
	mpi_cond *custom_cond;
};

struct mpi_mutex_msg {
	msgtype type;
	int clock;
	int sigtag;
};

class mpi_mutex {
	private:
		pthread_t thread;
		mpi_mutex_info info;
		int prepare_lock();
		int prepare_unlock();
	public:
		mpi_mutex(int _tag);
		~mpi_mutex();
		int lock();
		int unlock();
		int wait(mpi_cond *cond);
		int signal(mpi_cond *cond);
};

#endif
