#include <dogeio.h>
#include <system.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define REPO_DIR "C:/apps/packages/"
#define APPS_DIR "C:/apps/"
#define MAX_PATH_LEN 256
#define MAX_NAMES 64

int kernel_package(int argc, char **argv) {
    if (argc < 1 || str_strcmp(argv[0], "help") == 0 || str_strcmp(argv[0], "-h") == 0 || str_strcmp(argv[0], "--help") == 0) {
        dogeio_println("--- Dogestore Package Manager Help ---");
        dogeio_println("Usage: dogestore <command> [arguments]");
        dogeio_println("");
        dogeio_println("Commands:");
        dogeio_println("  install <pkg>     Decompress and install an .lz4 package or folder ending with .lz4 from /apps/packages/");
        dogeio_println("  uninstall <pkg>   Remove an installed package executable from /apps/");
        dogeio_println("  list              Show uninstalled packages available for installation");
        dogeio_println("  search <query>    Search for installable packages in the repository");
        dogeio_println("  help              Display this help menu screen");
        return 0;
    }

    if (str_strcmp(argv[0], "install") == 0) {
        char* old_dir = fs_dirname();
        if (argc < 2) {
            dogeio_println("[PKG] Error: Missing package name.");
            dogeio_println("Usage: dogestore install <package_name>");
            return -1;
        }
        char *pkg_name = argv[1];

        if ((str_strlen(REPO_DIR) + str_strlen(pkg_name) + 5) >= MAX_PATH_LEN ||
            (str_strlen(APPS_DIR) + str_strlen(pkg_name) + 5) >= MAX_PATH_LEN) {
            dogeio_println("[PKG] Error: Package name is too long.");
            return -1;
        }

        char pkg_path[MAX_PATH_LEN];
        str_strcpy(pkg_path, REPO_DIR);
        str_strcat(pkg_path, pkg_name);

        if (fs_isdir(pkg_path)) {
            size_t pkg_len = str_strlen(pkg_name);
            if (pkg_len <= 4 || str_strcmp(&pkg_name[pkg_len - 4], ".lz4") != 0) {
                dogeio_println("[PKG] Error: Folder name in repository must end with .lz4.");
                return -1;
            }

            char original_dir[MAX_PATH_LEN + 4];
            char *curr = fs_dirname();
            if (curr) {
                str_strcpy(original_dir, curr);
            } else {
                str_strcpy(original_dir, "/");
            }

            if (fs_chdir(pkg_path) != 0) {
                dogeio_println("[PKG] Error: Failed to enter package directory.");
                return -1;
            }

            char entries[MAX_NAMES][256];
            int count = fs_list_entries(".", entries, MAX_NAMES);
            if (count < 0) {
                (void)fs_chdir(original_dir);
                dogeio_println("[PKG] Error: Unable to list package folder.");
                return -1;
            }

            int installed_count = 0;
            int failed_count = 0;
            for (int i = 0; i < count; i++) {
                size_t entry_len = str_strlen(entries[i]);
                if (entry_len > 4 && str_strcmp(&entries[i][entry_len - 4], ".lz4") == 0) {
                    char file_clean_name[MAX_PATH_LEN];
                    memset(file_clean_name, 0, sizeof(file_clean_name));
                    for (size_t j = 0; j < entry_len - 4 && j < MAX_PATH_LEN - 1; j++) {
                        file_clean_name[j] = entries[i][j];
                    }

                    if (str_strlen(APPS_DIR) + str_strlen(file_clean_name) + 5 >=
                        MAX_PATH_LEN) {
                        dogeio_print("[PKG] Error: Package entry path is too long: ");
                        dogeio_println(file_clean_name);
                        failed_count++;
                        continue;
                    }

                    char sub_src_path[MAX_PATH_LEN];
                    str_strcpy(sub_src_path, entries[i]);

                    char sub_dst_path[MAX_PATH_LEN];
                    str_strcpy(sub_dst_path, APPS_DIR);
                    str_strcat(sub_dst_path, file_clean_name);
                    str_strcat(sub_dst_path, ".elf");

                    uint64_t sub_size = 0;
                    bool sub_is_dir = false;
                    if (fs_get_info(sub_src_path, &sub_size, &sub_is_dir) == 0 && !sub_is_dir && sub_size > 0) {
                        dogeio_print("[PKG] Installing sub-package payload: ");
                        dogeio_println(file_clean_name);
                        if (kernel_lz4_decompress(sub_src_path, sub_size,
                                                  sub_dst_path) == 0) {
                            installed_count++;
                        } else {
                            failed_count++;
                        }
                    } else {
                        dogeio_print("[PKG] Error: Invalid package entry: ");
                        dogeio_println(entries[i]);
                        failed_count++;
                    }
                }
            }

            if (fs_chdir(original_dir) != 0) {
                dogeio_println("[PKG] Error: Unable to restore working directory.");
                return -1;
            }

            if (installed_count == 0) {
                dogeio_println("[PKG] Warning: No valid .lz4 files found inside the package folder.");
                return -1;
            }
            if (failed_count == 0) {
                dogeio_println("[PKG] Folder package installed successfully.");
                return 0;
            }
            dogeio_println("[PKG] Error: One or more folder entries failed to install.");
            return -1;
        }

        char src_path[MAX_PATH_LEN];
        str_strcpy(src_path, REPO_DIR);
        str_strcat(src_path, pkg_name);
        str_strcat(src_path, ".lz4");

        char dst_path[MAX_PATH_LEN];
        str_strcpy(dst_path, APPS_DIR);
        str_strcat(dst_path, pkg_name);
        str_strcat(dst_path, ".elf");

        uint64_t src_size = 0;
        bool is_dir = false;
        
        if (fs_get_info(src_path, &src_size, &is_dir) != 0 || src_size == 0 || is_dir) {
            dogeio_println("[PKG] Error: Package source file (.lz4) does not exist in /apps/packages/.");
            return -1;
        }

        dogeio_println("[PKG] Decompressing package payload to /apps...");
        if (kernel_lz4_decompress(src_path, src_size, dst_path) != 0) {
            dogeio_println("[PKG] Error: Failed to install package payload.");
            return -1;
        }
        fs_chdir(old_dir);
        dogeio_println("[PKG] Package installed successfully.");
    } 
    else if (str_strcmp(argv[0], "uninstall") == 0) {
        if (argc < 2) {
            dogeio_println("[PKG] Error: Missing package name.");
            dogeio_println("Usage: dogestore uninstall <package_name>");
            return -1;
        }
        char *pkg_name = argv[1];

        if ((str_strlen(REPO_DIR) + str_strlen(pkg_name) + 5) >= MAX_PATH_LEN ||
            (str_strlen(APPS_DIR) + str_strlen(pkg_name) + 5) >= MAX_PATH_LEN) {
            dogeio_println("[PKG] Error: Package name is too long.");
            return -1;
        }

        char src_path[MAX_PATH_LEN];
        str_strcpy(src_path, REPO_DIR);
        str_strcat(src_path, pkg_name);
        str_strcat(src_path, ".lz4");

        char dst_path[MAX_PATH_LEN];
        str_strcpy(dst_path, APPS_DIR);
        str_strcat(dst_path, pkg_name);
        str_strcat(dst_path, ".elf");

        uint64_t src_size = 0;
        bool is_dir = false;
        if (fs_get_info(src_path, &src_size, &is_dir) != 0 || is_dir ||
            src_size == 0) {
            dogeio_println("[PKG] Error: Cannot uninstall. Matching source file (.lz4) missing from /apps/packages/.");
            return -1;
        }

        if (!fs_exists(dst_path)) {
            dogeio_println("[PKG] Error: Package is not installed in /apps.");
            return -1;
        }

        if (fs_delete(dst_path)) {
            dogeio_println("[PKG] Deleted executable application from /apps.");
            dogeio_println("[PKG] Package uninstalled cleanly.");
        } else {
            dogeio_println("[PKG] Error: Could not delete application file.");
            return -1;
        }
    } 
    else if (str_strcmp(argv[0], "list") == 0) {
        static char lz4_files[MAX_NAMES][256];
        memset(lz4_files, 0, sizeof(lz4_files));

        int lz4_count = fs_list_files(REPO_DIR, lz4_files, MAX_NAMES);

        dogeio_println("--- Available Packages (Uninstalled) ---");
        if (lz4_count < 0) {
            dogeio_println("[PKG] Error: Unable to list package repository.");
            return -1;
        }
        if (lz4_count == 0) {
            dogeio_println("No source packages found in /apps/packages/.");
            return 0;
        }

        int shown_count = 0;
        for (int i = 0; i < lz4_count; i++) {
            size_t len = str_strlen(lz4_files[i]);
            if (len <= 4) continue;

            if (str_strcmp(&lz4_files[i][len - 4], ".lz4") == 0) {
                char clean_name[MAX_PATH_LEN];
                memset(clean_name, 0, sizeof(clean_name));
                
                for (size_t j = 0; j < len - 4 && j < (MAX_PATH_LEN - 1); j++) {
                    clean_name[j] = lz4_files[i][j];
                }

                char check_elf[MAX_PATH_LEN];
                str_strcpy(check_elf, APPS_DIR);
                str_strcat(check_elf, clean_name);
                str_strcat(check_elf, ".elf");

                if (!fs_exists(check_elf)) {
                    dogeio_print("-> ");
                    dogeio_println(clean_name);
                    shown_count++;
                }
            }
        }
        if (shown_count == 0) {
            dogeio_println("All repository packages are currently installed.");
        }
    }
    else if (str_strcmp(argv[0], "search") == 0) {
        if (argc < 2) {
            dogeio_println("[PKG] Error: Missing search term.");
            dogeio_println("Usage: dogestore search <query>");
            return -1;
        }
        char *search_term = argv[1];
        size_t search_len = str_strlen(search_term);

        static char lz4_files[MAX_NAMES][256];
        memset(lz4_files, 0, sizeof(lz4_files));

        int lz4_count = fs_list_files(REPO_DIR, lz4_files, MAX_NAMES);

        dogeio_print("--- Search Results for '");
        dogeio_print(search_term);
        dogeio_println("' ---");

        if (lz4_count < 0) {
            dogeio_println("[PKG] Error: Unable to search package repository.");
            return -1;
        }
        if (lz4_count == 0) {
            dogeio_println("No packages found in repository.");
            return 0;
        }

        int found_count = 0;
        for (int i = 0; i < lz4_count; i++) {
            size_t len = str_strlen(lz4_files[i]);
            if (len <= 4) continue;

            if (str_strcmp(&lz4_files[i][len - 4], ".lz4") == 0) {
                char clean_name[MAX_PATH_LEN];
                memset(clean_name, 0, sizeof(clean_name));
                
                for (size_t j = 0; j < len - 4 && j < (MAX_PATH_LEN - 1); j++) {
                    clean_name[j] = lz4_files[i][j];
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
                    char check_elf[MAX_PATH_LEN];
                    str_strcpy(check_elf, APPS_DIR);
                    str_strcat(check_elf, clean_name);
                    str_strcat(check_elf, ".elf");

                    dogeio_print("-> ");
                    dogeio_print(clean_name);
                    if (fs_exists(check_elf)) {
                        dogeio_println(" [Installed]");
                    } else {
                        dogeio_println(" [Available]");
                    }
                    found_count++;
                }
            }
        }
        if (found_count == 0) {
            dogeio_println("No matching packages found.");
        }
    }
    else {
        dogeio_print("[PKG] Error: Unknown command '");
        dogeio_print(argv[0]);
        dogeio_println("'. Type 'dogestore help' for instructions.");
        return -1;
    }
    return 0;
}