#include "../library/string.h"
#include "../library/dogeio.h"
#include "../library/bool.h"
#include "../library/stdint.h"
#include "../library/stddef.h"

#pragma pack(push, 1)

typedef struct {
    uint32_t signature;          
    uint16_t version_needed;
    uint16_t general_purpose_bit;
    uint16_t compression_method; 
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t file_name_length;
    uint16_t extra_field_length;
} zip_local_header_t;

typedef struct {
    uint32_t signature;          
    uint16_t version_made_by;
    uint16_t version_needed;
    uint16_t general_purpose_bit;
    uint16_t compression_method;
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t file_name_length;
    uint16_t extra_field_length;
    uint16_t file_comment_length;
    uint16_t disk_number_start;
    uint16_t internal_file_attr;
    uint32_t external_file_attr;
    uint32_t local_header_offset;
} zip_cd_header_t;

typedef struct {
    uint32_t signature;          
    uint16_t disk_number;
    uint16_t disk_with_cd;
    uint16_t disk_entries;
    uint16_t total_entries;
    uint32_t cd_size;
    uint32_t cd_offset;          
    uint16_t comment_length;
} zip_eocd_t;

#pragma pack(pop)

uint32_t calculate_crc32(const uint8_t* data, size_t length) {
    if (data == NULL || length == 0) return 0;
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

size_t zip_compress_store(const char* filename, uint32_t data_len) {
    if (filename == NULL || data_len == 0) {
        return 0;
    }
    // 16 KB of DATA
    uint8_t out_buf[16384];
    uint8_t* p = out_buf;
    uint8_t file_data[data_len]; 
    read_file_raw(filename, file_data, data_len);
    uint16_t name_len = (uint16_t)(str_strlen(filename));
    uint32_t file_crc = calculate_crc32(file_data, data_len);

    zip_local_header_t* local = (zip_local_header_t*)p;
    local->signature = 0x04034b50;
    local->version_needed = 10; 
    local->general_purpose_bit = 0;
    local->compression_method = 0; 
    local->last_mod_time = 0;
    local->last_mod_date = 0;
    local->crc32 = file_crc;
    local->compressed_size = data_len;
    local->uncompressed_size = data_len;
    local->file_name_length = name_len;
    local->extra_field_length = 0;
    p += sizeof(zip_local_header_t);

    memcpy(p, filename, name_len); p += name_len;
    memcpy(p, file_data, data_len); p += data_len;

    uint32_t cd_offset = (uint32_t)(p - out_buf);

    zip_cd_header_t* cd = (zip_cd_header_t*)p;
    cd->signature = 0x02014b50;
    cd->version_made_by = 45;       
    cd->version_needed = 10;
    cd->general_purpose_bit = 0;
    cd->compression_method = 0;
    cd->last_mod_time = 0;
    cd->last_mod_date = 0;
    cd->crc32 = file_crc;
    cd->compressed_size = data_len;
    cd->uncompressed_size = data_len;
    cd->file_name_length = name_len;
    cd->extra_field_length = 0;
    cd->file_comment_length = 0;
    cd->disk_number_start = 0;
    cd->internal_file_attr = 0;
    cd->external_file_attr = 0;
    cd->local_header_offset = 0; 
    p += sizeof(zip_cd_header_t);
    
    memcpy(p, filename, name_len); p += name_len;
    uint32_t cd_end = (uint32_t)(p - out_buf);

    zip_eocd_t* eocd = (zip_eocd_t*)p;
    eocd->signature = 0x06054b50;
    eocd->disk_number = 0;
    eocd->disk_with_cd = 0;
    eocd->disk_entries = 1;
    eocd->total_entries = 1;
    eocd->cd_size = cd_end - cd_offset;
    eocd->cd_offset = cd_offset;
    eocd->comment_length = 0;
    p += sizeof(zip_eocd_t);

    return (size_t)(p - out_buf);
}

int zip_decompress_file(const uint8_t* zip_data, size_t zip_size,
                        const char* target_filename, uint16_t target_name_len,
                        uint8_t* out_decompressed_buf, size_t* out_len) {
    if (zip_data == NULL || zip_size < sizeof(zip_eocd_t) || target_filename == NULL || out_len == NULL) {
        return -1; 
    }

    const zip_eocd_t* eocd = NULL;
    for (size_t i = zip_size - sizeof(zip_eocd_t); i > 0; i--) {
        if (*(const uint32_t*)(zip_data + i) == 0x06054b50) {
            eocd = (const zip_eocd_t*)(zip_data + i);
            break;
        }
    }
    if (!eocd) return -2; 

    const uint8_t* cd_ptr = zip_data + eocd->cd_offset;
    
    for (uint16_t entry = 0; entry < eocd->total_entries; entry++) {
        const zip_cd_header_t* cd_file = (const zip_cd_header_t*)cd_ptr;
        if (cd_file->signature != 0x02014b50) return -3; 

        const char* current_filename = (const char*)(cd_ptr + sizeof(zip_cd_header_t));

        if (cd_file->file_name_length == target_name_len && 
            str_strncmp(current_filename, target_filename, target_name_len) == 0) {
            
            const uint8_t* local_ptr = zip_data + cd_file->local_header_offset;
            const zip_local_header_t* local_file = (const zip_local_header_t*)local_ptr;
            if (local_file->signature != 0x04034b50) return -4; 

            const uint8_t* comp_data = local_ptr + sizeof(zip_local_header_t) 
                                       + local_file->file_name_length 
                                       + local_file->extra_field_length;

            *out_len = local_file->uncompressed_size;

            if (local_file->compression_method == 0) {
                if (out_decompressed_buf != NULL) {
                    memcpy(out_decompressed_buf, comp_data, local_file->uncompressed_size);
                    if (calculate_crc32(out_decompressed_buf, *out_len) != local_file->crc32) {
                        return -5; 
                    }
                }
                return 0; 
            } 
            else if (local_file->compression_method == 8) {
                return -6; 
            }
            return -7; 
        }

        cd_ptr += sizeof(zip_cd_header_t) + cd_file->file_name_length 
               + cd_file->extra_field_length + cd_file->file_comment_length;
    }

    return -8; 
}

void _start(int argc, char** argv) {
    if (argc < 1 || argv == (void*)0 || argv[0] == (void*)0) {
        print("Usage: zip <file>\n");
        sys_exit(1);
    } 
    
    if (str_strcmp(argv[0], "--help") == 0) {
        println("Zip thingy");
        print("Usage: zip <file>\n");
        sys_exit(0);
    }
    
    if (argc == 2 && str_strcmp(argv[0], "--help") != 0) {
        
    }
    sys_exit(0);
}
