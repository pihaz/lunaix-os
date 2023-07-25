#ifndef __LUNAIX_TYPES_H
#define __LUNAIX_TYPES_H

#include <lunaix/compiler.h>
#include <stdarg.h>
#include <stddef.h>
#include <usr/lunaix/types.h>

#define PACKED __attribute__((packed))

// TODO: WTERMSIG

// TODO: replace the integer type with these. To make thing more portable.

typedef unsigned char u8_t;
typedef unsigned short u16_t;
typedef unsigned int u32_t;
typedef unsigned long long u64_t;
typedef unsigned long ptr_t;

typedef signed long ssize_t;
typedef int pid_t;
typedef unsigned long size_t;
typedef unsigned long off_t;

typedef u64_t lba_t;

#endif /* __LUNAIX_TYPES_H */
