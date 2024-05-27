#include <syscall.h>
#include <errno.h>
#include <transfer.h>
#include <arch.h>
#include <_syscallnum.h>
#include <string.h>

int SysTime(size_t ptr, size_t op, size_t len, size_t ptr2, size_t) {
	/* TODO: store this somewhere proper */
	uint64_t timezone_offset = 1000000ULL * 60 * 60 * 10;

	if (op == 0) {
		/*
		 * Get local time
		 */
		uint64_t timevalue = ArchGetUtcTime(timezone_offset) + timezone_offset;
		struct transfer io;
		io = CreateTransferWritingToUser((void*) ptr, sizeof(uint64_t), 0);
		return PerformTransfer(&timevalue, &io, sizeof(uint64_t));

	} else if (op == 1) {
		/*
		 * Set local time
		 */
		uint64_t timevalue;
		struct transfer io;
		io = CreateTransferReadingFromUser((void*) ptr, sizeof(uint64_t), 0);
		int res = PerformTransfer(&timevalue, &io, sizeof(uint64_t));
		if (res != 0) {
			return res;
		}
		return ArchSetUtcTime(timevalue - timezone_offset, timezone_offset);

	} else if (op == 2) {
		/*
		 * Get timezone
		 */
		const char* tz_string = "Australia/Sydney";
		if (strlen(tz_string) >= len) {
			return ENAMETOOLONG;
		}

		int res = WriteStringToUsermode(tz_string, (char*) ptr, strlen(tz_string) + 1);
		if (res != 0) {
			return res;
		}

		struct transfer io;
		io = CreateTransferWritingToUser((void*) ptr2, sizeof(uint64_t), 0);
		return PerformTransfer(&timezone_offset, &io, sizeof(uint64_t));

	} else if (op == 3) {
		/*
		 * Set timezone
		 */
		return ENOSYS;

	} else {
		return EINVAL;
	}
}
