#include <mpi_cond.hpp>

mpi_cond::mpi_cond(int _tag) {
	tag = _tag;
};
int mpi_cond::get_tag() {
	return tag;
};
pthread_cond_t* mpi_cond::get_pthread_cond() {
	return &pcond;
};
		
