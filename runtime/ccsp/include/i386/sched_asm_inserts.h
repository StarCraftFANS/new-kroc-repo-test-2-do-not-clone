/*
 *	Pentium/386 and GCC specific assembler inserts
 *	Copyright (C) 1998 Jim Moores
 *	Modification copyright (C) 1999-2002 Fred Barnes  <frmb2@ukc.ac.uk>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * This file attempts to isolate as many as possible of the platform 
 * specific features of this package as possible.  The macros have been
 * designed so that it should be easy to port this kernel to a more
 * register based machine fairly easily (eg Alpha, ARM etc).  Note that
 * there are numerous functions in this file labelled such that are no
 * longer used.
 */

#ifndef SCHED_ASM_INSERTS_H
#define SCHED_ASM_INSERTS_H

/*{{{  architecture dependent kernel call declarations */
#define K_CALL_DEFINE(X) \
	void __attribute__ ((regparm(3))) kernel_##X (word param0, sched_t *sched, word *Wptr)
#define K_CALL_PTR(X) \
	((void *) (kernel_##X))

#define K_CALL_HEADER \
	__attribute__ ((unused)) \
	unsigned int return_address = (unsigned int) __builtin_return_address (0);
#define K_CALL_PARAM(N) \
	((N) == 0 ? param0 : sched->cparam[(N) - 1])
/*}}}*/

/* these for debugging mostly */
#define LOAD_ESP(X) \
	__asm__ __volatile__ ("\n\tmovl %%esp, %0\n\t" : "=r" (X))

#ifdef CHECKING_MODE
#define SAVE_EIP(X) \
	__asm__ __volatile__ ("		\n"		\
		"	movl $0f, %0	\n"		\
		"0:			\n"		\
		: "=m" ((X))				\
	)
#define TRACE_RETURN(addr)	\
	do { 						\
		sched->mdparam[8] = (word) Wptr;	\
		sched->mdparam[9] = (word) (addr);	\
		SAVE_EIP (sched->mdparam[11]);		\
	} while (0)
#else
#define	TRACE_RETURN(addr)	do { } while (0)
#endif /* CHECKING_MODE */

/*{{{  _K_SETGLABEL - internal global label define for inside asm blocks */
#define LABEL_ALIGN ".p2align 4	\n"
#ifndef NO_ASM_TYPE_DIRECTIVE
#define LABEL_TYPE(P,X) ".type "#P""#X", @function \n"
#else
#define LABEL_TYPE(P,X) 
#endif

#define _K_SETGLABEL(X) \
	LABEL_ALIGN \
	LABEL_TYPE(_,X) \
	".globl _"#X"	\n" \
	"	_"#X":	\n" \
	"	"#X":	\n"

#define _K_SETGGLABEL(X) \
	LABEL_ALIGN \
	LABEL_TYPE(_,X) \
	".globl _"#X"	\n" \
	"	_"#X":	\n" \
	LABEL_TYPE( ,X) \
	".globl "#X"	\n" \
	"	"#X":	\n"
/*}}}*/

/*{{{  outgoing entry-point macros*/
#define K_ZERO_OUT() \
	TRACE_RETURN (return_address); \
	__asm__ __volatile__ ("\n" \
		"	movl	%0, %%ebp		\n" \
		"	movl	640(%%esi), %%esp	\n" \
		"	jmp	*%2			\n" \
		: /* no outputs */ \
		: "g" (Wptr), "S" (sched), "q" (return_address) \
		: "memory")

#define K_ZERO_OUT_JRET() \
	TRACE_RETURN (Wptr[Iptr]); \
	__asm__ __volatile__ ("				\n" \
		"	movl	%0, %%ebp		\n" \
		"	movl	640(%%esi), %%esp	\n" \
		"	jmp	*-4(%%ebp)		\n" \
		: /* no outputs */ \
		: "g" (Wptr), "S" (sched) \
		: "memory")

#define K_ONE_OUT(A) \
	TRACE_RETURN (return_address); \
	__asm__ __volatile__ ("\n" \
		"	movl	%0, %%ebp		\n" \
		"	movl	640(%%esi), %%esp	\n" \
		"	jmp	*%2			\n" \
		: /* no outputs */ \
		: "g" (Wptr), "S" (sched), "q" (return_address), \
		  "a" (A) \
		: "memory")

#define K_TWO_OUT(A,B) \
	TRACE_RETURN (return_address); \
	__asm__ __volatile__ ("\n" \
		"	movl	%0, %%ebp		\n" \
		"	movl	640(%%esi), %%esp	\n" \
		"	jmp	*%2			\n" \
		: /* no outputs */ \
		: "g" (Wptr), "S" (sched), "q" (return_address), \
		  "d" (B), "a" (A) \
		: "memory")

#define K_THREE_OUT(A,B,C) \
	TRACE_RETURN (return_address); \
	__asm__ __volatile__ ("\n" \
		"	movl	%0, %%ebp		\n" \
		"	movl	640(%%esi), %%esp	\n" \
		"	jmp	*%2			\n" \
		: /* no outputs */ \
		: "g" (Wptr), "S" (sched), "D" (return_address), \
		  "c" (C), "d" (B), "a" (A) \
		: "memory")
/*}}}*/

/*{{{  entry and label functions */
#define K_ENTRY(init,stack,Wptr,Fptr) \
	__asm__ __volatile__ ("				\n" \
		"	subl $64, %0			\n" \
		"	andl $0xffffffe0, %0		\n" \
		"	subl $768, %0			\n" \
		"	movl %0, %%esp			\n" \
		"	call *%3			\n" \
		: /* no outputs */ \
		: "a" (stack), "c" (Wptr), "d" (Fptr), "r" (init) \
		: "memory")
#define K_SETLABEL(X) \
	__asm__ __volatile__ ("		\n"	\
		".align 16		\n"	\
		"	"#X":		\n"	\
		: : : "memory", "cc")
#define K_SETGLABEL(X) \
	__asm__ __volatile__ ("		\n"	\
		_K_SETGLABEL (X)		\
		: : : "memory", "cc")
/*}}}*/

/*{{{  CIF helpers */
#define K_CIF_BCALLN(func, argc, argv, ret) \
	__asm__ __volatile__ ("				\n" \
		"	pushl	%%ebp			\n" \
		"	movl	%%esp, %%ebp		\n" \
		"	subl	%%ecx, %%esp		\n" \
		"	andl	$-16, %%esp		\n" \
		"	movl	%%esp, %%edi		\n" \
		"	cld				\n" \
		"	rep				\n" \
		"	movsb				\n" \
		"	call	*%%eax			\n" \
		"	movl	%%ebp, %%esp		\n" \
		"	popl	%%ebp			\n" \
		: "=a" (ret) \
		: "0" (func), "c" (argc << WSH), "S" (argv) \
		: "cc", "memory", "edx", "edi")
#define K_CIF_ENDP_STUB(X) \
	__asm__ __volatile__ ("				\n" \
		_K_SETGGLABEL (X)			\
		"	movl	-28(%%ebp), %%ebp	\n" \
		"	movl	-4(%%ebp), %%eax	\n" \
		"	jmp	*%%eax			\n" \
		: /* no outputs */ \
		: /* no inputs */ \
		: "memory")
#define K_CIF_PROC_STUB(X, wptr) \
	__asm__ __volatile__ ("				\n" \
		_K_SETGGLABEL (X)			\
		"	movl	(%%ebp), %%esp		\n" \
		"	popl	%%eax			\n" \
		"	call	*%%eax			\n" \
		"	movl	(%%esp), %%eax		\n" \
		"	subl	$32, %%esp		\n" \
		: "=a" (wptr) \
		: /* no inputs */ \
		: "cc", "memory", "ecx", "edx", "esi", "edi")
#define K_CIF_SCHED_CALL(sched, stack, call, wptr) \
	__asm__ __volatile__ ("				\n" \
		"	movl	%0, %%esp		\n" \
		"	jmp	*%1			\n" \
		: /* no outputs */ \
		:  "r" (stack), "r" (call), "S" (sched), "a" (wptr) \
		: "cc", "memory")
/*}}}*/

#endif /* sched_asm_inserts.h */

