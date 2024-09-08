//  runtime_common.s: DragonBASIC runtime environment
//
//  Copyright (C) 2015 Ulrich Hecht
//  Source code reconstructed with permission from MF/TIN executable version
//  1.4.3, Copyright (C) 2003-2004 Jeff Massung
//
//  This software is provided 'as-is', without any express or implied
//  warranty.  In no event will the authors be held liable for any damages
//  arising from the use of this software.
//
//  Permission is granted to anyone to use this software for any purpose,
//  including commercial applications, and to alter it and redistribute it
//  freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software
//     in a product, an acknowledgment in the product documentation would be
//     appreciated but is not required.
//  2. Altered source versions must be plainly marked as such, and must not be
//     misrepresented as being the original software.
//  3. This notice may not be removed or altered from any source distribution.
//
//  Ulrich Hecht
//  ulrich.hecht@gmail.com


.global _tin_entry
.global _tin_entry_p
.global _tin_iwram_table_p
.global _thumbthunk
.global irq_handler_p
.global irq_handler
.global _end_irq_handler

.equ _thumbthunk_iwram, 0x3000ffe
.global _thumbthunk_iwram

.org 0xe0
_tin_entry:
	sub	r7, sp, #0x200

	// Clear OAM and BG staging memory.
	mov	r1, #0x3000000
	orr	r1, r1, #0x600
	mov	r2, #0
_oam_loop:
	subs	r1, r1, #4
	str	r2, [r1]
	cmp	r1, #0x3000000
	bne	_oam_loop

	// Set user interrupt vectors to NOP so there won't be any bad
	// surprises when enabling interrupts.
	adr	r5, _empty
	orr	r1, r1, #0x600
	str	r5, [r1, #0x10]
	str	r5, [r1, #0x14]
	str	r5, [r1, #0x18]
	str	r5, [r1, #0x1c]
	str	r5, [r1, #0x20]
	str	r5, [r1, #0x24]
	str	r5, [r1, #0x28]
	str	r5, [r1, #0x2c]
	str	r5, [r1, #0x30]
	str	r5, [r1, #0x34]
	str	r5, [r1, #0x38]

	ldr	r5, _tin_iwram_table_p
	cmp	r5, #0
	beq	_iwram_done

_fun_loop:
	ldr	r1, [r5]
	cmp	r1, #0
	beq	_iwram_done
	ldr	r2, [r5, #4]
	ldr	r3, [r5, #8]
_copy_loop:
	ldr	r6, [r1], #4
	str	r6, [r2], #4
	subs	r3, r3, #4
	bne	_copy_loop
	add	r5, r5, #12
	b	_fun_loop


_iwram_done:
	ldr	r5, _thumbthunk_iwram_p
	ldrh	r6, _thumbthunk
	strh	r6, [r5]

	ldr	r5, _tin_entry_p
	ldr	r6, irq_handler_p
	mov	r1, #0x4000000
	str	r6, [r1, #-4]
	ldr	r6, waitcnt_val
	str	r6, [r1, #0x204]
	bx	r5
waitcnt_val:
	.word	0x4317
_thumbthunk_iwram_p:
	.word _thumbthunk_iwram
irq_handler_p:
	# Will be overwritten by the TIN compiler if the IRQ handler
	# is copied to IWRAM.
	.word irq_handler
_tin_entry_p:
	# will be overwritten by the TIN compiler
	.word 0xffffffff
_tin_iwram_table_p:
	# will be overwritten by the TIN compiler if there is code that
	# must be copied to IWRAM
	.word 0

irq_handler:
	push	{r4-r11, lr}
	mov	r9, #0x4000000
	orr	r8, r9, #0x200
	ldrh	r10, [r8, #2]

	ldrh	r8, [r9, #-8]
	orr	r8, r8, r10
	strh	r8, [r9, #-8]

	mov	r8, #0x3000000
	orr	r8, r8, #0x600

	add	lr, pc, #4
	tst	r10, #0x0008
	ldrne	pc, [r8, #16]
	add	lr, pc, #4
	tst	r10, #0x0010
	ldrne	pc, [r8, #20]
	add	lr, pc, #4
	tst	r10, #0x1000
	ldrne	pc, [r8, #24]
	add	lr, pc, #4
	tst	r10, #0x0002
	ldrne	pc, [r8, #28]
	add	lr, pc, #4
	tst	r10, #0x0001
	ldrne	pc, [r8, #32]
	add	lr, pc, #4
	tst	r10, #0x0004
	ldrne	pc, [r8, #36]
	add	lr, pc, #4
	tst	r10, #0x0020
	ldrne	pc, [r8, #40]
	add	lr, pc, #4
	tst	r10, #0x0100
	ldrne	pc, [r8, #44]
	add	lr, pc, #4
	tst	r10, #0x0200
	ldrne	pc, [r8, #48]
	add	lr, pc, #4
	tst	r10, #0x0400
	ldrne	pc, [r8, #52]
	add	lr, pc, #4
	tst	r10, #0x0800
	ldrne	pc, [r8, #56]

	orr	r8, r9, #0x200
	strh	r10, [r8, #2]
	pop	{r4-r11, lr}
_empty:
	bx	lr
_end_irq_handler:

.thumb
_thumbthunk:
	bx	r2
.arm
