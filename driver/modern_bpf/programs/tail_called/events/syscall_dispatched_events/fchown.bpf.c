/*
 * Copyright (C) 2022 The Falco Authors.
 *
 * This file is dual licensed under either the MIT or GPL 2. See MIT.txt
 * or GPL2.txt for full copies of the license.
 */

#include <helpers/interfaces/fixed_size_event.h>

/*=============================== ENTER EVENT ===========================*/

SEC("tp_btf/sys_enter")
int BPF_PROG(fchown_e,
	     struct pt_regs *regs,
	     long id)
{
	struct ringbuf_struct ringbuf;
	if(!ringbuf__reserve_space(&ringbuf, ctx, FCHOWN_E_SIZE))
	{
		return 0;
	}

	ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_FCHOWN_E);

	/*=============================== COLLECT PARAMETERS  ===========================*/

	// Here we have no parameters to collect.

	/*=============================== COLLECT PARAMETERS  ===========================*/

	ringbuf__submit_event(&ringbuf);

	return 0;
}

/*=============================== ENTER EVENT ===========================*/

/*=============================== EXIT EVENT ===========================*/

SEC("tp_btf/sys_exit")
int BPF_PROG(fchown_x,
	     struct pt_regs *regs,
	     long ret)
{

	struct ringbuf_struct ringbuf;
	if(!ringbuf__reserve_space(&ringbuf, ctx, FCHOWN_X_SIZE))
	{
		return 0;
	}

	ringbuf__store_event_header(&ringbuf, PPME_SYSCALL_FCHOWN_X);

	/*=============================== COLLECT PARAMETERS  ===========================*/

	/* Parameter 1: res (type: PT_ERRNO) */
	ringbuf__store_s64(&ringbuf, ret);

	/* Parameter 2: fd (type: PT_FD) */
	s32 fd = (s32)extract__syscall_argument(regs, 0);
	ringbuf__store_s64(&ringbuf, (s64)fd);

	/* Parameter 3: uid (type: PT_UINT32) */
	unsigned long uid = extract__syscall_argument(regs, 1);
	ringbuf__store_u32(&ringbuf, uid);

	/* Parameter 4: gid (type: PT_UINT32) */
	unsigned long gid = extract__syscall_argument(regs, 2);
	ringbuf__store_u32(&ringbuf, gid);

	/*=============================== COLLECT PARAMETERS  ===========================*/

	ringbuf__submit_event(&ringbuf);

	return 0;
}

/*=============================== EXIT EVENT ===========================*/
