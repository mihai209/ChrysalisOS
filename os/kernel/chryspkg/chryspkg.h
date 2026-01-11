/* kernel/chryspkg/chryspkg.h */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize package manager (load catalog) */
void chryspkg_init(void);

/* List available applications from catalog */
void chryspkg_list(void);

/* Install a local .petal package */
/* Returns 0 on success, non-zero on failure */
int chryspkg_install(const char* petal_path);

#ifdef __cplusplus
}
#endif
