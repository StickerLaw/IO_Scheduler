# IO Scheduler
## Introduction

## Running Environment
	* Linux Kernel v4.12
## Design
### Coop_data 
	struct coop_data {
		struct list_head queue; 		/* list by deadline */
		struct list_head vrt_queue;		/* list by vruntime - 10 node max */
		int    fifo_expire;				/* fifo keep running time */
		int    vrt_expire;				/* vrt keep running time */
		int    vrt_count;				/* node count in vrt queue */
		int	   force_vrt;				/* force dispatch vrt queue */
		int    force_expire;			/* force keep running time */
		u64	   lowest_vrt;				/* lowerest_vrt */
	};

*	Fifo queue: Which intends to maintain the requests in the order they arrive. 
*	Vruntime queue: If the process’s vruntime is low, it is added to this queue.
### Coop_add_request
![alt text](https://github.com/RandallDW/IO_Scheduler/blob/master/images/add_request.png "add request block diagram")
### Dispatch 
![alt text](https://github.com/RandallDW/IO_Scheduler/blob/master/images/dispatch.png "dispatch block diagram")


	
## Execute Program
	* $ make 
	* $ sudo insmod coop.ko
	* $ sudo echo coop > /sys/block/sda/queue/scheduler
### Checking current io_scheduler
	* $ cat /sys/block/sda/queue/scheduler
### Changing current io_scheduler
	* $ echo <new io_scheduler name> > /sys/block/sda/queue/scheduler
