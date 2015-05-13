 /* Copyright 2014 Alexandre Luiz Brisighello Filho <albf.unicamp@gmail.com> */

#include <spitz/barray.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

#define UNUSED(x) (void)(x);

// Granularity factor.
int num_for_task= 100;

/*---------------------------------------------------------------------------*/
/* JOB MANAGER --------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
struct prime_jm {
	int numpoints;
};

void *spits_job_manager_new(int argc, char *argv[])
{
	UNUSED(argc);
	struct prime_jm *self = (struct prime_jm *) malloc(sizeof(struct prime_jm));
	self->numpoints = atoi(argv[0]) -1;
	//printf("self->numpoints: %d\n", self->numpoints);
	return self;
}

int spits_job_manager_next_task(void *user_data, struct byte_array *ba)
{
	struct prime_jm *self = (struct prime_jm *) user_data;
	//printf("self->numpoints: %d\n", self->numpoints);
	if (self->numpoints < 0) {
		return 0;
        }

	byte_array_pack32(ba, self->numpoints);
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

// Check if a given number is prime.
void spits_worker_run(void *user_data, struct byte_array *task, struct byte_array *result)
{
    // Local Variables
    int test_value;
    int number;
    int i; 
    double sqrt_value;
    int sqrt_cast;
    int is_prime;
    int total_prime=0;
    
    UNUSED(user_data);
    byte_array_unpack32(task, &test_value);

    // Resize the byte array to fit all prime numbers.
    byte_array_resize(result, (size_t)(num_for_task*4));
    printf("LEN: %d\n", (int)result->len);

    //sleep(5);

    // Search for all the numbers is the granularity range.
    for(number = (test_value)*num_for_task; number < ((test_value+1)*num_for_task); number++) {
        //printf("PRIME.C => Testing : %d\n", number);
        sqrt_value = sqrt(number);
        sqrt_cast = (int) sqrt_value;

        // Check if it is smaller than 2.
        if(number<2) {
            continue;
        }
        
        // Get the square root value.
        if(sqrt_value > ((double)sqrt_cast)) {
            sqrt_cast++;
        }

        // Test all the values until the sqrt (2 or more).
        is_prime=1;
        for(i=2; i<=sqrt_cast; i++) {
            if((number%i)==0) {
                is_prime = 0;
                break;
            }
        }
        // If it's a prime, pack it.
        if(is_prime == 1) { 
            //printf("PACKING %d\n", number);
            byte_array_pack32(result, number);
            //printf("LEN: %d\n", (int)result->len);
            total_prime++;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* COMMIT -------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

// Linked list with the results
struct prime_list{
    int value;
    struct prime_list * next;
};

// Pointer to the list with results
void *spits_setup_commit(int argc, char *argv[])
{
	UNUSED(argc);
	UNUSED(argv);
	struct prime_list * list_pointer = (struct prime_list *) malloc(sizeof(struct prime_list));
	list_pointer->value = -1;
	list_pointer->next = NULL;
	return list_pointer;
}

void spits_commit_pit(void *user_data, struct byte_array *result)
{
	int x;
	struct prime_list * list_pointer = (struct prime_list *) user_data; 
	struct prime_list * insertion;
   
	UNUSED(user_data);
    
	while(byte_array_unpack32(result, &x)) {
		//printf("COMMITTING %d\n", x);
		insertion = (struct prime_list *) malloc (sizeof(struct prime_list));
		insertion->value = x;
		insertion->next = list_pointer->next;
		list_pointer->next = insertion;
	}
}

void spits_commit_job(void *user_data, struct byte_array *final_result)
{
    struct prime_list * iter;
    struct prime_list * bf;
    int dirty=1;
    int loop_over=0;    
    int aux;

    UNUSED(user_data);
    UNUSED(final_result);
    
    bf = (struct prime_list *) user_data;
    iter = bf-> next;

    // Bubble Sort
    while(dirty==1) {
       dirty = 0; 
       loop_over = 0;

        while (loop_over == 0) {
            if(bf->value > iter->value) {
                aux=bf->value;
                bf->value = iter->value;
                iter->value = aux;
                dirty=1;
            }

            if(iter->next!=NULL) {
                bf=iter;
                iter=iter->next;
            }
            else {
                bf=(struct prime_list *) user_data;
                iter = bf->next;
                loop_over=1;
            }
        }
    }


    // Just print everyone.
    printf("Prime Numbers : \n");
    
    iter = (struct prime_list *) user_data;
    iter = iter->next;
    while(iter != NULL) {
        printf("%d", (int) iter->value);
        iter = iter->next;
        printf("\n");
    }
    printf("\n");       
}
