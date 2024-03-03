/* General headers */
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/skbuff.h>
#include <linux/freezer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/highmem.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <linux/vmalloc.h>
#include <asm/pgalloc.h>

/* File IO-related headers */
#include <linux/fs.h>
#include <linux/bio.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>

/* Sleep and timer headers */
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/sched/types.h>
#include <linux/pci.h>

#include "../common.h"

/* Simple licensing stuff */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("student");
MODULE_DESCRIPTION("Project 2, CSE 330 Spring 2024");
MODULE_VERSION("0.01");

/* Calls which start and stop the ioctl teardown */
bool memalloc_ioctl_init(void);
void memalloc_ioctl_teardown(void);
static long memalloc_ioctl(struct file *f, unsigned int cmd, unsigned long arg);

/* Project 2 Solution Variable/Struct Declarations */
#define MAX_PAGES       	4096
#define MAX_ALLOCATIONS 	100

// Structures
static dev_t            	dev = 0;
static struct class*    	memalloc_class;
static struct cdev      	memalloc_cdev;

struct ioctl_data {
    int number;
};

/* Page table allocation helper functions defined in kmod_helper.c */
pud_t*  memalloc_pud_alloc(p4d_t* p4d, unsigned long vaddr);
pmd_t*  memalloc_pmd_alloc(pud_t* pud, unsigned long vaddr);
void	memalloc_pte_alloc(pmd_t* pmd, unsigned long vaddr);

#if defined(CONFIG_X86_64)
	#define PAGE_PERMS_RW   	 PAGE_SHARED
	#define PAGE_PERMS_R   	 PAGE_READONLY
#else
	#define PAGE_PERMS_RW   	 __pgprot(_PAGE_DEFAULT | PTE_USER | PTE_NG | PTE_PXN | PTE_UXN | PTE_WRITE)
	#define PAGE_PERMS_R   	 __pgprot(_PAGE_DEFAULT | PTE_USER | PTE_NG | PTE_PXN | PTE_UXN | PTE_RDONLY)
#endif

struct alloc_info       	alloc_req;
struct free_info        	free_req;
int allocCount = 0;

/* Required file ops. */
static struct file_operations fops =
{
	.owner      	= THIS_MODULE,
	.unlocked_ioctl = memalloc_ioctl,
};

/* Init and Exit functions */
static int __init memalloc_module_init(void) {
	/* Allocate a character device. */
	if (alloc_chrdev_region(&dev, 0, 1, "memalloc") < 0) {
    	printk("error: couldn't allocate chardev region.\n");
    	return -1;
	}
	printk("[*] Allocated chardev.\n");

	/* Initialize the chardev with my fops. */
	cdev_init(&memalloc_cdev, &fops);

	if (cdev_add(&memalloc_cdev, dev, 1) < 0) {
    	printk("[x] Couldn't add virtdev cdev.\n");
    	goto cdevfailed;
	}
	printk("[*] Allocated cdev.\n");

	if ((memalloc_class = class_create("memalloc_class")) == NULL) {
    	printk("[X] couldn't create class.\n");
    	goto cdevfailed;
	}
	printk("[*] Allocated class.\n");

	if ((device_create(memalloc_class, NULL, dev, NULL, "memalloc")) == NULL) {
    	printk("[X] couldn't create device.\n");
    	goto classfailed;
	}
	printk("[*] Virtual device added.\n");

	return 0;

classfailed:
	class_destroy(memalloc_class);
cdevfailed:
	unregister_chrdev_region(dev, 1);

	return -1;
	/*printk("Hello from the memalloc module!\n");
	return 0;*/
}

static void __exit memalloc_module_exit(void) {
	/* Teardown IOCTL */
	//printk("Goodbye from the memalloc module!\n");
 	if (memalloc_class) {
    	device_destroy(memalloc_class, dev);
    	class_destroy(memalloc_class);
	}
	cdev_del(&memalloc_cdev);
	unregister_chrdev_region(dev,1);
	printk("[*] Virtual device removed.\n");
}

/* IOCTL handler for vmod. */
static long memalloc_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
	switch (cmd)
	{
	case ALLOCATE:
   	 allocCount++;
   	 if (allocCount > 100) {
   		 return -3;
   	 }
   	 	if(copy_from_user((void*) &alloc_req, (void*)arg, sizeof(struct alloc_info))){
   			 printk("Error: User didn't send right message.\n");
       		 return -1;
   	 	}
   	 	if (alloc_req.num_pages > 4096) {
   			 return -2;
   	 	}
   		 /* allocate a set of pages */
   		 printk("IOCTL: alloc(%lx, %d, %d)\n", alloc_req.vaddr, alloc_req.num_pages, alloc_req.write);
        	unsigned long vaddr = alloc_req.vaddr;
        	int alloc_Requests = 0;
   	 //printk("vaddr = %lx", vaddr);
    for (int i = 0; i < alloc_req.num_pages; i++) {
    	pgd_t *pgd;
    	p4d_t *p4d;
    	pud_t *pud;
    	pmd_t *pmd;
    	pte_t *pte;

    	pgd = pgd_offset(current->mm, vaddr);
    	if (pgd_none(*pgd)) {
   	 //printk("Error: pgd should always be mapped (something is really wrong!).\n");
   	 return -1;
    	}
   	 //printk("TEST1");
    	p4d = p4d_offset(pgd, vaddr);
    	if (p4d_none(*p4d) || p4d_bad(*p4d)) {
   	 //printk("No P4D allocated; page must be unmapped.\n");
   	 //return -1;
   	 memalloc_pud_alloc(p4d, vaddr);
   	 p4d = p4d_offset(pgd, vaddr);
   	 alloc_Requests += 1;
    	}

    	//printk("TEST");
    	pud = pud_offset(p4d, vaddr);
    	if (pud_none(*pud)) {
   	 printk("No PUD allocated; page must be unmapped.\n");
   	 //return -1;
   	 memalloc_pmd_alloc(pud, vaddr);
   	 pud = pud_offset(p4d, vaddr);
   	 alloc_Requests += 1;
    	}
   	 //printk("TEST3");
    	pmd = pmd_offset(pud, vaddr);
    	if (pmd_none(*pmd)) {
   	 printk("No PMD allocated; page must be unmapped.\n");
   	 //return -1;
   	 //pmd = memalloc_pmd_alloc(pud, vaddr);
   	 memalloc_pte_alloc(pmd, vaddr);
   	 pmd = pmd_offset(pud, vaddr);
   	 alloc_Requests += 1;
    	}
   	 //printk("TEST4");
    	pte = pte_offset_kernel(pmd, vaddr);
    	if (pte_none(*pte)) {
   	 printk("No PTE allocated; page must be unmapped.\n");
   	 //memalloc_pte_alloc(pmd, vaddr);
   	 //return -1;
    	}
   	 //printk("TEST5");
    	if (pte_present(*pte)) {
   	 printk("Page is already mapped; cannot allocate new memory.\n");
   	 return -1;
    	}

    	// If the code reaches here, the memory is not already allocated, proceed with allocation
    	//printk("Allocating memory...\n");

    	gfp_t gfp = GFP_KERNEL_ACCOUNT;
    	void *virt_addr = (void*)get_zeroed_page(gfp);
    	if (!virt_addr) {
   	 printk(KERN_ERR "Failed to allocate memory using vmalloc\n");
   	 return -ENOMEM;
    	}

    	// Get the physical address of the page
    	unsigned long paddr = __pa(virt_addr);
    	//set_pte_at((current->mm, vaddr, pte, pfn_pte((paddr >> PAGE_SHIFT), PAGE_PERMS_R));
    	//set_pte_at(current->mm,vaddr,pte,pfn_pte((paddr >> PAGE_SHIFT),PAGE_PERMS_R));
    	if (alloc_req.write == 1) {
   		 set_pte_at(current->mm,vaddr,pte,pfn_pte((paddr >> PAGE_SHIFT),PAGE_PERMS_RW));
    	}
    	else {
   		 set_pte_at(current->mm,vaddr,pte,pfn_pte((paddr >> PAGE_SHIFT),PAGE_PERMS_R));
    	}
    	//set_pte_at(current->mm,vaddr,pte,pfn_pte((paddr >> PAGE_SHIFT),PAGE_PERMS_RW));
    	alloc_Requests += 1;
    	vaddr += 4096;
    	//printk("ALLOC REQUEST NUMBER: %d", alloc_Requests);
    	//printk(alloc_Requests);
    	/*if (alloc_Requests > 100) {
   		 return -3;
    	}*/
    }
    	break;
	case FREE:   	 
    	/* free allocated pages */
    	unsigned long vaddr2 = free_req.vaddr;
   	 printk("IOCTL: free(%lx)\n", free_req.vaddr);
   	 	pgd_t *pgd;
    	p4d_t *p4d;
    	pud_t *pud;
    	pmd_t *pmd;
    	pte_t *pte;
    	pgd = pgd_offset(current->mm, vaddr2);
    	p4d = p4d_offset(pgd, vaddr2);
    	pud = pud_offset(p4d, vaddr2);
    	pmd = pmd_offset(pud, vaddr2);
    	pte = pte_offset_kernel(pmd, vaddr2);
    	set_pte_at(current->mm, vaddr2, pte, __pte(0));
    	struct page* pg = virt_to_page(vaddr2);
    	put_page(pg);
   	 break;   	 
	default:
    	printk("Error: incorrect IOCTL command.\n");
    	return -1;
	}
	return 0;
}


/* Initialize the module for IOCTL commands */
bool memalloc_ioctl_init(void) {
	return false;
}

void memalloc_ioctl_teardown(void) {
	/* Destroy the classes too (IOCTL-specific). */
    	if (memalloc_class) {
    	device_destroy(memalloc_class, dev);
    	class_destroy(memalloc_class);
	}
	cdev_del(&memalloc_cdev);
	unregister_chrdev_region(dev,1);
	printk("[*] Virtual device removed.\n");
}

module_init(memalloc_module_init);
module_exit(memalloc_module_exit);
