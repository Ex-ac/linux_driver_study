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

#include <linux/sched.h>
#include <linux/seq_file.h>

#include <linux/slab.h>

#include <linux/sched/signal.h>


#include <linux/workqueue.h>

// #include <asm-generic/hardirq.h>

int delay = HZ;
module_param(delay, int, S_IRUGO);

enum jit_types
{
    JIT_BUSY        = 0,
    JIT_SCHED       = 1,
    JIT_QUEUE       = 2,
    JIT_SCHEDTO     = 3,
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
    char *buf;
    unsigned long prev_jiffies;
    struct tasklet_struct tasklet;
    wait_queue_head_t wait_queue;
    int loops;
    int hi;
};



static int jit_fn(struct seq_file *sfilp, void *data)
{
    unsigned long j0, j1;
    wait_queue_head_t wait_queue;
    enum jit_types jit_types = (enum jit_types)(sfilp->private);
    init_waitqueue_head(&wait_queue);

    j0 = jiffies;
    j1 = j0 + delay;

    seq_printf(sfilp, "jit_types: %d\n", jit_types);
    
    
    switch (jit_types)
    {
    case JIT_BUSY:
    while (time_before(jiffies, j1))
    {
        cpu_relax();
    }

        break;
    
    case JIT_SCHED:
        while (time_before(jiffies, j1))
        {
            schedule();
        }
        break;

    case JIT_QUEUE:
        wait_event_interruptible_timeout(wait_queue, 0, delay);

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
    .owner = THIS_MODULE,
    .open = jit_busy_open,
    .release = single_release,
    .read = seq_read,
    .llseek = seq_lseek,
};

static const struct file_operations sched_proc_fops = 
{
    .owner = THIS_MODULE,
    .open = jit_sched_open,
    .release = single_release,
    .read = seq_read,
    .llseek = seq_lseek,
};

static const struct file_operations queue_proc_fops = 
{
    .owner = THIS_MODULE,
    .open = jit_queue_open,
    .release = single_release,
    .read = seq_read,
    .llseek = seq_lseek,
};

static const struct file_operations schedto_proc_fops = 
{
    .owner = THIS_MODULE,
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
    .owner = THIS_MODULE,
    .open = current_open,
    .release = single_release,
    .read = seq_read,
    .llseek = seq_lseek,
};


static void jit_tasklet_fn(unsigned long arg)
{
    struct jit_tasklet *jit_tasklet = (struct jit_tasklet *)(arg);
    unsigned long j = jiffies;
    printk(KERN_INFO "%s loops:%d\n\t", __func__, jit_tasklet->loops);

    jit_tasklet->buf += sprintf(jit_tasklet->buf, "%9li  %3li     %i    %6i   %i   %s\n", jiffies, jit_tasklet->prev_jiffies - j, in_interrupt() ? 1 : 0, current->pid, smp_processor_id(), current->comm);
    printk(KERN_INFO "sprintf\n\t");

    if (--jit_tasklet->loops)
    {
        jit_tasklet->prev_jiffies = j;
        if (jit_tasklet->hi)
        {
            tasklet_hi_schedule(&jit_tasklet->tasklet);
            
        }
        else
        {
            tasklet_schedule(&jit_tasklet->tasklet);
        }
        printk(KERN_INFO "tasklet_schedule\n\t");
    }
    else
    {
        printk(KERN_INFO "wake_up\n\t");
        wake_up_interruptible(&jit_tasklet->wait_queue);
    }

}

static int jit_tasklet_show(struct seq_file *sfilp, void *data)
{
    struct jit_tasklet *jit_tasklet;
    char *buf;

    unsigned long j = jiffies;
    printk(KERN_INFO "%s:\n\t", __func__);
    
    jit_tasklet = kzalloc(sizeof(struct jit_tasklet), GFP_KERNEL);
    printk(KERN_INFO "kzalloc jit_tasklet\n\t");
    if (!jit_tasklet)
    {
        return -ENOMEM;
    }
    
    buf = kzalloc(4096, GFP_KERNEL);
    printk(KERN_INFO "kzalloc buf\n\t");

    if (!buf)
    {
        return -ENOMEM;
    }

    init_waitqueue_head(&jit_tasklet->wait_queue);
    printk(KERN_INFO "init_waitqueue_head\n\t");

    seq_printf(sfilp, "   time   delta  inirq    pid   cpu command\n");
    seq_printf(sfilp, "%9li  %3li     %i    %6i   %i   %s\n", j, 0L, in_interrupt() ? 1: 0, current->pid, smp_processor_id(), current->comm);

    jit_tasklet->hi = (int)(sfilp->private);
    jit_tasklet->prev_jiffies = j;
    jit_tasklet->buf = buf;
    jit_tasklet->loops = JIT_ASYNC_LOOPS;

    tasklet_init(&jit_tasklet->tasklet, jit_tasklet_fn, (unsigned long)jit_tasklet);
    printk(KERN_INFO "tasklet_init\n\t");

    if (jit_tasklet->hi == 1)
    {
        
        tasklet_hi_schedule(&jit_tasklet->tasklet);
        printk(KERN_INFO "tasklet_hi_schedule\n\t");
    }
    else
    {
        tasklet_schedule(&jit_tasklet->tasklet);
        printk(KERN_INFO "tasklet_schedule\n\t");
    }

    printk(KERN_INFO "wait_event_interrupible\n\t");
    wait_event_interruptible(jit_tasklet->wait_queue, jit_tasklet->loops == 0);
    
    if (signal_pending(current))
    {
        printk(KERN_INFO "signal_pending\n\t");
        return -ERESTARTSYS;
    }

    seq_printf(sfilp, "%s", buf);

    kfree(buf);
    kfree(jit_tasklet);

    return 0;
}


static int jit_tasklet_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, &jit_tasklet_show, 0);
}

static int jit_tasklet_hi_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, &jit_tasklet_show, 1);
}


static const struct file_operations jit_taskle_fops = 
{
    .owner = THIS_MODULE,
    .open = jit_tasklet_open,
    .release = single_release,
    .read = seq_read,
    .llseek = seq_lseek,
};

static const struct file_operations jit_taskle_hi_fops = 
{
    .owner = THIS_MODULE,
    .open = jit_tasklet_hi_open,
    .release = single_release,
    .read = seq_read,
    .llseek = seq_lseek,
};


struct jit_timer
{
    unsigned long prev_jiffies;
    wait_queue_head_t wait_queue;
    struct timer_list timer;
    int loops;
    char *buf;
};

static void jit_timer_fn(struct timer_list *timer)
{
    struct jit_timer *jit_timer = container_of(timer, struct jit_timer, timer);

    unsigned long j = jiffies;

    jit_timer->buf += sprintf(jit_timer->buf, "%9li  %3li     %i    %6i   %i   %s\n", j, j - jit_timer->prev_jiffies, in_interrupt() ? 1 : 0, current->pid, smp_processor_id(), current->comm);

    if (--jit_timer->loops)
    {
        jit_timer->timer.expires = j + delay;
        jit_timer->prev_jiffies = j;
        add_timer(&jit_timer->timer);
    }
    else
    {
        wake_up_interruptible(&jit_timer->wait_queue);
    }
}

static int jit_timer_show(struct seq_file *sfilp, void *data)
{
    struct jit_timer *jit_timer;
    char *buf;
    unsigned long j = jiffies;

    jit_timer = kzalloc(sizeof(struct jit_timer), GFP_KERNEL);
    if (!jit_timer)
    {
        return -ENOMEM;
    }

    buf = kzalloc(4096, GFP_KERNEL);
    if (!buf)
    {
        return -ENOMEM;
    }

    init_waitqueue_head(&jit_timer->wait_queue);
    
    timer_setup(&jit_timer->timer, &jit_timer_fn, TIMER_SOFTIRQ);

    jit_timer->buf = buf;

    jit_timer->prev_jiffies = j;
    jit_timer->loops = JIT_ASYNC_LOOPS;
    jit_timer->timer.expires = j + delay;
    

    seq_printf(sfilp, "   time   delta  inirq    pid   cpu command\n");
    seq_printf(sfilp, "%9li  %3li     %i    %6i   %i   %s\n", j, 0L, in_interrupt() ? 1 : 0, current->pid, smp_processor_id(), current->comm);

    add_timer(&jit_timer->timer);

    wait_event_interruptible(jit_timer->wait_queue, jit_timer->loops == 0);

    if (signal_pending(current))
    {
        return -ERESTARTSYS;
    }

    seq_printf(sfilp, "%s", buf);

    kfree(buf);
    kfree(jit_timer);

    return 0;
}

static int jit_timer_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, &jit_timer_show, NULL);
}

static const struct file_operations jit_timer_ops = 
{
    .owner = THIS_MODULE,
    .open = jit_timer_open,
    .read = seq_read,
    .release = single_release,
    .llseek = seq_lseek,
};

struct jit_work
{
    struct delayed_work work;
    char *buf;
    int loops;
    struct workqueue_struct *work_queue;
    wait_queue_head_t wait_queue;
    unsigned long prev_jiffies;
};

static void jit_workqueue_fn(struct work_struct *work)
{
    struct delayed_work *delayed_work = container_of(work, struct delayed_work, work);

    struct jit_work *jit_work = container_of(delayed_work, struct jit_work, work);
    unsigned long j = jiffies;

    printk(KERN_ALERT "into %s %d\n", __func__, jit_work->loops);

    jit_work->buf += sprintf(jit_work->buf, "%9li  %3li     %i    %6i   %i   %s\n", j, j - jit_work->prev_jiffies, in_interrupt() ? 1 : 0, current->pid, smp_processor_id(), current->comm);

    jit_work->prev_jiffies = j;
    if (--jit_work->loops)
    {   
        printk(KERN_ALERT "queue_delayed_work\n");
        queue_delayed_work(jit_work->work_queue, &jit_work->work, delay);
    }
    else
    {
        wake_up_interruptible(&jit_work->wait_queue);
    }
    
}

static int jit_workqueue_show(struct seq_file *sfilp, void *data)
{
    struct jit_work *jit_work;
    unsigned long j = jiffies;
    char *buf;
    //DEFINE_WAIT(wait);



    DECLARE_WAITQUEUE(wait, current);

    printk(KERN_ALERT "into %s\n", __func__);
    
    jit_work = kzalloc(sizeof(struct jit_work), GFP_KERNEL);
    if (!jit_work)
    {
        return -ENOMEM;
    }

    buf = kzalloc(4096, GFP_KERNEL);
    if (!buf)
    {
        return -ENOMEM;
    }

    printk(KERN_ALERT "init data\n");

    init_waitqueue_head(&jit_work->wait_queue);
    jit_work->work_queue = create_workqueue("jit_work");
    // INIT_WORK(&jit_work->work, &jit_workqueue_fn);
    INIT_DELAYED_WORK(&jit_work->work, jit_workqueue_fn);
    // jit_work->work.data = jit_work;

    jit_work->buf = buf;
    jit_work->prev_jiffies = j;
    jit_work->loops = JIT_ASYNC_LOOPS;

    printk(KERN_ALERT "seq_printf");

    seq_printf(sfilp, "   time   delta  inirq    pid   cpu command\n");
    seq_printf(sfilp, "%9li  %3li     %i    %6i   %i   %s\n", j, 0L, in_interrupt() ? 1 : 0, current->pid, smp_processor_id(), current->comm);

    printk(KERN_ALERT "init work\n");
    queue_delayed_work(jit_work->work_queue, &jit_work->work, delay);
    
    printk(KERN_ALERT "perpare_to_wait\n");
    prepare_to_wait(&jit_work->wait_queue, &wait, TASK_INTERRUPTIBLE);
    
    printk(KERN_ALERT "schedule\n");

    schedule();

    printk(KERN_ALERT "finish_wait\n");
    finish_wait(&jit_work->wait_queue, &wait);

    seq_printf(sfilp, "%s", buf);

    if (jit_work->loops != 0)
    {
        printk(KERN_ALERT "work quene fun can't work\n");
        cancel_delayed_work_sync(&jit_work->work);
    }

    printk(KERN_ALERT "destroy_workqueue\n");
     
    destroy_workqueue(jit_work->work_queue);

    kfree(buf);
    kfree(jit_work);

    return 0;
}

static int jit_work_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, &jit_workqueue_show, NULL);
}

static const struct file_operations jit_work_fops =
{
    .owner = THIS_MODULE,
    .open = jit_work_open,
    .read = seq_read,
    .release = seq_release,
};

static void jit_proc_create(void)
{
    proc_create("jit_busy", 0, NULL, &busy_proc_fops);
    proc_create("jit_sched", 0, NULL, &sched_proc_fops);
    proc_create("jit_queue", 0, NULL, &queue_proc_fops);
    proc_create("jit_schedto", 0, NULL, &schedto_proc_fops);


    proc_create("jit_current", 0, NULL, &current_proc_fops);

    proc_create("jit_tasklet", 0, NULL, &jit_taskle_fops);
    proc_create("jit_tasklet_hi", 0, NULL, &jit_taskle_hi_fops);
    proc_create("jit_timer", 0, NULL, &jit_timer_ops);

    proc_create("jit_work", 0, NULL, &jit_work_fops);

    printk(KERN_INFO "jit: create proc\n");
}

static void jit_remove_proc_entry(void)
{
    remove_proc_entry("jit_busy", NULL);
    remove_proc_entry("jit_sched", NULL);
    remove_proc_entry("jit_queue", NULL);
    remove_proc_entry("jit_schedto", NULL);

    remove_proc_entry("jit_current", NULL);

    remove_proc_entry("jit_tasklet", NULL);
    remove_proc_entry("jit_tasklet_hi", NULL);
    remove_proc_entry("jit_timer", NULL);

    remove_proc_entry("jit_work", NULL);
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
