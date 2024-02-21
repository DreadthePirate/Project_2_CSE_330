struct alloc_info{
	unsigned long vaddr;
	int 		  num_pages;
	bool		  write;		// permission-related
};

struct free_info{
	unsigned long vaddr;
};

#define ALLOCATE		_IOW('a', 'a', struct alloc_info *)
#define FREE			_IOW('a', 'b', struct free_info *)