CXX = g++
AS = nasm
LD = ld

CXXFLAGS = -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti
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

# -------------------------
# C++ OBJECTS
# -------------------------

$(BUILD)/kernel.o: kernel/kernel.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/idt.o: kernel/arch/i386/idt.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/irq.o: kernel/interrupts/irq.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/keyboard.o: kernel/drivers/keyboard.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/terminal.o: kernel/terminal.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/interrupts.o: kernel/interrupts/interrupts.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/pic.o: kernel/drivers/pic.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/gdt.o: kernel/arch/i386/gdt.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/gdt_flush.o: kernel/arch/i386/gdt.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/isr.o: kernel/interrupts/isr.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/shell.o: kernel/shell/shell.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/cmd_clear.o: kernel/cmds/clear.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/cmd_reboot.o: kernel/cmds/reboot.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/cmd_shutdown.o: kernel/cmds/shutdown.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/cmd_registry.o: kernel/cmds/registry.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/fs.o: kernel/fs/fs.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/cmd_ls.o: kernel/cmds/ls.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/cmd_cat.o: kernel/cmds/cat.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/cmd_touch.o: kernel/cmds/touch.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/bootlogo.o: kernel/bootlogo/bootlogo.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/pit.o: kernel/drivers/pit.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/load.o: kernel/debug/load.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/serial.o: kernel/drivers/serial.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/rtc.o: kernel/drivers/rtc.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/clock.o: kernel/time/clock.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/cmd_date.o: kernel/cmds/date.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/speaker.o: kernel/drivers/speaker.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/cmd_beep.o: kernel/cmds/beep.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/audio.o: kernel/drivers/audio/audio.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/shortcuts.o: kernel/shortcuts/shortcuts.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/string.o: kernel/string.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/cmd_play.o: kernel/cmds/play.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(BUILD)/mlb.o: kernel/mlb/mlb.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

# -------------------------
# LINK
# -------------------------

$(ISO)/boot/$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# -------------------------
# UTILITARE
# -------------------------

iso: all
	grub-mkrescue -o chrysalis.iso $(ISO)

run: iso
	qemu-system-x86_64 -cdrom chrysalis.iso

clean:
	rm -rf build *.iso

consolerun: iso
	qemu-system-x86_64 -cdrom chrysalis.iso -nographic


# -------------------------
# HELP
# -------------------------

help:
	@echo ""
	@echo "Chrysalis OS - Makefile commands"
	@echo "--------------------------------"
	@echo "make           -> compileaza kernelul"
	@echo "make iso       -> creeaza imaginea bootabila ISO"
	@echo "make run       -> ruleaza OS-ul in QEMU"
	@echo "make clean     -> sterge fisierele generate"
	@echo "make help      -> afiseaza acest mesaj"
	@echo "make consolerun -> ruleaza in terminal qemu"
	@echo "npm run audiorun -> ruleaza audio in qemu"
	@echo ""
