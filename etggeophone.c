#include "etggeophone.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
//#include <linux/kernel.h>

#define DRIVER_NAME "geophone"
#define DEVICE_CLASS "geophone_class"

#define GEOPHONE_FIRST_MINOR    0
#define GEOPHONE_DEVICE_TOTNUM  2

static dev_t geophone_device_num;
static struct class *geophone_class;
//static struct cdev geophone_cdev[GEOPHONE_DEVICE_TOTNUM];

typedef struct geophone {
    struct cdev cdev;
    struct mutex mutex;
    struct mutex r_mutex;
    u16 read_count;
    GEOPHONE_DATA geophone_data;
} GEOPHONE;

static GEOPHONE geophone[GEOPHONE_DEVICE_TOTNUM];

static u16 def_pin_value[GEOPHONE_DEVICE_TOTNUM] = {0, 1};

int etggeophone_open(struct inode* inode, struct file* filp) {
    GEOPHONE* geophoneptr;

    geophoneptr = container_of(inode->i_cdev, GEOPHONE, cdev);
    // geophoneptr->geophone_data.tot_count = 0;
    // geophoneptr->geophone_data.int_count = 0;
    // geophoneptr->geophone_data.tofs = 0;
    // geophoneptr->geophone_data.iofs = 0;

    filp->private_data = geophoneptr;

    return 0;
}

ssize_t etggeophone_read (struct file * filp, char __user * buf, size_t count, loff_t * offset) {
    GEOPHONE* geophoneptr;

    geophoneptr = filp->private_data;
    mutex_lock(&geophoneptr->r_mutex);
    geophoneptr->read_count++;
    if(geophoneptr->read_count == 1) {
        mutex_lock(&geophoneptr->mutex);
    }
    mutex_unlock(&geophoneptr->r_mutex);
    
    if(copy_to_user(buf, &geophoneptr->geophone_data, sizeof(GEOPHONE_DATA)) != 0) {
        return -EIO;
    }

    mutex_lock(&geophoneptr->r_mutex);
    geophoneptr->read_count--;
    if(geophoneptr->read_count == 0) {
        mutex_unlock(&geophoneptr->mutex);
    }
    mutex_unlock(&geophoneptr->r_mutex);
    
    return sizeof(GEOPHONE_DATA);
}

int etggeophone_release(struct inode* inode, struct file* filp) {
    GEOPHONE* geophoneptr;

    geophoneptr = container_of(inode->i_cdev, GEOPHONE, cdev);
    mutex_lock(&geophoneptr->mutex);

    geophoneptr->geophone_data.tot_count = 0;
    geophoneptr->geophone_data.int_count = 0;
    geophoneptr->geophone_data.tofs = 0;
    geophoneptr->geophone_data.iofs = 0;

    mutex_unlock(&geophoneptr->mutex);
    
    return 0;
}

static irqreturn_t gpio_interrupt_top_handler(int irq, void* dev_id) {
    return IRQ_WAKE_THREAD;
}

static irqreturn_t gpio_interrupt_bottom_handler(int irq, void* dev_id) {
    GEOPHONE* geophoneptr;
    u32 u32_max_value;

    u32_max_value = (u32)-1;

    geophoneptr = (GEOPHONE*)dev_id;

    mutex_lock(&geophoneptr->mutex);    

    if(geophoneptr->geophone_data.tot_count < u32_max_value){
        geophoneptr->geophone_data.tot_count++;
    } else {
        geophoneptr->geophone_data.tot_count = 0;
        geophoneptr->geophone_data.tofs++;
    }
    if(geophoneptr->geophone_data.int_count < u32_max_value){
        geophoneptr->geophone_data.int_count++;
    } else {
        geophoneptr->geophone_data.int_count = 0;
        geophoneptr->geophone_data.iofs++;
    }

    mutex_unlock(&geophoneptr->mutex);

    return IRQ_HANDLED;
}

int etggeophone_addpin_handle(GEOPHONE* geophoneptr, GEOPHONE_PARAMS* geophone_params) {
    u16 pin_number;
    int error;

    pin_number = geophone_params->pin_number;

    if(!gpio_is_valid((int)pin_number)) {
        pr_err("PIN %d Not Valid !\n", pin_number);
        return -1;
    }
    if(geophoneptr->geophone_data.used) {
        pr_err("A PIN is already present\n");
        return 1;
    }
    error = gpio_request(pin_number, "GEOPHONE");
    if(error != 0) {
        pr_err("Error Adding PIN %d, retval: %d\n", pin_number, error);
        return error;
    }
    gpio_direction_input(pin_number);

    geophoneptr->geophone_data.used = true;
    geophoneptr->geophone_data.pin_number = pin_number;
    geophoneptr->geophone_data.tofs = 0;
    geophoneptr->geophone_data.iofs = 0;
    geophoneptr->geophone_data.tot_count = 0;
    geophoneptr->geophone_data.int_count = 0;

    error = request_threaded_irq(gpio_to_irq(pin_number), gpio_interrupt_top_handler, 
                                gpio_interrupt_bottom_handler, IRQF_TRIGGER_RISING,
                                DRIVER_NAME, (void*)geophoneptr);
    if(error) {
        pr_err("Error registering irq\n");
        gpio_free(pin_number);
        geophoneptr->geophone_data.used = false;
        return -1;
    }

    return 0;
}

void etggeophone_getdata_handle(GEOPHONE_DATA* geophone_data) {
    geophone_data->int_count = 0;
    geophone_data->iofs = 0;
}

long etggeophone_removepin_handle(GEOPHONE* geophoneptr, GEOPHONE_PARAMS* params) {
    if(params == NULL) {
        if (!geophoneptr->geophone_data.used) {
            pr_err("No PIN in use\n");
            return -1;
        }   
        free_irq(gpio_to_irq(geophoneptr->geophone_data.pin_number), (void*)geophoneptr); 
        gpio_free(geophoneptr->geophone_data.pin_number);
        geophoneptr->geophone_data.used = false;
        pr_info("Removed pin: %d\n", geophoneptr->geophone_data.pin_number);
    } else {
        free_irq(gpio_to_irq(params->pin_number), (void*)geophoneptr); 
        gpio_free(params->pin_number);
        pr_info("geophone_data.pin_number: %d - params->pin_number: %d\n", geophoneptr->geophone_data.pin_number, params->pin_number);
        if(geophoneptr->geophone_data.pin_number == params->pin_number) {
            geophoneptr->geophone_data.used = false;
        }
        pr_info("Removed pin2: %d\n", params->pin_number);
    }
    
    return 0;
}

long etggeophone_ioctl(struct file* filp, unsigned int cmd, unsigned long arg) {
    GEOPHONE* geophoneptr;
    GEOPHONE_PARAMS geophone_params;
    
    switch(cmd) {
        case GPIOC_ADDPIN:
            geophoneptr = filp->private_data;
            if(copy_from_user(&geophone_params, (GEOPHONE_PARAMS*)arg, sizeof(GEOPHONE_PARAMS)) != 0){
                return -EIO;
            }
            return (long)etggeophone_addpin_handle(geophoneptr, &geophone_params);
        break;
        case GPIOC_GETDATA:
            geophoneptr = filp->private_data;
            mutex_lock(&geophoneptr->r_mutex);
            geophoneptr->read_count++;
            if(geophoneptr->read_count == 1) {
                mutex_lock(&geophoneptr->mutex);
            }
            mutex_unlock(&geophoneptr->r_mutex);
            
            if(copy_to_user((GEOPHONE_DATA*)arg, &geophoneptr->geophone_data, sizeof(GEOPHONE_DATA)) != 0) {
                return -EIO;
            }
            etggeophone_getdata_handle(&geophoneptr->geophone_data);

            mutex_lock(&geophoneptr->r_mutex);
            geophoneptr->read_count--;
            if(geophoneptr->read_count == 0) {
                mutex_unlock(&geophoneptr->mutex);
            }
            mutex_unlock(&geophoneptr->r_mutex);
        break;
        case GPIOC_REMOVEPIN:
            geophoneptr = filp->private_data;
            if(arg == NULL) {
                return (long)etggeophone_removepin_handle(geophoneptr, NULL);
            } else {
                if(copy_from_user(&geophone_params, (GEOPHONE_PARAMS*)arg, sizeof(GEOPHONE_PARAMS)) != 0){
                    return -EIO;
                }
                return (long)etggeophone_removepin_handle(geophoneptr, &geophone_params);
            }            
            
        break;
        default:
            return -ENOTTY;
    }

    return 0;
}

struct file_operations f_ops = {
    .owner          = THIS_MODULE,
    .open           = etggeophone_open,
    .read           = etggeophone_read,
    .release        = etggeophone_release,
    .unlocked_ioctl = etggeophone_ioctl
};

static int __init geophone_init(void) {
    int alloc_res;
    int i;
    dev_t curr_dev;
    
    pr_info("Geophone initialization!\n");

    alloc_res = alloc_chrdev_region(&geophone_device_num, 0, GEOPHONE_DEVICE_TOTNUM, DRIVER_NAME);
    if (alloc_res) {
        pr_err("Device registration failed\n");
        return -1;
    }
    geophone_class = class_create(THIS_MODULE, DEVICE_CLASS);

    for(i=0; i<GEOPHONE_DEVICE_TOTNUM; i++) {
        cdev_init(&geophone[i].cdev, &f_ops);
        geophone[i].cdev.owner = THIS_MODULE;
        geophone[i].geophone_data.pin_number = def_pin_value[i];
        mutex_init(&geophone[i].mutex);
        mutex_init(&geophone[i].r_mutex);
        geophone[i].read_count = 0;
        curr_dev = MKDEV(MAJOR(geophone_device_num), MINOR(geophone_device_num)+i);
        cdev_add(&geophone[i].cdev, curr_dev, 1);
        device_create(geophone_class, NULL, curr_dev, NULL, DRIVER_NAME "%d", i);
    }

    return 0;

}

static void __exit geophone_exit(void) {
    int i;
    pr_info("Geophone exit\n");

    for(i=0; i<GEOPHONE_DEVICE_TOTNUM; i++) {
        device_destroy(geophone_class, MKDEV(MAJOR(geophone_device_num), MINOR(geophone_device_num)+i));
        cdev_del(&geophone[i].cdev);
    }
    class_unregister(geophone_class);
    class_destroy(geophone_class);
    unregister_chrdev_region(0, GEOPHONE_DEVICE_TOTNUM);
}

module_init(geophone_init);
module_exit(geophone_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lorenzo Giuliani <lorenzo.giuliani@etgsrl.it>");
MODULE_DESCRIPTION("Geophone Kernel Module");