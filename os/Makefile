CXX = g++
CC = gcc
AS = nasm
LD = ld

# flags comune
COMMON_FLAGS = -ffreestanding -O2 -Wall -Wextra -Ikernel/include -m32
# CFLAGS += -DSCHED_COOPERATIVE # uncomment = fallback safe mode

# C / C++
CFLAGS = $(COMMON_FLAGS)
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
	$(BUILD)/arch/tss.o \
	$(BUILD)/arch/tss_flush.o \
	$(BUILD)/arch/syscall.o \
	$(BUILD)/arch/syscall_asm.o \
	$(BUILD)/arch/isr_default.o \
	$(BUILD)/arch/syscall_dispatch.o \
	$(BUILD)/elf.o \
	$(BUILD)/process.o \
	$(BUILD)/exec.o \
	$(BUILD)/stubs/exec_fallbacks.o \
	$(BUILD)/cmds/elf.o \
	$(BUILD)/cmds/elf_debug.o \
	$(BUILD)/cmds/elf_crash.o \
	$(BUILD)/hardware/pci.o \
	$(BUILD)/hardware/acpi.o \
	$(BUILD)/arch/interrupts.o \
	$(BUILD)/hardware/msr.o \
	$(BUILD)/hardware/apic.o \
	$(BUILD)/hardware/lapic.o \
	$(BUILD)/hardware/ioapic.o \
	$(BUILD)/hardware/hpet.o \
	$(BUILD)/usb/usb_core.o \
	$(BUILD)/usb/uhci.o \
	$(BUILD)/usb/usb_hid.o \
	$(BUILD)/usb/usb_hub.o \
	$(BUILD)/usb/usb_msc.o \
	$(BUILD)/smp/smp.o \
	$(BUILD)/smp/trampoline_obj.o \



# -------------------------
# TARGETURI PRINCIPALE
# -------------------------

.PHONY: all iso run clean help consolerun dirs

all: $(ISO)/boot/$(KERNEL)

$(BUILD):
	mkdir -p $(BUILD)

dirs: $(BUILD)
	mkdir -p $(BUILD)/elf
	mkdir -p $(BUILD)/proc
	mkdir -p $(BUILD)/stubs
	mkdir -p $(BUILD)/cmds
	mkdir -p $(BUILD)/arch
	mkdir -p $(BUILD)/arch/i386
	mkdir -p $(BUILD)/arch/i386/cpu
	mkdir -p $(BUILD)/arch/i386/apic
	mkdir -p $(BUILD)/smp
	mkdir -p $(BUILD)/hardware



# -------------------------
# ASSEMBLY
# -------------------------

$(BUILD)/boot.o: kernel/boot.asm | dirs
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/gdt_flush.o: kernel/arch/i386/gdt.asm | dirs
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/interrupts.o: kernel/interrupts/interrupts.asm | dirs
	$(AS) $(ASFLAGS) $< -o $@

# -------------------------
# C++ OBJECTS
# -------------------------

$(BUILD)/kernel.o: kernel/kernel.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/idt.o: kernel/arch/i386/idt.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/irq.o: kernel/interrupts/irq.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/keyboard.o: kernel/drivers/keyboard.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/terminal.o: kernel/terminal.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/pic.o: kernel/drivers/pic.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/gdt.o: kernel/arch/i386/gdt.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/isr.o: kernel/interrupts/isr.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/shell.o: kernel/shell/shell.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_clear.o: kernel/cmds/clear.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_reboot.o: kernel/cmds/reboot.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_shutdown.o: kernel/cmds/shutdown.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_registry.o: kernel/cmds/registry.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/fs.o: kernel/fs/fs.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_ls.o: kernel/cmds/ls.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_cat.o: kernel/cmds/cat.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_touch.o: kernel/cmds/touch.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/bootlogo.o: kernel/bootlogo/bootlogo.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/pit.o: kernel/drivers/pit.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/load.o: kernel/debug/load.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/serial.o: kernel/drivers/serial.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/rtc.o: kernel/drivers/rtc.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/clock.o: kernel/time/clock.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_date.o: kernel/cmds/date.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/speaker.o: kernel/drivers/speaker.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_beep.o: kernel/cmds/beep.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/audio.o: kernel/drivers/audio/audio.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/shortcuts.o: kernel/shortcuts/shortcuts.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/string.o: kernel/string.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_play.o: kernel/cmds/play.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/mlb.o: kernel/mlb/mlb.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/mouse.o: kernel/drivers/mouse.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/keyboard_buffer.o: kernel/input/keyboard_buffer.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/keymap.o: kernel/keyboard/keymap.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/keymap_us.o: kernel/keyboard/keymap_us.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/keymap_ro.o: kernel/keyboard/keymap_ro.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/vga.o: kernel/video/vga.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/timer.o: kernel/time/timer.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/cmd_uptime.o: kernel/cmds/uptime.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_ticks.o: kernel/cmds/ticks.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_help.o: kernel/cmds/help.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/event_queue.o: kernel/events/event_queue.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/disk.o: kernel/cmds/disk.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_fat.o: kernel/cmds/fat.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/vfs.o: kernel/fs/vfs/vfs.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ramfs.o: kernel/fs/ramfs/ramfs.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/cmd_vfs.o: kernel/cmds/vfs.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/user.o: kernel/user/user.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_login.o: kernel/cmds/login.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/panic.o: kernel/panic.cpp kernel/panic.h | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_crash.o: kernel/cmds/crash.cpp kernel/cmds/crash.h | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

# -------------------------
# C OBJECTS (folosim gcc)
# -------------------------

$(BUILD)/ata.o: kernel/storage/ata.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/fat_fs.o: kernel/fs/fat/fat.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/pmm.o: kernel/memory/pmm.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/cmd_pmm.o: kernel/cmds/pmm.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/paging.o: kernel/paging.c kernel/paging.h | dirs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/mem/%.o: kernel/mem/%.c | dirs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Ikernel/include -ffreestanding -c $< -o $@

$(BUILD)/panic_sys.o: kernel/panic_sys.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

build/user_prog.bin: kernel/user/user_prog.S | dirs
	$(AS) -f bin $< -o $@

$(BUILD)/user_blob.o: kernel/user/user_blob.c build/user_prog.bin | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_mem.o: kernel/cmds/mem.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/address_space.o: kernel/mm/address_space.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/user_mem.o: kernel/mm/user_mem.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/copy_user.o: kernel/mm/copy_user.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/vmm.o: kernel/mm/vmm.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/mm/paging.o: kernel/mm/paging.c | dirs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/load_cr3.o: arch/x86/load_cr3.S | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/task/%.o: kernel/task/%.c | dirs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Ikernel/include -c $< -o $@

# compile assembly (AT&T) for i386
$(BUILD)/arch/%.o: arch/i386/%.S | dirs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -m32 -ffreestanding -c $< -o $@

$(BUILD)/sched/pcb.o: kernel/sched/pcb.c kernel/sched/pcb.h | dirs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -m32 -c $< -o $@

$(BUILD)/sched/scheduler.o: kernel/sched/scheduler.c kernel/sched/scheduler.h kernel/sched/pcb.h | dirs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -m32 -c $< -o $@

$(BUILD)/ram.o: kernel/detect/ram.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/tpm.o: kernel/detect/tpm.c | dirs
	$(CC) $(CFLAGS) -Ikernel/include -c $< -o $@

$(BUILD)/videomemory.o: kernel/detect/videomemory.c | dirs
	$(CC) $(CFLAGS) -Ikernel/include -c $< -o $@

$(BUILD)/cmd_credits.o: kernel/cmds/credits.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_echo.o: kernel/cmds/echo.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_fortune.o: kernel/cmds/fortune.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_buildinfo.o: kernel/cmds/buildinfo.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_sysfetch.o: kernel/cmds/sysfetch.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmd_chrysver.o: kernel/cmds/chrysver.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/arch/tss.o: kernel/arch/i386/tss.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/arch/tss_flush.o: kernel/arch/i386/tss_flush.S | dirs
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/arch/syscall.o: kernel/arch/i386/syscall.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/arch/syscall_asm.o: kernel/arch/i386/syscall.S | dirs
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/arch/isr_default.o: kernel/arch/i386/isr_default.S | dirs
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/arch/syscall_dispatch.o: kernel/arch/i386/syscall_dispatch.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/elf.o: kernel/elf/elf.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/process.o: kernel/proc/process.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/exec.o: kernel/proc/exec.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/stubs/exec_fallbacks.o: kernel/stubs/exec_fallbacks.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/cmds/elf.o: kernel/cmds/elf.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmds/elf_debug.o: kernel/cmds/elf_debug.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cmds/elf_crash.o: kernel/cmds/elf_crash.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/hardware/pci.o: kernel/hardware/pci.cpp
	@mkdir -p $(BUILD)/hardware
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/hardware/acpi.o: kernel/hardware/acpi.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/arch/interrupts.o: kernel/arch/i386/interrupts.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/hardware/msr.o: kernel/hardware/msr.c | dirs
	@mkdir -p $(BUILD)/hardware
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/hardware/apic.o: kernel/hardware/apic.c | dirs
	@mkdir -p $(BUILD)/hardware
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/hardware/lapic.o: kernel/hardware/lapic.c | dirs
	@mkdir -p $(BUILD)/hardware
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/hardware/ioapic.o: kernel/hardware/ioapic.c | dirs
	@mkdir -p $(BUILD)/hardware
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/hardware/hpet.o: kernel/hardware/hpet.cpp | dirs
	@mkdir -p $(BUILD)/hardware
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/usb/usb_core.o: kernel/usb/usb_core.c | dirs
	@mkdir -p $(BUILD)/usb
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/usb/uhci.o: kernel/usb/uhci.c | dirs
	@mkdir -p $(BUILD)/usb
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/usb/usb_hid.o: kernel/usb/usb_hid.c | dirs
	@mkdir -p $(BUILD)/usb
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/usb/usb_hub.o: kernel/usb/usb_hub.c | dirs
	@mkdir -p $(BUILD)/usb
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/usb/usb_msc.o: kernel/usb/usb_msc.c | dirs
	@mkdir -p $(BUILD)/usb
	$(CC) $(CFLAGS) -c $< -o $@

# -------------------------
# SMP & Trampoline
# -------------------------

$(BUILD)/smp/smp.o: kernel/smp/smp.c kernel/smp/smp.h | dirs
	$(CC) $(CFLAGS) -c $< -o $@

# Rules for the trampoline
# This creates a flat binary of the trampoline code
$(BUILD)/smp/trampoline.bin: kernel/smp/trampoline.S | dirs
	$(AS) -f bin $< -o $@

# Convert binary trampoline to object file to link with kernel
$(BUILD)/smp/trampoline_obj.o: $(BUILD)/smp/trampoline.bin
	objcopy -I binary -O elf32-i386 -B i386 $< $@

# Note: You will need to ensure trampoline.bin is available to the kernel, e.g., via GRUB module or embedding.


# -------------------------
# LINK
# -------------------------

$(ISO)/boot/$(KERNEL): $(OBJS) | dirs
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