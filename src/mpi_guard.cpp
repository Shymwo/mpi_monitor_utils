#include <mpi_guard.hpp>

mpi_guard::mpi_guard(mpi_mutex *_m) {
	m = _m;
	m->lock();
};

mpi_guard::~mpi_guard() {
	m->unlock();
};
