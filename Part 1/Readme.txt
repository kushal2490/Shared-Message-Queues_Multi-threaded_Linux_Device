Files in the folder
-------------------------------------------------------------------------
p_testthread.c
qbuffer_dyn.h
-------------------------------------------------------------------------

p_testthread.c is a userspace application which is used to generate 4 sender threads, 1 bus daemon thread,
and 3 receiver threads. The 4 sender threads keep generating messages upto 2000 messages. If the sender
thread fails to write to "bus_in_q", the message is dropped.
The bus daemon reads the "bus_in_q" and writes the message to the randomly selected destination number
into the respective queue. If the destination queue is full then the bus daemon drops the message.
The receiver thread receives all the messages sent to it by the bus daemon until the receiver queue is empty.

qbuffer_dyn.h consists of the functions 
sq_create() - which is used to allocate memory dynamically to a message queue.
sq_delete() - which is used to free the allocated memory per queue.
sq_write() - used to enqueue data in a circular array of messages which are created dynamically and returns -1 
		when the queue is full.
sq_read() - used to dequeue message from the circular queue and returns -1 when the queue i empty.


===============================================================================================================
Compilation Steps
===============================================================================================================
User program Compilation
------------------------
1) Compile p_testthread.c :-
	gcc -o p_testthread p_testthread.c -lpthread -lm

2) Run the executable testdriver file
	./p_testthread
