#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <time.h>

#define Q_SIZE 10

typedef struct message_info
{
	int message_id;
	int source_id;
	int destination_id;
	char var_string[80];
	time_t acc_qtime;
	time_t ctime;
}message;

typedef struct Queue
{
        int start;
        int end;
        message *array[Q_SIZE];
}buffer;

buffer* sq_create();
int is_buffer_full(buffer *b);
int is_buffer_empty(buffer *b);
int sq_write(buffer *b, message *msg);
void sq_delete(buffer *b);

buffer* sq_create(buffer *b)
{ 	
	int i;

	b = (buffer*)malloc(sizeof(buffer));
        b->end = 0;
        b->start = 0;
	for(i=0;i<Q_SIZE;i++) {
		b->array[i] = (message*)malloc(sizeof(message));
		b->array[i] = NULL;
	}

//	printf("buffer init done %d %d \n",b->start, b->end);

	return b;

}

void sq_delete(buffer *b)
{
	free(b->array[Q_SIZE]);
	free(b);
}

int is_buffer_full(buffer *b)
{
	return ((b->end + 1) % Q_SIZE == b->start); }

int is_buffer_empty(buffer *b)
{ 
	return (b->end == b->start); }

int sq_write(buffer *b, message *msg)
{
        int ret = -1;
	struct timeval tv;
	time_t t1;

        if(is_buffer_full(b))
                return -1;
	printf("ENQUEing\n");

	gettimeofday(&tv, NULL);
	msg->ctime = tv.tv_usec;	

        b->array[b->end] = (message *) malloc(sizeof(message));
	memcpy(b->array[b->end],msg,sizeof(message));
        ret = b->end;
        b->end = (b->end + 1) % Q_SIZE;

return ret;
}

int sq_read(buffer *b, message *msg)
{
        int ret = -1;
        int temp;
	struct timeval tv1;
	time_t t2;

	printf("DEQUEUing \n");
        if(is_buffer_empty(b)) 
                return -1;
	memcpy(msg,b->array[b->start],sizeof(message));

	gettimeofday(&tv1, NULL);
	msg->acc_qtime = msg->acc_qtime + (tv1.tv_usec - msg->ctime);
	msg->ctime = tv1.tv_usec;
	printf("TIME READ = %ld tv1s = %ld  %ld\n",msg->ctime, tv1.tv_sec, tv1.tv_usec );
        ret = b->start;
        free(b->array[b->start]);
	b->start = (b->start + 1) % Q_SIZE;

return ret;
}

