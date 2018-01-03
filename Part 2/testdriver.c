//Safe output somewhat
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <linux/errno.h>

#define CPU_FREQ 768 		//768MHz CPU cycles per second
#define BASE_PERIOD 1000

unsigned int BUS_IN_Q = 0;
unsigned int BUS_IN_Q1 = 0;

unsigned int BUS_OUT_Q = 0;
unsigned int BUS_OUT_Q1 = 0;
unsigned int BUS_OUT_Q2 = 0;
unsigned int BUS_OUT_Q3 = 0;
unsigned int BUS_OUT_Q11 = 0;
unsigned int BUS_OUT_Q22 = 0;
unsigned int BUS_OUT_Q33 = 0;

pthread_mutex_t mutex;
pthread_t sendert[4], busdaemon, receivert[3];
pthread_attr_t tattr1[4],tattr2,tattr3[3];
struct sched_param param;
int flag = 0, sflag=0, bflag=0;
int errno;
int msg_sent, msg_received, msg_dropped;

long qarray[2000];

struct message
{
        int message_id;			//GLOBAL_SEQUENCE_NUMBER	
        int source_id;
        int destination_id;
        char var_string[80];		// variable length random string
        long acc_qtime;			// accumulated queueing time
        long ctime;			// current CPU time
};

const int period_multiplier[] = {12,22,24,26,28,15,17,19};
const int thread_priority[] = {90,94,95,96,97,91,92,93};

unsigned int GLOBAL_SEQUENCE_NUMBER = 1;
unsigned int GLOBAL_SEQUENCE_NUMBER1 = 1;
unsigned int GLOBAL_SEQUENCE_NUMBER2 = 1;

unsigned int GLOBAL_RECEIVER_NUMBER = 1;
unsigned int GLOBAL_BUSDAEMON_NUMBER = 1;
unsigned int rcount = 0;
long sum_qtime=0L, avg_qtime=0L;

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

/* Sender Thread Function */
void* s_func(void *ptr)
{
	int ret, fd;
	int id;
	char* variable_string = NULL;
	
	struct timespec time;
	struct message *msg;
	msg = (struct message *)malloc(sizeof(struct message));

	id = *((int *)ptr);

clock_gettime(CLOCK_MONOTONIC, &time);

	while(GLOBAL_SEQUENCE_NUMBER <= 2000)
	{	
		pthread_mutex_lock(&mutex);
		msg->message_id = GLOBAL_SEQUENCE_NUMBER++;
		pthread_mutex_unlock(&mutex);
		
		msg->source_id = id;
		msg->destination_id = rand() % 3;
		variable_string = (char *)malloc(sizeof(char)*80);
		gen_rand(variable_string);
		strcpy(msg->var_string, variable_string);
		msg->acc_qtime = 0;
		msg->ctime = 0;
		
		fd = open("/dev/bus_in_q", O_RDWR);
		if(fd == -1)
		{
		//	printf("Cannot OPEN Device File 'bus_in_q' \n");
			exit(-1);
		}
		
#if 1	
		ret = write(fd, (void *)msg, sizeof(struct message));
		if(ret == 0) {
			pthread_mutex_lock(&mutex);
			BUS_IN_Q++;
			BUS_IN_Q1++;
			pthread_mutex_unlock(&mutex);
			//printf("Successful Write 'bus_in_q' \n");
			//printf("Message written by SENDER to bus_in_q by Source:%d\t message:%d\t Destination:%d\t Message:%s  BUS_IN_Q = %d BUS_IN_Q1 = %d \n",msg->source_id, msg->message_id, msg->destination_id,msg->var_string, BUS_IN_Q, BUS_IN_Q1);
		}
		else if(ret == -1) {
			errno = EINVAL;
			//printf("BUS_IN_Q FULL\n");
			pthread_mutex_lock(&mutex);
			GLOBAL_SEQUENCE_NUMBER++;
			pthread_mutex_unlock(&mutex);
			time.tv_nsec = time.tv_nsec + (period_multiplier[id+1] * BASE_PERIOD * 1000);
			clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time, 0);
			}
#endif
		
		close(fd);

	//sleep
	time.tv_nsec = time.tv_nsec + (period_multiplier[id+1] * BASE_PERIOD * 1000);
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time, 0);
	}
	free(msg);
return NULL;
}

/* Bus Daemon Thread Function */
void* bus_func(void* ptr1)
{
	int fdin,fdout;
	int retr,retw;
	int select_dest;	//Select the Receiver Queue according to random Destination number
	int count;
	

	struct timespec time1;
	struct message *msg;
	msg = (struct message *)malloc(sizeof(struct message));
	
	clock_gettime(CLOCK_MONOTONIC, &time1);

	while(GLOBAL_SEQUENCE_NUMBER1 <= 2000)
	{
		fdin = open("/dev/bus_in_q", O_RDWR);
		if(fdin == -1)
		{
			//printf("Cannot OPEN Device File: 'bus_in_q' \n");
			exit(-1);
		}
		retr = read(fdin, (void *)msg, sizeof(struct message));
		if(retr == -1) {
			errno = EINVAL;
			//printf("Reading from bus_in_q device failed\n");
		time1.tv_nsec = time1.tv_nsec + (period_multiplier[0] * BASE_PERIOD * 1000);
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time1, 0);
			//exit(-1);
		}
		else if (retr == 0) {
		pthread_mutex_lock(&mutex);
		BUS_IN_Q1--;
		BUS_OUT_Q++;
		select_dest = msg->destination_id;
		pthread_mutex_unlock(&mutex);

      		//printf("Message read by BUS DAEMON Source:%d\t message:%d\t Destination:%d\t Message:%s BUS_IN_Q1=%d BUS_OUT_Q=%d\n",        	msg->source_id, msg->message_id, msg->destination_id,msg->var_string, BUS_IN_Q1, BUS_OUT_Q);
		}
		
		close(fdin);

		

		switch(select_dest)
		{
			case 0:
				fdout = open("/dev/bus_out_q1", O_RDWR);
				if(fdout == -1) {
					//printf("Cannot OPEN Device File: 'bus_out_q1'\n");
					exit(-1);
				}
				break;
			case 1: 
				fdout = open("/dev/bus_out_q2", O_RDWR);
				if(fdout == -1) {
					//printf("Cannot OPEN Device File: 'bus_out_q2'\n");
					exit(-1);
				}
				break;
			case 2:
				fdout = open("/dev/bus_out_q3", O_RDWR);
				if(fdout == -1) {
					//printf("Cannot OPEN Device File: 'bus_out_q3'\n");
					exit(-1);
				}
				break;
		}

		retw = write(fdout, (void *)msg, sizeof(struct message));
		if(retw == 0) {
			
			if(select_dest == 0){

				pthread_mutex_lock(&mutex);
				//printf("Message written by BUS DAEMON to RECEIVER %d Source:%d\t message:%d\t Destination:%d\t Message:%s\n",select_dest, msg->source_id, msg->message_id, msg->destination_id,msg->var_string);
				BUS_OUT_Q1++;
				GLOBAL_SEQUENCE_NUMBER1++;
				pthread_mutex_unlock(&mutex);}
			else if(select_dest == 1){

				pthread_mutex_lock(&mutex);
				//printf("Message written by BUS DAEMON to RECEIVER %d Source:%d\t message:%d\t Destination:%d\t Message:%s\n",select_dest, msg->source_id, msg->message_id, msg->destination_id,msg->var_string);
				BUS_OUT_Q2++;
				GLOBAL_SEQUENCE_NUMBER1++;
				pthread_mutex_unlock(&mutex);}
			else if(select_dest == 2){

				pthread_mutex_lock(&mutex);
				//printf("Message written by BUS DAEMON to RECEIVER %d Source:%d\t message:%d\t Destination:%d\t Message:%s\n",select_dest, msg->source_id, msg->message_id, msg->destination_id,msg->var_string);
				BUS_OUT_Q3++;
				GLOBAL_SEQUENCE_NUMBER1++;
				pthread_mutex_unlock(&mutex);}
			}
		else if(retw == -1) {
			errno = EINVAL;
			GLOBAL_SEQUENCE_NUMBER1++;
			time1.tv_nsec = time1.tv_nsec + (period_multiplier[0] * BASE_PERIOD * 1000);
			clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time1, 0);
			
			//printf("Could not WRITE to the device file OUT Queue\n");
			}
		close(fdout);

	//sleep
	time1.tv_nsec = time1.tv_nsec + (period_multiplier[0] * BASE_PERIOD * 1000);
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time1, 0);

// && BUS_IN_Q1 == 0
//(GLOBAL_SEQUENCE_NUMBER1 == BUS_OUT_Q)
#if 1
		if( BUS_IN_Q1 == 0 && sflag == 1)		//BUS_IN_Q is empty
			break;
		if(BUS_IN_Q1<0)
			break;
#endif
	}
	free(msg);

return NULL;
}		

/* Receiver Thread Function */
void* r_func(void *ptr2)
{
	int fd;
	int ret;
	int select_out = *((int *)ptr2);
	long qtime;			// store queueing time after receiving message
	int i=0;			//qarray counter
	
	struct timespec time2;
	struct message *msg;
	msg = (struct message *)malloc(sizeof(struct message));
	
	clock_gettime(CLOCK_MONOTONIC, &time2);
//GLOBAL_RECEIVER_NUMBER
//!(flag)
//!flag && (BUS_OUT_Q-- >= 0)
	while(GLOBAL_SEQUENCE_NUMBER2 <= 2000)
	{
		switch(select_out)
		{	
			case 0:
				fd = open("/dev/bus_out_q1", O_RDWR);
				if(fd == -1) {
					//printf("Cannot OPEN Device File: 'bus_out_q1' \n");
					exit(-1);
				}
				break;
			case 1:
				fd = open("/dev/bus_out_q2", O_RDWR);			
				if(fd == -1) {
					//printf("Cannot OPEN Device File: 'bus_out_q2' \n");
					exit(-1);
				}
				break;
			case 2:
				fd = open("/dev/bus_out_q3", O_RDWR);			
				if(fd == -1) {
					//printf("Cannot OPEN Device File: 'bus_out_q3' \n");
					exit(-1);
				}
				break;
		}
		ret = read(fd, (void *)msg, sizeof(struct message));
		if (ret == 0) {
		
		pthread_mutex_lock(&mutex);
				BUS_OUT_Q--;
				pthread_mutex_unlock(&mutex);

			if(select_out == 0) {
				pthread_mutex_lock(&mutex);
					qtime = (msg->acc_qtime);
					qarray[i++] = qtime;	
					sum_qtime = sum_qtime + qtime;
 		//printf("Message read by RECEIVER %d Source:%d\t message:%d\t Destination:%d\t Message:%s QUEUEING TIME =%lduS\n",select_out, msg->source_id, msg->message_id, msg->destination_id,msg->var_string, qtime/768);  //CPU FREQUENCY = 768MHz
					BUS_OUT_Q11++;
					GLOBAL_SEQUENCE_NUMBER2++;
					pthread_mutex_unlock(&mutex);}
			else if(select_out == 1){
				pthread_mutex_lock(&mutex);
					qtime = (msg->acc_qtime);
					qarray[i++] = qtime;
					sum_qtime = sum_qtime + qtime;
 		//printf("Message read by RECEIVER %d Source:%d\t message:%d\t Destination:%d\t Message:%s QUEUEING TIME =%lduS\n",select_out, msg->source_id, msg->message_id, msg->destination_id,msg->var_string, qtime/768);  //CPU FREQUENCY = 768MHz
					BUS_OUT_Q22++;
					GLOBAL_SEQUENCE_NUMBER2++;
					pthread_mutex_unlock(&mutex);}
			else if(select_out == 2) {
				pthread_mutex_lock(&mutex);
					qtime = (msg->acc_qtime);	
					qarray[i++] = qtime;
					sum_qtime = sum_qtime + qtime;
 		//printf("Message read by RECEIVER %d Source:%d\t message:%d\t Destination:%d\t Message:%s QUEUEING TIME =%lduS\n",select_out, msg->source_id, msg->message_id, msg->destination_id,msg->var_string, qtime/768);  //CPU FREQUENCY = 768MHz
					BUS_OUT_Q33++;
					GLOBAL_SEQUENCE_NUMBER2++;
					pthread_mutex_unlock(&mutex);}
			}
		
		else if(ret == -1) {
		//close(fd);
		errno = EINVAL;
		rcount += 1;
		GLOBAL_SEQUENCE_NUMBER2++;
		//printf("Reading from bus_out_q device failed  %d\n", rcount);
		}
	
#if 1
	close(fd);	
	//sleep
	time2.tv_nsec = time2.tv_nsec + (period_multiplier[select_out + 5] * BASE_PERIOD * 1000);
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time2, 0);
//BUS_OUT_Q == 0 && bflag == 1 
	if(GLOBAL_SEQUENCE_NUMBER2 == BUS_OUT_Q && bflag == 1)
		{
			//flag = 1;
			//printf("\n\n&&&&&&&&&&&& FLAG SET &&&&&&&&&&&&&&&&\n\n");
			break;
		}
	
#endif

	}
return NULL;
}

int main()
{
	int i=0,j=0;
	int k=1;
	int p=5;
	int m=0, n=0;
	int ret;

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

		ret = pthread_create(&(sendert[i]), &(tattr1[i]), s_func, arg);

		if(ret!=0)
			printf("Thread creation failed %d\n", ret);
		//printf(" Sender %d Thread details %lu\n",i,(unsigned long) sendert[i]);
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

	ret = pthread_create(&busdaemon, &tattr2, bus_func, NULL);
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

		ret = pthread_create(&(receivert[j]),&(tattr3[j]), r_func, arg2);

		if(ret!=0)
			printf(" Thread creation failed %d\n", ret);
		j++;
		p++;
	}
#endif

	while(m<4)
		pthread_join(sendert[m++],NULL);

sflag=1;
	pthread_join(busdaemon,NULL);
bflag=1;	

#if 1	
	while(n<3)
		pthread_join(receivert[n++],NULL);
#endif
	if(BUS_IN_Q > (BUS_OUT_Q1+BUS_OUT_Q2+BUS_OUT_Q3))
		msg_sent = BUS_IN_Q;
	else
		msg_sent = BUS_OUT_Q1+BUS_OUT_Q2+BUS_OUT_Q3;

	
	
	/* Calculate mean and standard deviation of message elapsed times */
	avg_qtime = (sum_qtime/(BUS_OUT_Q11+BUS_OUT_Q22+BUS_OUT_Q33))/768;  //Average Queueing Time(Mean)
	int a,N;
	double sum, std_dev;
	N = (BUS_OUT_Q11+BUS_OUT_Q22+BUS_OUT_Q33);
	msg_received = N;
	for(a=0;a<=N;a++)
	{
		sum += ((qarray[a]/768) - avg_qtime) * ((qarray[a]/768) - avg_qtime);
	}
		sum = sum/N;
		std_dev = sqrt(sum);	//Standard Deviation
		
	msg_dropped = 2000 - msg_received;


	pthread_mutex_destroy(&mutex);


	printf("\n\nMessages Sent = %d, Messages Received = %d, Messages Dropped = %d\n", msg_sent, msg_received, msg_dropped);
	printf("\nAVERAGE ELAPSED TIME = %lduS\n", (sum_qtime/(BUS_OUT_Q11+BUS_OUT_Q22+BUS_OUT_Q33))/768);
	printf("\nStandard Deviation of Elapsed Time = %lf \n\n", std_dev);

	
	return 0;

}
