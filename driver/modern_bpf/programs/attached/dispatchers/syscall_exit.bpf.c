/*
 * Copyright (C) 2022 The Falco Authors.
 *
 * This file is dual licensed under either the MIT or GPL 2. See MIT.txt
 * or GPL2.txt for full copies of the license.
 */

#include <helpers/interfaces/syscalls_dispatcher.h>
#include <helpers/interfaces/attached_programs.h>

/* From linux tree: /include/trace/events/syscall.h
 * TP_PROTO(struct pt_regs *regs, long ret),
 */
SEC("tp_btf/sys_exit")
int BPF_PROG(sys_exit,
	     struct pt_regs *regs,
	     long ret)
{
	u32 syscall_id = extract__syscall_id(regs);

#ifdef CAPTURE_SOCKETCALL
	/* we convert it here in this way the syscall will be treated exactly as the original one */
	if(syscall_id == __NR_socketcall)
	{
		syscall_id = convert_network_syscalls(regs);
	}
#endif

	/* The `syscall-id` can refer to both 64-bit and 32-bit architectures.
	 * Right now we filter only 64-bit syscalls, all the 32-bit syscalls
	 * will be dropped with `syscalls_dispatcher__check_32bit_syscalls`.
	 *
	 * If the syscall is not interesting we drop it.
	 */
	if(!syscalls_dispatcher__64bit_interesting_syscall(syscall_id))
	{
		return 0;
	}

	if(sampling_logic(ctx, syscall_id, SYSCALL))
	{
		return 0;
	}

	if(syscalls_dispatcher__check_32bit_syscalls())
	{
		return 0;
	}

	if (maps__get_drop_failed() && ret < 0)
	{
		return 0;
	}

	bpf_tail_call(ctx, &syscall_exit_tail_table, syscall_id);

	return 0;
}
