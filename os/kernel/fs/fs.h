#pragma once

struct FSNode {
    const char* name;
    const char* data;
    int size;
};

void fs_init();
void fs_create(const char* name, const char* content);
void fs_list();
const FSNode* fs_find(const char* name);
