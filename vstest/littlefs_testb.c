
#if 0

#include "lfs.h"

// variables used by the filesystem
lfs_t lfs;
lfs_file_t file;

uint32_t mem[32 * 1024 * 1024 / 4];

// Read a region in a block. Negative error codes are propagated
// to the user.
int user_provided_block_device_read(const struct lfs_config *c, lfs_block_t block,
    lfs_off_t off, void *buffer, lfs_size_t size)
{
    uint8_t *mem8 = (uint8_t *)mem;
    uint8_t *src = &mem8[block * 4096 + off];
    printf("read block %i off %i size %i data 0x%x (%i)\n", block, off, size, *(uint32_t *)src, *(uint32_t *)src);
    memcpy(buffer, src, size);
    return 0;
}

// Program a region in a block. The block must have previously
// been erased. Negative error codes are propagated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
int user_provided_block_device_prog(const struct lfs_config *c, lfs_block_t block,
    lfs_off_t off, const void *buffer, lfs_size_t size)
{
    printf("p--> block %i off %i size %i data 0x%x (%i)\n", block, off, size, *(uint32_t *)buffer, *(uint32_t *)buffer);
    uint8_t *mem8 = (uint8_t *)mem;
    uint8_t *dst = &mem8[block * 4096 + off];
    memcpy(dst, buffer, size);
    return 0;
}

// Erase a block. A block must be erased before being programmed.
// The state of an erased block is undefined. Negative error codes
// are propagated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
int user_provided_block_device_erase(const struct lfs_config *c, lfs_block_t block)
{
    printf("ERASE ERASE ERASE block %i\n", block);
    return 0;
}

// Sync the state of the underlying block device. Negative error codes
// are propagated to the user.
int user_provided_block_device_sync(const struct lfs_config *c)
{
    printf("==================================== sync ====================================\n");
    return 0;
}

// configuration of the filesystem is provided by this struct
const struct lfs_config cfg = {
    // block device operations
    .read  = user_provided_block_device_read,
    .prog  = user_provided_block_device_prog,
    .erase = user_provided_block_device_erase,
    .sync  = user_provided_block_device_sync,

    // block device configuration
    .read_size = 128, // read alignment, smallest possible read
    .prog_size = 128, // write alignment, smallest possible write
    .block_size = 4096,
    .block_count = sizeof(mem) / 4096,
    .cache_size = 512, // write burst cache for small writes
    .lookahead_size = 32, // size of in-RAM free blocks lookahead bitmap (cache)
    .block_cycles = 100,
};

void boot()
{
    // mount the filesystem
    int err = lfs_mount(&lfs, &cfg);

    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err) {
        lfs_format(&lfs, &cfg);
        lfs_mount(&lfs, &cfg);
    }

    // read current count
    uint32_t boot_count = 0;
    lfs_file_open(&lfs, &file, "boot_count", LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_read(&lfs, &file, &boot_count, sizeof(boot_count));

    // update boot count
    boot_count += 1;
    lfs_file_rewind(&lfs, &file);
    lfs_file_write(&lfs, &file, &boot_count, sizeof(boot_count));

    // remember the storage is not updated until the file is closed successfully
    lfs_file_close(&lfs, &file);

    // release any resources we were using
    lfs_unmount(&lfs);

    // print the boot count
    printf("boot_count: %d\n", boot_count);
}

void writebigfile(char *filename, uint8_t val)
{
    printf("open big file\n");
    lfs_file_open(&lfs, &file, filename, LFS_O_RDWR | LFS_O_CREAT);
    uint8_t testdata[4096];
    memset(testdata, val, sizeof(testdata));
    printf("part 1 write 4096 bytes\n");
    lfs_file_write(&lfs, &file, testdata, sizeof(testdata));
    printf("part 2 write 4096 bytes\n");
    lfs_file_write(&lfs, &file, testdata, sizeof(testdata));
    printf("part 3 write 4096 bytes\n");
    lfs_file_write(&lfs, &file, testdata, sizeof(testdata));
    printf("close big file\n");
    lfs_file_close(&lfs, &file);
}

void writedummyfile(char *filename, uint8_t val)
{
    printf("open dummy file\n");
    lfs_file_open(&lfs, &file, filename, LFS_O_RDWR | LFS_O_CREAT);
    uint8_t testdata[4096];
    memset(testdata, val, sizeof(testdata));
    printf("dummy 4096 bytes\n");
    lfs_file_write(&lfs, &file, testdata, sizeof(testdata));
    printf("close dummy file\n");
    lfs_file_close(&lfs, &file);
}

lfs_block_t tryreserve(char *filename, uint32_t size)
{
    printf("open reserve file\n");
    lfs_file_open(&lfs, &file, filename, LFS_O_WRONLY | LFS_O_CREAT);
    printf("reserve bytes\n");
    lfs_file_reserve(&lfs, &file, size, 0);
    lfs_block_t res;
    lfs_file_reserved(&lfs, &file, &res);
    printf("close reserve file\n");
    lfs_file_close(&lfs, &file);
    return res;
}

lfs_block_t tryopenexisting(char *filename)
{
    printf("open read reserve file\n");
    lfs_file_open(&lfs, &file, filename, LFS_O_RDONLY | LFS_O_CREAT);
    lfs_block_t res;
    lfs_file_reserved(&lfs, &file, &res);
    printf("close read reserve file\n");
    lfs_file_close(&lfs, &file);
    return res;
}

// entry point
int main(void)
{
    boot(); boot(); boot();
    boot(); boot(); boot();

    int err = lfs_mount(&lfs, &cfg);
    writebigfile("hello world", 0xBA);
    writebigfile("test 123", 0xC9);
    lfs_unmount(&lfs);
    
    err = lfs_mount(&lfs, &cfg);
    assert(lfs.version == LFS_DISK_VERSION_BASE);
    writedummyfile("0", 0xBA);
    writedummyfile("1", 0xC9);
    writedummyfile("2", 0xDE);
    lfs_unmount(&lfs);

    err = lfs_mount(&lfs, &cfg);
    assert(lfs.version == LFS_DISK_VERSION_BASE);
    tryreserve("xyz", 4097);
    assert(lfs.version == LFS_DISK_VERSION_FLAT);
    tryreserve("abc", 4096);
    lfs_block_t res0 = tryreserve("def", 4095);
    lfs_ssize_t size0 = lfs_fs_size(&lfs);
    printf("\n\n===> total file size %i\n\n\n", size0);
    lfs_block_t res1 = tryreserve("def", 17 * 1024 * 1024);
    lfs_ssize_t size1 = lfs_fs_size(&lfs);
    printf("\n\n===> total file size %i\n\n\n", size1);
    lfs_block_t res2 = tryreserve("def", 2 * 1024 * 1024); // replaces that last file!
    lfs_ssize_t size2 = lfs_fs_size(&lfs);
    printf("\n\n===> total file size %i\n\n\n", size2);
    lfs_block_t res3 = tryreserve("new", 16 * 1024 * 1024);
    lfs_ssize_t size3 = lfs_fs_size(&lfs);
    printf("\n\n===> total file size %i\n\n\n", size3);
    lfs_unmount(&lfs);

    printf("\n\n===> total file size %i %i %i %i at %i %i %i %i\n\n\n", size0, size1, size2, size3, res0, res1, res2, res3);

    err = lfs_mount(&lfs, &cfg);
    assert(lfs.version == LFS_DISK_VERSION_FLAT);
    lfs_block_t resn = tryopenexisting("def");
    printf("\n\n===> addr %i\n\n\n", resn);
    lfs_unmount(&lfs);
}

#else

#include "lfs.h"
#include "bd/lfs_rambd.h"

lfs_t lfs;
lfs_rambd_t bd;

#define LFS_READ_SIZE 16
#define LFS_PROG_SIZE LFS_READ_SIZE
#define LFS_BLOCK_SIZE 512
#define LFS_BLOCK_COUNT 1023
#define LFS_BLOCK_CYCLES -1
#define LFS_CACHE_SIZE (64 % LFS_PROG_SIZE == 0 ? 64 : LFS_PROG_SIZE)
#define LFS_LOOKAHEAD_SIZE 256
#define LFS_ERASE_VALUE 0xff
#define LFS_ERASE_CYCLES 0
#define LFS_BADBLOCK_BEHAVIOR LFS_TESTBD_BADBLOCK_PROGERROR

// #define FILES = 3
// #define SIZE = '(((LFS_BLOCK_SIZE-8)*(LFS_BLOCK_COUNT-6)) / FILES)'
// #define CYCLES = [1, 10]

const struct lfs_config cfg = {
    // block device operations
    .context = &bd,
    .read  = lfs_rambd_read,
    .prog  = lfs_rambd_prog,
    .erase = lfs_rambd_erase,
    .sync  = lfs_rambd_sync,

    // block device configuration
    .read_size = LFS_READ_SIZE, // read alignment, smallest possible read
    .prog_size = LFS_PROG_SIZE, // write alignment, smallest possible write
    .block_size = LFS_BLOCK_SIZE,
    .block_count = LFS_BLOCK_COUNT,
    .cache_size = LFS_CACHE_SIZE, // write burst cache for small writes
    .lookahead_size = LFS_LOOKAHEAD_SIZE, // size of in-RAM free blocks lookahead bitmap (cache)
    .block_cycles = LFS_BLOCK_CYCLES,
};

#define FILES 5
#define CYCLES 256
#define ROUNDS 256

int main()
{
    // test 1 (small allocations)

    {
        lfs_rambd_create(&cfg);

        const char *names[FILES] = { "aegus", "galemus", "kaemon", "phyxon", "sekovix" };
        lfs_file_t files[FILES];
        lfs_block_t blocks[FILES];

        lfs_format(&lfs, &cfg);
        lfs_mount(&lfs, &cfg);

        // check if small allocations behave as expected

        lfs_file_open(&lfs, &files[0], names[0], LFS_O_WRONLY | LFS_O_CREAT);
        lfs_file_reserve(&lfs, &files[0], cfg.block_size - 1, 0);
        lfs_file_reserved(&lfs, &files[0], &blocks[0]);
        lfs_file_close(&lfs, &files[0]);

        lfs_file_open(&lfs, &files[1], names[1], LFS_O_WRONLY | LFS_O_CREAT);
        lfs_file_reserve(&lfs, &files[1], cfg.block_size, 0);
        lfs_file_reserved(&lfs, &files[1], &blocks[1]);
        assert(blocks[1] == blocks[0] + 1
            || blocks[1] < blocks[0]);
        lfs_file_close(&lfs, &files[1]);

        lfs_file_open(&lfs, &files[2], names[2], LFS_O_WRONLY | LFS_O_CREAT);
        lfs_file_reserve(&lfs, &files[2], cfg.block_size + 1, 0);
        lfs_file_reserved(&lfs, &files[2], &blocks[2]);
        assert(blocks[2] == blocks[1] + 1
            || blocks[2] < blocks[1]);
        lfs_file_close(&lfs, &files[2]);

        lfs_file_open(&lfs, &files[3], names[3], LFS_O_WRONLY | LFS_O_CREAT);
        lfs_file_reserve(&lfs, &files[3], cfg.block_size * 2 + 1, 0);
        lfs_file_reserved(&lfs, &files[3], &blocks[3]);
        assert(blocks[3] == blocks[2] + 2
            || blocks[3] < blocks[2]);
        lfs_file_close(&lfs, &files[3]);

        lfs_file_open(&lfs, &files[4], names[4], LFS_O_WRONLY | LFS_O_CREAT);
        lfs_file_reserve(&lfs, &files[4], cfg.block_size, 0);
        lfs_file_reserved(&lfs, &files[4], &blocks[4]);
        assert(blocks[4] == blocks[3] + 3
            || blocks[4] < blocks[3]);
        lfs_file_close(&lfs, &files[4]);

        lfs_unmount(&lfs);

        lfs_rambd_destroy(&cfg);
    }

    // test 2 (expected out of space errors)

    {
        lfs_rambd_create(&cfg);

#define WANG_HASH(seed) ((((((seed) ^ 61) ^ ((seed) >> 16) * 9) ^ ((((seed) ^ 61) ^ ((seed) >> 16) * 9) >> 4)) * 0x27d4eb2d) ^ ((((((seed) ^ 61) ^ ((seed) >> 16) * 9) ^ ((((seed) ^ 61) ^ ((seed) >> 16) * 9) >> 4)) * 0x27d4eb2d) >> 15))

        const char *names[FILES] = { "aubermouth", "barkdell", "dingleton", "hobwelly", "waverton" };
        lfs_file_t files[FILES];

        for (int r = 0; r < ROUNDS; ++r)
        {
            lfs_format(&lfs, &cfg);
            lfs_mount(&lfs, &cfg);

            // initial files
            lfs_ssize_t nover = 2; // max fs overhead expected (blocks)
            lfs_ssize_t nblocks = 0; // blocks allocated

            // test many reservations behave as expected
            for (int c = 0; c < CYCLES; ++c)
            {
                for (int n = 0; n < FILES; ++n)
                {
                    ++nover;
                    lfs_ssize_t bnew = WANG_HASH(r * CYCLES * FILES + c * FILES  + n) % (cfg.block_count / FILES);
                    bool expectok = nblocks + nover + bnew <= (lfs_ssize_t)cfg.block_count;
                    bool expectfail = nblocks + bnew > (lfs_ssize_t)cfg.block_count;

                    // put some random flags
                    int flags = WANG_HASH(bnew) & 1 ? LFS_R_COPY : 0;
                    if (expectok && (WANG_HASH(bnew * 2) & 1)) flags |= LFS_R_OVERWRITE;
                    if (expectok && (WANG_HASH(bnew * 3) & 1)) flags |= LFS_R_TRUNCATE;
                    if (WANG_HASH(bnew * 4) & 1) flags |= LFS_R_FRONT;
                    if (WANG_HASH(bnew * 5) & 1) flags |= LFS_R_GOBBLE;
                    if (WANG_HASH(bnew * 6) & 1) flags = 0;

                    lfs_file_open(&lfs, &files[n], names[n], LFS_O_WRONLY | LFS_O_CREAT);
                    lfs_ssize_t szold = lfs_file_size(&lfs, &files[n]);
                    lfs_ssize_t bold = szold ? ((szold - 1) / cfg.block_size) + 1 : 0;
                    assert(bold == szold / cfg.block_size);
                    int err = lfs_file_reserve(&lfs, &files[n], cfg.block_size * bnew, flags);
                    if (expectfail) assert(err == LFS_ERR_NOSPC);
                    lfs_file_close(&lfs, &files[n]);

                    if (!err) {
                        nblocks -= bold;
                        nblocks += bnew;
                    } else if (expectok) {
                        assert(err == LFS_ERR_NOSPC);
                        printf("fail\n");
                        // if no space but there should be the remaining space is fragmented
                        lfs_file_open(&lfs, &files[n], names[n], LFS_O_WRONLY | LFS_O_CREAT);
                        assert(lfs_file_size(&lfs, &files[n]) == szold);
                        err = lfs_file_reserve(&lfs, &files[n], cfg.block_size, flags);
                        assert(err == LFS_ERR_OK); // we should be able to reserve at least one block
                        lfs_file_close(&lfs, &files[n]);
                        if (!err) {
                            nblocks -= bold;
                            nblocks += 1;
                        }
                    }

                    printf("%i\n", nblocks);
                }
            }

            lfs_unmount(&lfs);
        }

        lfs_rambd_destroy(&cfg);
    }

    // test 3 (parallel reservations)

    {
        lfs_rambd_create(&cfg);

        const char *names[FILES] = { "borea", "miani", "nistia", "rosilio", "stalli" };
        lfs_file_t files[FILES];
        lfs_file_t file;
        int err;

        lfs_format(&lfs, &cfg);
        lfs_mount(&lfs, &cfg);

        lfs_ssize_t nblocks = (cfg.block_count - 2 - FILES - 1) / (FILES + 1);
        assert(nblocks > 0);
        lfs_ssize_t nbtest = cfg.block_count - (nblocks * FILES) + 1;
        assert(nbtest > 0);
        assert((nblocks * FILES) + nbtest > (lfs_ssize_t)cfg.block_count);

        for (int n = 0; n < FILES; n++) {
            lfs_file_open(&lfs, &files[n], names[n], LFS_O_WRONLY | LFS_O_CREAT);
        }
        for (int n = 0; n < FILES; n++) {
            int err = lfs_file_reserve(&lfs, &files[n], nblocks * cfg.block_size, 0);
            assert(err == LFS_ERR_OK);
        }

        assert(lfs_fs_size(&lfs) >= nblocks * FILES);
        lfs_file_open(&lfs, &file, "test", LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &file, nbtest * cfg.block_size, 0);
        assert(err == LFS_ERR_NOSPC); // there should be no space now
        lfs_file_close(&lfs, &file);

        for (int n = 0; n < FILES; n++) {
            lfs_file_close(&lfs, &files[n]);
        }

        assert(lfs_fs_size(&lfs) >= nblocks * FILES);
        lfs_file_open(&lfs, &file, "test", LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &file, nbtest * cfg.block_size, 0);
        assert(err == LFS_ERR_NOSPC); // there should be no space now
        lfs_file_close(&lfs, &file);

        lfs_unmount(&lfs);

        lfs_rambd_destroy(&cfg);
    }

    // test 4 (repeated reservations)

    {
        lfs_rambd_create(&cfg);

        const char *names[FILES] = { "koizun", "nenxing", "qailo", "shengwo", "yinpiang" };
        lfs_file_t files[FILES];
        int err;

        lfs_format(&lfs, &cfg);
        lfs_mount(&lfs, &cfg);
        assert(lfs.version == LFS_DISK_VERSION_BASE);

        lfs_ssize_t szhalf = (cfg.block_count - 8) / 2;
        assert(szhalf > 0);
        lfs_ssize_t szeighth = (cfg.block_count - 8) / 8;
        assert(szeighth > 0);

        // big half file can definitely be allocated now
        lfs_file_open(&lfs, &files[0], names[0], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &files[0], cfg.block_size * szhalf, 0);
        assert(lfs.version == LFS_DISK_VERSION_FLAT);
        assert(err == LFS_ERR_OK);
        lfs_file_close(&lfs, &files[0]);

        // one eight definitely as well
        lfs_file_open(&lfs, &files[1], names[1], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &files[1], cfg.block_size * szeighth, 0);
        assert(err == LFS_ERR_OK);
        lfs_file_close(&lfs, &files[1]);

        lfs_unmount(&lfs);
        lfs_mount(&lfs, &cfg);
        assert(lfs.version == LFS_DISK_VERSION_FLAT);

        // free one half
        lfs_file_open(&lfs, &files[0], names[0], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &files[0], 0, 0);
        assert(err == LFS_ERR_OK);
        // attempting to reserve again should fail since this space is still committed
        err = lfs_file_reserve(&lfs, &files[0], cfg.block_size * szhalf, 0);
        assert(err == LFS_ERR_NOSPC);
        lfs_file_close(&lfs, &files[0]);

        // allocate one half twice
        lfs_file_open(&lfs, &files[0], names[0], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &files[0], cfg.block_size * szhalf, 0);
        assert(err == LFS_ERR_OK); // must succeed because the free was committed
        err = lfs_file_reserve(&lfs, &files[0], cfg.block_size * szhalf, 0);
        assert(err == LFS_ERR_OK); // must still succeed because we discarded the previous
        lfs_file_close(&lfs, &files[0]);

        // overallocate
        lfs_file_open(&lfs, &files[2], names[2], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &files[2], cfg.block_size * szhalf, 0);
        assert(err == LFS_ERR_NOSPC); // this should fail
        lfs_file_close(&lfs, &files[2]);

        // free one eighth
        lfs_file_open(&lfs, &files[1], names[1], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &files[1], 0, 0);
        assert(err == LFS_ERR_OK);
        lfs_file_close(&lfs, &files[1]);

        // allocate one eighth
        lfs_file_open(&lfs, &files[3], names[3], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &files[3], cfg.block_size * szeighth, 0);
        assert(err == LFS_ERR_OK); // we should have the space since we just freed it
        lfs_file_close(&lfs, &files[3]);

        // allocate again after uncommitted free
        lfs_file_open(&lfs, &files[0], names[0], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &files[0], 0, 0);
        assert(err == LFS_ERR_OK);
        err = lfs_file_reserve(&lfs, &files[0], cfg.block_size * szhalf, 0);
        assert(err == LFS_ERR_NOSPC); // must fail because we did not commit the free
        lfs_file_close(&lfs, &files[0]);

        // the last free should have been committed now, test flags
        lfs_file_open(&lfs, &files[0], names[0], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &files[0], cfg.block_size * szhalf, 0);
        assert(err == LFS_ERR_OK); // we should have the space
        // flag error
        err = lfs_file_reserve(&lfs, &files[0], cfg.block_size * szhalf, LFS_R_ERRED);
        assert(err == LFS_ERR_OK); // this should not give any error
        lfs_file_close(&lfs, &files[0]);

        // the error flag should cause the last commit not to be applied
        lfs_file_open(&lfs, &files[0], names[0], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &files[0], cfg.block_size * szhalf, 0);
        assert(err == LFS_ERR_OK); // we should have the space
        lfs_file_close(&lfs, &files[0]);

        // reserve again the same file
        lfs_file_open(&lfs, &files[0], names[0], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &files[0], cfg.block_size * szhalf, 0);
        assert(err == LFS_ERR_NOSPC); // this should fail since the space is still committed
        lfs_file_close(&lfs, &files[0]);

        // truncate
        lfs_file_open(&lfs, &files[0], names[0], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_truncate(&lfs, &files[0], cfg.block_size * szeighth);
        assert(err == LFS_ERR_OK);
        lfs_file_close(&lfs, &files[0]);

        // grow and shrink
        lfs_file_open(&lfs, &files[0], names[0], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_truncate(&lfs, &files[0], cfg.block_size * szhalf);
        assert(err == LFS_ERR_OK);
        err = lfs_file_truncate(&lfs, &files[0], cfg.block_size * szeighth);
        assert(err == LFS_ERR_OK);
        lfs_file_close(&lfs, &files[0]);

        // allocate two eighth
        lfs_file_open(&lfs, &files[4], names[4], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &files[4], cfg.block_size * szeighth * 2, 0);
        assert(err == LFS_ERR_OK); // we should have the space since we just freed it
        lfs_file_close(&lfs, &files[4]);

        // allocate one eighth, dont check
        lfs_file_open(&lfs, &files[1], names[1], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &files[1], cfg.block_size * szeighth, 0);
        lfs_file_close(&lfs, &files[1]);

        // allocate one eighth, dont check
        lfs_file_open(&lfs, &files[2], names[2], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_reserve(&lfs, &files[2], cfg.block_size * szeighth, 0);
        lfs_file_close(&lfs, &files[2]);

        // attempt to grow should definitely fail
        lfs_file_open(&lfs, &files[0], names[0], LFS_O_WRONLY | LFS_O_CREAT);
        err = lfs_file_truncate(&lfs, &files[0], cfg.block_size * szhalf);
        assert(err == LFS_ERR_NOSPC);
        lfs_file_close(&lfs, &files[0]);

        lfs_unmount(&lfs);

        lfs_rambd_destroy(&cfg);
    }
}

#endif
