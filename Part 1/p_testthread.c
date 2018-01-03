#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/errno.h>
#include <sched.h>
#include <math.h>

#include "qbuffer_dyn.h"

buffer *bus_in_q, *bus_out_q1, *bus_out_q2, *bus_out_q3;
pthread_mutex_t mutex;
pthread_t sendert[4], busdaemon, receivert[3];
pthread_attr_t tattr1[4],tattr2,tattr3[3];
struct sched_param param;

const int period_multiplier[] = {12,22,24,26,28,15,17,19};
const int thread_priority[] = {90,94,95,96,97,91,92,93};
unsigned int GLOBAL_SEQUENCE_NUMBER = 1;
unsigned int GLOBAL_SEQUENCE_NUMBER1 = 1;
unsigned int GLOBAL_SEQUENCE_NUMBER2 = 1;
unsigned int BUS_IN_Q=0, BUS_IN_Q1=0, BUS_OUT_Q=0, BUS_OUT_Q1=0, BUS_OUT_Q2=0, BUS_OUT_Q3=0,BUS_OUT_Q11=0,BUS_OUT_Q22=0,BUS_OUT_Q33=0;
int sflag = 0, bflag = 0;
long qarray[2000];
int msg_sent, msg_received, msg_dropped;
long sum_qtime=0L, avg_qtime=0L;

#define BASE_PERIOD 1000
#define NANO 1000000000LL

void gen_rand(char* string)
{
	char charset[] = "0123456789"
			"abcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int length = (rand() % 70) + 10;
	while (length-- > 0) {
		int index = rand() % 62;
		*string++ = charset[index];
  	}
	*string = '\0';
}


void* sender_func(void *ptr)
{
	int ret;
	int select;	
	int sleep;
	int errno;
	char* random_string = NULL;
	struct timespec time;

	message *new_msg;
	new_msg = (message *) malloc (sizeof(struct message_info));
    	
	select = *((int *)ptr);

        clock_gettime(CLOCK_MONOTONIC, &time);
		
	while(GLOBAL_SEQUENCE_NUMBER <= 100)
	{
		pthread_mutex_lock(&mutex);
		new_msg->message_id = GLOBAL_SEQUENCE_NUMBER++;
		pthread_mutex_unlock(&mutex);
	
		new_msg->source_id = select;
		new_msg->destination_id = rand() % 3;
		random_string = (char *)malloc(sizeof(char) * 80);
		gen_rand(random_string);
		strcpy(new_msg->var_string, random_string);
		new_msg->acc_qtime=0;
		new_msg->ctime=0;
	
//printf("Calling SQ_WRITE \n");

		pthread_mutex_lock(&mutex);
		ret = sq_write(bus_in_q, new_msg);		//calling enqueue
		pthread_mutex_unlock(&mutex);
		if(ret == sizeof(new_msg)) {
			pthread_mutex_lock(&mutex);
			GLOBAL_SEQUENCE_NUMBER++;
			pthread_mutex_unlock(&mutex);
			//printf("BUS_IN_Q FULL\n");
			errno = EINVAL;
			exit(-1);
		}
		else {
			//printf("BUS_IN_Q = %d\n",BUS_IN_Q);
			pthread_mutex_lock(&mutex);
			BUS_IN_Q++;
			BUS_IN_Q1++;
			pthread_mutex_unlock(&mutex);	
		//printf("Message Enqueued by Source %d is %s at  %d %d \n",new_msg->source_id, bus_in_q->array[(bus_in_q->end - 1) % Q_SIZE]->var_string, bus_in_q->start, bus_in_q->end - 1);	
		}	
	
		time.tv_nsec = time.tv_nsec + (period_multiplier[select] * BASE_PERIOD * 1000);
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time, 0);
	}
	free(new_msg);
	free(random_string);

}

void* busdaemon_func(void *arg1)
{
	message *msg = (message *)malloc(sizeof(message));
	int ret;
	int dest;
	struct timespec time2;

	clock_gettime(CLOCK_MONOTONIC, &time2);
  
	
	while(GLOBAL_SEQUENCE_NUMBER1 <= 100)
	{
		pthread_mutex_lock(&mutex);
		ret = sq_read(bus_in_q, msg);
//printf("Message read by BUS DAEMON Source:%d\t message:%d\t Destination:%d\t Message:%s BUS_IN_Q1=%d BUS_OUT_Q=%d\n",	msg->source_id, msg->message_id, msg->destination_id,msg->var_string, BUS_IN_Q1, BUS_OUT_Q);
//printf("After Dequeue Message = %s at %d to be sent to DEST=%d\n",msg->var_string, ret, msg->destination_id);
		pthread_mutex_unlock(&mutex);

		if(ret == -1) 
		{
			//printf("QUEUE is EMPTY\n");		
                	time2.tv_nsec = time2.tv_nsec + (period_multiplier[0] * BASE_PERIOD * 1000);
                	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time2, 0);
//			dest = msg->destination_id;
			
		}
		else {
			pthread_mutex_lock(&mutex);
			BUS_IN_Q1--;
			BUS_OUT_Q++;
			dest = msg->destination_id;
			pthread_mutex_unlock(&mutex); 
			if(dest == 0)
			{
				pthread_mutex_lock(&mutex);
				ret = sq_write(bus_out_q1, msg);
				pthread_mutex_unlock(&mutex);
				
				if(ret == sizeof(msg)) {
					GLOBAL_SEQUENCE_NUMBER1++;
					exit(-1);
					}
				else
				{
					//printf("Message Enqueued by DAEMON is %s at  %d %d \n",bus_out_q1->array[(bus_out_q1->end - 1) % Q_SIZE]->var_string, bus_out_q1->start, (bus_out_q1->end - 1) % Q_SIZE);
				pthread_mutex_lock(&mutex);
				BUS_OUT_Q1++;
				GLOBAL_SEQUENCE_NUMBER1++;
				pthread_mutex_unlock(&mutex);
				}
			}
			else if(dest == 1)
			{
				pthread_mutex_lock(&mutex);
				ret = sq_write(bus_out_q2, msg);
				pthread_mutex_unlock(&mutex);
				if(ret == sizeof(msg)) {
					GLOBAL_SEQUENCE_NUMBER1++;
					exit(-1);
					}
				else
				{
					//printf("Message Enqueued by DAEMON is %s at  %d %d \n",bus_out_q2->array[(bus_out_q2->end - 1) % Q_SIZE]->var_string, bus_out_q2->start, (bus_out_q2->end - 1)  % Q_SIZE);
				pthread_mutex_lock(&mutex);
				BUS_OUT_Q2++;
				GLOBAL_SEQUENCE_NUMBER1++;
				pthread_mutex_unlock(&mutex);
				}
			}
			else if(dest == 2)
			{
				pthread_mutex_lock(&mutex);
				ret = sq_write(bus_out_q3, msg);
				pthread_mutex_unlock(&mutex);
				if(ret == sizeof(msg)) {
					GLOBAL_SEQUENCE_NUMBER1++;
					exit(-1);
					}
				else
				{
					//printf("Message Enqueued by DAEMON is %s at  %d %d \n",bus_out_q3->array[(bus_out_q3->end - 1) % Q_SIZE]->var_string, bus_out_q3->start, (bus_out_q3->end - 1) % Q_SIZE);
			pthread_mutex_lock(&mutex);
				BUS_OUT_Q3++;
				GLOBAL_SEQUENCE_NUMBER1++;
			pthread_mutex_unlock(&mutex);
				}
			}
		}			
		time2.tv_nsec = time2.tv_nsec + (period_multiplier[0] * BASE_PERIOD * 1000);
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time2, 0);
		
		if(BUS_IN_Q1 == 0 && sflag == 1)
			break;
		if(BUS_IN_Q1<0)
			break;

	}
	//printf("EXIT WHILE BUS DAEMON\n");
	free(msg);
}
#if 1
void* receiver_func(void *ptr2)
{

	int ret;
	int stop=0;
	int no = *((int *) ptr2);
	long qtime;
	int i=0;
	struct timespec time3;
	//printf("receiver func \n");

	message *rmsg0 = (message *)malloc(sizeof(message));
	message *rmsg1 = (message *)malloc(sizeof(message));
	message *rmsg2 = (message *)malloc(sizeof(message));

	clock_gettime(CLOCK_MONOTONIC, &time3);
       //         printf("TIME 3 secs=%ld nsecs=%ld\n", time3.tv_sec, time3.tv_nsec);
	
	while(GLOBAL_SEQUENCE_NUMBER2 <= 100)
	{	
#if 1
		if(no == 0)
		{	
		pthread_mutex_lock(&mutex);
		        ret = sq_read(bus_out_q1, rmsg0);	
		pthread_mutex_unlock(&mutex);
			if(ret!=-1)
			{	pthread_mutex_lock(&mutex);
				qtime = rmsg0->acc_qtime;
				qarray[i++] = qtime;
				//printf("Q TIME = %ld\n",qtime);
				BUS_OUT_Q11++;
				GLOBAL_SEQUENCE_NUMBER2++;
				pthread_mutex_unlock(&mutex);
			//printf("Dequeued by RX0 Message = %s at %d %d RET=%d\n",rmsg0->var_string, bus_out_q1->start, bus_out_q1->end,ret);			}
		}
		else if(no == 1)
		{	
		pthread_mutex_lock(&mutex);
		        ret = sq_read(bus_out_q2, rmsg1);	
		pthread_mutex_unlock(&mutex);
			if(ret!=-1)
			{	pthread_mutex_lock(&mutex);
				qtime = rmsg1->acc_qtime;
				qarray[i++] = qtime;
				//printf("Q TIME = %ld\n",qtime);
				BUS_OUT_Q22++;
				GLOBAL_SEQUENCE_NUMBER2++;
				pthread_mutex_unlock(&mutex);
//printf("Dequeued by RX1 Message = %s at %d %d RET=%d\n",rmsg1->var_string, bus_out_q2->start, bus_out_q2->end, ret);
			}
		}
		else if(no == 2)
		{	
		pthread_mutex_lock(&mutex);
		        ret = sq_read(bus_out_q3, rmsg2);	
		pthread_mutex_unlock(&mutex);
			if(ret!=-1)
			{	pthread_mutex_lock(&mutex);
				qtime = rmsg2->acc_qtime;
				qarray[i++] = qtime;
				//printf("Q TIME = %ld\n",qtime);	
				BUS_OUT_Q33++;
				GLOBAL_SEQUENCE_NUMBER2++;
				pthread_mutex_unlock(&mutex);
//printf("Dequeued by RX2 Message = %s at %d %d RET=%d\n",rmsg2->var_string, bus_out_q3->start, bus_out_q3->end, ret);	
			}
		}
//(BUS_OUT_Q11 + BUS_OUT_Q22 + BUS_OUT_Q33) == (BUS_OUT_Q1 + BUS_OUT_Q2 + BUS_OUT_Q3)
//		if(ret==-1)
//		{
			if(GLOBAL_SEQUENCE_NUMBER2 == BUS_OUT_Q && bflag == 1)
				break; //stop = 1;
			else
			{
				time3.tv_nsec = time3.tv_nsec + (period_multiplier[no+5] * BASE_PERIOD * 1000);
				clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time3, 0);	
			}
//		}
#endif

#if 0
		switch(no) {
		case 0: 
			//printf("Reading bus_out_q1 \n");
			pthread_mutex_lock(&mutex);
		        ret = sq_read(bus_out_q1, rmsg0);
			
			if(ret == -1)
			{
				//exit(-1);
				
			//	time3.tv_nsec = time3.tv_nsec + (period_multiplier[select] * BASE_PERIOD);
		        //        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time3, 0);
			}
			else
			{
				GLOBAL_SEQUENCE_NUMBER2++;
				//count3++;	
        		printf("Dequeued by RX0 Message = %s at %d %d RET=%d\n",rmsg0->var_string, bus_out_q1->start, bus_out_q1->end, ret);
        		pthread_mutex_unlock(&mutex);
			}
		case 1:	
			//printf("Reading bus_out_q2 \n");
                        pthread_mutex_lock(&mutex);
                        ret = sq_read(bus_out_q2, rmsg1);
			if(ret == -1)
			{
				//exit(-1);
				
			//	time3.tv_nsec = time3.tv_nsec + (period_multiplier[select] * BASE_PERIOD);
		        //        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time3, 0);
			}
			else {
				GLOBAL_SEQUENCE_NUMBER2++;
				//count4++;
        		printf("Dequeued by RX1 Message = %s at %d %d RET=%d\n",rmsg1->var_string, bus_out_q2->start, bus_out_q2->end, ret);
                        pthread_mutex_unlock(&mutex);
			}

		case 2:
			//printf("Reading bus_out_q3 \n");
                        pthread_mutex_lock(&mutex);
                        ret = sq_read(bus_out_q3, rmsg2);
			if(ret == -1)
			{
				//exit(-1);
				
			//	time3.tv_nsec = time3.tv_nsec + (period_multiplier[select] * BASE_PERIOD);
		        //        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time3, 0);
			}
			else {
			GLOBAL_SEQUENCE_NUMBER2++;
				//count5++;
        		printf("Dequeued by RX2 Message = %s at %d %d RET=%d\n",rmsg2->var_string, bus_out_q3->start, bus_out_q3->end, ret);
                        pthread_mutex_unlock(&mutex);
			}	
			}
#endif              
		
                      	time3.tv_nsec = time3.tv_nsec + (period_multiplier[no] * BASE_PERIOD * 1000);
		        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time3, 0);  

			if((GLOBAL_SEQUENCE_NUMBER2 == (BUS_OUT_Q1 + BUS_OUT_Q2 + BUS_OUT_Q3)) && bflag == 1)
			{
			//flag = 1;
			//printf("\n\n&&&&&&&&&&&& FLAG SET &&&&&&&&&&&&&&&&\n\n");
			break;
			}
	}
	free(rmsg0);
	free(rmsg1);
	free(rmsg2);
	return NULL;
}
#endif
int main()
{
	int i=0,j=0;
	int k=1;
	int p=5;
	int q;
	int ret;
#if 1
	bus_in_q = (buffer *)malloc(sizeof(buffer));
	bus_out_q1 = (buffer *)malloc(sizeof(buffer));
	bus_out_q2 = (buffer *)malloc(sizeof(buffer));
	bus_out_q3 = (buffer *)malloc(sizeof(buffer));
#endif
#if 1
	bus_in_q = sq_create(bus_in_q);
	bus_out_q1 = sq_create(bus_out_q1);
	bus_out_q2 = sq_create(bus_out_q2);
	bus_out_q3 = sq_create(bus_out_q3);
#endif
//	printf("after buffer init %d %d\n",bus_in_q->start, bus_in_q->end);

	if (pthread_mutex_init(&mutex, NULL) != 0) 
	{
		printf("\n mutex init failed\n");
		//return 1;
	}

	while(i<4)
	{

		int *arg = malloc(sizeof(int));
		*arg = i;

		/* initialized with default attributes */
		ret = pthread_attr_init (&(tattr1[i]));
		/* safe to get existing scheduling param */
		ret = pthread_attr_getschedparam (&(tattr1[i]), &param);
		/* set the priority; others are unchanged */
		param.sched_priority = thread_priority[k];
	//printf("Priority for TX thread %d = %d\n", i, param.sched_priority);
		/* setting the new scheduling param */
		ret = pthread_attr_setschedparam (&(tattr1[i]), &param);

		ret = pthread_create(&(sendert[i]), &(tattr1[i]), sender_func, arg);

		if(ret!=0)
			printf("Thread creation failed %d\n", ret);
		i++;
		k++;
	}

	/* initialized with default attributes */
	ret = pthread_attr_init (&tattr2);
	/* safe to get existing scheduling param */
	ret = pthread_attr_getschedparam (&tattr2, &param);
	/* set the priority; others are unchanged */
	param.sched_priority = thread_priority[0];
	//printf("Priority for BUS thread  = %d\n", param.sched_priority);
	/* setting the new scheduling param */
	ret = pthread_attr_setschedparam (&tattr2, &param);

	ret = pthread_create(&busdaemon, &tattr2, busdaemon_func, NULL);
	if(ret!=0)
		printf("Thread creation of bus daemon failed \n");
#if 1
	while(j<3)
	{
		int *arg2 = malloc(sizeof(int));
		*arg2 = j;

		/* initialized with default attributes */
		ret = pthread_attr_init (&(tattr3[j]));
		/* safe to get existing scheduling param */
		ret = pthread_attr_getschedparam (&(tattr3[j]), &param);
		/* set the priority; others are unchanged */
		param.sched_priority = thread_priority[p];
		//printf("Priority for RX thread %d = %d\n", j, param.sched_priority);
		/* setting the new scheduling param */
		ret = pthread_attr_setschedparam (&(tattr3[j]), &param);

		ret = pthread_create(&(receivert[j]),&(tattr3[j]), receiver_func, arg2);

		if(ret!=0)
			printf(" Thread creation failedi %d\n", ret);
		j++;
		p++;
	}
#endif
	int m=0;
	while(m<4)
		pthread_join(sendert[m++],NULL);

//printf("\n\n************ BUS_IN_Q = %d,BUS_OUT_Q = %d, BUS_OUT_Q1 = %d, BUS_OUT_Q2 = %d, BUS_OUT_Q3 = %d *************\n\n", BUS_IN_Q, BUS_OUT_Q, BUS_OUT_Q1, BUS_OUT_Q2, BUS_OUT_Q3);

sflag = 1;

	pthread_join(busdaemon,NULL);
#if 1
bflag = 1;
	int n=0;
	while(n<3)
		pthread_join(receivert[n++],NULL);
#endif

	/* Calculate mean and standard deviation of message elapsed times */
	avg_qtime = (sum_qtime/(BUS_OUT_Q11+BUS_OUT_Q22+BUS_OUT_Q33));  //Average Queueing Time(Mean)
	int a,N;
	double sum, std_dev;
	N = (BUS_OUT_Q11+BUS_OUT_Q22+BUS_OUT_Q33);
	msg_received = N;
	for(a=0;a<=N;a++)
	{
		
		sum += ((qarray[a]) - avg_qtime) * ((qarray[a]) - avg_qtime);
	}
		sum = sum/N;
		std_dev = sqrt(sum);	//Standard Deviation
		
	msg_dropped = 2000 - msg_received;




	//printf("\nSUM QTIME = %lduS\n", sum_qtime);
	printf("\nMessages Sent = %d, Messages Received = %d, Messages Dropped = %d\n", msg_sent, msg_received, msg_dropped);
	printf("\nAVERAGE ELAPSED TIME = %lduS\n", (sum_qtime/(BUS_OUT_Q11+BUS_OUT_Q22+BUS_OUT_Q33)));
	printf("\nStandard Deviation = %lf \n", std_dev);
	

	pthread_mutex_destroy(&mutex);

#if 1
	free(bus_in_q);
	free(bus_out_q1);
	free(bus_out_q2);
	free(bus_out_q3);
#endif	
	return 0;
}
