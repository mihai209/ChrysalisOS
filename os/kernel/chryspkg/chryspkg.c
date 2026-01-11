/* kernel/chryspkg/chryspkg.c */
#include "chryspkg.h"
#include "../include/stdio.h"
#include "../include/string.h"
#include "../mem/kmalloc.h"
#include "../drivers/serial.h"
#include "../cmds/fat.h"

/* Configuration */
#define CATALOG_PATH "/system/pkg/catalog.json"
#define INSTALLED_DIR "/system/pkg/installed"
#define MAX_JSON_SIZE (16 * 1024)

/* State */
static char* catalog_json = NULL;
static char repo_url[128] = {0};
static int app_count = 0;

/* Helpers */
extern void serial(const char *fmt, ...);

/* Minimal JSON String Search */
static char* json_find_value(const char* json, const char* key, char* out_buf, int max_len) {
    if (!json || !key) return NULL;

    char search[64];
    /* Construct "key": */
    search[0] = '"';
    int i = 0;
    while (key[i] && i < 60) { search[i+1] = key[i]; i++; }
    search[i+1] = '"';
    search[i+2] = ':';
    search[i+3] = 0;

    char* pos = strstr(json, search);
    if (!pos) return NULL;

    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t' || *pos == '\n') pos++;

    if (*pos == '"') {
        pos++; /* Skip quote */
        int j = 0;
        while (*pos && *pos != '"' && j < max_len - 1) {
            out_buf[j++] = *pos++;
        }
        out_buf[j] = 0;
        return out_buf;
    }
    return NULL;
}

/* TAR Header Structure (Standard ustar) */
typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
} tar_header_t;

/* Octal string to int */
static unsigned int oct2bin(const char *str, int size) {
    unsigned int n = 0;
    const char *c = str;
    while (size-- > 0) {
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}

/* API Implementation */

void chryspkg_init(void) {
    serial("[PKG] init\n");

    /* Ensure filesystem structure exists */
    fat_automount();
    
    if (!fat32_directory_exists("/system")) fat32_create_directory("/system");
    if (!fat32_directory_exists("/system/pkg")) fat32_create_directory("/system/pkg");
    if (!fat32_directory_exists(INSTALLED_DIR)) fat32_create_directory(INSTALLED_DIR);

    /* Create default catalog if missing */
    if (fat32_get_file_size(CATALOG_PATH) < 0) {
        serial("[PKG] Creating default catalog...\n");
        const char* def_cat = "{\n"
            "  \"catalog_version\": 1,\n"
            "  \"repository\": \"https://chrys-pkg.vercel.app\",\n"
            "\n"
            "  \"apps\": [\n"
            "    {\n"
            "      \"name\": \"clickme\",\n"
            "      \"version\": \"1.0\",\n"
            "      \"file\": \"clickme-1.0.petal\",\n"
            "      \"type\": \"gui\",\n"
            "      \"entry\": \"/usr/bin/clickme\",\n"
            "      \"description\": \"First FlyUI test app\"\n"
            "    }\n"
            "  ]\n"
            "}";
        if (fat32_create_file(CATALOG_PATH, def_cat, strlen(def_cat)) != 0) {
            serial("[PKG] Error creating catalog.json\n");
        }
    }

    /* 1. Load Catalog */
    FILE* f = fopen(CATALOG_PATH, "r");
    if (!f) {
        serial("[PKG] Error: Cannot open catalog at %s\n", CATALOG_PATH);
        return;
    }

    /* Get size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > MAX_JSON_SIZE) {
        serial("[PKG] Error: Catalog size invalid (%ld)\n", size);
        fclose(f);
        return;
    }

    catalog_json = (char*)kmalloc(size + 1);
    if (!catalog_json) {
        serial("[PKG] Error: OOM loading catalog\n");
        fclose(f);
        return;
    }

    fread(catalog_json, 1, size, f);
    catalog_json[size] = 0;
    fclose(f);

    /* 2. Parse Repository */
    if (json_find_value(catalog_json, "repository", repo_url, 127)) {
        serial("[PKG] repo: %s\n", repo_url);
    } else {
        serial("[PKG] Warn: No repository defined\n");
    }

    /* 3. Count Apps (Naive count of "name":) */
    char* p = catalog_json;
    app_count = 0;
    while ((p = strstr(p, "\"name\":"))) {
        app_count++;
        p++;
    }
    serial("[PKG] apps: %d\n", app_count);
}

void chryspkg_list(void) {
    if (!catalog_json) {
        serial("[PKG] Error: Catalog not loaded\n");
        return;
    }

    /* Iterate manually through the JSON to find app blocks */
    char* p = catalog_json;
    char name[64], ver[32], type[32];

    while ((p = strstr(p, "{"))) {
        /* Check if this block has a name */
        char* name_pos = strstr(p, "\"name\":");
        if (!name_pos) break;
        
        /* Limit search to this object roughly */
        char* end_obj = strstr(p, "}");
        if (name_pos > end_obj) { p = end_obj; continue; }

        /* Extract fields */
        /* Note: This is a hacky parser, assumes order or proximity */
        /* We use a temporary buffer for the object to use json_find_value safely */
        int len = end_obj - p + 1;
        if (len > 1024) len = 1024;
        
        char obj_buf[1025];
        memcpy(obj_buf, p, len);
        obj_buf[len] = 0;

        if (json_find_value(obj_buf, "name", name, 63) &&
            json_find_value(obj_buf, "version", ver, 31) &&
            json_find_value(obj_buf, "type", type, 31)) {
            
            serial("[PKG] %s %s (%s)\n", name, ver, type);
        }

        p = end_obj + 1;
    }
}

int chryspkg_install(const char* petal_path) {
    serial("[PKG] installing %s\n", petal_path);

    FILE* f = fopen(petal_path, "r");
    if (!f) {
        serial("[PKG] Error: Cannot open package file\n");
        return -1;
    }

    /* Check for GZIP signature (1F 8B) */
    unsigned char sig[2];
    if (fread(sig, 1, 2, f) == 2) {
        if (sig[0] == 0x1F && sig[1] == 0x8B) {
            serial("[PKG] Error: GZIP compression not supported yet. Please unzip.\n");
            fclose(f);
            return -1;
        }
    }
    fseek(f, 0, SEEK_SET);

    /* TAR Extraction Loop */
    tar_header_t header;
    char pkg_name[64] = {0};

    while (fread(&header, 1, 512, f) == 512) {
        /* Check for end of archive (empty block) */
        if (header.name[0] == 0) break;

        unsigned int size = oct2bin(header.size, 11);
        
        /* Check filename prefix "files/" */
        if (strncmp(header.name, "files/", 6) == 0) {
            char* dest_path = header.name + 6; /* Strip "files/" */
            
            /* Skip directories (size 0, type 5) */
            if (header.typeflag == '5' || size == 0) {
                /* Maybe mkdir? VFS usually handles it on open or we skip */
            } else {
                /* Extract File */
                char full_dest[128];
                if (dest_path[0] == 0) continue; /* Root dir itself */
                
                /* Construct absolute path */
                snprintf(full_dest, 127, "/%s", dest_path);
                
                /* Check existence */
                FILE* check = fopen(full_dest, "r");
                if (check) {
                    serial("[PKG] Skip existing: %s\n", full_dest);
                    fclose(check);
                } else {
                    /* Write file */
                    serial("[PKG] extracted %s\n", full_dest);
                    
                    /* Read data from TAR */
                    void* data = kmalloc(size);
                    if (data) {
                        fread(data, 1, size, f);
                        
                        /* Write to disk */
                        FILE* out = fopen(full_dest, "w");
                        if (out) {
                            fwrite(data, 1, size, out);
                            fclose(out);
                        } else {
                            serial("[PKG] Error writing %s\n", full_dest);
                        }
                        kfree(data);
                        
                        /* Align to 512 bytes in TAR */
                        int padding = 512 - (size % 512);
                        if (padding < 512) fseek(f, padding, SEEK_CUR);
                        
                        /* Capture package name from binary path for marker */
                        /* Heuristic: if file is /usr/bin/NAME, assume NAME is pkg */
                        if (strncmp(full_dest, "/usr/bin/", 9) == 0) {
                            strncpy(pkg_name, full_dest + 9, 63);
                        }
                        continue; /* Loop continues */
                    }
                }
            }
        }
        
        /* Skip data block if we didn't extract */
        int blocks = (size + 511) / 512;
        fseek(f, blocks * 512, SEEK_CUR);
    }

    fclose(f);

    /* Create installed marker */
    if (pkg_name[0]) {
        /* Ensure directory exists (hacky mkdir via VFS or just assume) */
        /* We just try to write the marker file */
        char marker_path[128];
        snprintf(marker_path, 127, "%s/%s", INSTALLED_DIR, pkg_name);
        
        /* We can't easily mkdir -p without VFS support, so we assume /system/pkg/installed exists 
           or we fail silently on marker. */
        FILE* m = fopen(marker_path, "w");
        if (m) {
            fprintf(m, "installed\n");
            fclose(m);
        }
    }

    serial("[PKG] done\n");
    return 0;
}
