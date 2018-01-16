#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/module.h>
#include <linux/bio.h>
#include <linux/slab.h>
#include <linux/init.h>

const int fifo_expire_number = 3;   
const int vrt_expire_number  = 3;   
const int vrt_queue_len 	 = 10;

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



static void coop_merged_requests(struct request_queue *q, struct request *rq,
		struct request *next) {
	list_del_init(&next->queuelist);
}

static int coop_dispatch(struct request_queue *q, int force)
{
	struct coop_data *cd = q->elevator->elevator_data;
	struct request *rq;

	if (list_empty(&cd->queue) && list_empty(&cd->vrt_queue)) {
		return 0;
	}

	if (list_empty(&cd->queue)) {
		goto vruntime_request;	
	}

	if (list_empty(&cd->vrt_queue)) {
		cd->fifo_expire = 0;
		cd->lowest_vrt = U64_MAX;
		goto fifo_request;

	}


	if (cd->force_vrt == 1) {
		goto force_vrt_request;	
	}

	if (cd->fifo_expire++ > fifo_expire_number) {
		goto vruntime_request;
	}
	else {
		goto fifo_request;
	}

force_vrt_request:
	if (cd->force_expire++ >= fifo_expire_number) {
		cd->force_expire = 0;
		cd->force_vrt = 0;
	}
	rq = list_first_entry_or_null(&cd->vrt_queue, struct request, queuelist);
	if (rq) {
		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq);
		cd->vrt_count -= 1;
		return 1;
	}
	return 0;

vruntime_request:
	if (cd->vrt_expire++ >= fifo_expire_number) {
		cd->fifo_expire = 0;
		cd->vrt_expire = 0;
	}

	rq = list_first_entry_or_null(&cd->vrt_queue, struct request, queuelist);
	if (rq) {
		list_del_init(&rq->queuelist);
		if (list_empty(&cd->vrt_queue))	{
			cd->lowest_vrt = U64_MAX;
		}
		elv_dispatch_sort(q, rq);
		cd->vrt_count -= 1;
		return 1;
	}
	return 0;

fifo_request:
	//printk(KERN_DEBUG "dispatch fifo");
	rq = list_first_entry_or_null(&cd->queue, struct request, queuelist);
	if (rq) {
		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq);
		return 1;
	}
	return 0;
}

/**
 * insert request to fifo_list, and vruntime rb_tree
 */
static void coop_add_request(struct request_queue *q, struct request *rq)
{
	u64 vruntime;
	struct coop_data *cd = q->elevator->elevator_data;

	vruntime = current->se.vruntime;
	if (vruntime < cd->lowest_vrt) {
		rq->fifo_time = jiffies + cd->fifo_expire;

		list_add(&rq->queuelist, &cd->vrt_queue);
		cd->vrt_count += 1;
		cd->lowest_vrt = vruntime;

		if (cd->vrt_count > vrt_queue_len) {
			cd->force_vrt = 1;
		}
		return;
	}
	else {
		rq->fifo_time = jiffies + cd->fifo_expire;
		list_add_tail(&rq->queuelist, &cd->queue);
	}
	return;
}

static struct request *
coop_former_request(struct request_queue *q, struct request *rq)
{
	struct coop_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.prev == &nd->queue)
		return NULL;
	return list_prev_entry(rq, queuelist);
}

static struct request *
coop_latter_request(struct request_queue *q, struct request *rq)
{
	struct coop_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.next == &nd->queue)
		return NULL;
	return list_next_entry(rq, queuelist);
}

static int coop_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct coop_data *nd;
	struct elevator_queue *eq;

	eq = elevator_alloc(q, e);
	if (!eq) {
		return -ENOMEM;
	}

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
	if (!nd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}

	eq->elevator_data = nd;

	nd->lowest_vrt   = U64_MAX;
	nd->fifo_expire  = 0;
	nd->vrt_expire   = 0;
	nd->vrt_count    = 0;
	nd->force_vrt    = 0;

	INIT_LIST_HEAD(&nd->queue);
	INIT_LIST_HEAD(&nd->vrt_queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
	return 0;
}


static void coop_exit_queue(struct elevator_queue *e)
{
	struct coop_data *cd = e->elevator_data;
	BUG_ON(!list_empty(&cd->queue));
	kfree(cd);
}

static struct elevator_type elevator_coop = {
	.ops.sq = {
		.elevator_merge_req_fn		= coop_merged_requests,
		.elevator_dispatch_fn		= coop_dispatch,
		.elevator_add_req_fn		= coop_add_request,
		.elevator_former_req_fn		= coop_former_request,
		.elevator_latter_req_fn		= coop_latter_request,
		.elevator_init_fn			= coop_init_queue,
		.elevator_exit_fn			= coop_exit_queue,
	},
	.elevator_name = "coop_iosched",
	.elevator_owner = THIS_MODULE,
};


static int __init coop_init(void) {
		return elv_register(&elevator_coop);
}

static void __exit coop_exit(void) {
		elv_unregister(&elevator_coop);
}

module_init(coop_init);
module_exit(coop_exit);


MODULE_AUTHOR("Dong Wang");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Co-op IO scheduler");
