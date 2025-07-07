#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <libbladeRF.h>

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#else
#include <curl/curl.h>
#endif

// Cross-platform file_exists function
bool file_exists(const char *filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

#include "sha256.h"

#include "bladeRF_json.h"

#define JSON_URL "https://nuand.com/versions.json"
#define BUFFER_SIZE 16384
#define MAX_VERSION_LENGTH 32
#define MAX_FILENAME_LENGTH 256
#define MAX_HASH_LENGTH 64
#define MAX_PATH_LENGTH 4096  // This should be sufficient for most Unix-like systems

#define MAX_SEARCH_PATHS 10

// Add a global variable for verbose output
bool verbose_output = false;

typedef struct {
    char **paths;
    int count;
} FPGASearchPaths;

// Function to check if a directory exists
static bool directory_exists(const char *path) {
#ifdef _WIN32
    DWORD attrib = GetFileAttributesA(path);
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

// Modified helper function to add a path if it exists
void add_path_if_exists(const char *path, FPGASearchPaths *search_paths)
{
    // check to make sure path is unique
    for (int i = 0; i < search_paths->count; i++) {
        if (strcmp(search_paths->paths[i], path) == 0) {
            return;
        }
    }
    if (directory_exists(path) && search_paths->count < MAX_SEARCH_PATHS) {
        search_paths->paths[search_paths->count] = strdup(path);
        search_paths->count++;
    }
}

// Helper function to create directory if it doesn't exist
bool create_directory_if_not_exists(const char *path) {
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return true;
    }
    return false;
#else
    if (mkdir(path, 0755) == 0 || errno == EEXIST) {
        return true;
    }
    return false;
#endif
}

// Function to get the preferred download directory
char* get_preferred_download_path() {
    static char download_path[MAX_PATH_LENGTH] = {0};

#ifdef _WIN32
    // Get the Roaming AppData folder
    char appdata_path[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata_path) != S_OK) {
        return NULL;
    }

    // Construct the Nuand directory path
    snprintf(download_path, sizeof(download_path), "%s\\Nuand", appdata_path);
    if (!create_directory_if_not_exists(download_path)) {
        return NULL;
    }

    // Construct the bladeRF directory path
    snprintf(download_path, sizeof(download_path), "%s\\Nuand\\bladeRF", appdata_path);
    if (!create_directory_if_not_exists(download_path)) {
        return NULL;
    }
#else
    // Unix-like systems (Linux, macOS)
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        } else {
            return NULL;
        }
    }

    // Construct ~/.config directory if it doesn't exist
    snprintf(download_path, sizeof(download_path), "%s/.config", home);
    if (!create_directory_if_not_exists(download_path)) {
        return NULL;
    }

    // Construct ~/.config/Nuand directory if it doesn't exist
    snprintf(download_path, sizeof(download_path), "%s/.config/Nuand", home);
    if (!create_directory_if_not_exists(download_path)) {
        return NULL;
    }

    // Construct ~/.config/Nuand/bladeRF directory if it doesn't exist
    snprintf(download_path, sizeof(download_path), "%s/.config/Nuand/bladeRF", home);
    if (!create_directory_if_not_exists(download_path)) {
        return NULL;
    }
#endif

    return download_path;
}

// Implementation of bladerf_get_fpga_paths
FPGASearchPaths* bladerf_get_fpga_paths(void) {
    FPGASearchPaths *search_paths = malloc(sizeof(FPGASearchPaths));
    if (!search_paths) {
        return NULL;
    }
    search_paths->paths = malloc(MAX_SEARCH_PATHS * sizeof(char*));
    if (!search_paths->paths) {
        free(search_paths);
        return NULL;
    }
    search_paths->count = 0;

    // First check preferred download paths
    char *preferred_path = get_preferred_download_path();
    if (preferred_path) {
        add_path_if_exists(preferred_path, search_paths);
    }

    // Check BLADERF_SEARCH_DIR environment variable
    char *env_path = getenv("BLADERF_SEARCH_DIR");
    if (env_path) {
        add_path_if_exists(env_path, search_paths);
    }

    // Check current directory
    char cwd[MAX_PATH_LENGTH];
    if (getcwd(cwd, sizeof(cwd))) {
        add_path_if_exists(cwd, search_paths);
    }

#ifdef _WIN32
    // Windows-specific paths
    char appdata_path[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata_path) == S_OK) {
        char nuand_path[MAX_PATH_LENGTH];
        snprintf(nuand_path, sizeof(nuand_path), "%s\\Nuand\\bladeRF", appdata_path);
        add_path_if_exists(nuand_path, search_paths);
    }

    // Check Windows installation directory from registry
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Nuand LLC", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char reg_path[MAX_PATH_LENGTH];
        DWORD dataSize = sizeof(reg_path);
        if (RegQueryValueExA(hKey, "Path", NULL, NULL, (LPBYTE)reg_path, &dataSize) == ERROR_SUCCESS) {
            add_path_if_exists(reg_path, search_paths);
        }
        RegCloseKey(hKey);
    }
#else
    // Unix-like systems (Linux, macOS, FreeBSD)
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }

    if (home) {
        char config_path[MAX_PATH_LENGTH];
        snprintf(config_path, sizeof(config_path), "%s/.config/Nuand/bladeRF", home);
        add_path_if_exists(config_path, search_paths);

        char nuand_path[MAX_PATH_LENGTH];
        snprintf(nuand_path, sizeof(nuand_path), "%s/.Nuand/bladeRF", home);
        add_path_if_exists(nuand_path, search_paths);
    }

    // Check system-wide directories
    add_path_if_exists("/etc/Nuand/bladeRF", search_paths);
    add_path_if_exists("/usr/share/Nuand/bladeRF", search_paths);
#endif

    return search_paths;
}

// Function to free the FPGASearchPaths structure
void free_fpga_search_paths(FPGASearchPaths *search_paths) {
    if (search_paths) {
        for (int i = 0; i < search_paths->count; i++) {
            free(search_paths->paths[i]);
        }
        free(search_paths->paths);
        free(search_paths);
    }
}

typedef struct {
    char version[MAX_VERSION_LENGTH];
    char filename[MAX_FILENAME_LENGTH];
    char md5[MAX_HASH_LENGTH+1];
    char sha256[MAX_HASH_LENGTH+1];
} VersionInfo;

// New structure to hold FPGA version information
typedef struct {
    char version[MAX_VERSION_LENGTH];
    VersionInfo *files;
    int num_files;
} FPGAVersionInfo;

// Modified UpdateInfo structure
typedef struct {
    VersionInfo firmware;
    FPGAVersionInfo *fpga_versions;
    int num_fpga_versions;
    char latest_fpga_version[MAX_VERSION_LENGTH]; // Add this line
    int latest_fpga_version_index;
    char latest_libbladerf_version[MAX_VERSION_LENGTH]; // Add this field for libbladeRF version
} UpdateInfo;

struct MemoryStruct {
    char *memory;
    size_t size;
};

// Function prototypes
bool download_json(const char* url, char* buffer, size_t buffer_size);
bool parse_json(const char* json_string, UpdateInfo* update_info);
bool check_fx3_version(const UpdateInfo* update_info, struct bladerf* dev);
bool check_fpga_version(const UpdateInfo* update_info, struct bladerf* dev);
bool download_file(const char* url, const char* filename);
bool verify_file(const char* filename, const char* expected_md5, const char* expected_sha256, const UpdateInfo* update_info);
bool update_fx3_device(struct bladerf* dev, const UpdateInfo* update_info);
bool check_rbf_files(const UpdateInfo* update_info);
void update_rbf_files(const UpdateInfo* update_info);
void update_fw_files(const UpdateInfo* update_info);
bool check_fw_files(const UpdateInfo* update_info);
bool check_libbladerf_version(const UpdateInfo* update_info);
bool check_and_load_bootloader(const UpdateInfo* update_info, struct bladerf** dev, bool auto_update);

bool remove_non_conforming = false;

void print_usage() {
    printf("Usage: bladeRF-update [OPTIONS]\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help     Display this help message\n");
    printf("  -y             Automatically update firmware and FPGA images\n");
    printf("  --rm           Remove non-conforming FPGA images during update\n");
    printf("  -v, --verbose  Display detailed output during operations\n");
    printf("\n");
    printf("Description:\n");
    printf("  bladeRF-update checks for and optionally installs updates for the bladeRF\n");
    printf("  firmware and FPGA images. Without options, it will only check for updates\n");
    printf("  and report the status, but won't make any changes.\n");
    printf("\n");
    printf("  Either -y or --rm is necessary to have bladeRF-update change anything.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  bladeRF-update         Check for updates without making changes\n");
    printf("  bladeRF-update -y      Automatically update all components\n");
    printf("  bladeRF-update --rm    Check and remove non-conforming FPGA images\n");
    printf("  bladeRF-update -y --rm Update all components and remove non-conforming images\n");
    printf("  bladeRF-update -v      Display detailed output during operations\n");
}

int main(int argc, char *argv[]) {
    printf("bladeRF-update utility will check for updates and optionally update firmware and FPGA images.\n");

    bool auto_update = false;
    bool fx3_up_to_date = false;
    bool fpga_up_to_date = false;
    bool need_to_update_bootloader = false;
    bool need_to_reset_device = false;
    bool outstanding_updates = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-y") == 0) {
            auto_update = true;
        } else if (strcmp(argv[i], "--rm") == 0) {
            remove_non_conforming = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose_output = true;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage();
            return 1;
        }
    }

    if (!auto_update && !remove_non_conforming) {
        printf("Warning: No action will be taken. Use -y to update or --rm to remove non-conforming files.\n");
    }

    printf("\n");

    char json_buffer[BUFFER_SIZE];
    if (!download_json(JSON_URL, json_buffer, BUFFER_SIZE)) {
        fprintf(stderr, "Failed to download version information\n");
        return 1;
    }

    UpdateInfo update_info;
    if (!parse_json(json_buffer, &update_info)) {
        fprintf(stderr, "Failed to parse version information\n");
        return 1;
    }

    // Check libbladeRF version
    bool libbladerf_up_to_date = check_libbladerf_version(&update_info);

    // Check RBF files regardless of device presence
    bool rbf_up_to_date;
    // Check FW files
    bool fw_up_to_date;

    if (auto_update) {
        printf("\n");
        update_rbf_files(&update_info);
        update_fw_files(&update_info);
    }

    printf("\n");
    rbf_up_to_date = check_rbf_files(&update_info);
    printf("\n");
    fw_up_to_date = check_fw_files(&update_info);
    printf("\n");

    if (!auto_update && (!rbf_up_to_date || !fw_up_to_date)) {
        printf("No updates performed. Run bladeRF-update -y to update files without a device connected.\n");
    }

    bool bladeRF_device_attached = false;
    struct bladerf *dev = NULL;
    int status = bladerf_open(&dev, NULL);
    if (status != 0) {
        printf("[?] No bladeRF device found initially. Checking for bootloaders...\n");

        if (check_and_load_bootloader(&update_info, &dev, auto_update)) {
            if (auto_update) {
                printf("[+] Successfully opened bladeRF device after firmware load!\n");
                printf("[ ] Attempting to update FX3 to latest firmware version.\n");
                if (update_fx3_device(dev, &update_info)) {
                    need_to_reset_device = true;
                } else {
                    printf("[-] Failed to flash FX3 firmware: %s\n", bladerf_strerror(status));
                    goto cleanup;
                }
            } else {
                printf("[!] Found bladeRF device in bootloader mode. Need to update bootloader, rerun bladeRF-update -y.\n");
                need_to_update_bootloader = true;
                goto cleanup;
            }
        } else {
            printf("[?] No bladeRF device or bootloader found. Cannot check FX3 firmware version.\n");
            printf("[!] Optionally connect a bladeRF device to check for updates.\n");
            goto cleanup;
        }
    }
    bladeRF_device_attached = true;

    // Device is attached, get versions

    printf("[+] Connected bladeRF device:\n");

    fx3_up_to_date = check_fx3_version(&update_info, dev);
    fpga_up_to_date = check_fpga_version(&update_info, dev);

    if (!fx3_up_to_date && auto_update) {
	printf("[ ] Attempting to update FX3 to latest firmware version.\n");
	fx3_up_to_date = update_fx3_device(dev, &update_info);
    }

    if (rbf_up_to_date && fx3_up_to_date && fpga_up_to_date && fw_up_to_date) {
        printf("[+] All components are up to date.\n");
    } else {
        printf("[!] Updates are available. Run bladeRF-update -y to automatically update.\n");
    }

    bladerf_close(dev);
cleanup:

    outstanding_updates = false;
#define SUMMARY_SPACING ">   "
    printf("\n\nSUMMARY:\n");
    if (rbf_up_to_date) {
        printf(SUMMARY_SPACING "[+] Downloaded FPGA RBF files are up to date!\n");
    } else {
        printf(SUMMARY_SPACING "[-] Downloaded FPGA RBF files are outdated or non-conforming. See above for details.\n");
        printf(SUMMARY_SPACING "[!] Run bladeRF-update -y --rm to cleanly update RBF files.\n");
        outstanding_updates = true;
    }

    if (fw_up_to_date) {
        printf(SUMMARY_SPACING "[+] Downloaded FX3 firmware files are up to date!\n");
    } else {
        printf(SUMMARY_SPACING "[-] Downloaded FX3 firmware files are outdated or missing. See above for details.\n");
        printf(SUMMARY_SPACING "[!] Run bladeRF-update -y --rm to cleanly update FX3 firmware files.\n");
        outstanding_updates = true;
    }

    if (libbladerf_up_to_date) {
        printf(SUMMARY_SPACING "[+] Installed libbladeRF library is up to date!\n");
    } else {
        printf(SUMMARY_SPACING "[!] See instructions above to update libbladeRF library.\n");
    }

    if (need_to_update_bootloader) {
        printf(SUMMARY_SPACING "[-] bladeRF bootloader found. Need to update bootloader.\n");
        printf(SUMMARY_SPACING "[!] Run bladeRF-update -y to update bootloader.\n");
    }

    if (bladeRF_device_attached) {
        if (fx3_up_to_date) {
            printf(SUMMARY_SPACING "[+] FX3 firmware is up to date.\n");
        } else {
            printf(SUMMARY_SPACING "[-] FX3 firmware is outdated or missing. See above for details.\n");
            printf(SUMMARY_SPACING "[!] Run bladeRF-update -y --rm to cleanly update FX3 firmware files.\n");
            outstanding_updates = true;
        }
        if (fpga_up_to_date) {
            printf(SUMMARY_SPACING "[+] FPGA is up to date.\n");
        } else {
            printf(SUMMARY_SPACING "[-] FPGA is outdated or missing. See above for details.\n");
            printf(SUMMARY_SPACING "[!] Run bladeRF-update -y --rm to cleanly update FPGA files.\n");
            outstanding_updates = true;
        }
    } else {
        printf(SUMMARY_SPACING "[!] Connect a bladeRF to check device's FX3 firmware.\n");
    }

    if (need_to_reset_device) {
        printf(SUMMARY_SPACING "[!] Power cycle bladeRF by disconnecting and reconnecting the USB cable.\n");
    }

    // Display help message if no arguments were provided
    if (argc == 1 || outstanding_updates) {
        printf("\n");
        printf("For more information rerun bladeRF-update with -h%s.\n\n", outstanding_updates ? " or -y to update" : "");
    }

    return 0;
}

// Add this callback function
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// Modify the download_json function
bool download_json(const char* url, char* buffer, size_t buffer_size) {
#ifdef _WIN32
    HINTERNET hInternet = InternetOpen("bladeRF-update", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet == NULL) {
        fprintf(stderr, "Failed to initialize WinINet\n");
        return false;
    }

    HINTERNET hConnect = InternetOpenUrl(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (hConnect == NULL) {
        fprintf(stderr, "Failed to open URL\n");
        InternetCloseHandle(hInternet);
        return false;
    }

    DWORD bytesRead = 0;
    DWORD totalBytesRead = 0;

    while (totalBytesRead < buffer_size - 1) {
        if (!InternetReadFile(hConnect, buffer + totalBytesRead, buffer_size - totalBytesRead - 1, &bytesRead)) {
            fprintf(stderr, "Failed to read from URL\n");
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return false;
        }

        if (bytesRead == 0) {
            break;
        }

        totalBytesRead += bytesRead;
    }

    buffer[totalBytesRead] = '\0';

    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
#else
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;

    chunk.memory = malloc(1);  // will be grown as needed by realloc
    chunk.size = 0;    // no data at this point

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl\n");
        free(chunk.memory);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "bladeRF-update");

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Failed to download JSON: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return false;
    }

    if (chunk.size >= buffer_size) {
        fprintf(stderr, "Downloaded data exceeds buffer size\n");
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return false;
    }

    memcpy(buffer, chunk.memory, chunk.size);
    buffer[chunk.size] = '\0';  // Ensure null-termination

    curl_easy_cleanup(curl);
    free(chunk.memory);
#endif

    return true;
}

// Modified parse_json function
bool parse_json(const char* json_string, UpdateInfo* update_info) {
    struct json_object *root, *firmware, *fpga, *latest, *versions, *version_obj, *filename, *md5, *sha256;
    struct json_object *libbladerf, *libbladerf_latest;

    root = json_tokener_parse(json_string);
    if (!root) {
        fprintf(stderr, "Failed to parse JSON\n");
        return false;
    }

    // Parse libbladeRF info
    if (json_object_object_get_ex(root, "libbladeRF", &libbladerf)) {
        if (json_object_object_get_ex(libbladerf, "latest", &libbladerf_latest)) {
            const char* latest_lib_version = json_object_get_string(libbladerf_latest);
            if (latest_lib_version) {
                strncpy(update_info->latest_libbladerf_version, latest_lib_version, MAX_VERSION_LENGTH - 1);
                update_info->latest_libbladerf_version[MAX_VERSION_LENGTH - 1] = '\0';
            } else {
                // Default to empty string if not found
                update_info->latest_libbladerf_version[0] = '\0';
            }
        } else {
            update_info->latest_libbladerf_version[0] = '\0';
        }
    } else {
        update_info->latest_libbladerf_version[0] = '\0';
    }

    // Parse firmware info
    if (!json_object_object_get_ex(root, "firmware", &firmware)) {
        fprintf(stderr, "Failed to get 'firmware' object from JSON\n");
        json_object_put(root);
        return false;
    }
    if (!json_object_object_get_ex(firmware, "latest", &latest)) {
        fprintf(stderr, "Failed to get 'latest' from firmware object\n");
        json_object_put(root);
        return false;
    }
    if (!json_object_object_get_ex(firmware, "versions", &versions)) {
        fprintf(stderr, "Failed to get 'versions' from firmware object\n");
        json_object_put(root);
        return false;
    }
    const char* latest_version = json_object_get_string(latest);
    if (!latest_version) {
        fprintf(stderr, "Failed to get latest version string\n");
        json_object_put(root);
        return false;
    }
    if (!json_object_object_get_ex(versions, latest_version, &version_obj)) {
        fprintf(stderr, "Failed to get version object for latest version\n");
        json_object_put(root);
        return false;
    }
    if (!json_object_object_get_ex(version_obj, "filename", &filename) ||
        !json_object_object_get_ex(version_obj, "md5", &md5) ||
        !json_object_object_get_ex(version_obj, "sha256", &sha256)) {
        fprintf(stderr, "Failed to get filename, md5, or sha256 from version object\n");
        json_object_put(root);
        return false;
    }

    const char* filename_str = json_object_get_string(filename);
    const char* md5_str = json_object_get_string(md5);
    const char* sha256_str = json_object_get_string(sha256);

    if (!filename_str || !md5_str || !sha256_str) {
        fprintf(stderr, "Failed to get string values for filename, md5, or sha256\n");
        json_object_put(root);
        return false;
    }

    strncpy(update_info->firmware.version, latest_version, MAX_VERSION_LENGTH - 1);
    strncpy(update_info->firmware.filename, filename_str, MAX_FILENAME_LENGTH - 1);
    strncpy(update_info->firmware.md5, md5_str, MAX_HASH_LENGTH);
    strncpy(update_info->firmware.sha256, sha256_str, MAX_HASH_LENGTH);

    update_info->firmware.version[MAX_VERSION_LENGTH - 1] = '\0';
    update_info->firmware.filename[MAX_FILENAME_LENGTH - 1] = '\0';
    update_info->firmware.md5[MAX_HASH_LENGTH] = '\0';
    update_info->firmware.sha256[MAX_HASH_LENGTH] = '\0';

    if (verbose_output) {
        printf("\nParsed Update Information:\n");
        printf("Firmware:\n");
        printf("  Version: %s\n", update_info->firmware.version);
        printf("  Filename: %s\n", update_info->firmware.filename);
        printf("  MD5: %s\n", update_info->firmware.md5);
        printf("  SHA256: %s\n", update_info->firmware.sha256);

        if (strlen(update_info->latest_libbladerf_version) > 0) {
            printf("Latest libbladeRF version: %s\n", update_info->latest_libbladerf_version);
        }
    }

    // Parse FPGA info
    if (!json_object_object_get_ex(root, "fpga", &fpga)) {
        fprintf(stderr, "Failed to get 'fpga' object from JSON\n");
        json_object_put(root);
        return false;
    }

    // Get the latest FPGA version
    struct json_object *latest_fpga;
    if (!json_object_object_get_ex(fpga, "latest", &latest_fpga)) {
        fprintf(stderr, "Failed to get 'latest' from FPGA object\n");
        json_object_put(root);
        return false;
    }
    const char* latest_fpga_version = json_object_get_string(latest_fpga);
    if (!latest_fpga_version) {
        fprintf(stderr, "Failed to get latest FPGA version string\n");
        json_object_put(root);
        return false;
    }

    strncpy(update_info->latest_fpga_version, latest_fpga_version, MAX_VERSION_LENGTH - 1);
    update_info->latest_fpga_version[MAX_VERSION_LENGTH - 1] = '\0';

    if (!json_object_object_get_ex(fpga, "versions", &versions)) {
        fprintf(stderr, "Failed to get 'versions' from FPGA object\n");
        json_object_put(root);
        return false;
    }

    update_info->num_fpga_versions = json_object_object_length(versions);
    update_info->fpga_versions = malloc(update_info->num_fpga_versions * sizeof(FPGAVersionInfo));
    if (!update_info->fpga_versions) {
        fprintf(stderr, "Failed to allocate memory for FPGA versions\n");
        json_object_put(root);
        return false;
    }

    int version_index = 0;
    json_object_object_foreach(versions, fpga_version, fpga_version_obj) {
        strncpy(update_info->fpga_versions[version_index].version, fpga_version, MAX_VERSION_LENGTH - 1);
        update_info->fpga_versions[version_index].version[MAX_VERSION_LENGTH - 1] = '\0';

        struct json_object *files;
        if (!json_object_object_get_ex(fpga_version_obj, "files", &files)) {
            fprintf(stderr, "Failed to get 'files' from FPGA version object\n");
            json_object_put(root);
            return false;
        }

        update_info->fpga_versions[version_index].num_files = json_object_object_length(files);
        update_info->fpga_versions[version_index].files = malloc(update_info->fpga_versions[version_index].num_files * sizeof(VersionInfo));
        if (!update_info->fpga_versions[version_index].files) {
            fprintf(stderr, "Failed to allocate memory for FPGA files\n");
            json_object_put(root);
            return false;
        }

        int file_index = 0;
        json_object_object_foreach(files, fpga_type, fpga_file) {
            struct json_object *md5_obj, *sha256_obj;
            if (!json_object_object_get_ex(fpga_file, "md5", &md5_obj) ||
                !json_object_object_get_ex(fpga_file, "sha256", &sha256_obj)) {
                fprintf(stderr, "Failed to get md5 or sha256 for FPGA type %s\n", fpga_type);
                json_object_put(root);
                return false;
            }

            const char* md5_str = json_object_get_string(md5_obj);
            const char* sha256_str = json_object_get_string(sha256_obj);

            if (!md5_str || !sha256_str) {
                fprintf(stderr, "Failed to get string values for md5 or sha256 for FPGA type %s\n", fpga_type);
                json_object_put(root);
                return false;
            }

            snprintf(update_info->fpga_versions[version_index].files[file_index].filename, MAX_FILENAME_LENGTH, "%s.rbf", fpga_type);
            strncpy(update_info->fpga_versions[version_index].files[file_index].md5, md5_str, MAX_HASH_LENGTH);
            strncpy(update_info->fpga_versions[version_index].files[file_index].sha256, sha256_str, MAX_HASH_LENGTH);

            update_info->fpga_versions[version_index].files[file_index].filename[MAX_FILENAME_LENGTH - 1] = '\0';
            update_info->fpga_versions[version_index].files[file_index].md5[MAX_HASH_LENGTH] = '\0';
            update_info->fpga_versions[version_index].files[file_index].sha256[MAX_HASH_LENGTH] = '\0';

            file_index++;
        }

        version_index++;
    }

    update_info->latest_fpga_version_index = -1;
    // Print statistics only for the latest FPGA version
    for (int i = 0; i < update_info->num_fpga_versions; i++) {
        if (strcmp(update_info->fpga_versions[i].version, latest_fpga_version) == 0) {
            update_info->latest_fpga_version_index = i;

            if (verbose_output) {
                printf("\nLatest FPGA Version: %s\n", latest_fpga_version);
                for (int j = 0; j < update_info->fpga_versions[i].num_files; j++) {
                    printf("  Type: %s\n", update_info->fpga_versions[i].files[j].filename);
                    printf("  Filename: %s\n", update_info->fpga_versions[i].files[j].filename);
                    printf("  MD5: %s\n", update_info->fpga_versions[i].files[j].md5);
                    printf("  SHA256: %s\n", update_info->fpga_versions[i].files[j].sha256);
                    printf("\n");
                }
            }
            break;
        }
    }

    json_object_put(root);
    return true;
}

// Implementation of check_fpga_version
bool check_fpga_version(const UpdateInfo* update_info, struct bladerf* dev) {
    struct bladerf_version version;
    if (bladerf_fpga_version(dev, &version) != 0) {
        fprintf(stderr, "Failed to get FPGA version\n");
        return false;
    }

    printf("[ ] FPGA version: v%d.%d.%d\n", version.major, version.minor, version.patch);
    char current_version[MAX_VERSION_LENGTH];
    snprintf(current_version, sizeof(current_version), "v%d.%d.%d", version.major, version.minor, version.patch);

    if (strcmp(current_version, update_info->latest_fpga_version) != 0) {
        printf("[-] FPGA is outdated. Current: %s, Latest: %s\n", current_version, update_info->latest_fpga_version);
        return false;
    }

    printf("[+] FPGA is up to date.\n");

    return true;
}

// Implementation of check_fx3_version
bool check_fx3_version(const UpdateInfo* update_info, struct bladerf* dev) {
    struct bladerf_version version;
    if (bladerf_fw_version(dev, &version) != 0) {
        fprintf(stderr, "[-] Failed to get firmware version\n");
        return false;
    }

    printf("[ ] FX3 Firmware version: v%d.%d.%d\n", version.major, version.minor, version.patch);
    char current_version[MAX_VERSION_LENGTH];
    snprintf(current_version, sizeof(current_version), "v%d.%d.%d", version.major, version.minor, version.patch);

    if (strcmp(current_version, update_info->firmware.version) != 0) {
        printf("[-] FX3 firmware is outdated. Current: %s, Latest: %s\n", current_version, update_info->firmware.version);
        return false;
    }

    printf("[+] FX3 firmware is up to date.\n");

    return true;
}

// Implementation of download_file
bool download_file(const char* url, const char* filename) {
#ifdef _WIN32
    HINTERNET hInternet = InternetOpen("bladeRF-update", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet == NULL) {
        fprintf(stderr, "Failed to initialize WinINet\n");
        return false;
    }

    HINTERNET hConnect = InternetOpenUrl(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (hConnect == NULL) {
        fprintf(stderr, "Failed to open URL\n");
        InternetCloseHandle(hInternet);
        return false;
    }

    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Failed to open file for writing\n");
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return false;
    }

    char buffer[4096];
    DWORD bytesRead;
    while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        fwrite(buffer, 1, bytesRead, file);
    }

    fclose(file);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
#else
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl\n");
        return false;
    }

    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Failed to open file for writing\n");
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "bladeRF-update");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Failed to download file: %s\n", curl_easy_strerror(res));
        fclose(file);
        curl_easy_cleanup(curl);
        return false;
    }

    fclose(file);
    curl_easy_cleanup(curl);
#endif

    return true;
}

// Modified implementation of verify_file
bool verify_file(const char* filename, const char* expected_md5, const char* expected_sha256, const UpdateInfo* update_info) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file for verification: %s\n", filename);
        return false;
    }

    SHA256_CTX sha256_ctx;
    SHA256_Init(&sha256_ctx);

    unsigned char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        SHA256_Update(&sha256_ctx, buffer, bytes_read);
    }

    fclose(file);

    unsigned char sha256_result[32];
    SHA256_Final(sha256_result, &sha256_ctx);

    char sha256_hex[65];

    for (int i = 0; i < 32; i++)
        sprintf(&sha256_hex[i*2], "%02x", sha256_result[i]);

    if (verbose_output) {
        printf("File: %s\n", filename);
        printf("SHA256:\n  Expected:   %s\n  Calculated: %s\n", expected_sha256, sha256_hex);
    }

    bool sha256_match = (strcmp(sha256_hex, expected_sha256) == 0);

    if (sha256_match) {
        if (verbose_output) {
            printf("Verification result: PASS\n");
        }
        return true;
    } else {
        printf("Verification result: FAIL\n");

        // Check for older versions
        const char* base_filename = strrchr(filename, '/');
        if (base_filename == NULL) {
            base_filename = filename;
        } else {
            base_filename++; // Skip the '/'
        }

        for (int i = 0; i < update_info->num_fpga_versions; i++) {
            for (int j = 0; j < update_info->fpga_versions[i].num_files; j++) {
                if (strcmp(base_filename, update_info->fpga_versions[i].files[j].filename) == 0) {
                    if (strcmp(sha256_hex, update_info->fpga_versions[i].files[j].sha256) == 0) {
                        printf("Verification result: OUT OF DATE. File matches an older version %s\n", update_info->fpga_versions[i].version);
                        return false;
                    }
                }
            }
        }

        printf("[-] File %s does not match any known version\n", filename);

        if (remove_non_conforming) {
            remove(filename);
            printf("[+] Removed file %s\n", filename);
            return true;
        }

        return false;
    }
}

// Implementation of update_fx3_device
bool update_fx3_device(struct bladerf* dev, const UpdateInfo* update_info) {
    int status = 0;
    printf("[ ] Updating FX3 firmware to latest version: %s\n", update_info->firmware.version);

    // Use a known path for the firmware file
    char firmware_path[MAX_PATH_LENGTH] = {0};

    // Use the preferred download path
    const char* download_path = get_preferred_download_path();
    if (download_path) {
#ifdef _WIN32
	snprintf(firmware_path, sizeof(firmware_path), "%s\\%s", download_path, update_info->firmware.filename);
#else
	snprintf(firmware_path, sizeof(firmware_path), "%s/%s", download_path, update_info->firmware.filename);
#endif

	printf("[ ] Using firmware at %s\n", firmware_path);
	printf("[ ] Flashing FX3 firmware to device...\n");
	status = bladerf_flash_firmware(dev, firmware_path);

	if (status == 0) {
	    printf("[+] Successfully flashed FX3 firmware. Please disconnect and reconnect your bladeRF.\n");
	    printf("[+] Firmware update requires a device reset. Closing device.\n");
	    return true;
	} else {
	    fprintf(stderr, "[-] Failed to flash FX3 firmware: %s\n", bladerf_strerror(status));
	}
    } else {
	fprintf(stderr, "[-] Could not determine firmware path\n");
    }

    return false;
}

// Modified helper function to check RBF files
bool check_rbf_files(const UpdateInfo* update_info) {
    FPGASearchPaths *search_paths = bladerf_get_fpga_paths();
    if (!search_paths) {
        fprintf(stderr, "Failed to get FPGA search paths\n");
        return false;
    }

    // Find the latest FPGA version
    const FPGAVersionInfo* latest_version = NULL;
    if (update_info->latest_fpga_version_index != -1) {
        latest_version = &update_info->fpga_versions[update_info->latest_fpga_version_index];
    }

    if (!latest_version) {
        fprintf(stderr, "No FPGA versions found in update info\n");
        free_fpga_search_paths(search_paths);
        return false;
    }

    if (verbose_output) {
        printf("\nChecking all RBF load paths for latest FPGA version: %s\n\n", latest_version->version);
    }
    printf("[ ] Checking for latest FPGA version: %s\n", latest_version->version);


    bool all_up_to_date = true;
    for (int j = 0; j < latest_version->num_files; j++) {
        bool file_found = false;
        for (int k = 0; k < search_paths->count; k++) {
            char filepath[MAX_PATH_LENGTH];
            snprintf(filepath, sizeof(filepath), "%s/%s", search_paths->paths[k], latest_version->files[j].filename);

            if (file_exists(filepath)) {
                file_found = true;
                if (!verify_file(filepath, latest_version->files[j].md5, latest_version->files[j].sha256, update_info)) {
                    printf("[-] FPGA %s is outdated or corrupted\n", filepath);
                    all_up_to_date = false;
                } else {
                    if (verbose_output) {
                        printf("[+] FPGA %s is up to date\n\n", filepath);
                    }
                }
                //break;
            }
        }
        if (!file_found) {
            printf("[-] FPGA %s is missing\n", latest_version->files[j].filename);
            all_up_to_date = false;
        }
    }

    if (all_up_to_date) {
        printf("[+] All FPGA images are up to date.\n");
    } else {
        printf("[-] Some FPGA images are outdated or non-conforming. See above for details.\n");
    }

    free_fpga_search_paths(search_paths);
    return all_up_to_date;
}

// Modified update_rbf_files function
void update_rbf_files(const UpdateInfo* update_info) {
    FPGASearchPaths *search_paths = bladerf_get_fpga_paths();
    if (!search_paths) {
        fprintf(stderr, "[CRITICAL] Failed to get FPGA search paths\n");
        return;
    }

    // Use the preferred download path
    const char* download_path = get_preferred_download_path();
    if (!download_path) {
        // Fall back to the first search path if preferred path is not available
        if (search_paths->count > 0) {
            download_path = search_paths->paths[0];
        } else {
            fprintf(stderr, "[CRITICAL] No valid download path available\n");
            free_fpga_search_paths(search_paths);
            return;
        }
    }

    // Find the latest FPGA version
    const FPGAVersionInfo* latest_version = NULL;
    if (update_info->latest_fpga_version_index != -1) {
        latest_version = &update_info->fpga_versions[update_info->latest_fpga_version_index];
    }

    if (!latest_version) {
        fprintf(stderr, "[CRITICAL] No FPGA versions found in update info\n");
        free_fpga_search_paths(search_paths);
        return;
    }

    printf("[ ] Files will be downloaded to: %s\n", download_path);
    printf("[ ] Updating RBF files to latest FPGA version: %s\n", latest_version->version);

    for (int j = 0; j < latest_version->num_files; j++) {
        // First check if an up-to-date version of this file already exists
        bool rbf_up_to_date = false;
        char existing_rbf_path[MAX_PATH_LENGTH] = {0};

        for (int k = 0; k < search_paths->count; k++) {
            char filepath[MAX_PATH_LENGTH];
            snprintf(filepath, sizeof(filepath), "%s/%s", search_paths->paths[k], latest_version->files[j].filename);

            if (file_exists(filepath)) {
                if (verify_file(filepath, latest_version->files[j].md5, latest_version->files[j].sha256, update_info)) {
                    rbf_up_to_date = true;
                    strncpy(existing_rbf_path, filepath, MAX_PATH_LENGTH - 1);
                    printf("[+] Found up-to-date RBF at %s, skipping download\n", filepath);
                    break;
                }
            }
        }

        if (rbf_up_to_date) {
            continue;
        }

        char url[1024];
        snprintf(url, sizeof(url), "https://www.nuand.com/fpga/%s/%s", latest_version->version, latest_version->files[j].filename);

        char filepath[MAX_PATH_LENGTH];
#ifdef _WIN32
        snprintf(filepath, sizeof(filepath), "%s\\%s", download_path, latest_version->files[j].filename);
#else
        snprintf(filepath, sizeof(filepath), "%s/%s", download_path, latest_version->files[j].filename);
#endif

        printf("[ ] Downloading %s...\n", latest_version->files[j].filename);
        if (download_file(url, filepath) && verify_file(filepath, latest_version->files[j].md5, latest_version->files[j].sha256, update_info)) {
            printf("[+] Successfully updated %s\n", latest_version->files[j].filename);
        } else {
            fprintf(stderr, "Failed to update %s\n", latest_version->files[j].filename);
        }
    }

    free_fpga_search_paths(search_paths);
}

// Modified update_fw_files function
void update_fw_files(const UpdateInfo* update_info) {
    FPGASearchPaths *search_paths = bladerf_get_fpga_paths();
    if (!search_paths) {
        fprintf(stderr, "Failed to get firmware search paths\n");
        return;
    }

    // First check if an up-to-date firmware file already exists
    bool fw_up_to_date = false;
    char existing_fw_path[MAX_PATH_LENGTH] = {0};

    printf("[ ] Updating firmware files to latest version: %s\n", update_info->firmware.version);

    for (int k = 0; k < search_paths->count; k++) {
        char filepath[MAX_PATH_LENGTH];

        // Check only version-specific file, not the generic bladeRF_fw.img
        snprintf(filepath, sizeof(filepath), "%s/%s", search_paths->paths[k], update_info->firmware.filename);
        if (file_exists(filepath)) {
            if (verify_file(filepath, update_info->firmware.md5, update_info->firmware.sha256, update_info)) {
                fw_up_to_date = true;
                strncpy(existing_fw_path, filepath, MAX_PATH_LENGTH - 1);
                printf("[+] Found up-to-date firmware at %s, skipping download\n", filepath);
                break;
            }
        }
    }

    if (fw_up_to_date) {
        free_fpga_search_paths(search_paths);
        return;
    }

    // Use the preferred download path
    const char* download_path = get_preferred_download_path();
    if (!download_path) {
        // Fall back to the first search path if preferred path is not available
        if (search_paths->count > 0) {
            download_path = search_paths->paths[0];
        } else {
            fprintf(stderr, "No valid download path available\n");
            free_fpga_search_paths(search_paths);
            return;
        }
    }

    printf("[ ] Updating firmware to latest version: %s\n", update_info->firmware.version);
    printf("[ ] Files will be downloaded to: %s\n", download_path);

    // Download the firmware image
    char url[1024];
    snprintf(url, sizeof(url), "https://www.nuand.com/fx3/%s", update_info->firmware.filename);

    char filepath[MAX_PATH_LENGTH];
#ifdef _WIN32
    snprintf(filepath, sizeof(filepath), "%s\\%s", download_path, update_info->firmware.filename);
#else
    snprintf(filepath, sizeof(filepath), "%s/%s", download_path, update_info->firmware.filename);
#endif

    printf("[ ] Downloading %s...\n", update_info->firmware.filename);
    if (download_file(url, filepath) && verify_file(filepath, update_info->firmware.md5, update_info->firmware.sha256, update_info)) {
        printf("[+] Successfully downloaded %s\n", update_info->firmware.filename);
    } else {
        fprintf(stderr, "[-] Failed to download %s\n", update_info->firmware.filename);
    }

    free_fpga_search_paths(search_paths);
}

// Helper function to check for firmware files
bool check_fw_files(const UpdateInfo* update_info) {
    FPGASearchPaths *search_paths = bladerf_get_fpga_paths();
    if (!search_paths) {
        fprintf(stderr, "Failed to get firmware search paths\n");
        return false;
    }

    if (verbose_output) {
        printf("\nChecking all paths for latest firmware version: %s\n\n", update_info->firmware.version);
    }
    printf("[ ] Checking for latest firmware version: %s\n", update_info->firmware.version);

    bool fw_up_to_date = false;
    bool found_outdated = false;

    for (int k = 0; k < search_paths->count; k++) {
        char filepath[MAX_PATH_LENGTH];

        // Check only version-specific file, not the generic bladeRF_fw.img
        snprintf(filepath, sizeof(filepath), "%s/%s", search_paths->paths[k], update_info->firmware.filename);
        if (file_exists(filepath)) {
            if (verify_file(filepath, update_info->firmware.md5, update_info->firmware.sha256, update_info)) {
                if (verbose_output) {
                    printf("[+] Firmware %s is up to date\n\n", filepath);
                }
                fw_up_to_date = true;
            } else {
                printf("[-] Firmware %s is outdated or corrupted.\n", filepath);
                printf("[!] Rerun bladeRF-update -y --rm to do a clean update.\n");
                found_outdated = true;
            }
        }
    }

    if (!fw_up_to_date) {
        printf("[-] Firmware files not found or outdated. See above for details.\n");
    } else if (found_outdated) {
        // If we found both up-to-date and outdated firmware, still mark as not up-to-date
        // so we don't report all firmware files are up to date when some are outdated
        fw_up_to_date = false;
    }

    free_fpga_search_paths(search_paths);
    if (fw_up_to_date) {
        printf("[+] Firmware files are up to date.\n");
    } else {
        printf("[-] Firmware files are outdated or non-conforming. See above for details.\n");
    }
    return fw_up_to_date;
}

// Function to check if current libbladeRF version is up to date
bool check_libbladerf_version(const UpdateInfo* update_info) {
    // Don't proceed if we didn't get a valid libbladeRF version from the JSON
    if (strlen(update_info->latest_libbladerf_version) == 0) {
        return true; // Assume up to date if no version info was available
    }

    struct bladerf_version version;
    bladerf_version(&version);

    char current_version[MAX_VERSION_LENGTH];
    snprintf(current_version, sizeof(current_version), "v%d.%d.%d", version.major, version.minor, version.patch);

    printf("[ ] Checking for latest libbladeRF version: %s\n", update_info->latest_libbladerf_version);
    if (strcmp(current_version, update_info->latest_libbladerf_version) != 0) {
        printf("[-] Installed libbladeRF library version %s is outdated.\n", current_version);
        printf("[-] Latest libbladeRF version: %s\n", update_info->latest_libbladerf_version);


#ifdef _WIN32
        printf("[!]    Please download the latest Windows installer from:\n");
        printf("[!]    https://www.nuand.com/win_installers/\n");
#else
        // For Linux/macOS
        printf("[!]    Please update using your package manager (e.g., apt-get install libbladerf)\n");
        printf("[!]    or build from source: https://github.com/Nuand/bladeRF\n");
#endif

        return false;
    } else {
        printf("[+] Installed libbladeRF version is up to date.\n");

    }

    return true;
}

// Added function to check for bootloaders and load firmware if found
bool check_and_load_bootloader(const UpdateInfo* update_info, struct bladerf** dev, bool auto_update) {
    struct bladerf_devinfo *list = NULL;
    int count = bladerf_get_bootloader_list(&list);

    if (count <= 0) {
        if (count < 0) {
            fprintf(stderr, "[-] Error checking for bootloaders: %s\n", bladerf_strerror(count));
        } else {
            printf("[ ] No bladeRF bootloaders found.\n");
        }
        return false;
    }

    printf("[+] Found %d bladeRF device(s) in bootloader mode.\n", count);
    if (!auto_update) {
        return true;
    }

    // Get the FX3 firmware file path
    FPGASearchPaths *search_paths = bladerf_get_fpga_paths();
    if (!search_paths) {
        fprintf(stderr, "[-] Failed to get firmware search paths\n");
        bladerf_free_device_list(list);
        return false;
    }

    // Find firmware file
    char firmware_path[MAX_PATH_LENGTH] = {0};
    bool found_firmware = false;

    for (int k = 0; k < search_paths->count && !found_firmware; k++) {
        snprintf(firmware_path, sizeof(firmware_path), "%s/%s",
                search_paths->paths[k], update_info->firmware.filename);
        if (file_exists(firmware_path)) {
            found_firmware = true;
        }
    }

    if (!found_firmware) {
        // If not found, check the preferred download path
        const char* download_path = get_preferred_download_path();
        if (download_path) {
            snprintf(firmware_path, sizeof(firmware_path), "%s/%s",
                    download_path, update_info->firmware.filename);
            if (file_exists(firmware_path)) {
                found_firmware = true;
            }
        }
    }

    if (!found_firmware) {
        fprintf(stderr, "[-] Could not find firmware file %s in any search path\n",
                update_info->firmware.filename);
        free_fpga_search_paths(search_paths);
        bladerf_free_device_list(list);
        return false;
    }

    // Use the first bootloader device found
    printf("[+] Using bootloader device at bus:%d addr:%d\n", list[0].usb_bus, list[0].usb_addr);
    printf("[+] Loading firmware %s\n", firmware_path);

    int status = bladerf_load_fw_from_bootloader(NULL, list[0].backend,
                                               list[0].usb_bus, list[0].usb_addr,
                                               firmware_path);

    free_fpga_search_paths(search_paths);
    bladerf_free_device_list(list);

    if (status != 0) {
        fprintf(stderr, "[-] Failed to load firmware: %s\n", bladerf_strerror(status));
        return false;
    }

    printf("[+] Successfully loaded firmware. Device should enumerate shortly.\n");

    // Sleep for 3 seconds
#ifdef _WIN32
    Sleep(3000);
#else
    sleep(3);
#endif

    // Try opening the device again after loading firmware
    printf("[+] Trying to open device again after firmware load...\n");
    status = bladerf_open(dev, NULL);

    if (status != 0) {
	    fprintf(stderr, "[-] Still no bladeRF device found after loading firmware.\n");
	    fprintf(stderr, "[!] Try unplugging and reconnecting the device.\n");
	    return false;
    }

    return true;
}
