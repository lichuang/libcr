#include <string.h>
#include "context.h"

#define ESP 0
#define EIP 1
#define EAX 2
#define ECX 3
// -----------
#define RSP 0
#define RIP 1
#define RBX 2
#define RDI 3
#define RSI 4

#define RBP 5
#define R12 6
#define R13 7
#define R14 8
#define R15 9
#define RDX 10
#define RCX 11
#define R8 12
#define R9 13

//----- --------
// 32 bit
// | regs[0]: ret |
// | regs[1]: ebx |
// | regs[2]: ecx |
// | regs[3]: edx |
// | regs[4]: edi |
// | regs[5]: esi |
// | regs[6]: ebp |
// | regs[7]: eax |  = esp
enum
{
	kEIP = 0,
	kESP = 7,
};

//-------------
// 64 bit
//low | regs[0]: r15 |
//    | regs[1]: r14 |
//    | regs[2]: r13 |
//    | regs[3]: r12 |
//    | regs[4]: r9  |
//    | regs[5]: r8  | 
//    | regs[6]: rbp |
//    | regs[7]: rdi |
//    | regs[8]: rsi |
//    | regs[9]: ret |  //ret func addr
//    | regs[10]: rdx |
//    | regs[11]: rcx | 
//    | regs[12]: rbx |
//hig | regs[13]: rsp |
enum
{
	kRDI = 7,
	kRSI = 8,
	kRETAddr = 9,
	kRSP = 13,
};

typedef struct param_t
{
  const void *s1;
  const void *s2;
} param_t;

//64 bit
/*
extern "C"
{
	extern void context_swap( context_t *,context_t* ) asm("context_swap");
};
*/

int context_init(context_t *context) {
  memset(context, 0, sizeof(context_t));
  return 0;
}

#if defined(__i386__)
int context_make(context_t *ctx, context_fun_t fun, const void *s, const void *s1) {
	//make room for coctx_param
	char *sp = ctx->sp + ctx->size - sizeof(param_t);
	sp = (char*)((unsigned long)sp & -16L);

	param_t* param = (param_t*)sp ;
	param->s1 = s;
	param->s2 = s1;

	memset(ctx->regs, 0, sizeof(ctx->regs));

	ctx->regs[kESP] = (char*)(sp) - sizeof(void*);
	ctx->regs[kEIP] = (char*)fun;

	return 0;
}

#elif defined(__x86_64__)
int context_make(context_t *ctx, context_fun_t fun, const void *s, const void *s1) {
	char *sp = ctx->sp + ctx->size;
	sp = (char*) ((unsigned long)sp & -16LL);

	memset(ctx->regs, 0, sizeof(ctx->regs));

	ctx->regs[kRSP] = sp - 8;

	ctx->regs[kRETAddr] = (char*)fun;

	ctx->regs[kRDI] = (char*)s;
	ctx->regs[kRSI] = (char*)s1;
	return 0;
}
#endif
