#include "../library/stddef.h"
#include "../library/stdint.h"
#include "../library/dogeio.h"
#include "../library/string.h"

#define MAX_SIZE 262144

#pragma pack(push, 1)

typedef struct {
    uint32_t signature;        
    uint16_t version_needed;   
    uint16_t flags;            
    uint16_t comp_method;      
    uint16_t mod_time;         
    uint16_t mod_date;         
    uint32_t crc32;            
    uint32_t comp_size;        
    uint32_t uncomp_size;      
    uint16_t filename_len;     
    uint16_t extra_len;        
} LocalHeader;

typedef struct {
    uint32_t signature;        
    uint16_t version_made;     
    uint16_t version_needed;   
    uint16_t flags;
    uint16_t comp_method;
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t comp_size;
    uint32_t uncomp_size;
    uint16_t filename_len;
    uint16_t extra_len;
    uint16_t comment_len;
    uint16_t disk_start;
    uint16_t internal_attr;
    uint32_t external_attr;
    uint32_t local_header_off; 
} CentralDirHeader;

typedef struct {
    uint32_t signature;        
    uint16_t disk_num;
    uint16_t cd_disk_num;
    uint16_t disk_entries;     
    uint16_t total_entries;    
    uint32_t cd_size;          
    uint32_t cd_offset;        
    uint16_t comment_len;
} EOCD;

#pragma pack(pop)

static size_t private_strlen(const char *str) {
    const char *s = str;
    while (*s) s++;
    return (size_t)(s - str);
}

static void private_memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
}

uint32_t calculate_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

size_t compress_stored(const char *filename, char* output, size_t file_size) {
    if (!file_exists(filename)) {
        return -(size_t)1;
    }
    uint8_t file_data[MAX_SIZE];
    read_file_raw(filename, file_data, file_size);
    uint8_t *ptr = file_data;
    uint16_t name_len = (uint16_t)private_strlen(filename);
    uint32_t crc = calculate_crc32(file_data, file_size);
    uint32_t truncated_size = (uint32_t)file_size;

    uint32_t local_header_offset = (uint32_t)(ptr - file_data);
    LocalHeader *lh = (LocalHeader *)ptr;
    lh->signature = 0x04034B50;
    lh->version_needed = 10;
    lh->flags = 0;
    lh->comp_method = 0; 
    lh->mod_time = 0x4621;
    lh->mod_date = 0x5894;
    lh->crc32 = crc;
    lh->comp_size = truncated_size;
    lh->uncomp_size = truncated_size;
    lh->filename_len = name_len;
    lh->extra_len = 0;
    ptr += sizeof(LocalHeader);

    private_memcpy(ptr, filename, name_len);
    ptr += name_len;
    private_memcpy(ptr, file_data, file_size);
    ptr += file_size;

    uint32_t central_dir_offset = (uint32_t)(ptr - file_data);
    CentralDirHeader *cd = (CentralDirHeader *)ptr;
    cd->signature = 0x02014B50;
    cd->version_made = 20;
    cd->version_needed = 10;
    cd->flags = 0;
    cd->comp_method = 0;
    cd->mod_time = 0x4621;
    cd->mod_date = 0x5894;
    cd->crc32 = crc;
    cd->comp_size = truncated_size;
    cd->uncomp_size = truncated_size;
    cd->filename_len = name_len;
    cd->extra_len = 0;
    cd->comment_len = 0;
    cd->disk_start = 0;
    cd->internal_attr = 0;
    cd->external_attr = 0;
    cd->local_header_off = local_header_offset;
    ptr += sizeof(CentralDirHeader);

    private_memcpy(ptr, filename, name_len);
    ptr += name_len;
    uint32_t central_dir_size = (uint32_t)(ptr - file_data) - central_dir_offset;

    EOCD *eocd = (EOCD *)ptr;
    eocd->signature = 0x06054B50;
    eocd->disk_num = 0;
    eocd->cd_disk_num = 0;
    eocd->disk_entries = 1;
    eocd->total_entries = 1;
    eocd->cd_size = central_dir_size;
    eocd->cd_offset = central_dir_offset;
    eocd->comment_len = 0;
    ptr += sizeof(EOCD);
    write_file_raw(output, file_data, sizeof(file_data));

    return (size_t)(ptr - file_data);
}

int decompress_stored(char* filename, char* output, uint8_t *out_dest, size_t *out_size) {
    if (!file_exists(filename)) {
        return -1;
    }
    uint8_t zip_buf[MAX_SIZE];
    read_file_raw(filename, zip_buf, MAX_SIZE);
    const LocalHeader *lh = (const LocalHeader *)zip_buf;
    
    if (lh->signature != 0x04034B50) {
        return -1; 
    }
    if (lh->comp_method != 0) {
        return -2; 
    }

    const uint8_t *payload_ptr = zip_buf + sizeof(LocalHeader) + lh->filename_len + lh->extra_len;
    
    uint32_t actual_crc = calculate_crc32(payload_ptr, lh->comp_size);
    if (actual_crc != lh->crc32) {
        return -3; 
    }

    private_memcpy(out_dest, payload_ptr, lh->comp_size);
    *out_size = (size_t)lh->comp_size;
    create_file(output);
    write_file_raw(output, out_dest, sizeof(out_dest));
    
    return 0; 
}


void _start(int argc, char** argv) {
    int status = 0;
    if (argc == 1 && str_strcmp(argv[0], "--help") == 0) {
        println("Usage: zip <file> <output>");
        println("No zip yet, be patient");
        status = 0;
    } else if (argc == 1 && str_strcmp(argv[0], "--please") == 0) {
        println("Be a little patient sir");
        status = 0;
    } else if (argc != 0) {
        println("Usage: zip <file> <output>");
        status = 2;
    }
    sys_exit((uint64_t)status);
}