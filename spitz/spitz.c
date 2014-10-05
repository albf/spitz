#include "spitz.h"

char * lib_path;
int CON_RETRIES = 3;

__thread int workerid = -1;
int nworkers;

int spitz_get_worker_id(void)
{
	return workerid;
}

int spitz_get_num_workers(void)
{
	return nworkers;
}
