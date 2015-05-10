#include <mpi_mutex.hpp>

#define MSGSIZE 3

// This class implements Ricart-Agrawala algorithm
// with addition of handling conditional variables


// converts struct to array to send it more easily
void messageToArray(mpi_mutex_msg msg, int *array) {
    array[0] = msg.type;   //type of the message
    array[1] = msg.clock;  //logical clock value of the process
    array[2] = msg.sigtag; //used to signal process waiting for condition
};

void arrayToMessage(int* array, mpi_mutex_msg *msg) {
    msg->type = (msgtype) array[0];
    msg->clock = array[1];
    msg->sigtag = array[2];
};

//Second thread implementation - used to receive messages and handle them
void *msg_monitor(void *p) {
	mpi_mutex_info *info = (mpi_mutex_info*)p;
	MPI::Status status;
	int my_id, size;
	my_id = MPI::COMM_WORLD.Get_rank();
	size = MPI::COMM_WORLD.Get_size();
	while (true) {
		mpi_mutex_msg msg;
		int array[3];
		int received = 0;
		int exit_monitor = 0;
		while (!received) {
			if (info->my_state == EXITING && (info->exit_count == size - 1)) {
				// turn off this process monitor method if everyone has already  
				// called destructor
				exit_monitor = 1;
				break;
			}
			// check for new messages each 1 microsecond to reduce cpu usage
			received = MPI::COMM_WORLD.Iprobe(MPI::ANY_SOURCE, info->tag);
			usleep(1);
		}
		if (exit_monitor) {
			break;
		}
		MPI::COMM_WORLD.Recv(array, MSGSIZE, MPI::INT, MPI::ANY_SOURCE, info->tag, status);
		arrayToMessage(array, &msg);
		// we want to avoid parallel handling message and locking distributed
		// mutex (setting state to waiting and setting logical clock should be
		// atomic) and we don't need to handle messages if we are using critical
		// section
		pthread_mutex_lock(&info->mutex);
		if (msg.clock > info->recv_clock) {
			// keep information about the highest logical clock of all
			info->recv_clock = msg.clock;
		}
		int recv_id = status.Get_source();
		// we have received a request made by another process 
		// to enter the critical section
		if (msg.type == REQUEST) {
			// send answer only if we are not interested in critical section
			// or our logical clock is higher than the other process
			if (info->my_state == INACTIVE or info->my_state == EXITING or 
				(msg.clock < info->my_clock || 
				(msg.clock == info->my_clock && recv_id < my_id))) {
				mpi_mutex_msg reply;
				reply.type = ANSWER;
				reply.clock = info->my_clock;
				reply.sigtag = 0;
				int reply_array[3];
				messageToArray(reply, reply_array);
				MPI::COMM_WORLD.Send(reply_array, MSGSIZE, MPI::INT, recv_id, info->tag);
			} else {
				// defer sending answer - send it after leaving critical section
				// see prepare_unlock
				info->waiting_list.push(recv_id);
			}
		} 
		// we have received answer from other process that we can enter 
		// critical section
		else if (msg.type == ANSWER) {
			if (info->my_state == WAITING) {
				info->answers_count++;
				if (info->answers_count == size - 1) {
					// we have received all answers and 
					// we can enter critical section
					info->my_state = ACTIVE;
					// signal main thread to wake up
					pthread_cond_signal(&info->cond);
					// wait for critical section to end because
					// before that we don't have to handle messages 
					while (info->my_state == ACTIVE) {
						pthread_cond_wait(&info->cond2, &info->mutex);
					}
				}
			}
		} 
		// we have received a signal that we can wake up ourselves
		else if (msg.type == SIGNAL) {
			// check if this signal is connected with our condition variable
			if (info->custom_cond && msg.sigtag == info->custom_cond->get_tag()) {
				pthread_cond_signal(info->custom_cond->get_pthread_cond());
			}
		} 
		// we have received info that other process called his destructor
		else if (msg.type == EXIT) {
			info->exit_count++;
		}
		pthread_mutex_unlock(&info->mutex);
	}
	
	return NULL;
};

mpi_mutex::mpi_mutex(int _tag) {
	info.tag = _tag;
	info.my_state = INACTIVE;
	info.my_clock = 0;
	info.recv_clock = 0;
	info.answers_count = 0;
	info.exit_count = 0;
	// creating thread to receive and handle messages
	pthread_create(&thread, NULL, msg_monitor, &info);
};

mpi_mutex::~mpi_mutex() {
	info.my_state = EXITING;
	mpi_mutex_msg msg;
	msg.type = EXIT;
	msg.clock = 0;
	msg.sigtag = 0;
	int array[3];
	messageToArray(msg, array);
	int i = 0;
	// inform others that we have done our work
	for (i = 0; i < MPI::COMM_WORLD.Get_size(); i++) {
		if (i != MPI::COMM_WORLD.Get_rank()) {
			MPI::COMM_WORLD.Send(array, MSGSIZE, MPI::INT, i, info.tag);
		}
	}
	// wait for second thread to exit 
	// (it may still need to handle other processes messages)
	pthread_join(thread, NULL);
};
	
int mpi_mutex::lock() {
	pthread_mutex_lock(&info.mutex);
	prepare_lock();
	return 0;
};

int mpi_mutex::unlock() {
	prepare_unlock();
	pthread_mutex_unlock(&info.mutex);
	return 0;
};

int mpi_mutex::wait(mpi_cond *cond) {
	if (info.my_state == ACTIVE) {
		// unlock distributed mutex
		prepare_unlock();
		// wait for second thread to wake you up
		info.custom_cond = cond;
		pthread_cond_wait(info.custom_cond->get_pthread_cond(), &info.mutex);
		// after this lock mutex again
		prepare_lock();
	} else {
		return -1;
	}
	return 0;
};

int mpi_mutex::signal(mpi_cond *cond) {
	mpi_mutex_msg msg;
	msg.type = SIGNAL;
	msg.clock = info.my_clock;
	msg.sigtag = cond->get_tag();
	int array[3];
	messageToArray(msg, array);
	int i = 0;
	// just send the signal to all processes
	// they will check if it concerns them
	for (i = 0; i < MPI::COMM_WORLD.Get_size(); i++) {
		if (i != MPI::COMM_WORLD.Get_rank()) {
			MPI::COMM_WORLD.Send(array, MSGSIZE, MPI::INT, i, info.tag);
		}
	}
	return 0;
};

int mpi_mutex::prepare_lock() {
	// set our state to waiting and set logical cock
	info.my_state = WAITING;
	info.my_clock = info.recv_clock + 1;
	int i = 0;
	mpi_mutex_msg msg;
	msg.type = REQUEST;
	msg.clock = info.my_clock;
	msg.sigtag = 0;
	int array[3];
	messageToArray(msg, array);
	// send request to all processes
	for (i = 0; i < MPI::COMM_WORLD.Get_size(); i++) {
		if (i != MPI::COMM_WORLD.Get_rank()) {
			MPI::COMM_WORLD.Send(array, MSGSIZE, MPI::INT, i, info.tag);
		}
	}
	// wait for all answers - second thread will wake you up
	while (info.my_state != ACTIVE) {
		pthread_cond_wait(&info.cond, &info.mutex);
	}
	return 0;
};

int mpi_mutex::prepare_unlock() {
	mpi_mutex_msg msg;
	msg.type = ANSWER;
	msg.clock = info.my_clock;
	msg.sigtag = 0;
	int array[3];
	messageToArray(msg, array);
	// send all defered answers
	while (!info.waiting_list.empty()) {
		int id = info.waiting_list.top();
		MPI::COMM_WORLD.Send(array, MSGSIZE, MPI::INT, id, info.tag);
		info.waiting_list.pop();
	}
	info.answers_count = 0;
	// set my state to inactive and wake up second thread
	info.my_state = INACTIVE;
	pthread_cond_signal(&info.cond2);
	return 0;
};
