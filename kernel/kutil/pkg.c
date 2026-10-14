#include <dogeio.h>
#include <system.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define REPO_DIR "/apps/pkgs/"
#define APPS_DIR "/apps/"
#define MAX_PATH_LEN 256
#define MAX_NAMES 64

int kernel_package(int argc, char **argv) {
    if (argc < 2) {
        dogeio_text_println("[PKG] Error: Command missing. Options: install, uninstall, list");
        return -1;
    }

    if (str_strcmp(argv[1], "install") == 0) {
        if (argc < 3) {
            dogeio_text_println("[PKG] Usage: pkg install <package_name>");
            return -1;
        }
        char *pkg_name = argv[2];

        char src_path[MAX_PATH_LEN];
        str_strcpy(src_path, REPO_DIR);
        str_strcat(src_path, pkg_name);
        str_strcat(src_path, ".rle");

        char dst_path[MAX_PATH_LEN];
        str_strcpy(dst_path, APPS_DIR);
        str_strcat(dst_path, pkg_name);

        uint64_t src_size = 0;
        bool is_dir = false;
        if (fs_get_info(src_path, &src_size, &is_dir) != 0 || src_size == 0) {
            dogeio_text_println("[PKG] Error: Package source file (.rle) does not exist in /apps/pkgs.");
            return -1;
        }

        dogeio_text_println("[PKG] Decompressing package payload to /apps...");
        kernel_rle_decompress(src_path, src_size, dst_path);
        dogeio_text_println("[PKG] Package installed successfully.");
    } 
    else if (str_strcmp(argv[1], "uninstall") == 0) {
        if (argc < 3) {
            dogeio_text_println("[PKG] Usage: pkg uninstall <package_name>");
            return -1;
        }
        char *pkg_name = argv[2];

        char dst_path[MAX_PATH_LEN];
        str_strcpy(dst_path, APPS_DIR);
        str_strcat(dst_path, pkg_name);

        if (!fs_exists(dst_path)) {
            dogeio_text_println("[PKG] Error: Package is not installed in /apps.");
            return -1;
        }

        if (fs_delete(dst_path) == 0) {
            dogeio_text_println("[PKG] Deleted executable application from /apps.");
            dogeio_text_println("[PKG] Package uninstalled cleanly.");
        } else {
            dogeio_text_println("[PKG] Error: Could not delete application file.");
            return -1;
        }
    } 
    else if (str_strcmp(argv[1], "list") == 0) {
        static char file_names[MAX_NAMES][256];
        memset(file_names, 0, sizeof(file_names));

        int count = fs_list_files(APPS_DIR, file_names, MAX_NAMES);
        if (count <= 0) {
            dogeio_text_println("[PKG] No applications currently installed in /apps.");
            return 0;
        }

        dogeio_text_println("--- Installed Packages ---");
        for (int i = 0; i < count; i++) {
            if (str_strcmp(file_names[i], "pkgs") == 0) {
                continue;
            }
            if (str_strlen(file_names[i]) > 0) {
                dogeio_text_print("-> ");
                dogeio_text_println(file_names[i]);
            }
        }
    } 
    else {
        dogeio_text_println("[PKG] Error: Unknown command. Use 'install', 'uninstall', or 'list'.");
        return -1;
    }
    return 0;
}
