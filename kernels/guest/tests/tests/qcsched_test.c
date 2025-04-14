#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sched.h>
#include <stdbool.h>
#include <pthread.h>
#include <syscall.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include "hypercall.h"

#define gettid() syscall(SYS_gettid)

struct arg_t {
	int cpu;
	bool *go;
};

void *thr(void *_arg) {
	struct arg_t *arg = (struct arg_t *)_arg;
	int cpu = arg->cpu;
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	if (sched_setaffinity(gettid(), sizeof(set), &set))
		perror("sched_setaffinity");
	hypercall(HCALL_INSTALL_BP, 0xffffffff81f02130, 0, 0);
	while(!*arg->go);
	hypercall(HCALL_ACTIVATE_BP, 0, 0, 0);
	setxattr("./poc", "hhh", "haohaoaho", 6, 0);
	printf("11111111111111111111111111111\n");
	hypercall(HCALL_DEACTIVATE_BP, 0, 0, 0);
	hypercall(HCALL_CLEAR_BP, 0, 0, 0);
}

void *thr1(void *_arg) {
    struct arg_t *arg = (struct arg_t *)_arg;
    int cpu = arg->cpu;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(gettid(), sizeof(set), &set))
        perror("sched_setaffinity");
    hypercall(HCALL_INSTALL_BP, 0xffffffff81f02130, 1, 0);
    while(!*arg->go);
    hypercall(HCALL_ACTIVATE_BP, 0, 0, 0);
	printf("hhhhhhhhhhhhhhhhhhhhh\n");
	printf("bbbbbbbbbbbbbbbbbbbbbb\n");
	printf("cccccccccccccccccccccc\n");
    setxattr("./poc", "hhh", "haohaoaho", 6, 0);
    hypercall(HCALL_DEACTIVATE_BP, 0, 0, 0);
    hypercall(HCALL_CLEAR_BP, 0, 0, 0);
}


void test_single_thread(void) {
	bool go = true;
	struct arg_t arg = {.cpu = 0, .go = &go};
	fprintf(stderr, "%s\n", __func__);
	hypercall(HCALL_RESET,0,0,0);
	hypercall(HCALL_VMI_HINT,VMI_HOOK,0xffffffff81f02130,0);
	hypercall(HCALL_PREPARE_BP, 1, 0, 0);
	thr(&arg);
}

void test_two_threads(void) {
	bool go = false;
	pthread_t pth1, pth2;
	struct arg_t arg0 = {.cpu = 0, .go = &go};
	struct arg_t arg1 = {.cpu = 1, .go = &go};;

	fprintf(stderr, "%s\n", __func__);

	hypercall(HCALL_RESET,0,0,0);

	hypercall(HCALL_PREPARE_BP, 2, 0, 0);

	pthread_create(&pth1, NULL, thr, &arg0);
	pthread_create(&pth2, NULL, thr1, &arg1);

	printf("---------------------\n");
	sleep(2);
	go = true;

	pthread_join(pth1, NULL);
	pthread_join(pth2, NULL);
}

int main(int argc, char *argv[])
{
	test_single_thread();
	test_two_threads();
	return 0;
}
