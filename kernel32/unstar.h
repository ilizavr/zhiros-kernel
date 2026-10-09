#define TAR_SECTOR_SIZE 512

struct ustar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} __attribute__((packed));

struct unstar_file_info {
    u32 header_lba;
    u32 data_lba;
    u32 file_size;
};

static u32 parse_octal(const char *str, size_t size) {
    u32 val = 0;
    for (size_t i = 0; i < size; i++) {
        char c = str[i];
        if (c == '\0' || c == ' ') {
            break;
        }
        if (c >= '0' && c <= '7') {
            val = (val << 3) | (c - '0');
        }
    }
    return val;
}

u32 getsize_unstar(struct file* file)
{
    if (!file) {
        return 0;
    }
    if (!file->other_info) {
        return 0;
    }
    struct unstar_file_info* info = (struct unstar_file_info*)file->other_info;
    return info->file_size;
}

u32 read_unstar(struct file* file, void* buffer, u32 size, u32 offset)
{
    if (!file || !file->other_info || !buffer) {
        return 0;
    }

    struct unstar_file_info* info = (struct unstar_file_info*)file->other_info;

    if (offset >= info->file_size) {
        return 0;
    }

    if (offset + size > info->file_size) {
        u32 old_size = size;
        size = info->file_size - offset;
    }

    u32 bytes_read = 0;
    u8 sector_buf[TAR_SECTOR_SIZE];

    u32 start_sector = info->data_lba + (offset / TAR_SECTOR_SIZE);
    u32 sector_offset = offset % TAR_SECTOR_SIZE;
    u32 remaining = size;
    u32 current_sector = start_sector;


    while (remaining > 0) {
        if (!file->dsk->lba_read(file->dsk, current_sector, (char*)sector_buf, 1)) {
            break;
        }

        u32 chunk = TAR_SECTOR_SIZE - sector_offset;
        if (chunk > remaining) {
            chunk = remaining;
        }

        memcpy((char*)buffer + bytes_read, sector_buf + sector_offset, chunk);

        bytes_read += chunk;
        remaining -= chunk;
        sector_offset = 0;
        current_sector++;
    }

    return bytes_read;
}

bool close_unstar()
{
    return true;
}

struct file* open_unstar(struct disk* dsk, char *path)
{
    if (!dsk || !path) {
        return 0;
    }

    while (*path == '/') {
        path++;
    }

    u32 current_lba = 0;
    u8 buf[TAR_SECTOR_SIZE];

    while (current_lba < dsk->size) {
        if (!dsk->lba_read(dsk, current_lba, (char*)buf, 1)) {
            return 0;
        }

        struct ustar_header* header = (struct ustar_header*)buf;

        if (header->name[0] == '\0') {
            break;
        }

        u32 file_size = parse_octal(header->size, sizeof(header->size));

        char full_path[256];
        full_path[0] = '\0';

        if (header->prefix[0] != '\0') {
            size_t p_len = strnlen(header->prefix, sizeof(header->prefix));
            memcpy(full_path, header->prefix, p_len);
            full_path[p_len] = '/';
            full_path[p_len + 1] = '\0';
        }

        size_t n_len = strnlen(header->name, sizeof(header->name));
        strncat(full_path, header->name, n_len);


        if (strcmp(full_path, path) == 0) {

            struct file* nf = kalloc(sizeof(struct file));
            struct unstar_file_info* info = kalloc(sizeof(struct unstar_file_info));

            if (!nf || !info) {
                return 0;
            }

            info->header_lba = current_lba;
            info->data_lba = current_lba + 1;
            info->file_size = file_size;

            nf->dsk = dsk;
            nf->read = read_unstar;
            nf->getsize = getsize_unstar;
            nf->write = 0;
            nf->close = (void*)close_unstar;
            nf->is_dir = (header->typeflag == '5');
            nf->path = path;
            nf->other_info = info;

            return nf;
        }

        u32 data_sectors = (file_size + TAR_SECTOR_SIZE - 1) / TAR_SECTOR_SIZE;
        current_lba += 1 + data_sectors;
    }

    return 0;
}
