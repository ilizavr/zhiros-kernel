extern API struct task* create_task(char * name, void* function);
extern API void yield();
extern void init_multitask();
//extern API void set_preemptive_mode(bool e);
extern API struct task* get_tasks();
