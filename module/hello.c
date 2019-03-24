#include <linux/init.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/noerr.h>

static char *author = "Ex-ac"; 
module_param(author, charp, S_IRUGO);

static int __init hello_init(void)
{
    printk(KERN_INFO "Hello world enter.\n");
    printk(KERN_INFO "Author %s\n", author);
    return 0;
}

module_init(hello_init);

static void __exit hello_exit(void)
{
    printk(KERN_INFO "Hello world exit.\n");
}

module_exit(hello_exit);


static int add_integar(int a, int b)
{
    return a + b;
}
static int sub_integar(int a, int b)
{
    return a - b;
}

EXPORT_SYMBOL_GPL(add_integar);
EXPORT_SYMBOL_GPL(sub_integar);

MODULE_AUTHOR("Ex-ac@outlook.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simpale Hello World Module");
MODULE_ALIAS("A simaple Hello World Module");



