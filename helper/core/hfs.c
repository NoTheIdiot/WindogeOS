#include <system.h>
#include <stdint.h>
#include <string.h>
#include <dogeio.h>
#include <basicutil.h>

int system_create_user(char* name, char* password) {
    fs_chdir("/system/pass");
    
    char passfile[128];
    str_strcpy(passfile, name);
    str_strcat(passfile, ".pass");
    fs_create(passfile);
    fs_write(passfile, password);
    
    fs_chdir("/");
    fs_chdir("users");
    fs_mkdir(name);
    
    fs_chdir("/");
    return 1;
}

int system_verify_user(const char* name, char* password) {
    fs_chdir("/system/pass");
    
    char passfile[128];
    str_strcpy(passfile, name);
    str_strcat(passfile, ".pass");
    
    char stored[256];
    for (int i = 0; i < 256; i++) {
        stored[i] = 0;
    }
    
    int bytes_read = fs_read(passfile, stored, sizeof(stored) - 1);
    fs_chdir("/");
    
    if (bytes_read < 0) {
        return 0;
    }
    stored[bytes_read] = '\0';
    return (str_strcmp(password, stored) == 0) ? 1 : 0;
}