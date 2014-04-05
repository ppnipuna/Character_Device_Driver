/* Minimal Character Device Driver.*/

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/proc_fs.h>	/* Necessary because we use the proc fs */
#include <asm/uaccess.h> /* Required for copy-to-user and copy-from-user functions */
#include <linux/semaphore.h> /* Required for using semaphore functions */
#include <linux/gpio.h> /*for communicating with the BeagleBoard*/
#include <linux/interrupt.h> /* provides data structures to emulate low level hardware */
#include <linux/platform_device.h>


/* Following parameters are required for interrupt handling */
#define INTERRUPT_DEVNAME "hello1"
#define USER_BUTTON_IRQ_NUM 165
#define A3B_DEV_MAJOR 234
#define A3B_DEV_FILE "hello1"
#define procfs_name "hello1"

struct proc_dir_entry *my_proc;

static int debug_enable = 0;
module_param(debug_enable, int, 0);
MODULE_PARM_DESC(debug_enable, "Enable module debug mode.");
struct file_operations hello_fops;

/* used for allocating space for linked list nodes */
static struct kmem_cache *cache=NULL;
/* linked list node */
typedef struct List_Node
{
	char ch;
	struct List_Node* next;

}List_Node;
/* head and tail nodes of the linked list */
List_Node *head=NULL, *tail=NULL;

static struct semaphore sem; /* we will use this semaphore to synchronize */
static int count=0;
static int sem_count; /* maintain the semaphore count */
static struct platform_driver pdev; /* part of the driver model interface to the platform bus */
/* a character array to store the characters read*/
static char read_array[15]; 
/* an integer to maintain the list size */
static int list_size;

/* Procfile read function : displays the count of available characters for reading */
int procfile_read(char *buf, char **start, off_t offset,
                   int count, int *eof, void *ch)
{
	int avail_cnt=0;
	List_Node *temp=NULL;

	printk(KERN_INFO "\nprocfile_read (/proc/%s) called..\n", procfs_name);
	
	temp = head;
	/* traverse through the list to find available characters */
	while(temp != NULL)
	{
		temp = temp->next;
		avail_cnt++;	
	}
	printk(KERN_INFO "\nTotal characters currently available for reading : %d \n",avail_cnt);	
    return 0;	
}

static int hello_open(struct inode *inode, struct file *file) 
{
printk("\nhello_open: successful\n");
return 0;
}

static int hello_release(struct inode *inode, struct file *file)
{
printk("\nhello_release: successful\n");
return 0;
}

static ssize_t hello_read(struct file *file, char *buf, size_t count,
loff_t *ptr)
{
	static int rd_cnt = 0;
	down(&sem); // block the semaphore
	if(head!=NULL)
	{
		List_Node *temp = head;
		printk(KERN_INFO "Inside hello_read(): Reading character: [%c]\n",head->ch);		
		read_array[rd_cnt] = head->ch;
		//printk(KERN_INFO "Inside hello_read(): Character put inside buffer: [%c]\n",read_array[rd_cnt]);
		rd_cnt++;
		temp = head;
		head=head->next;
		kmem_cache_free(cache,temp);
	}
	else
	{
		/* means list is empty, in this case glow the user LED0 */
		/* first requirement of the project handled */
		printk(KERN_INFO "Inside hello_read(): List is empty. Glowing user LED0 !!\n");		
		gpio_set_value(149,1);	
	}
	up(&sem); // release the semaphore
	return 0;
}

static ssize_t hello_write(struct file *file, const char *buf,
size_t count, loff_t * ppos)
{
	down(&sem); /* block the semaphore */
	while(*buf != '\0')
	{
		/* check if cache chain is created or no */
		if(cache == NULL)
		{
			cache= kmem_cache_create("my_cache",sizeof(List_Node), 0,0,NULL);
		}
		/* check if linked list has any data or is empty */
		if(head == NULL)
		{
			/* create the head node first */
			printk("Inside hello_write(): Creating the head(first) node..\n");
			head = (List_Node *) kmem_cache_alloc(cache,GFP_KERNEL);
			if(head == NULL)
			{
				printk("\n**** Error **** Memory Allocation Failed !!\n");
				up(&sem);
				return -1;
			}	
			head->next=NULL;
			tail=head;

			head->ch = *buf;
			printk("Inside hello_write(): Character added to the linked list: %c\n",*buf);
			
		}
		else
		{
			/* add nodes to already existing linked list */

			tail->next= (List_Node *) kmem_cache_alloc(cache,GFP_KERNEL);
			if(tail->next == NULL)
			{
				printk("\n**** Error **** Memory Allocation Failed !!\n");
				up(&sem);
				return -1;
			}
			tail->next->ch = *buf;
			printk("Inside hello_write(): Character added to the linked list: %c\n",*buf);
			tail->next->next = NULL;
			tail = tail->next;
		}
	buf++;
	}
	up(&sem);
	return 0;
}

/* Now we will write a interrupt handler to service the user button pressed interrupt */
/* this is the second requirement of the project */
void usr_button_irq_handler(int irq, void *device_id, struct pt_regs *regs){
	printk(KERN_INFO "User button pressed!!\nGlowing LED now...\n");
	gpio_set_value(149,1);
}

static int hello_ioctl(struct inode *inode, struct file *file,
unsigned int cmd, unsigned long arg)
{
printk("hello_ioctl: cmd=%u, arg=%lu\n", cmd, arg);
return 0;
}

static int __init hello_init(void)
{
int ret, ret1;
sema_init(&sem,1);
printk("Inside hello_init(): Semaphore initialized !\n");
sem_count = 1;

/* required to turn off the userLED0 of the beagleboard*/
gpio_set_value(149,0);

printk("Hello Example Init - debug mode is %s\n",
debug_enable ? "enabled" : "disabled");

ret = register_chrdev(A3B_DEV_MAJOR, "hello1", &hello_fops);
	if (ret < 0) {
	printk("Error registering hello device\n");
	goto hello_fail1;
	}
printk("Hello: registered module successfully!\n");

/* Init processing here... */

	my_proc  = create_proc_entry(procfs_name, 0644, NULL);
	
	if (my_proc == NULL) {
		remove_proc_entry(procfs_name, NULL);
		printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
		       procfs_name);
		return -ENOMEM;
	}

	/* Now we will request an interrupt line/IRQ channel for the user button interrupt */
	/* the handler for this is already written which will glow userLED0 of beagleboard */
	ret1 = request_irq(USER_BUTTON_IRQ_NUM,usr_button_irq_handler, IRQF_SHARED | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                     INTERRUPT_DEVNAME, &pdev);
	if(ret1 != 0)
	{
		/* interrupt line could not be allocated */
		printk("Inside hello_init(): ERROR : Cannot allocate IRQ channel %d, error return code: %d\n", USER_BUTTON_IRQ_NUM, ret1);
        printk("Error codes EBUSY: %d, EINVAL: %d \n", EBUSY, EINVAL);    
        remove_proc_entry(A3B_DEV_FILE, NULL);    
        unregister_chrdev(A3B_DEV_MAJOR, A3B_DEV_FILE);    
        return -EIO;
	}
	printk("Inside hello_init(): IRQ channel %d allocated successfully for user interrupt !\n",USER_BUTTON_IRQ_NUM);
	
	my_proc->read_proc = procfile_read;
	//my_proc->owner 	 = THIS_MODULE;
	my_proc->mode 	 = S_IFREG | S_IRUGO;
	my_proc->uid 	 = 0;
	my_proc->gid 	 = 0;
	my_proc->size 	 = 37;

	printk(KERN_INFO "/proc/%s created\n", procfs_name);	
        
	return 0;	/* everything is ok */

hello_fail1:
return ret;
}
/* function called on call to rmmod<module_name>*/
static void __exit hello_exit(void)
{
	List_Node* temp  = head;
	
	printk(KERN_INFO "\nInside hello_exit()\n");
	/* free the memory of all remaining nodes inside the linked list*/
	while(temp != NULL)	
	{
		printk(KERN_INFO "Inside hello_exit(): Character [%c]'s memory is deallocated\n", temp->ch);		
		kmem_cache_free(cache,temp);
		temp = temp->next;
	}
	/* destroy the cache chain created earlier */
	kmem_cache_destroy(cache);

	unregister_chrdev(A3B_DEV_MAJOR, "hello1");
	remove_proc_entry(procfs_name, NULL);
	printk(KERN_INFO "/proc/%s removed\n", procfs_name);
}

struct file_operations hello_fops = {
owner: THIS_MODULE,
read: hello_read,
write: hello_write,
unlocked_ioctl: hello_ioctl,
open: hello_open,
release: hello_release,
};

module_init(hello_init);
module_exit(hello_exit);
MODULE_AUTHOR("Chris Hallinan");
MODULE_DESCRIPTION("Hello World Example");
MODULE_LICENSE("GPL");

