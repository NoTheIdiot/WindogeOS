#include "../library/stddef.h"
#include "../library/stdint.h"
#include "../library/dogeio.h"
#include "../library/string.h"

#define MAX_SIZE 8192

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

static int text_equal(const char *left, const char *right) {
    if (!left || !right) return 0;
    while (*left != '\0' && *right != '\0' && *left == *right) {
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

uint32_t calculate_crc32(const uint8_t *data, size_t length) {
    if (!data) return 0;
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

/* Explicitly initialized static buffers in .data to avoid .bss loader faults */
static uint8_t file_data[MAX_SIZE] __attribute__((section(".data.zip"))) = {0};
static uint8_t zip_buf[MAX_SIZE] __attribute__((section(".data.zip"))) = {0};

size_t compress_stored(const char *filename, const char *output) {
    if (!filename || !output) return (size_t)-1;

    dogec_stat_t st;
    memset(&st, 0, sizeof(st));
    if (stat(filename, &st) != 0 || !st.exists || st.is_dir) {
        return (size_t)-1;
    }
    size_t file_size = (size_t)st.size;
    if (file_size > MAX_SIZE) {
        return (size_t)-1;
    }

    read_file(filename, (char *)file_data, file_size);
    uint32_t crc = calculate_crc32(file_data, file_size);

    uint8_t *ptr = zip_buf;
    uint16_t name_len = (uint16_t)str_strlen(filename);
    uint32_t truncated_size = (uint32_t)file_size;

    uint32_t local_header_offset = (uint32_t)(ptr - zip_buf);
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

    memcpy(ptr, filename, name_len);
    ptr += name_len;

    memcpy(ptr, file_data, file_size);
    ptr += file_size;

    uint32_t central_dir_offset = (uint32_t)(ptr - zip_buf);
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

    memcpy(ptr, filename, name_len);
    ptr += name_len;
    uint32_t central_dir_size = (uint32_t)(ptr - zip_buf) - central_dir_offset;

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

    create_file(output);
    write_file(output, (const char *)zip_buf);

    return (size_t)(ptr - zip_buf);
}

int decompress_stored(const char *filename, const char *output) {
    if (!filename || !output) return -1;

    dogec_stat_t st;
    memset(&st, 0, sizeof(st));
    if (stat(filename, &st) != 0 || !st.exists) {
        return -1;
    }
    size_t zip_size = (size_t)st.size;
    if (zip_size > MAX_SIZE) {
        return -1;
    }

    read_file(filename, (char *)zip_buf, zip_size);
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

    create_file(output);
    write_file(output, (const char *)payload_ptr);
    
    return 0; 
}

void _start(int argc, char **argv) {
    int status = 0;

    if (argc == 1 && argv != NULL && argv[0] != NULL) {
        if (text_equal(argv[0], "--help")) {
            println("Usage: zip <file> <output.zip>");
            println("Usage: zip <input.zip> <output>");
            sys_exit(0);
            while (1) {}
        }
        if (text_equal(argv[0], "--version")) {
            println("WindogeOS zip 1.0");
            sys_exit(0);
            while (1) {}
        }
        if (text_equal(argv[0], "--please")) {
            println("Be a little patient sir");
            sys_exit(0);
            while (1) {}
        }
    }

    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[0][0] == '\0' || argv[1][0] == '\0') {
        println("Usage: zip <file> <output.zip>");
        println("Usage: zip <input.zip> <output>");
        sys_exit(2);
        while (1) {}
    }

    const char *input = argv[0];
    const char *output = argv[1];
    
    size_t len = str_strlen(output);
    int is_zip_out = (len > 4 && text_equal(output + len - 4, ".zip"));
    
    if (is_zip_out) {
        size_t res = compress_stored(input, output);
        if (res != (size_t)-1) {
            println("Compressed successfully!");
            status = 0;
        } else {
            println("Compression failed!");
            status = 1;
        }
    } else {
        int res = decompress_stored(input, output);
        if (res == 0) {
            println("Decompressed successfully!");
            status = 0;
        } else {
            println("Decompression failed!");
            status = 1;
        }
    }

    sys_exit((uint64_t)status);
    while (1) {}
}