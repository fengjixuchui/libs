#include "../../event_class/event_class.h"

#ifdef __NR_open
TEST(SyscallEnter, openE)
{
	auto evt_test = get_syscall_event_test(__NR_open, ENTER_EVENT);

	evt_test->enable_capture();

	/*=============================== TRIGGER SYSCALL  ===========================*/

	/* Syscall special notes:
	 * With `O_TMPFILE` flag the pathname must be a directory
	 * but here it is a filename so the call will fail!
	 */
	const char* pathname = "mock_path";
	int flags = O_RDWR | O_TMPFILE | O_DIRECTORY;
	mode_t mode = 0;
	assert_syscall_state(SYSCALL_FAILURE, "open", syscall(__NR_open, pathname, flags, mode));

	/*=============================== TRIGGER SYSCALL  ===========================*/

	evt_test->disable_capture();

	evt_test->assert_event_presence();

	if(HasFatalFailure())
	{
		return;
	}

	evt_test->parse_event();

	evt_test->assert_header();

	/*=============================== ASSERT PARAMETERS  ===========================*/

	/* Parameter 1: name (type: PT_FSPATH) */
	evt_test->assert_charbuf_param(1, pathname);

	/* Parameter 2: flags (type: PT_FLAGS32) */
	evt_test->assert_numeric_param(2, (uint32_t)PPM_O_RDWR | PPM_O_TMPFILE | PPM_O_DIRECTORY);

	/* Parameter 3: mode (type: PT_UINT32) */
	evt_test->assert_numeric_param(3, (uint32_t)mode);

	/*=============================== ASSERT PARAMETERS  ===========================*/

	evt_test->assert_num_params_pushed(3);
}

TEST(SyscallEnter, openE_max_path)
{
	auto evt_test = get_syscall_event_test(__NR_open, ENTER_EVENT);

	evt_test->enable_capture();

	/*=============================== TRIGGER SYSCALL  ===========================*/

	/* Syscall special notes:
	 * With `O_TMPFILE` flag the pathname must be a directory
	 * but here it is a filename so the call will fail!
	 */
	std::string pathname("");
	pathname.insert(0, PPM_MAX_PATH_SIZE - 1, 'A');
	int flags = O_RDWR | O_TMPFILE | O_DIRECTORY;
	mode_t mode = 0;
	assert_syscall_state(SYSCALL_FAILURE, "open", syscall(__NR_open, pathname.c_str(), flags, mode));

	/*=============================== TRIGGER SYSCALL  ===========================*/

	evt_test->disable_capture();

	evt_test->assert_event_presence();

	if(HasFatalFailure())
	{
		return;
	}

	evt_test->parse_event();

	evt_test->assert_header();

	/*=============================== ASSERT PARAMETERS  ===========================*/

	/* Parameter 1: name (type: PT_FSPATH) */
	evt_test->assert_charbuf_param(1, pathname.c_str());

	/* Parameter 2: flags (type: PT_FLAGS32) */
	evt_test->assert_numeric_param(2, (uint32_t)PPM_O_RDWR | PPM_O_TMPFILE | PPM_O_DIRECTORY);

	/* Parameter 3: mode (type: PT_UINT32) */
	evt_test->assert_numeric_param(3, (uint32_t)mode);

	/*=============================== ASSERT PARAMETERS  ===========================*/

	evt_test->assert_num_params_pushed(3);
}
#endif
