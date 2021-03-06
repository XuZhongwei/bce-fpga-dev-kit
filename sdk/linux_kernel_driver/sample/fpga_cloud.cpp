/*
 * Copyright (C) 2017 Baidu, Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "fpga_cloud.h"

namespace {

pthread_once_t mmap_once = PTHREAD_ONCE_INIT;

void *mmap_base = NULL;

void mmap_func(void)
{
    int fd = open("/dev/xdma0_user", O_RDWR | O_SYNC);
    if (fd == -1) {
        printf("Fail to open /dev/xdma0_user, errno=%d\n", errno);
        abort();
    }

    /* map 192KB */
    mmap_base = mmap(0, (128 + 64) << 10, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmap_base == (void *)-1) {
        printf("map_base == -1\n");
        abort();
    }

    return;
}

}

namespace fpga {
namespace cloud {
namespace api {

int reg_read(int dev, uint64_t addr, uint64_t *value)
{
    int ret = pthread_once(&::mmap_once, &::mmap_func);
    if (ret != 0) {
        printf("pthread_once failed\n");
        abort();
    }

    *value = *(uint64_t *)((uint64_t)::mmap_base + addr);
    return 0;
}

int reg_write(int dev, uint64_t addr, uint64_t value)
{
    int ret = pthread_once(&::mmap_once, &::mmap_func);
    if (ret != 0) {
        printf("pthread_once failed\n");
        abort();
    }

    *(uint64_t *)((uint64_t)::mmap_base + addr) = value;
    return 0;
}

int reg_read_32(int dev, uint64_t addr, uint32_t *value)
{
    int ret = pthread_once(&::mmap_once, &::mmap_func);
    if (ret != 0) {
        printf("pthread_once failed\n");
        abort();
    }

    *value = *(uint32_t *)((uint64_t)::mmap_base + addr);
    return 0;
}

int reg_write_32(int dev, uint64_t addr, uint32_t value)
{
    int ret = pthread_once(&::mmap_once, &::mmap_func);
    if (ret != 0) {
        printf("pthread_once failed\n");
        abort();
    }

    *(uint32_t *)((uint64_t)::mmap_base + addr) = value;
    return 0;
}

int fpga_memcpy(int dev, uint64_t dest, uint64_t src, size_t n, int to_dev)
{
    if (to_dev) {
        int fd = open("/dev/xdma0_h2c_0", O_WRONLY);
        if (fd == -1) {
            printf("Fail to open /dev/xdma0_h2c_0, errno=%d\n", errno);
            return -ENODEV;
        }

        /* XXX: bytes written may be less than **count** in 3.10.0_2-0-0-2, buggy kernel? */
        size_t remaining = n;
        while (remaining > 0) {
            ssize_t nbytes = pwrite(fd, reinterpret_cast<void *>(src + n - remaining), remaining,
                                    static_cast<off_t>(dest + n - remaining));
            if (nbytes < 0) {
                close(fd);
                return -errno;
            }

            remaining -= nbytes;
        }

        close(fd);
        return 0;
    } else {
        int fd = open("/dev/xdma0_c2h_0", O_RDONLY);
        if (fd == -1) {
            printf("Fail to open /dev/xdma0_c2h_0, errno=%d\n", errno);
            return -ENODEV;
        }

        size_t remaining = n;
        while (remaining > 0) {
            ssize_t nbytes = pread(fd, reinterpret_cast<void *>(dest + n - remaining), remaining,
                                   static_cast<off_t>(src + n - remaining));
            if (nbytes < 0) {
                close(fd);
                return -errno;
            }

            remaining -= nbytes;
        }

        close(fd);
        return 0;
    }
}

}
}
}

