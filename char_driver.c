#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/spinlock_types.h>
#include <linux/sched.h>

#define DEVICE "/dev/mycdrv"

//data access directions
#define REGULAR 0
#define REVERSE 1

//NUM_DEVICES defaults to 3 unless specified during insmod
static int NUM_DEVICES = 3;
module_param(NUM_DEVICES, int, S_IRUGO);//set parameters

#define CDRV_IOC_MAGIC 'Z'
#define ASP_CHGACCDIR _IOW(CDRV_IOC_MAGIC, 1, int)
#define ASP_CLEAN _IO(CDRV_IOC_MAGIC,0x10)
#define ASP_GETVALUE _IOR(CDRV_IOC_MAGIC,0x11,int)
#define ASP_SETVALUE _IOW(CDRV_IOC_MAGIC,0x12,int)


struct ASP_mycdrv {
	struct cdev dev;
	struct semaphore sem;
	int length;
	int direction;
	unsigned char buffer[50];
};

static dev_t devNo;
static struct ASP_mycdrv *ASP_mycdrv;
struct class *cls;


static int cdrv_open(struct inode *inode, struct file *filp)
{
	struct ASP_mycdrv *mycdrv;
	mycdrv = container_of(inode->i_cdev,struct ASP_mycdrv,dev);
	filp->private_data = mycdrv;
	down_interruptible(&mycdrv ->sem);
	mycdrv -> direction = 1; // data access direction defaults to 1 (regular)
	up(&mycdrv -> sem);	
	return 0;
}

static int cdrv_release(struct inode *inode,struct file *filp)
{
	struct ASP_mycdrv *mycdrv = filp -> private_data;
	up(&mycdrv -> sem);
	return 0;
}


static ssize_t cdrv_read(struct file *filp, char __user *ubuf, size_t count, loff_t *ppos)
{
	int n, i, retval;
	char *buf;
	struct ASP_mycdrv *mycdrv = filp->private_data;
	down_interruptible(&mycdrv ->sem);
	if(*ppos == mycdrv->length){
		up(&mycdrv -> sem);
		return 0;
	}
	if(count > mycdrv->length - *ppos){
		n = mycdrv->length - *ppos;
	}	
	else{
		n = count;
		printk("n = %d\n",n);
	}

	buf = mycdrv->buffer + *ppos;
	for (i = 0; i < n; i++){
		retval = copy_to_user(ubuf,buf, 1);
		if(retval != 0)	return -EFAULT;
		*ppos += mycdrv -> direction;
	}

	printk("character driver read.\n");
	up(&mycdrv -> sem);
	return n;
}


static ssize_t cdrv_write(struct file *filp, const char __user *ubuf, size_t count, loff_t *ppos)
{
	int n, i, retval;
	char *buf;
	struct ASP_mycdrv *mycdrv = filp->private_data;
	down_interruptible(&mycdrv ->sem);
	if(*ppos == sizeof(mycdrv->buffer)){
		up(&mycdrv -> sem);
		return -1;
	}
	if(count > sizeof(mycdrv->buffer) - *ppos)
		n = sizeof(mycdrv->buffer) - *ppos;
	else
		n = count;

	buf = mycdrv->buffer + *ppos;
	for (i = 0; i < n; i++){
		retval = copy_from_user(buf, ubuf, 1);
		if(retval != 0)
			return -EFAULT;
		*ppos += mycdrv ->direction ;
		mycdrv->length += mycdrv ->direction ;
	}
	printk("character driver write.\n");
	up(&mycdrv -> sem);
	return n;
}

static loff_t cdrv_llseek(struct file *filp,loff_t offset,int orig)
{
	loff_t newpos = 0;
	struct ASP_mycdrv *mycdrv = filp->private_data;

	switch(orig){
		case 0: // SEEKSET 
			newpos = offset;
			break;
		case 1: // SEEKCUR
			newpos = filp ->f_pos + offset;
			break;
		case 2: // SEEKEND
			newpos = sizeof(mycdrv->buffer) -1 + offset;
			break;
		default: 
			return -EINVAL;
	}
	if ((newpos < 0)||(newpos > sizeof(mycdrv->buffer)))
		return -EINVAL;
	filp->f_pos = newpos;
	return newpos;
}

long cdrv_unlocked_ioctl(struct file *filp, unsigned int cmd,unsigned long arg)
{
	struct ASP_mycdrv *mycdrv = filp->private_data;
	int retval = 0;
	switch(cmd){
		case ASP_CLEAN:
			memset(mycdrv->buffer, 0, sizeof(mycdrv->buffer));
			break;

		case ASP_SETVALUE:
			mycdrv->length = arg;
			break;

		case ASP_GETVALUE:
			retval = put_user(mycdrv->length, (int *)arg);
			break;

		case ASP_CHGACCDIR:
			if (arg == REGULAR){
                        	mycdrv -> direction = 1;
				break;
			}
			if (arg == REVERSE){
				mycdrv -> direction = -1;
                        	break;
			}
		default:
			return -EFAULT;
	}

	return retval;
}


static const struct file_operations fifo_operations = {
	.owner = THIS_MODULE,
	.open = cdrv_open,
	.read = cdrv_read,
	.write = cdrv_write,
	.llseek = cdrv_llseek,
	.unlocked_ioctl = cdrv_unlocked_ioctl,
	.release = cdrv_release,
};


int __init cdrv_init(void)
{
	int i = 0, n = 0, retval;
	struct device *device;
	ASP_mycdrv = kzalloc(NUM_DEVICES * sizeof(struct ASP_mycdrv), GFP_KERNEL);
	if(!ASP_mycdrv)
		return -ENOMEM;

	retval = alloc_chrdev_region(&devNo,0,NUM_DEVICES,DEVICE);
	if(retval < 0){
		printk("Fail to register device!\n");
		goto err_register_chrdev_region;
	}


	cls = class_create(THIS_MODULE, DEVICE);
	if(IS_ERR(cls)){
		retval = PTR_ERR(cls);
		goto err_class_create;
	}
	printk("%d devices have been created!\n",NUM_DEVICES);

	for(n = 0;n < NUM_DEVICES;n ++){
		cdev_init(&ASP_mycdrv[n].dev, &fifo_operations);
		retval = cdev_add(&ASP_mycdrv[n].dev, devNo + n, 1);
		if (retval < 0)
			goto err_cdev_add;
		device = device_create(cls, NULL, devNo + n, NULL, "mycdrv%d",n);
		sema_init(&ASP_mycdrv[n].sem, 1000);
		if(IS_ERR(device)){
			retval = PTR_ERR(device);
			printk("Fail to device_create\n");
			goto err_device_create;
		}
	}
	printk("mycdrv has been registrated to system!\n");
	return 0;


	err_register_chrdev_region:
		return retval;

	err_class_create:
		unregister_chrdev_region(devNo, NUM_DEVICES);
	
	err_cdev_add:
		for(i = 0;i < n;i ++){
			cdev_del(&ASP_mycdrv[i].dev);
		}

	err_device_create:
		for(i = 0;i < n;i ++){
			device_destroy(cls,devNo + i);
		}
}

void __exit cdrv_exit(void)
{
	int i;
	for(i = 0;i < NUM_DEVICES;i ++){
		device_destroy(cls, devNo + i);
	}

	class_destroy(cls);

	for(i = 0;i < NUM_DEVICES;i ++){
		cdev_del(&ASP_mycdrv[i].dev);
	}

	kfree(ASP_mycdrv);
	unregister_chrdev_region(devNo, NUM_DEVICES);

	return;
}

module_init(cdrv_init);
module_exit(cdrv_exit);

MODULE_AUTHOR("Chengtao Wang");
MODULE_LICENSE("GPL v2");
