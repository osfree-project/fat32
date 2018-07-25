/*
 * Block driver for RAW files (os2)
 *
 * Copyright (c) 2006 Fabrice Bellard
 * Copyright (c) 2018 Valery V. Sedletski
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "qemu-common.h"
#include "qemu-timer.h"
#include "block_int.h"
#include "module.h"

#define  INCL_DOSMODULEMGR
#define  INCL_DOSFILEMGR
#define  INCL_DOSERRORS
#define  INCL_LONGLONG
#include <os2.h>

int largefile = 0;

APIRET APIENTRY (*pDosOpenL)(PSZ, PHFILE, PULONG, ULONGLONG,
             ULONG, ULONG, ULONG, PEAOP2) = 0;
APIRET APIENTRY (*pDosSetFilePtrL)(HFILE, LONGLONG, ULONG, PULONGLONG) = 0;
APIRET APIENTRY (*pDosSetFileSizeL)(HFILE, ULONGLONG) = 0;

typedef struct BDRVRawState {
    HFILE hf;
} BDRVRawState;

int is_largefile_supported(void)
{
    HMODULE hmod;
    PFN pfn;
    APIRET rc;

    // find DOSCALLS handle
    DosQueryModuleHandle("DOSCALLS", &hmod);

    // find DosOpenL address
    rc = DosQueryProcAddr(hmod, 981, NULL, &pfn);

    if (! rc)
        pDosOpenL = (void *)pfn;

    // find DosSetFilePtrL address
    rc = DosQueryProcAddr(hmod, 988, NULL, &pfn);

    if (! rc)
        pDosSetFilePtrL = (void *)pfn;
    
    // find DosSetFileSizeL address
    rc = DosQueryProcAddr(hmod, 989, NULL, &pfn);

    if (! rc)
        pDosSetFileSizeL = (void *)pfn;

    if (pDosOpenL)
        return 1;

    return 0;
}

static int raw_open(BlockDriverState *bs, const char *filename, int flags)
{
    BDRVRawState *s = bs->opaque;
    ULONG open_flags, open_mode;
    ULONG action = 0;
    APIRET rc;

    if ((flags & BDRV_O_ACCESS) == O_RDWR) {
        open_mode = OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYWRITE;
    } else {
        open_mode = OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE;
    }
    if (flags & BDRV_O_CREAT) {
        open_flags = OPEN_ACTION_CREATE_IF_NEW;
    } else {
        open_flags = OPEN_ACTION_OPEN_IF_EXISTS;
    }
    if ((flags & BDRV_O_NOCACHE))
        open_mode |= OPEN_FLAGS_NO_CACHE;
    else if (!(flags & BDRV_O_CACHE_WB))
        open_mode |= OPEN_FLAGS_WRITE_THROUGH;

    if (largefile)
    {
        rc = (pDosOpenL)((PSZ)filename, &s->hf, &action,
                      0, 0, open_flags, open_mode,
                      NULL);
    }
    else
    {
        rc = DosOpen(filename, &s->hf, &action,
                     0, 0, open_flags, open_mode,
                     NULL);
    }

    if (rc || s->hf == NULLHANDLE)
    {
        if (rc == ERROR_ACCESS_DENIED)
        {
            return -EACCES;
        }
        return -1;
    }
    return 0;
}

static int raw_read(BlockDriverState *bs, int64_t sector_num,
                    uint8_t *buf, int nb_sectors)
{
    BDRVRawState *s = bs->opaque;
    LONGLONG offset = sector_num * 512;
    ULONG count = nb_sectors * 512;
    ULONGLONG actual = 0;
    APIRET rc;

    if (largefile)
    {
        rc = (pDosSetFilePtrL)(s->hf, offset, FILE_BEGIN, &actual);

        if (rc)
            return -1;
    }
    else
    {
        rc = DosSetFilePtr(s->hf, offset, FILE_BEGIN, (PULONG)&actual);

        if (rc)
            return -1;
    }
    rc = DosRead(s->hf, buf, count, (PULONG)&actual);
    if (rc)
        return actual;
    if (actual == count)
        actual = 0;
    return actual;
}

static int raw_write(BlockDriverState *bs, int64_t sector_num,
                     const uint8_t *buf, int nb_sectors)
{
    BDRVRawState *s = bs->opaque;
    LONGLONG offset = sector_num * 512;
    ULONG count = nb_sectors * 512;
    ULONGLONG actual = 0;
    APIRET rc;

    if (largefile)
    {
        rc = (pDosSetFilePtrL)(s->hf, offset, FILE_BEGIN, &actual);

        if (rc)
            return -1;
    }
    else
    {
        rc = DosSetFilePtr(s->hf, offset, FILE_BEGIN, (PULONG)&actual);

        if (rc)
            return -1;
    }
    rc = DosWrite(s->hf, (void *)buf, count, (PULONG)&actual);
    if (rc)
        return actual;
    return actual;
    if (actual == count)
         actual = 0;
    return actual;
}

static void raw_flush(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;
    DosResetBuffer(s->hf);
}

static void raw_close(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;
    DosClose(s->hf);
}

static int raw_truncate(BlockDriverState *bs, int64_t offset)
{
    BDRVRawState *s = bs->opaque;
    APIRET rc;
    if (largefile)
    {
        rc = (pDosSetFileSizeL)(s->hf, offset);
    }
    else
    {
        rc = DosSetFileSize(s->hf, offset);
    }
    if (rc)
        return -EIO;
    return 0;
}

static int64_t raw_getlength(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;
    ULONGLONG size;
    APIRET rc;

    if (largefile)
    {
        FILESTATUS3L Info;

        rc = DosQueryFileInfo(s->hf, FIL_STANDARDL, &Info, sizeof(Info));
        size = Info.cbFile;
    }
    else
    {
        FILESTATUS3 Info;

        rc = DosQueryFileInfo(s->hf, FIL_STANDARD, &Info, sizeof(Info));
        size = (ULONGLONG)Info.cbFile;
    }
    if (rc)
        return -EIO;
    return size;
}

static int raw_create(const char *filename, QEMUOptionParameter *options)
{
    HFILE hf;
    int64_t total_size = 0;
    ULONG action;
    APIRET rc;

    /* Read out options */
    while (options && options->name)
    {
        if (!strcmp(options->name, BLOCK_OPT_SIZE))
        {
            total_size = options->value.n / 512;
        }
        options++;
    }

    if (largefile)
    {
        rc = (*pDosOpenL)((PSZ)filename, &hf, &action, total_size * 512, 0,
                      OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS,
                      OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYWRITE,
                      NULL);
    }
    else
    {
        rc = DosOpen(filename, &hf, &action, (ULONG)total_size * 512, 0,
                     OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS,
                     OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYWRITE,
                     NULL);
    }
    if (rc || ! hf)
        return -EIO;
    DosClose(hf);
    return 0;
}

/***********************************************/
/* host device */

static QEMUOptionParameter raw_create_options[] = {
    {
        .name = BLOCK_OPT_SIZE,
        .type = OPT_SIZE,
        .help = "Virtual disk size"
    },
    { NULL }
};

BlockDriver bdrv_raw = {
    .format_name	= "raw",
    .instance_size	= sizeof(BDRVRawState),
    .bdrv_open		= raw_open,
    .bdrv_close		= raw_close,
    .bdrv_create	= raw_create,
    .bdrv_flush		= raw_flush,
    .bdrv_read		= raw_read,
    .bdrv_write		= raw_write,
    .bdrv_truncate	= raw_truncate,
    .bdrv_getlength	= raw_getlength,

    .create_options = raw_create_options,
};
