#include<linux/module.h>
#include<linux/uaccess.h>
#include<linux/fs.h>
#include<linux/atomic.h>
#include<linux/kernel.h>
#include<linux/cdev.h>
#include<linux/init.h>
//MACROS 
#define DRIVER_NAME "LIGHT_CONTROLLER"
#define DEVICE_NAME "CONTROLLER"
#define IOCTL_MAGIC 'a'
#define IOCTL_ON  _IOW(IOCTL_MAGIC,1,int)
#define IOCTL_OFF _IOW(IOCTL_MAGIC,2,int)


static dev_t device_number;
static struct cdev cdev_ptr;
static struct class *class_ptr;

//LIGHT CONTROLLER STRUCTURE 
struct light_state{
	atomic_t brightness;
	atomic_t temperature;
	atomic_t is_on;
	atomic_t active_users;
};
static struct light_state state; 

static int f_open(struct inode *i,struct file *f_ptr)
{
       atomic_inc(&state.active_users);
       return 0;
}
static int f_release(struct inode *i,struct file *f_ptr)
{
	atomic_dec(&state.active_users);
	return 0;
}
static ssize_t f_write(struct file *f_ptr,const char __user *buff,size_t size,loff_t *ppos)
{   
        int kbuff[2];
	pr_info("value of on %d" ,atomic_read(&state.is_on));
	if(!atomic_read(&state.is_on))
	{
		return -EPERM;
	}

 	if(copy_from_user(kbuff,buff,sizeof(kbuff)))
	{return -EFAULT;}
		atomic_set(&state.brightness,kbuff[0]);
		atomic_set(&state.temperature,kbuff[1]);

	
	return sizeof(kbuff);
	
}
static ssize_t f_read(struct file *f_ptr,char __user *buff,size_t size,loff_t *ppos)
{  
    int kbuf[4];
	kbuf[0]=atomic_read(&state.brightness);
	kbuf[1]=atomic_read(&state.temperature);
	kbuf[2]=atomic_read(&state.is_on);
	kbuf[3]=atomic_read(&state.active_users);
  if(copy_to_user(buff,kbuf,sizeof(kbuf)))
  {
	  return -EFAULT;
  }
	return sizeof(kbuf);
}
static long  f_ioctl(struct file *f_ptr,unsigned int cmd,unsigned long arg)
{
	switch(cmd)
	{
		case IOCTL_ON:
			atomic_set(&state.is_on,1);
			break;
	        case IOCTL_OFF:
	               atomic_set(&state.is_on,0);
	               break;
	}
return 0;	
}
static struct file_operations fops=
{
	.owner=THIS_MODULE,
	.read=f_read,
	.write=f_write,
	.open=f_open,
	.release=f_release,
	.unlocked_ioctl=f_ioctl,
};
static int __init f_init(void )
{
	int ret;
	ret=alloc_chrdev_region(&device_number,0,1,DEVICE_NAME);
	if(ret<0)
	{	pr_err("ERROR IN ALLOCATING REGION\n");
	return -1;
	}
	cdev_init(&cdev_ptr,&fops);
	cdev_add(&cdev_ptr,device_number,1);
	class_ptr=class_create(DEVICE_NAME);
	if(!class_ptr)
	{
		cdev_del(&cdev_ptr);
		unregister_chrdev_region(device_number,1);
	    pr_err("CLASS NOT CREATED\n");
	  return -1;
	}
	if(!device_create(class_ptr,NULL,device_number,NULL,DEVICE_NAME))
	{   class_destroy(class_ptr);
		cdev_del(&cdev_ptr);
		unregister_chrdev_region(device_number,1);
		return -1;
	}
	atomic_set(&state.brightness,0);
	atomic_set(&state.temperature,0);
	atomic_set(&state.is_on,0);
	atomic_set(&state.active_users,0);
	return 0;
}
static void __exit f_exit(void)
{   device_destroy(class_ptr,device_number);
    class_destroy(class_ptr);
	cdev_del(&cdev_ptr);
	unregister_chrdev_region(device_number,1);
}
module_init(f_init);
module_exit(f_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ANJAN PRASAD");


	
