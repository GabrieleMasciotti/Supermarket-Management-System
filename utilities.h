
// ----------------------- FUNZIONI AUSILIARIE DI CONTROLLO ERRORI -----------------------

void* Malloc(size_t size){
	void * res=malloc(size);
	if(res==NULL){
		fprintf(stderr,"malloc failed");
		exit(EXIT_FAILURE);}
	return res;
}

void Pthread_mutex_lock(pthread_mutex_t* mtx){
	int lockerr=pthread_mutex_lock(mtx);
	if(lockerr!=0){
		fprintf(stderr,"lock failed; new attempt...\n");
		lockerr=pthread_mutex_lock(mtx);
		if(lockerr!=0){
			fprintf(stderr,"lock failed again; program termination.\n");
			exit(EXIT_FAILURE);
		}
	}
}

void Pthread_mutex_unlock(pthread_mutex_t* mtx){
	int lockerr=pthread_mutex_unlock(mtx);
	if(lockerr!=0){
		fprintf(stderr,"unlock failed; new attempt...\n");
		lockerr=pthread_mutex_unlock(mtx);
		if(lockerr!=0){
			fprintf(stderr,"unlock failed again; program termination.\n");
			exit(EXIT_FAILURE);
		}
	}
}

void Pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mtx){
	int conderr=pthread_cond_wait(cond,mtx);
	if(conderr!=0){
		fprintf(stderr,"condition variable wait failed; new attempt...\n");
		conderr=pthread_cond_wait(cond,mtx);
		if(conderr!=0){
			fprintf(stderr,"condition variable wait failed again; program termination.\n");
			exit(EXIT_FAILURE);
		}
	}
}

void Pthread_cond_signal(pthread_cond_t* cond){
	int conderr=pthread_cond_signal(cond);
	if(conderr!=0){
		fprintf(stderr,"condition variable signal failed; new attempt...\n");
		conderr=pthread_cond_signal(cond);
		if(conderr!=0){
			fprintf(stderr,"condition variable signal failed again; program termination.\n");
			exit(EXIT_FAILURE);
		}
	}
}

