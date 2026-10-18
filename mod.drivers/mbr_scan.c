#include "../lib/zhirtypes.h"
#include "../lib/string.h"
#include "../lib/zhirapi.h"

#pragma pack(push, 1) //disable padding

struct mbr_part
{
    u8 boot_flag;
    u8 start_chs[3];
    u8 sys_id;
    u8 end_chs[3];
    u32 start_lba;
    u32 total_sectors;
};

struct other_info
{
    u32 start_lba;
    struct disk* parent_disk;
};

struct mbr
{
    u8 bootstrap[446];
    struct mbr_part partitions[4];
    u16 signature;
};

static bool mbr_lba_read(struct disk* dsk, u32 lba, char* buffer, u32 blocks)
{
    struct other_info* info = (struct other_info*)dsk->other_info;
    if (!info || !info->parent_disk) return false;

    return info->parent_disk->lba_read(info->parent_disk, lba + info->start_lba, buffer, blocks);
}

static bool mbr_lba_write(struct disk* dsk, u32 lba, char* buffer, u32 blocks)
{
    struct other_info* info = (struct other_info*)dsk->other_info;
    if (!info || !info->parent_disk) return false;

    return info->parent_disk->lba_write(info->parent_disk, lba + info->start_lba, buffer, blocks);
}

static u32 mbrscan(struct disk* d)
{
    if (!d || !d->lba_read) return 0;

    NEW(struct mbr, m);
    if (!m) return 0;

    if (!d->lba_read(d, 0, (char*)m, 1)) {
        free(m);
        return 0;
    }

    if (m->signature != 0xAA55) {
        free(m);
        return 0;
    }

    u32 partition_count = 0;

    for(int i = 0; i < 4; i++)
    {
        if(m->partitions[i].total_sectors == 0 || m->partitions[i].sys_id == 0x00)
            continue;

        NEW(struct disk, partdisk);
        NEW(struct other_info, partinfo);

        partinfo->start_lba = m->partitions[i].start_lba;
        partinfo->parent_disk = d;

        partdisk->other_info = partinfo;
        partdisk->lba_read = mbr_lba_read;
        partdisk->lba_write = mbr_lba_write;
        partdisk->size = m->partitions[i].total_sectors;
        partdisk->name = "mbr";

        d->partitions[i] = partdisk;
        mbrscan(partdisk);

        partition_count++;
    }

    free(m);

    return partition_count;
}

INIT void init(void* (*_resolve_function)(char* name))
{
    api_init(_resolve_function);

    register_function("_mbrscan", mbrscan, "_mbrscan(struct disk*) -> u32 partitioncount");
}
