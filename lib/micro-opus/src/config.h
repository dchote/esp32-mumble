/* config.h for micro-opus on ESP32 (PlatformIO/Arduino) */
#ifndef CONFIG_H
#define CONFIG_H

#define HAVE_INTTYPES_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1

#define PACKAGE_BUGREPORT "opus@xiph.org"
#define PACKAGE_NAME "opus"
#define PACKAGE_STRING "opus 1.5.2"
#define PACKAGE_TARNAME "opus"
#define PACKAGE_VERSION "1.5.2"

#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_LONG_LONG 8
#define SIZEOF_SHORT 2

#define STDC_HEADERS 1
#define VERSION "1.5.2"

#ifndef __cplusplus
/* #undef inline */
#endif

#define restrict __restrict

#endif /* CONFIG_H */
