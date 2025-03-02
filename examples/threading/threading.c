#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void wait_for_milliseconds(int milliseconds) {
    usleep(milliseconds * 1000); // Convert milliseconds to microseconds
}

void* threadfunc(void* thread_param)
{
     
    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    int rc=0;
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    wait_for_milliseconds(thread_func_args->wait_to_obtain_ms);
    rc = pthread_mutex_lock(thread_func_args->mutex);
    if ( rc != 0 ){
        printf("pthread_mutex_lock failed with %d\n",rc);
        thread_func_args->thread_complete_success = false;
    } else {
        wait_for_milliseconds(thread_func_args->wait_to_release_ms);
        rc = pthread_mutex_unlock(thread_func_args->mutex);
        if ( rc != 0 ){
            printf("pthread_mutex_unlock failed with %d\n",rc);
            thread_func_args->thread_complete_success = false;
        } else {
            thread_func_args->thread_complete_success = true;
        }
   
    }

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    int rc = -1;
    struct thread_data* thread_param_ptr = malloc(sizeof(struct thread_data));
    if (thread_param_ptr == NULL) {
        printf("failed to allocate memory for thread_data\n");
        return false;
    }

    thread_param_ptr->wait_to_obtain_ms       = wait_to_obtain_ms;
    thread_param_ptr->wait_to_release_ms      = wait_to_release_ms;
    thread_param_ptr->thread_complete_success = false;
    thread_param_ptr->mutex = mutex;
#if 0
    rc = pthread_mutex_init(thread_param_ptr->mutex, NULL);
    if (rc != 0) {
        printf("failed to initialize thread mutex\n");
        free(thread_param_ptr);
        return false;
    }
#endif        
    rc = pthread_create(
        thread,
        NULL, //use default attribute
        threadfunc,
        thread_param_ptr);
    if (rc != 0) {
        printf("failed to create thread\n");
        return false;
    }
    
    return true;
}

