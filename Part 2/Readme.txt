Files in the folder
-------------------------------------------------------------------------
Squeue.c
testdriver.c
-------------------------------------------------------------------------

Squeue.c implements four shared queues namely "bus_in_q", "bus_out_q1", "bus_out_q2",
"bus_out_q3" which are created as kernel devices.
Each Queue is thread safe with semaphore locking.
Each queue device consists of ring of pointers (circular buffer) which point to a dynamically allocated 
message structure.
Each message consists of message id, sender id, destination id, accumulated queueing time & current time
and variable length character string.

testdriver.c is a userspace application which is used to generate 4 sender threads, 1 bus daemon thread,
and 3 receiver threads. The 4 sender threads keep generating messages upto 2000 messages. If the sender
thread fails to write to "bus_in_q", the message is dropped.
The bus daemon reads the "bus_in_q" and writes the message to the randomly selected destination number
into the respective queue. If the destination queue is full then the bus daemon drops the message.
The receiver thread receives all the messages sent to it by the bus daemon until the receiver queue is empty.

The accumulated queueing time is calculated using the high precision time stamp counter in the kernel.

All the thread functions run for total of 2000 counts and the loop terminates when either of the "bus_in_q" or
"bus_out_q" is empty for sender and receiver threads respectively.
Various Counters are used to achieve this and keep track of that message present in each queue and the outgoing
message count of each queue.

===============================================================================================================
Compilation Steps
===============================================================================================================
User program Compilation
------------------------
1) Compile testdriver.c :-
	gcc -o testdriver testdriver.c -lpthread -lm
2) Run the executable testdriver file
	./testdriver

Driver Compilation
------------------
Run the following commands after switching to su mode:-
	:$ sudo su
	:$ <enter password>

1) Create kernel object of Squeue.c
	:$ make 

2) Insert the kernel module
	:$ insmod Squeue.ko

3) Run the testdriver executable
	:$ ./testdriver

4) To retest it, first remove the modules
	:$ rmmod Squeue

5) Repeat step (2) to (4) to retest

6) Clean the object files
	:$ make clean
========================================================================================================================
Inorder to compile for Galileo Board include the following to Makefile to compile the driver
----------------------------------------------------------------------------------------------
IOT_HOME = /opt/iot-devkit/1.7.3/sysroots
#PWD:= $(shell pwd)

KDIR:=$(IOT_HOME)/i586-poky-linux/usr/src/kernel
PATH := $(PATH):$(IOT_HOME)/x86_64-pokysdk-linux/usr/bin/i586-poky-linux

CC = i586-poky-linux-gcc
ARCH = x86
CROSS_COMPILE = i586-poky-linux-
SROOT=$(IOT_HOME)/i586-poky-linux/

APP = gmem_tester

obj-m:= Squeue.o

all:
        make ARCH=x86 CROSS_COMPILE=i586-poky-linux- -C $(KDIR) M=$(PWD) modules
        i586-poky-linux-gcc -o $(APP) main.c --sysroot=$(SROOT)

clean:
        rm -f *.ko
        rm -f *.o
        rm -f Module.symvers
        rm -f modules.order
        rm -f *.mod.c
        rm -rf .tmp_versions
        rm -f *.mod.c
        rm -f *.mod.o
        rm -f \.*.cmd
        rm -f Module.markers
        rm -f $(APP) 
========================================================================================================



