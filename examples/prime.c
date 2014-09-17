#include <spitz/barray.h>
#include <stdio.h>
#include <math.h>

#define UNUSED(x) (void)(x);

/*---------------------------------------------------------------------------*/
/* JOB MANAGER --------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
struct prime_jm {
	int numpoints;
};

void *spits_job_manager_new(int argc, char *argv[])
{
	UNUSED(argc);
	struct prime_jm *self = malloc(sizeof(*self));
	self->numpoints = atoi(argv[0]);
	return self;
}

int spits_job_manager_next_task(void *user_data, struct byte_array *ba)
{
	struct prime_jm *self = user_data;
	if (self->numpoints == 0)
		return 0;

	byte_array_pack64(ba, self->numpoints);
	self->numpoints--;

	return 1;
}

/*---------------------------------------------------------------------------*/
/* WORKER -------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void *spits_worker_new(int argc, char **argv)
{
	UNUSED(argc);
	UNUSED(argv);
	return NULL;
}

void spits_worker_run(void *user_data, struct byte_array *task, struct byte_array *result)
{
        // Local Variables
	uint64_t test_value;
        uint64_t i; 
        double sqrt_value;
        uint64_t sqrt_cast;
        uint64_t zero=0;
        
	UNUSED(user_data);
        byte_array_unpack64(task, &test_value);

        sqrt_value = sqrt(test_value);
        sqrt_cast = (uint64_t) sqrt_value;

        if(sqrt_value > ((double)sqrt_cast))
            sqrt_cast++;

        for(i=1; i<=sqrt_cast; i++) {
            if((test_value%i)==0) {
                byte_array_pack64(result, zero);
                return; 
            }
        }
        
    byte_array_pack64(result, test_value);
}

/*---------------------------------------------------------------------------*/
/* COMMIT -------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
struct prime_list{
    uint64_t value;
    struct prime_list * next;
};

struct prime_list * list_pointer;

void *spits_setup_commit(int argc, char *argv[])
{
	UNUSED(argc);
	struct prime_jm *self = malloc(sizeof(*self));
	self->numpoints = atoi(argv[0]);
	return self;
}

void spits_commit_pit(void *user_data, struct byte_array *result)
{
    uint64_t x; 
    struct prime_list * insertion;   
   
    UNUSED(user_data);
    
	byte_array_unpack64(result, &x);
    
    if(x != 0) {
        if(list_pointer == NULL) {
            list_pointer = (struct prime_list *) malloc (sizeof(struct prime_list));
            list_pointer->value = x;
            list_pointer->next = NULL;
        }
        else {
            insertion = (struct prime_list *) malloc (sizeof(struct prime_list));
            insertion->value = x;
            insertion->next = list_pointer;
            list_pointer = insertion;
        }
    }
}

void spits_commit_job(void *user_data, struct byte_array *final_result)
{
    struct prime_list * iter;
    
    UNUSED(user_data);
    UNUSED(final_result);
    iter = list_pointer;

    printf("Prime Numbers : ");
    while(iter != NULL) {
        printf("%d", (int) iter->value);
        iter = iter->next;
    }
    printf("\n");       
}
