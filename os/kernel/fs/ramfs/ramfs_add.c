/* kernel/fs/ramfs/ramfs_add.c */
#include "../../fs/fs.h"
#include "../../mem/kmalloc.h"
#include "../../string.h"

/* Funcție externă din ramfs.c care returnează rădăcina */
extern FSNode* ramfs_root(void);

void ramfs_create_file(const char* name, const void* data, size_t len) {
    FSNode* root = ramfs_root();
    if (!root) return;

    /* Alocăm nodul nou */
    FSNode* file = (FSNode*)kmalloc(sizeof(FSNode));
    if (!file) return;
    
    memset(file, 0, sizeof(FSNode));
    strcpy(file->name, name);
    file->length = len;
    file->data = (void*)data; /* Pointăm direct la datele din modulul Multiboot */
    file->flags = FS_FILE; 
    
    /* Îl adăugăm la începutul listei de copii ai rădăcinii */
    file->next = root->children;
    root->children = file;
    file->parent = root;
}
