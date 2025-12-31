#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*command_fn)(const char*);

typedef struct {
    const char* name;
    command_fn func;
} Command;

#ifdef __cplusplus
}
#endif
