#include <dogeio.h>
#include <system.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define REPO_DIR "/apps/packages/"
#define APPS_DIR "/apps/"
#define MAX_PATH_LEN 256
#define MAX_NAMES 64

int kernel_package(int argc, char **argv) {
    if (argc < 1 || str_strcmp(argv[0], "help") == 0 || str_strcmp(argv[0], "-h") == 0 || str_strcmp(argv[0], "--help") == 0) {
        dogeio_text_println("--- Dogestore Package Manager Help ---");
        dogeio_text_println("Usage: dogestore <command> [arguments]");
        dogeio_text_println("");
        dogeio_text_println("Commands:");
        dogeio_text_println("  install <pkg>    Decompress and install an .rle package from /apps/packages/");
        dogeio_text_println("  uninstall <pkg>  Remove an installed package executable from /apps/");
        dogeio_text_println("  list             Show uninstalled packages available for installation");
        dogeio_text_println("  search <query>   Search for installable packages in the repository");
        dogeio_text_println("  help             Display this help menu screen");
        return 0;
    }

    if (str_strcmp(argv[0], "install") == 0) {
        if (argc < 2) {
            dogeio_text_println("[PKG] Error: Missing package name.");
            dogeio_text_println("Usage: dogestore install <package_name>");
            return -1;
        }
        char *pkg_name = argv[1];

        if ((str_strlen(REPO_DIR) + str_strlen(pkg_name) + 5) >= MAX_PATH_LEN ||
            (str_strlen(APPS_DIR) + str_strlen(pkg_name) + 4) >= MAX_PATH_LEN) {
            dogeio_text_println("[PKG] Error: Package name is too long.");
            return -1;
        }

        char src_path[MAX_PATH_LEN];
        str_strcpy(src_path, REPO_DIR);
        str_strcat(src_path, pkg_name);
        str_strcat(src_path, ".rle");

        char dst_path[MAX_PATH_LEN];
        str_strcpy(dst_path, APPS_DIR);
        str_strcat(dst_path, pkg_name);
        str_strcat(dst_path, ".bin");

        uint64_t src_size = 0;
        bool is_dir = false;
        if (fs_get_info(src_path, &src_size, &is_dir) != 0 || src_size == 0) {
            dogeio_text_println("[PKG] Error: Package source file (.rle) does not exist in /apps/packages/.");
            return -1;
        }

        dogeio_text_println("[PKG] Decompressing package payload to /apps...");
        kernel_rle_decompress(src_path, src_size, dst_path);
        dogeio_text_println("[PKG] Package installed successfully.");
    } 
    else if (str_strcmp(argv[0], "uninstall") == 0) {
        if (argc < 2) {
            dogeio_text_println("[PKG] Error: Missing package name.");
            dogeio_text_println("Usage: dogestore uninstall <package_name>");
            return -1;
        }
        char *pkg_name = argv[1];

        if ((str_strlen(REPO_DIR) + str_strlen(pkg_name) + 5) >= MAX_PATH_LEN ||
            (str_strlen(APPS_DIR) + str_strlen(pkg_name) + 4) >= MAX_PATH_LEN) {
            dogeio_text_println("[PKG] Error: Package name is too long.");
            return -1;
        }

        char src_path[MAX_PATH_LEN];
        str_strcpy(src_path, REPO_DIR);
        str_strcat(src_path, pkg_name);
        str_strcat(src_path, ".rle");

        char dst_path[MAX_PATH_LEN];
        str_strcpy(dst_path, APPS_DIR);
        str_strcat(dst_path, pkg_name);
        str_strcat(dst_path, ".bin");

        uint64_t src_size = 0;
        bool is_dir = false;
        if (fs_get_info(src_path, &src_size, &is_dir) != 0 || src_size == 0) {
            dogeio_text_println("[PKG] Error: Cannot uninstall. Matching source file (.rle) missing from /apps/packages/.");
            return -1;
        }

        if (!fs_exists(dst_path)) {
            dogeio_text_println("[PKG] Error: Package is not installed in /apps.");
            return -1;
        }

        if (fs_delete(dst_path)) {
            dogeio_text_println("[PKG] Deleted executable application from /apps.");
            dogeio_text_println("[PKG] Package uninstalled cleanly.");
        } else {
            dogeio_text_println("[PKG] Error: Could not delete application file.");
            return -1;
        }
    } 
    else if (str_strcmp(argv[0], "list") == 0) {
        static char rle_files[MAX_NAMES][256];
        memset(rle_files, 0, sizeof(rle_files));

        int rle_count = fs_list_files(REPO_DIR, rle_files, MAX_NAMES);

        dogeio_text_println("--- Available Packages (Uninstalled) ---");
        if (rle_count <= 0) {
            dogeio_text_println("No source packages found in /apps/packages/.");
            return 0;
        }

        int shown_count = 0;
        for (int i = 0; i < rle_count; i++) {
            size_t len = str_strlen(rle_files[i]);
            if (len <= 4) continue;

            if (str_strcmp(&rle_files[i][len - 4], ".rle") == 0) {
                char clean_name[MAX_PATH_LEN];
                memset(clean_name, 0, sizeof(clean_name));
                
                for (size_t j = 0; j < len - 4 && j < (MAX_PATH_LEN - 1); j++) {
                    clean_name[j] = rle_files[i][j];
                }

                char check_bin[MAX_PATH_LEN];
                str_strcpy(check_bin, APPS_DIR);
                str_strcat(check_bin, clean_name);
                str_strcat(check_bin, ".bin");

                if (!fs_exists(check_bin)) {
                    dogeio_text_print("-> ");
                    dogeio_text_println(clean_name);
                    shown_count++;
                }
            }
        }
        if (shown_count == 0) {
            dogeio_text_println("All repository packages are currently installed.");
        }
    }
    else if (str_strcmp(argv[0], "search") == 0) {
        if (argc < 2) {
            dogeio_text_println("[PKG] Error: Missing search term.");
            dogeio_text_println("Usage: dogestore search <query>");
            return -1;
        }
        char *search_term = argv[1];
        size_t search_len = str_strlen(search_term);

        static char rle_files[MAX_NAMES][256];
        memset(rle_files, 0, sizeof(rle_files));

        int rle_count = fs_list_files(REPO_DIR, rle_files, MAX_NAMES);

        dogeio_text_print("--- Search Results for '");
        dogeio_text_print(search_term);
        dogeio_text_println("' ---");

        if (rle_count <= 0) {
            dogeio_text_println("No packages found in repository.");
            return 0;
        }

        int found_count = 0;
        for (int i = 0; i < rle_count; i++) {
            size_t len = str_strlen(rle_files[i]);
            if (len <= 4) continue;

            if (str_strcmp(&rle_files[i][len - 4], ".rle") == 0) {
                char clean_name[MAX_PATH_LEN];
                memset(clean_name, 0, sizeof(clean_name));
                
                for (size_t j = 0; j < len - 4 && j < (MAX_PATH_LEN - 1); j++) {
                    clean_name[j] = rle_files[i][j];
                }

                size_t clean_len = str_strlen(clean_name);
                bool match_found = false;

                if (search_len <= clean_len) {
                    for (size_t n = 0; n <= clean_len - search_len; n++) {
                        bool local_match = true;
                        for (size_t m = 0; m < search_len; m++) {
                            if (clean_name[n + m] != search_term[m]) {
                                local_match = false;
                                break;
                            }
                        }
                        if (local_match) {
                            match_found = true;
                            break;
                        }
                    }
                }

                if (match_found) {
                    char check_bin[MAX_PATH_LEN];
                    str_strcpy(check_bin, APPS_DIR);
                    str_strcat(check_bin, clean_name);
                    str_strcat(check_bin, ".bin");

                    dogeio_text_print("-> ");
                    dogeio_text_print(clean_name);
                    if (fs_exists(check_bin)) {
                        dogeio_text_println(" [Installed]");
                    } else {
                        dogeio_text_println(" [Available]");
                    }
                    found_count++;
                }
            }
        }
        if (found_count == 0) {
            dogeio_text_println("No matching packages found.");
        }
    }
    else {
        dogeio_text_print("[PKG] Error: Unknown command '");
        dogeio_text_print(argv[0]);
        dogeio_text_println("'. Type 'dogestore help' for instructions.");
        return -1;
    }
    return 0;
}
