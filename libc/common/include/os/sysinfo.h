#pragma once

/**
 * @return The number of kilobytes of physical memory available to the kernel in
 *         total. Some hardware-mapped or reserved memory may not be included.
 *         This value may be rounded to a multiple of some architecture-defined
 *         size.
 */
size_t OsGetTotalMemoryKilobytes(void);

/**
 * @return The number of kilobytes of yet-unallocated physical memory. This
 *         value is likely to fluctuate, and may be rounded to a multiple of
 *         some architecture-defined size.
 */
size_t OsGetFreeMemoryKilobytes(void);

/**
 * Returns version information about the operating system kernel.
 * 
 * @param major  The kernel major version is written here. Must not be NULL. 
 *               If an error occurs, this value will be -1.
 * @param minor  The kernel minor version is written here. Must not be NULL. 
 *               If an error occurs, this value will be -1.
 * @param string A human-readable string for the kernel name and version will be
 *               written here. If an error occurs, or if the string to be
 *               written is longer than `length`, then the contents at this
 *               address, up to `length` may be modified in an undefined way.
 * @param length The maximum length of the string to write to `string`.
 */
void OsGetVersion(int* major, int* minor, char* string, int length);
