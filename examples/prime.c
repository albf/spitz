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
	struct prime_jm *self = malloc(sizeof(*self));
	self->numpoints = atoi(argv[0]);
	return self;
}

int spits_job_manager_next_task(void *user_data, struct byte_array *ba)
{
	struct prime_jm *self = user_data;
	if (self->numpoints == 0) {
		return 0;
        }

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

// Check if a given number is prime.
void spits_worker_run(void *user_data, struct byte_array *task, struct byte_array *result)
{
    // Local Variables
    uint64_t test_value;
    uint64_t number;
    uint64_t i; 
    double sqrt_value;
    uint64_t sqrt_cast;
    uint64_t zero=0;
    int is_prime;
    uint64_t total_prime=0;
    size_t len_b;
    
    UNUSED(user_data);
    byte_array_unpack64(task, &test_value);

    // Resize the byte array to fit all prime numbers.
    if(num_for_task > 2) {
        byte_array_resize(result, (size_t)(num_for_task*8));
    }

    // Pack zero to keep a place for the total primes found, added in the end.
    byte_array_pack64(result, zero);

    // Search for all the numbers is the granularity range.
    for(number = (test_value-1)*num_for_task; number < ((test_value)*num_for_task); number++) {
        //printf("PRIME.C => Testing : %" PRIu64 "\n", number);
        //sleep(1);
        sqrt_value = sqrt(number);
        sqrt_cast = (uint64_t) sqrt_value;

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
            byte_array_pack64(result, number);
            total_prime++;
        }
    }
        
    // Pack the number of primes found in total in this range.
    len_b = result->len;
    result->len=8;
    byte_array_pack64(result, total_prime);
    result->len=len_b;
}

/*---------------------------------------------------------------------------*/
/* COMMIT -------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

// Linked list with the results
struct prime_list{
    uint64_t value;
    struct prime_list * next;
};

// Pointer to the list with results
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
    uint64_t i;
    uint64_t value;
    struct prime_list * insertion;   
   
    UNUSED(user_data);
    
   // Get the total values found.
    byte_array_unpack64(result, &x);

    // Checks if the value passed is different then zero and insert in the list.
       for(i=0; i<x; i++) {
           byte_array_unpack64(result, &value); 
           
            if(list_pointer == NULL) {
                list_pointer = (struct prime_list *) malloc (sizeof(struct prime_list));
                list_pointer->value = value;
                list_pointer->next = NULL;
            }
            else {
                insertion = (struct prime_list *) malloc (sizeof(struct prime_list));
                insertion->value = value;
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

    // Just print everyone.
    printf("Prime Numbers : ");
    while(iter != NULL) {
        printf("%d", (int) iter->value);
        iter = iter->next;
        printf("\n");
    }
    printf("\n");       
}