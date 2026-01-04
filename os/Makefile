CXX = g++
CC  = gcc
AS  = nasm
LD  = ld

# flags comune
COMMON_FLAGS = -ffreestanding -O2 -Wall -Wextra -Ikernel/include -m32
# CFLAGS += -DSCHED_COOPERATIVE   # uncomment = fallback safe mode

# C / C++
CFLAGS   = $(COMMON_FLAGS)
CXXFLAGS = $(COMMON_FLAGS) -fno-exceptions -fno-rtti

ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld

BUILD = build
ISO = iso
KERNEL = kernel.bin

OBJS = \
	$(BUILD)/boot.o \
	$(BUILD)/gdt_flush.o \
	$(BUILD)/gdt.o \
	$(BUILD)/kernel.o \
	$(BUILD)/idt.o \
	$(BUILD)/irq.o \
	$(BUILD)/interrupts.o \
	$(BUILD)/keyboard.o \
	$(BUILD)/pic.o \
	$(BUILD)/shell.o \
	$(BUILD)/isr.o \
	$(BUILD)/terminal.o \
	$(BUILD)/cmd_clear.o \
	$(BUILD)/cmd_reboot.o \
	$(BUILD)/cmd_shutdown.o \
	$(BUILD)/cmd_registry.o \
	$(BUILD)/bootlogo.o \
	$(BUILD)/fs.o \
	$(BUILD)/cmd_ls.o \
	$(BUILD)/cmd_cat.o \
	$(BUILD)/cmd_touch.o \
	$(BUILD)/pit.o \
	$(BUILD)/load.o \
	$(BUILD)/serial.o \
	$(BUILD)/rtc.o \
	$(BUILD)/clock.o \
	$(BUILD)/cmd_date.o \
	$(BUILD)/speaker.o \
	$(BUILD)/cmd_beep.o \
	$(BUILD)/audio.o \
	$(BUILD)/shortcuts.o \
	$(BUILD)/string.o \
	$(BUILD)/cmd_play.o \
	$(BUILD)/mlb.o \
	$(BUILD)/mouse.o \
	$(BUILD)/keyboard_buffer.o \
	$(BUILD)/keymap.o \
	$(BUILD)/keymap_us.o \
	$(BUILD)/keymap_ro.o \
	$(BUILD)/vga.o \
	$(BUILD)/timer.o \
	$(BUILD)/cmd_uptime.o \
	$(BUILD)/cmd_ticks.o \
	$(BUILD)/cmd_help.o \
	$(BUILD)/event_queue.o \
	$(BUILD)/ata.o \
	$(BUILD)/disk.o \
	$(BUILD)/fat_fs.o \
	$(BUILD)/cmd_fat.o \
	$(BUILD)/vfs.o \
	$(BUILD)/ramfs.o \
	$(BUILD)/cmd_vfs.o \
	$(BUILD)/pmm.o \
	$(BUILD)/cmd_pmm.o \
	$(BUILD)/paging.o \
	$(BUILD)/user.o \
	$(BUILD)/cmd_login.o \
	$(BUILD)/mem/kmalloc.o \
	$(BUILD)/mem/kmalloc_init.o \
	$(BUILD)/mem/buddy.o \
	$(BUILD)/mem/slab.o \
	$(BUILD)/user_blob.o \
	$(BUILD)/panic.o \
	$(BUILD)/cmd_crash.o \
	$(BUILD)/panic_sys.o \
	$(BUILD)/cmd_mem.o \
	$(BUILD)/address_space.o \
	$(BUILD)/user_mem.o \
	$(BUILD)/copy_user.o \
	$(BUILD)/vmm.o \
	$(BUILD)/mm/paging.o \
	$(BUILD)/load_cr3.o \
	$(BUILD)/task/task.o \
    $(BUILD)/task/sched.o \
    $(BUILD)/arch/switch.o \
	$(BUILD)/sched/pcb.o \
    $(BUILD)/sched/scheduler.o \
	$(BUILD)/ram.o \
	$(BUILD)/tpm.o \
	$(BUILD)/videomemory.o \
	$(BUILD)/cmd_credits.o \
	$(BUILD)/cmd_echo.o \
    $(BUILD)/cmd_fortune.o \
    $(BUILD)/cmd_buildinfo.o \
    $(BUILD)/cmd_sysfetch.o \
    $(BUILD)/cmd_chrysver.o \
	build/arch/tss.o \
    build/arch/tss_flush.o \




# -------------------------
# TARGETURI PRINCIPALE
# -------------------------

.PHONY: all iso run clean help consolerun

all: $(ISO)/boot/$(KERNEL)

$(BUILD):
	mkdir -p $(BUILD)

# -------------------------
# ASSEMBLY
# -------------------------

$(BUILD)/boot.o: kernel/boot.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/gdt_flush.o: kernel/arch/i386/gdt.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/interrupts.o: kernel/interrupts/interrupts.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

# -------------------------
# C++ OBJECTS
# -------------------------

$(BUILD)/kernel.o: kernel/kernel.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/idt.o: kernel/arch/i386/idt.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/irq.o: kernel/interrupts/irq.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/keyboard.o: kernel/drivers/keyboard.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/terminal.o: kernel/terminal.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/pic.o: kernel/drivers/pic.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/gdt.o: kernel/arch/i386/gdt.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/isr.o: kernel/interrupts/isr.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/shell.o: kernel/shell/shell.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_clear.o: kernel/cmds/clear.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_reboot.o: kernel/cmds/reboot.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_shutdown.o: kernel/cmds/shutdown.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_registry.o: kernel/cmds/registry.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/fs.o: kernel/fs/fs.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_ls.o: kernel/cmds/ls.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_cat.o: kernel/cmds/cat.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_touch.o: kernel/cmds/touch.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/bootlogo.o: kernel/bootlogo/bootlogo.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/pit.o: kernel/drivers/pit.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/load.o: kernel/debug/load.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/serial.o: kernel/drivers/serial.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/rtc.o: kernel/drivers/rtc.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/clock.o: kernel/time/clock.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_date.o: kernel/cmds/date.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/speaker.o: kernel/drivers/speaker.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_beep.o: kernel/cmds/beep.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/audio.o: kernel/drivers/audio/audio.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/shortcuts.o: kernel/shortcuts/shortcuts.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/string.o: kernel/string.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_play.o: kernel/cmds/play.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/mlb.o: kernel/mlb/mlb.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/mouse.o: kernel/drivers/mouse.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/keyboard_buffer.o: kernel/input/keyboard_buffer.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/keymap.o: kernel/keyboard/keymap.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/keymap_us.o: kernel/keyboard/keymap_us.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/keymap_ro.o: kernel/keyboard/keymap_ro.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/vga.o: kernel/video/vga.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/timer.o: kernel/time/timer.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/cmd_uptime.o: kernel/cmds/uptime.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_ticks.o: kernel/cmds/ticks.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_help.o: kernel/cmds/help.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/event_queue.o: kernel/events/event_queue.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/disk.o: kernel/cmds/disk.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_fat.o: kernel/cmds/fat.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/vfs.o: kernel/fs/vfs/vfs.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ramfs.o: kernel/fs/ramfs/ramfs.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/cmd_vfs.o: kernel/cmds/vfs.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/user.o: kernel/user/user.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_login.o: kernel/cmds/login.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/panic.o: kernel/panic.cpp kernel/panic.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_crash.o: kernel/cmds/crash.cpp kernel/cmds/crash.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/vmm.o: kernel/mm/vmm.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# -------------------------
# C OBJECTS (folosim gcc)
# -------------------------

$(BUILD)/ata.o: kernel/storage/ata.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/fat_fs.o: kernel/fs/fat/fat.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/pmm.o: kernel/memory/pmm.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/cmd_pmm.o: kernel/cmds/pmm.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/paging.o: kernel/paging.c kernel/paging.h | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/mem/%.o: kernel/mem/%.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Ikernel/include -ffreestanding -c $< -o $@

$(BUILD)/panic_sys.o: kernel/panic_sys.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@


build/user_prog.bin: kernel/user/user_prog.S
	$(AS) -f bin $< -o $@

$(BUILD)/user_blob.o: kernel/user/user_blob.c build/user_prog.bin | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_mem.o: kernel/cmds/mem.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/address_space.o: kernel/mm/address_space.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/user_mem.o: kernel/mm/user_mem.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/copy_user.o: kernel/mm/copy_user.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/vmm.o: kernel/mm/vmm.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/mm/paging.o: kernel/mm/paging.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/load_cr3.o: arch/x86/load_cr3.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/task/%.o: kernel/task/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Ikernel/include -c $< -o $@

# compile assembly (AT&T) for i386
$(BUILD)/arch/%.o: arch/i386/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -m32 -ffreestanding -c $< -o $@


$(BUILD)/sched/pcb.o: kernel/sched/pcb.c kernel/sched/pcb.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -m32 -c $< -o $@

$(BUILD)/sched/scheduler.o: kernel/sched/scheduler.c kernel/sched/scheduler.h kernel/sched/pcb.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -m32 -c $< -o $@

$(BUILD)/ram.o: kernel/detect/ram.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/tpm.o: kernel/detect/tpm.c
	$(CC) $(CFLAGS) -Ikernel/include -c $< -o $@

$(BUILD)/videomemory.o: kernel/detect/videomemory.c
	$(CC) $(CFLAGS) -Ikernel/include -c $< -o $@

$(BUILD)/cmd_credits.o: kernel/cmds/credits.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@


$(BUILD)/cmd_echo.o: kernel/cmds/echo.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_fortune.o: kernel/cmds/fortune.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_buildinfo.o: kernel/cmds/buildinfo.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_sysfetch.o: kernel/cmds/sysfetch.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_chrysver.o: kernel/cmds/chrysver.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/arch/tss.o: kernel/arch/i386/tss.c
	$(CC) $(CFLAGS) -c $< -o $@

build/arch/tss_flush.o: kernel/arch/i386/tss_flush.S
	$(AS) $(ASFLAGS) $< -o $@

# -------------------------
# LINK
# -------------------------

$(ISO)/boot/$(KERNEL): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# -------------------------
# ISO & RUN
# -------------------------

iso: all
	grub-mkrescue -o chrysalis.iso $(ISO)

run: iso
	qemu-system-x86_64 -cdrom chrysalis.iso -m 512

consolerun: iso
	qemu-system-x86_64 -cdrom chrysalis.iso -nographic

clean:
	rm -rf $(BUILD) *.iso $(ISO)/boot/$(KERNEL)

# -------------------------
# HELP
# -------------------------

help:
	@echo ""
	@echo "Chrysalis OS - Makefile commands"
	@echo "--------------------------------"
	@echo "make           -> compilează kernelul"
	@echo "make iso       -> creează imaginea bootabilă ISO"
	@echo "make run       -> rulează OS-ul în QEMU (cu grafică)"
	@echo "make consolerun-> rulează OS-ul în QEMU (doar consolă)"
	@echo "make clean     -> șterge fișierele generate"
	@echo "make help      -> afișează acest mesaj"
	@echo ""