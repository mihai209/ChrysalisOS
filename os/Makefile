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
	$(BUILD)/terminal.o


# -------------------------
# TARGETURI PRINCIPALE
# -------------------------

.PHONY: all iso run clean help

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
	@echo ""
