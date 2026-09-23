// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define LWIP_PLATFORM_DIAG(args) do { printf args; } while (0)
#define LWIP_PLATFORM_ASSERT(message) do { fprintf(stderr, "%s\n", message); abort(); } while (0)
#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif
