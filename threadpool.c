#include <pthread.h>
#include <stdio.h>
#include "threadpool.h"
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>



void* do_work(void* thread_pool) {
    threadpool *p = (threadpool*)thread_pool;
    work_t * qwork;
    while (1) {
        p->qsize = p->qsize;
        pthread_mutex_lock(p->qlock);

        while (p->qsize == 0) {
            if (p->shutdown == 1) {
                pthread_mutex_unlock(p->qlock);
                pthread_exit(NULL);
            }
            pthread_mutex_unlock(p->qlock);
            pthread_cond_wait(&(p->q_not_empty), p->qlock);

            //If after the process wakes up, we need to destroy the processes
            if (p->shutdown == 1) {
                pthread_mutex_unlock(p->qlock);
                pthread_exit(NULL);
            }
        }
        qwork = p->qhead;
        p->qsize--;
        if(p->qsize == 0)
            p->qhead = p->qtail = NULL;
        else
            p->qhead = qwork->next;

        // We release the destroy that went to sleep
        // that the queue was still full
        if(p->shutdown != 1 && p->qsize == 0 )
            pthread_cond_signal(&(p->q_empty));

        //So, we in case: p->size > 0 or p->dont_accept == 0 --> we sent work to thread.
        pthread_mutex_unlock(p->qlock);
        qwork->routine(qwork->arg);
        free(qwork);

    }
}



threadpool* create_threadpool(int sum_t){
    threadpool *thread_pool;
    if(sum_t <= 0 || sum_t > MAXT_IN_POOL){
        printf("You need to type number bigger from 0");
        return NULL;
    }

    thread_pool = (threadpool*) malloc(sizeof(threadpool));
    if(thread_pool == NULL){
        perror("The malloc is failed");
        return NULL;
    }

    thread_pool->qhead = NULL;
    thread_pool->qtail = NULL;
    thread_pool->qsize = 0;
    thread_pool->shutdown = 0;
    thread_pool->dont_accept = 0;
    thread_pool->num_threads = sum_t;
    thread_pool->qsize = 0;
    thread_pool->threads = (pthread_t*)malloc(sizeof (pthread_t)*sum_t);
    if(!thread_pool->threads){
        printf("The malloc is failed");
        return NULL;
    }
    thread_pool->qlock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if(!thread_pool->qlock){
        printf("The malloc is failed");
        return NULL;
    }

    if(pthread_mutex_init( thread_pool->qlock , NULL) ){
        perror("pthread_mutex_init: ");
        return NULL;
    }
    if(pthread_cond_init(&(thread_pool->q_empty) , NULL) ){
        perror("pthread_cond_init: ");
        return NULL;
    }
    if(pthread_cond_init(&(thread_pool->q_not_empty) , NULL)){
        perror("pthread_cond_init: ");
        return NULL;
    }

    int i , rec;
    for( i = 0 ; i < sum_t ; i ++){
        rec = pthread_create(&(thread_pool->threads[i]) , NULL , do_work , thread_pool);
        if(rec != 0){
            perror("pthread_create:");
            free(thread_pool->qlock);
            free(thread_pool->threads);
            return NULL;
        }
    }
    return thread_pool;
}



//gets the pool, a pointer to the thread execution routine, and the argument to the thread execution routine. dispatch should
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    if(from_me->dont_accept == 1)
        return;

    work_t * new_work = (work_t*)malloc(sizeof(work_t));
    if(new_work == NULL){
        printf("The malloc is failed");
        return ;
    }

    new_work->routine = dispatch_to_here;
    new_work->arg = arg;
    new_work->next = NULL;

    if(from_me->dont_accept == 1){
        free(new_work);
        return;
    }

    pthread_mutex_lock(from_me->qlock);
    if(from_me->qsize == 0){
        from_me->qhead = new_work;
        from_me->qtail = new_work;
        pthread_cond_signal(&(from_me->q_not_empty));
    }
    else{
        from_me->qtail->next = new_work;
        from_me->qtail = new_work;
    }
    from_me->qsize ++;
    pthread_mutex_unlock(from_me->qlock);

}




void destroy_threadpool(threadpool* destroyme){
    void *retval;
    pthread_mutex_lock(destroyme->qlock);
    destroyme->dont_accept = 1;

    while(destroyme->qsize > 0)
        pthread_cond_wait(&(destroyme->q_empty), destroyme->qlock);

    destroyme->shutdown = 1;
    pthread_cond_broadcast(&(destroyme->q_not_empty));
    pthread_mutex_unlock(destroyme->qlock);

    for(int i = 0 ; i < destroyme->num_threads ; i ++){
        pthread_cond_broadcast(&(destroyme->q_not_empty));
        pthread_join(destroyme->threads[i] , &retval);
    }

    //free
    free(destroyme->threads);
    pthread_mutex_destroy(destroyme->qlock);
    free(destroyme->qlock);
    pthread_cond_destroy(&(destroyme->q_not_empty));
    pthread_cond_destroy(&(destroyme->q_empty));
    //free
    free(destroyme);
}

