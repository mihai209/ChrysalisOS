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
	$(BUILD)/kernel.o

# -------------------------
# TARGETURI PRINCIPALE
# -------------------------

.PHONY: all iso run clean help

all: $(ISO)/boot/$(KERNEL)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: kernel/boot.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/kernel.o: kernel/kernel.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $@

$(ISO)/boot/$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# -------------------------
# COMENZI UTILE
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
