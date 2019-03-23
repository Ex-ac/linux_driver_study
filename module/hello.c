#include <linux/init.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/err.h>

static void __init hello_init(void)
{
    printfk(KERN_INFO "Hello world enter.\n");
    return 0;
}

module_init(hello_init);

static void __exit hello_exit(void)
{
    printfk(KERN_INFO "Hello world exit.\n");
}

module_exit(hello_exit);

MODULE_AUTHOR("Ex-ac@outlook.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simpale Hello World Module");
MODULE_ALIAS("A simaple Hello World Module");



