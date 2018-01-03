#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/pci.h>
#include <linux/param.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>

//#include "qbuffer.h"

#define DEVICE_NAME			"Squeue"

#define DEVICES				4

#define Q_SIZE 11

struct message
{
        int message_id;
        int source_id;
        int destination_id;
        char var_string[80];
        long acc_qtime;			//accumulated Queueing time
        long ctime;			//Current cpu time	
};	

/* per device structure */
struct queue_dev {
	struct cdev cdev;		/* per device cdev structure */
	char name[20];			/* Name of Device */
	struct message *array[Q_SIZE];	/* Ring of Message pointers */
	int start;
	int end;
	struct semaphore lock;		/* per device semaphore */
}*queue_devp[DEVICES];

static dev_t queue_dev_number;		/* Allotted device number */
struct class *queue_dev_class;		/* Tie with the device model */

/* Time Stamp Counter */
uint64_t tsc(void)                     
{
     uint32_t lo, hi;
     asm volatile("rdtsc" : "=a" (lo), "=d" (hi));

	return (( (uint64_t)lo)|( (uint64_t)hi)<<32 );
}

/* Open Squeue_driver */
int queue_driver_open(struct inode *inode, struct file *file)
{
	struct queue_dev *queue_devp;
	queue_devp = container_of(inode->i_cdev, struct queue_dev, cdev);
	file->private_data = queue_devp;
	printk("Opened Device %s\n", queue_devp->name);	
	return 0;
}

/* Release Squeue_driver */
int queue_driver_release(struct inode *inode, struct file *file )
{
	struct queue_dev* queue_devp = file->private_data;
	printk("\nRelease %s -> closing %s\n", DEVICE_NAME, queue_devp->name);
	return 0;
}

/* Read from Squeue device */
ssize_t queue_driver_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	struct message *msg_k;
	struct queue_dev *queue_devp = file->private_data;
	uint64_t tsc2;

	msg_k = (struct message *)kmalloc(sizeof(struct message), GFP_KERNEL);

	if(queue_devp->start == queue_devp->end)
	{
		printk("QUEUE IS EMPTY \n");
		return -1;
	}
	
	down(&(queue_devp->lock));

	msg_k = queue_devp->array[queue_devp->start];
	tsc2 = tsc();
	msg_k->acc_qtime = (msg_k->acc_qtime) + (tsc2 - msg_k->ctime);
	msg_k->ctime = tsc2;
	ret = copy_to_user(buf, msg_k, sizeof(struct message));
	queue_devp->start = (queue_devp->start + 1) % Q_SIZE;
	printk(KERN_INFO "Pointer start: %d\n", queue_devp->start);
	printk("\n\n\n DEQUEUED Message ID is \t%d Source ID is \t%d Destination ID is \t%d string element is \t%s\n\n\n",msg_k->message_id,
	msg_k->source_id, msg_k->destination_id, msg_k->var_string);

	kfree(msg_k);

	up(&queue_devp->lock);
	
	return ret;
}

/* Write to Squeue device */
ssize_t queue_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int ret;
	struct queue_dev *queue_devp = file->private_data;
	struct message *msg_u;
	uint64_t tsc1;
	
	msg_u = (struct message *)kmalloc(sizeof(struct message), GFP_KERNEL);

	if((queue_devp->end + 1) % Q_SIZE == queue_devp->start)
	{
		printk("QUEUE IS FULL \n");
		return -1;
	}
	
	down(&(queue_devp->lock));
	tsc1 = tsc();
	ret = copy_from_user(msg_u, buf, sizeof(struct message));
/*	if(ret1)
	{	
		up(&(queue_devp->lock));
		return -EFAULT;
	}*/
//	ret = enqueue(&(queue_devp->b), &msg_u);
	msg_u->ctime = tsc1;
	queue_devp->array[queue_devp->end] = msg_u;

	printk(KERN_INFO "Pointer end: %d\n", queue_devp->end);
	printk("Enqueued Message IS:: MID:\t%d\tSID:%d\tRID:%d\t %s\n",
      queue_devp->array[queue_devp->end]->message_id, queue_devp->array[queue_devp->end]->source_id,
      queue_devp->array[queue_devp->end]->destination_id,
      queue_devp->array[queue_devp->end]->var_string);

	queue_devp->end = (queue_devp->end + 1) % Q_SIZE;
	up(&(queue_devp->lock));

	return ret;	
}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations queue_fops = {
	.owner	= THIS_MODULE,		/* Owner */
	.open	= queue_driver_open,	/* Open Method */
	.release= queue_driver_release,	/* Release Method */
	.write	= queue_driver_write,	/* Write Method */
	.read	= queue_driver_read,	/* Read Method */
};

int __init queue_driver_init(void)
{
	int ret;
	int i,k;

	/* Request dynamic allocation of a device major number */
	if(alloc_chrdev_region(&queue_dev_number, 0, 4, DEVICE_NAME) < 0) {
		printk(KERN_DEBUG "Cannot Register Device\n");
		return -1;
	}

	/* Populate sysfs entries */
	queue_dev_class = class_create(THIS_MODULE, DEVICE_NAME);
	
	for(i = 0; i < DEVICES; i++) 
	{
		/* Allocate memory for the per-device structure bus_in_q */
		queue_devp[i] = kmalloc(sizeof(struct queue_dev), GFP_KERNEL);
	
		if(!queue_devp[i]) {
			printk("Bad kmalloc for %d\n",i); return -ENOMEM;
		}

		/* Request I/O region */
		if(i==0)
		strcpy(queue_devp[i]->name, "bus_in_q");
		else if(i==1)
		strcpy(queue_devp[i]->name, "bus_out_q1");
		else if(i==2)
		strcpy(queue_devp[i]->name, "bus_out_q2");
		else if(i==3)
		strcpy(queue_devp[i]->name, "bus_out_q3");

		/* Connect the file operations with the cdev */
		cdev_init(&queue_devp[i]->cdev, &queue_fops);
		queue_devp[i]->cdev.owner = THIS_MODULE;
	
		/* Connect the major/minor number to the cdev */
		ret = cdev_add(&queue_devp[i]->cdev, MKDEV(MAJOR(queue_dev_number), MINOR(queue_dev_number)+i), DEVICES);
		if(ret) {
			printk("Bad cdev for device %d\n",i);
			return ret;
		}
	
		/* Send uevents to udev, so it'll create /dev nodes */
		if(i==0)
		device_create(queue_dev_class, NULL, MKDEV(MAJOR(queue_dev_number), MINOR(queue_dev_number)+i), NULL, "%s","bus_in_q");	
		else if(i==1)		
		device_create(queue_dev_class, NULL, MKDEV(MAJOR(queue_dev_number), MINOR(queue_dev_number)+i), NULL, "%s", "bus_out_q1");
		else if(i==2)
		device_create(queue_dev_class, NULL, MKDEV(MAJOR(queue_dev_number), MINOR(queue_dev_number)+i), NULL, "%s", "bus_out_q2");
		else if(i==3)
		device_create(queue_dev_class, NULL, MKDEV(MAJOR(queue_dev_number), MINOR(queue_dev_number)+i), NULL, "%s", "bus_out_q3");
	
		/* Initialise Buffer */
	#if 1
		for(k=0;k<Q_SIZE;k++)
			queue_devp[i]->array[k] = NULL;
		queue_devp[i]->start = 0;
		queue_devp[i]->end = 0;
	#endif		
		printk("Buffer Initialized\n");

		/* Initialise Semaphore */
		sema_init(&(queue_devp[i]->lock),1);
	
		printk("Sempahore lock Initialized\n");
	}
	
	printk("Shared Message Queue Driver %s Initialized.\n", DEVICE_NAME);
	
	return 0;
}

void __exit queue_driver_exit(void)
{
	int i;

	/* Release the major number */
	unregister_chrdev_region((queue_dev_number), DEVICES);
	
	for( i = 0; i < DEVICES; i++)
	{
		/* Destroy Devices */
		device_destroy(queue_dev_class, MKDEV(MAJOR(queue_dev_number), i));
		cdev_del(&queue_devp[i]->cdev);
		kfree(queue_devp[i]);
	}
	
	class_destroy(queue_dev_class);

	printk("Device Driver %s removed\n", DEVICE_NAME);
}

module_init(queue_driver_init);
module_exit(queue_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ESP_Assignment_1 Fall_2016");
