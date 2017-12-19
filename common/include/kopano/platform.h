/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <kopano/zcdefs.h>

enum {
	KC_DESIRED_FILEDES = 8192,
};

  #ifdef HAVE_CONFIG_H
  #include "config.h"
  #endif
  #include <kopano/platform.linux.h>
#include <chrono>
#include <string>
#include <type_traits>
#include <cstddef>
#include <endian.h>
#include <pthread.h>

namespace KC {

#define KOPANO_SYSTEM_USER		"SYSTEM"
#define KOPANO_SYSTEM_USER_W	L"SYSTEM"

/* This should match what is used in proto.h for __size */
typedef int gsoap_size_t;

/*
 * Platform independent functions
 */
extern _kc_export HRESULT UnixTimeToFileTime(time_t, FILETIME *);
extern _kc_export HRESULT FileTimeToUnixTime(const FILETIME &, time_t *);
extern _kc_export void UnixTimeToFileTime(time_t, int *hi, unsigned int *lo);
extern _kc_export time_t FileTimeToUnixTime(unsigned int hi, unsigned int lo);

void	RTimeToFileTime(LONG rtime, FILETIME *pft);
extern _kc_export void FileTimeToRTime(const FILETIME *, LONG *rtime);
extern _kc_export HRESULT UnixTimeToRTime(time_t unixtime, LONG *rtime);
extern _kc_export HRESULT RTimeToUnixTime(LONG rtime, time_t *unixtime);
extern _kc_export struct tm *gmtime_safe(const time_t *timer, struct tm *result);
extern _kc_export double timespec2dbl(const struct timespec &);
extern bool operator==(const FILETIME &, const FILETIME &) noexcept;
extern _kc_export bool operator >(const FILETIME &, const FILETIME &) noexcept;
extern bool operator>=(const FILETIME &, const FILETIME &) noexcept;
extern _kc_export bool operator <(const FILETIME &, const FILETIME &) noexcept;
extern bool operator<=(const FILETIME &, const FILETIME &) noexcept;
extern _kc_export time_t operator -(const FILETIME &, const FILETIME &);

/* convert struct tm to time_t in timezone UTC0 (GM time) */
#ifndef HAVE_TIMEGM
time_t timegm(struct tm *t);
#endif

// mkdir -p
extern _kc_export int CreatePath(const char *);

// Random-number generators
extern _kc_export void rand_init(void);
extern _kc_export int rand_mt(void);
extern _kc_export void rand_get(char *p, int n);
extern _kc_export char *get_password(const char *prompt);

/**
 * Memory usage calculation macros
 */
#define MEMALIGN(x) (((x) + alignof(void *) - 1) & ~(alignof(void *) - 1))

#define MEMORY_USAGE_MAP(items, map)		(items * (sizeof(map) + sizeof(map::value_type)))
#define MEMORY_USAGE_LIST(items, list)		(items * (MEMALIGN(sizeof(list) + sizeof(list::value_type))))
#define MEMORY_USAGE_HASHMAP(items, map)	MEMORY_USAGE_MAP(items, map)
#define MEMORY_USAGE_STRING(str)			(str.capacity() + 1)
#define MEMORY_USAGE_MULTIMAP(items, map)	MEMORY_USAGE_MAP(items, map)

extern _kc_export ssize_t read_retry(int, void *, size_t);
extern _kc_export ssize_t write_retry(int, const void *, size_t);

extern _kc_export void set_thread_name(pthread_t, const std::string &);
extern _kc_export void my_readahead(int fd);
extern _kc_export void give_filesize_hint(int fd, off_t len);

extern _kc_export bool force_buffers_to_disk(int fd);
extern _kc_export int ec_relocate_fd(int);
extern _kc_export void kcsrv_blocksigs(void);
extern _kc_export unsigned long kc_threadid(void);

/* Determine the size of an array */
template<typename T, size_t N> constexpr inline size_t ARRAY_SIZE(T (&)[N]) { return N; }

/* Get the one-past-end item of an array */
template<typename T, size_t N> constexpr inline T *ARRAY_END(T (&a)[N]) { return a + N; }

template<typename T> constexpr const IID &iid_of();
template<typename T> static inline constexpr const IID &iid_of(const T &)
{
	return iid_of<typename std::remove_cv<typename std::remove_pointer<T>::type>::type>();
}

using time_point = std::chrono::time_point<std::chrono::steady_clock>;

template<typename T> static constexpr inline double dur2dbl(const T &t)
{
	return std::chrono::duration_cast<std::chrono::duration<double>>(t).count();
}

#if (defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN) || \
    (defined(_BYTE_ORDER) && _BYTE_ORDER == _BIG_ENDIAN)
	/* We need to use constexpr functions, and htole16 unfortunately is not. */
#	define cpu_to_le16(x) __builtin_bswap16(x)
#	define cpu_to_le32(x) __builtin_bswap32(x)
#	define cpu_to_be64(x) (x)
#	define le16_to_cpu(x) __builtin_bswap16(x)
#	define le32_to_cpu(x) __builtin_bswap32(x)
#	define be64_to_cpu(x) (x)
#else
#	define cpu_to_le16(x) (x)
#	define cpu_to_le32(x) (x)
#	define cpu_to_be64(x) __builtin_bswap64(x)
#	define le16_to_cpu(x) (x)
#	define le32_to_cpu(x) (x)
#	define be64_to_cpu(x) __builtin_bswap64(x)
#endif

} /* namespace */

#define IID_OF(T) namespace KC { template<> inline constexpr const IID &iid_of<T>() { return IID_ ## T; } }
#define IID_OF2(T, U) namespace KC { template<> inline constexpr const IID &iid_of<T>() { return IID_ ## U; } }

#endif // PLATFORM_H
