#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/time.h>
#include <linux/timer.h>

#include <linux/kernel.h>
#include <linux/types.h>

#include <linux/proc_fs.h>

#include <linux/spinlock.h>

#include <linux/interrupt.h>

// #include <asm-generic/hardirq.h>

int delay = HZ;
module_param(delay, int, S_IRUGO);

enum jit_types
{
    JIT_BUSY,
    JIT_SCHED,
    JIT_QUEUE,
    JIT_SCHEDTO,
};

struct jit_data
{
    unsigned long prev_jiffies;
    int loops;
    enum jit_types types; 
};

#define JIT_ASYNC_LOOPS     5


struct jit_queue
{
    struct jit_data data;
    wait_queue_head_t wait_queue;
};

struct jit_tasklet
{
    struct jit_data data;
    struct tasklet_struct tasklet;
};



static int jit_fn(struct seq_file *sfilp, void *data)
{
    unsigned long j0, j1;
    wait_queue_head_t wait_queue;

    init_waitqueue_head(&wait_queue);

    j0 = jiffies;
    j1 = j0 + delay;

    switch ((int)*data)
    {
    case JIT_BUSY:
        while (time_before(j0, j1))
        {
            cpu_relax();
        }
        break;
    
    case JIT_SCHED:
        while (time_before(j0, j1))
        {
            schedule();
        }
        break;

    case JIT_QUEUE:
        wait_event_interruptible_timeout(&wait_queue, 0, delay);
        break;

    case JIT_SCHEDTO:
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(delay);
        set_current_state(TASK_RUNNING);
        break;
    }
    j1 = jiffies;

    seq_printf(sfilp, "%9li %9li %9li\n", j0, j1, j1 - j0);

    return 0;
}



static int jit_busy_open(struct inode *inode, struct file *filp)
{
    return  single_open(filp, &jit_fn, JIT_BUSY);
}

static int jit_sched_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, &jit_fn, JIT_SCHED);
}

static int jit_queue_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, &jit_fn, JIT_QUEUE);
}

static int jit_schedto_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, &jit_fn, JIT_SCHEDTO);
}

static const struct file_operations busy_proc_fops = 
{
    .open = jit_busy_open,
    .release = single_release,
    .read = seq_read,
    .llseek = seq_lseek,
};

static const struct file_operations sched_proc_fops = 
{
    .open = jit_sched_open,
    .release = single_release,
    .read = seq_read,
    .llseek = seq_lseek,
};

static const struct file_operations queue_proc_fops = 
{
    .open = jit_queue_open,
    .release = single_release,
    .read = seq_read,
    .llseek = seq_lseek,
};

static const struct file_operations schedto_proc_fops = 
{
    .open = jit_schedto_open,
    .release = single_release,
    .read = seq_read,
    .llseek = seq_lseek,
};




static int jit_current_time(struct seq_file *sfilp, void *data)
{
    struct timeval tval;
    struct timespec tsp;

    unsigned long j0;
    uint64_t j1;

    j0 = jiffies;
    j1 = get_jiffies_64();
    do_gettimeofday(&tval);
    tsp = current_kernel_time();

    seq_printf(sfilp, "0x%08lx 0x%016Lx %10i.%06i\n"
                    "%40i.%09i\n", j0, j1, (int)(tval.tv_sec), (int)(tval.tv_usec), (int)(tsp.tv_sec), (int)(tsp.tv_nsec));
    return 0;
}

static int current_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, &jit_current_time, NULL);
}

static const struct file_operations current_proc_fops = 
{
    .open = current_open,
    .release = single_release,
    .read = seq_read,
    .llseek = seq_lseek,
};




static void jit_proc_create(void)
{
    proc_create("jit_busy", 0, NULL, &busy_proc_fops);
    proc_create("jit_sched", 0, NULL, &sched_proc_fops);
    proc_create("jit_queue", 0, NULL, &queue_proc_fops);
    proc_create("jit_schedto", 0, NULL, &schedto_proc_fops);


    proc_create("jit_current", 0, NULL, &current_proc_fops);
}

static void jit_remove_proc_entry(void)
{
    remove_proc_entry("jit_busy", NULL);
    remove_proc_entry("jit_sched", NULL);
    remove_proc_entry("jit_queue", NULL);
    remove_proc_entry("jit_schedto", NULL);

    remove_proc_entry("jit_current", NULL);
}


static int __init jit_init(void)
{
    jit_proc_create();

    return 0;
}

module_init(jit_init);


static void __exit jit_exit(void)
{
    jit_remove_proc_entry();
}

module_exit(jit_exit);

MODULE_LICENSE("GPL v2");
