#include "coroutine.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#if __APPLE__ && __MACH__
#include <sys/ucontext.h>
#else 
#include <ucontext.h>
#endif 

#define STACK_SIZE (1024*1024)
#define DEFAULT_COROUTINE 16

struct coroutine;

/**
* Э�̵�����
*/
struct schedule {
	char stack[STACK_SIZE];	// ����ʱջ

	ucontext_t main; // ��Э�̵�������
	int nco; // ��ǰ����Э�̸���
	int cap; // Э�̹������ĵ�ǰ���������������ͬʱ֧�ֶ��ٸ�Э�̡���������ˣ����������
	int running; // �������е�Э��ID
	struct coroutine** co; // һ��һά���飬���ڴ��Э�� 
};

/*
* Э��
*/
struct coroutine {
	coroutine_func func; // Э�����õĺ���
	void* ud;  // Э�̲���
	ucontext_t ctx; // Э��������
	struct schedule* sch; // ��Э�������ĵ�����
	ptrdiff_t cap; 	 // �Ѿ�������ڴ��С
	ptrdiff_t size; // ��ǰЭ������ʱջ������������Ĵ�С
	int status;	// Э�̵�ǰ��״̬
	char* stack; // ��ǰЭ�̵ı�������������ʱջ
};

/*
* �½�һ��Э��
* ��Ҫ����Ҳ�Ƿ����ڴ漰����ֵ
*/
struct coroutine*
	_co_new(struct schedule* S, coroutine_func func, void* ud) {
	struct coroutine* co = malloc(sizeof(*co));
	co->func = func;
	co->ud = ud;
	co->sch = S;
	co->cap = 0;
	co->size = 0;
	co->status = COROUTINE_READY; // Ĭ�ϵ����״̬����COROUTINE_READY
	co->stack = NULL;
	return co;
}

/**
* ɾ��һ��Э��
*/
void
_co_delete(struct coroutine* co) {
	free(co->stack);
	free(co);
}

/**
* ����һ��Э�̵�����
*/
struct schedule*
	coroutine_open(void) {
	// ����������Ҫ���Ƿ����ڴ棬ͬʱ����ֵ
	struct schedule* S = malloc(sizeof(*S));
	S->nco = 0;
	S->cap = DEFAULT_COROUTINE;
	S->running = -1;
	S->co = malloc(sizeof(struct coroutine*) * S->cap);
	memset(S->co, 0, sizeof(struct coroutine*) * S->cap);
	return S;
}

/**
* �ر�һ��Э�̵�������ͬʱ������为������
* @param S ���˵������ر�
*/
void
coroutine_close(struct schedule* S) {
	int i;
	// �رյ�ÿһ��Э��
	for(i = 0; i < S->cap; i++) {
		struct coroutine* co = S->co[i];

		if(co) {
			_co_delete(co);
		}
	}

	// �ͷŵ�
	free(S->co);
	S->co = NULL;
	free(S);
}

/**
* ����һ��Э�̶���
* @param S ��Э�������ĵ�����
* @param func ��Э�̺���ִ����
* @param ud func�Ĳ���
* @return �½���Э�̵�ID
*/
int
coroutine_new(struct schedule* S, coroutine_func func, void* ud) {
	struct coroutine* co = _co_new(S, func, ud);
	if(S->nco >= S->cap) {
		// ���ĿǰЭ�̵������Ѿ����ڵ���������������ô��������
		int id = S->cap;	// �µ�Э�̵�idֱ��Ϊ��ǰ�����Ĵ�С
		// ���ݵķ�ʽΪ������Ϊ��ǰ������2�������ַ�ʽ��Hashmap����������
		S->co = realloc(S->co, S->cap * 2 * sizeof(struct coroutine*));
		// ��ʼ���ڴ�
		memset(S->co + S->cap, 0, sizeof(struct coroutine*) * S->cap);
		//��Э�̷��������
		S->co[S->cap] = co;
		// ����������Ϊ����
		S->cap *= 2;
		// ��δ�������е�Э�̵ĸ��� 
		++S->nco;
		return id;
	} else {
		// ���ĿǰЭ�̵�����С�ڵ���������������ȡһ��ΪNULL��λ�ã������µ�Э��
		int i;
		for(i = 0; i < S->cap; i++) {
			/*
			 * Ϊʲô�� i%S->cap,����Ҫ��nco+i��ʼ��
			 * ����ʵҲ����һ���Ż����԰ɣ���Ϊǰnco�кܴ���ʶ���NULL�ģ�ֱ������ȥ����
			*/
			int id = (i + S->nco) % S->cap;
			if(S->co[id] == NULL) {
				S->co[id] = co;
				++S->nco;
				return id;
			}
		}
	}
	assert(0);
	return -1;
}

/*
 * ͨ��low32��hi32 ƴ����struct schedule��ָ�룬����ΪʲôҪ�����ַ�ʽ��������ֱ�Ӵ�struct schedule*�أ�
 * ��Ϊmakecontext�ĺ���ָ��Ĳ�����int�ɱ��б���64λ�£�һ��intû������һ��ָ��
*/
static void
mainfunc(uint32_t low32, uint32_t hi32) {
	uintptr_t ptr = (uintptr_t) low32 | ((uintptr_t) hi32 << 32);
	struct schedule* S = (struct schedule*) ptr;

	int id = S->running;
	struct coroutine* C = S->co[id];
	C->func(S, C->ud);	// �м��п��ܻ��в��ϵ�yield
	_co_delete(C);
	S->co[id] = NULL;
	--S->nco;
	S->running = -1;
}

/**
* �л�����ӦЭ����ִ��
*
* @param S Э�̵�����
* @param id Э��ID
*/
void
coroutine_resume(struct schedule* S, int id) {
	assert(S->running == -1);
	assert(id >= 0 && id < S->cap);

	// ȡ��Э��
	struct coroutine* C = S->co[id];
	if(C == NULL)
		return;

	int status = C->status;
	switch(status) {
	case COROUTINE_READY:
		//��ʼ��ucontext_t�ṹ��,����ǰ�������ķŵ�C->ctx����
		getcontext(&C->ctx);
		// ����ǰЭ�̵�����ʱջ��ջ������ΪS->stack
		// ÿ��Э�̶���ô���ã��������ν�Ĺ���ջ����ע�⣬������ջ����
		C->ctx.uc_stack.ss_sp = S->stack;
		C->ctx.uc_stack.ss_size = STACK_SIZE;
		C->ctx.uc_link = &S->main; // ���Э��ִ���꣬���л�����Э����ִ��
		S->running = id;
		C->status = COROUTINE_RUNNING;

		// ����ִ��C->ctx����, ����S��Ϊ��������ȥ
		uintptr_t ptr = (uintptr_t) S;
		makecontext(&C->ctx, (void (*)(void)) mainfunc, 2, (uint32_t) ptr, (uint32_t) (ptr >> 32));

		// ����ǰ�������ķ���S->main�У�����C->ctx���������滻����ǰ������
		swapcontext(&S->main, &C->ctx);
		break;
	case COROUTINE_SUSPEND:
		// ��Э���������ջ�����ݣ���������ǰ����ʱջ��
		// ����C->size��yieldʱ�б���
		memcpy(S->stack + STACK_SIZE - C->size, C->stack, C->size);
		S->running = id;
		C->status = COROUTINE_RUNNING;
		swapcontext(&S->main, &C->ctx);
		break;
	default:
		assert(0);
	}
}

/*
* ����Э�̵�ջ���ݱ���������������ջ�����ݱ��浽Э��C��ջ��
* @top ջ�ף�ջ����ߵ�ַ
*
*/
static void
_save_stack(struct coroutine* C, char* top) {
	// ���dummy�ܹؼ�������ȡ����ջ�Ĺؼ�
	// ����ǳ����䣬�漰��linux���ڴ�ֲ���ջ�ǴӸߵ�ַ��͵�ַ��չ�����
	// top(S->stack + STACK_SIZE)��������ʱջ��ջ��
	// dummy����ʱ��ջ�У��϶���λ����͵�ַ��λ�ã���ջ��
	// top - &dummy ������ջ������
	char dummy = 0;
	assert(top - &dummy <= STACK_SIZE);
	if(C->cap < top - &dummy) {
		free(C->stack);
		C->cap = top - &dummy;
		C->stack = malloc(C->cap);
	}
	C->size = top - &dummy;
	memcpy(C->stack, &dummy, C->size);
}

/**
* ����ǰ�������е�Э���ó����л�����Э����
* @param S Э�̵�����
*/
void
coroutine_yield(struct schedule* S) {
	// ȡ����ǰ�������е�Э��
	int id = S->running;
	assert(id >= 0);

	struct coroutine* C = S->co[id];
	assert((char*) &C > S->stack);

	// ����ǰ���е�Э�̵�ջ���ݱ�������
	_save_stack(C, S->stack + STACK_SIZE);

	// ����ǰջ��״̬��Ϊ ����
	C->status = COROUTINE_SUSPEND;
	S->running = -1;

	// ����������Կ�����ֻ�ܴ�Э���л�����Э����
	swapcontext(&C->ctx, &S->main);
}

int
coroutine_status(struct schedule* S, int id) {
	assert(id >= 0 && id < S->cap);
	if(S->co[id] == NULL) {
		return COROUTINE_DEAD;
	}
	return S->co[id]->status;
}

/**
* ��ȡ�������е�Э�̵�ID
*
* @param S Э�̵�����
* @return Э��ID
*/
int
coroutine_running(struct schedule* S) {
	return S->running;
}