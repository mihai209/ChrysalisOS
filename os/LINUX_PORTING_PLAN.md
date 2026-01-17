# ChrysalisOS Linux Porting Plan

## Strategy: Userspace Port

Instead of replacing Linux kernel, we'll run Chrysalis as userspace framework on top of Alpine Linux + Busybox.

### Architecture

```
Hardware
  ↓
BIOS/UEFI
  ↓
GRUB Bootloader
  ↓
Alpine Linux Kernel (6.6-virt)
  ↓
Busybox Utilities + Init System
  ↓
Chrysalis Framework (userspace daemon)
  ↓
Chrysalis GUI Applications
  ↓
User
```

## Implementation Plan

### Phase 1: Core Compatibility Layer (CRITICAL)

**Goals:**
- Create C API compatible with Chrysalis kernel calls
- Map Chrysalis syscalls to Linux syscalls
- Port memory, process, and filesystem layers

**Files to Create:**
1. `linux_compat/types_linux.h` - Linux-compatible types
2. `linux_compat/syscall_map.h` - Syscall translation
3. `linux_compat/memory.c` - Memory management wrapper
4. `linux_compat/process.c` - Process management wrapper
5. `linux_compat/filesystem.c` - Filesystem wrapper

### Phase 2: Core Library Ports

**Port these in order:**
1. `kernel/string.h` → Pure C, no changes needed
2. `kernel/include/types.h` → Adapt types to Linux
3. `kernel/terminal.h` → Adapt to Linux framebuffer
4. `kernel/input/keyboard_buffer.h` → Use Linux input events
5. `kernel/video/framebuffer.h` → Use Linux framebuffer

### Phase 3: Shell and Commands

**Strategy:** Convert to standalone executables

- Port shell to run as userspace process
- Convert each command (41 total) to Linux utility
- Link against glibc or musl
- Ensure Busybox compatibility

### Phase 4: GUI Framework

**Strategy:** Use Linux graphics stack

- Adapt FlyUI to use X11 or framebuffer
- Create window manager on Linux
- Port graphics primitives to Linux

### Phase 5: Applications

**Strategy:** Convert to GTK/Qt or framebuffer apps

- Port each app (calculator, notepad, paint, etc.)
- Link to proper graphics library
- Ensure userspace isolation

## Compatibility Layers Needed

### Memory Management
```c
// Chrysalis kernel style
void* kmalloc(size_t size);
void kfree(void* ptr);

// Maps to Linux
void* malloc(size_t size);
void free(void* ptr);
```

### Filesystem
```c
// Chrysalis kernel style
int vfs_open(const char* path, int flags);

// Maps to Linux
int open(const char* path, int flags, ...);
```

### Process Management
```c
// Chrysalis kernel style
process_t* process_create(const char* name);

// Maps to Linux
pid_t fork(void);
```

### Video/Graphics
```c
// Chrysalis kernel style
void set_pixel(int x, int y, uint32_t color);

// Maps to Linux framebuffer
int fb_fd = open("/dev/fb0", ...);
write(fb_fd, pixel_data, size);
```

## Build Strategy

1. Keep original /os/ kernel code as reference
2. Create linux_port/ directory with ported code
3. Compile with:
   ```bash
   gcc -o chrysalis_daemon chrysalis_daemon.c linux_compat/*.c
   ```
4. Run as userspace process in Alpine Linux

## Component Status

| Component | Status | Effort | Priority |
|-----------|--------|--------|----------|
| Types/includes | Ready | Low | CRITICAL |
| Memory layer | Simple | Low | CRITICAL |
| Filesystem layer | Simple | Low | CRITICAL |
| Shell | Medium | Medium | HIGH |
| Commands (41x) | Medium | Medium | HIGH |
| GUI framework | Hard | High | MEDIUM |
| Applications | Hard | High | MEDIUM |

## Testing Strategy

1. **Stage 1:** Verify Linux syscall mapping works
2. **Stage 2:** Test shell with basic commands
3. **Stage 3:** Test file I/O and process creation
4. **Stage 4:** Test GUI on framebuffer
5. **Stage 5:** Test full application suite

## Success Criteria

- ✅ Chrysalis shell accessible in Alpine Linux
- ✅ Commands work via Busybox compatibility
- ✅ File system accessible
- ✅ Memory management functional
- ✅ GUI displays on boot
- ✅ Applications launch and run
- ✅ Everything boots from single ISO

