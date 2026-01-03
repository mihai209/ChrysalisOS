🟢 NIVEL 1 — Fundament (kernel minim funcțional)

✅ = deja funcțional
🔲 = urmează

| Status | Componentă          | Note                         |
| ------ | ------------------- | ---------------------------- |
| ✅      | GDT                 | corect                       |
| ✅      | IDT                 | ok                           |
| ✅      | ISR / IRQ           | stabil                       |
| ✅      | PIC 8259            | remap ok                     |
| ✅      | PIT (timer)         | funcțional                   |
| ✅      | Keyboard (IRQ1)     | funcționează                 |
| ✅      | Terminal VGA (text) | stabil                       |
| ✅     | Serial COM1         | stabil |
| ✅     | CMOS / RTC          | ușor, util                   |
| ✅      | PC Speaker          | simplu + fun                 |


🟡 NIVEL 2 — Input & timing
(organizare internă, calitate de OS)

| Status | Componentă        | De ce contează  |
| ------ | ----------------- | --------------- |
| ✅     | Mouse PS/2        | IRQ12           |
| ✅     | Keyboard buffer   | input corect    |
| ✅     | Keymap (US / RO)  | extensibil      |
| ✅     | Timer abstraction | `sleep(ms)`     |
| ✅     | Uptime / ticks    | sistem stabil   |
| da dracu stie     | Delay calibrat    | pentru drivere  |
| ✅     | Event queue       | bază pentru GUI |


🔵 NIVEL 3 — Storage
(primul „big leap”)

| Status | Componentă            | Comentariu          |
| ------ | --------------------- | ------------------- |
| ✅      | ATA PIO               | ideal pt început    |
| ✅      | Detectare HDD         | identify            |
| ✅     | Read sector           | milestone major     |
| ✅     | Write sector          | atenție la corupere |
| [N]     | Cache simplu          | performanță         |
| ✅     | Partition table (MBR) | necesar             |
| ✅     | FAT12 / FAT16         | ușor                |
| ✅     | FAT32                 | mai greu            |
| ✅     | VFS                   | arhitectură curată  |

🟣 NIVEL 4 — Memorie
(fără asta nu există multitasking real)

| Status | Componentă              | Note          |
| ------ | ----------------------- | ------------- |
| ✅     | Physical Memory Manager | bitmap        |
| ✅      | Paging x86              | schimbă jocul |
| ✅     | Virtual Memory          | izolare       |
| 🔲     | Heap kernel (`kmalloc`) | obligatoriu   |
| 🔲     | slab / buddy            | optimizare    |
| 🔲     | user memory isolation   | securitate    |

🟠 NIVEL 5 — Procese & multitasking
(când devine „OS adevărat”)

| Status | Componentă            |               |
| ------ | --------------------- | ------------- |
| 🔲     | task struct           | baza          |
| 🔲     | context switch        | greu dar fain |
| 🔲     | scheduler RR          | simplu        |
| 🔲     | kernel threads        |               |
| 🔲     | user mode             | ring 3        |
| 🔲     | syscalls (`int 0x80`) |               |
| 🔲     | ELF loader            |               |
| 🔲     | exec()                |               |

🔴 NIVEL 6 — Hardware avansat
(opțional, dar impresionant)

| Status | Componentă       |
| ------ | ---------------- |
| 🔲     | PCI bus          |
| 🔲     | ACPI             |
| 🔲     | APIC / IOAPIC    |
| 🔲     | SMP (multi-core) |
| 🔲     | HPET             |
| 🔲     | USB              |
| 🔲     | AHCI             |
| 🔲     | VESA framebuffer |
| 🔲     | GPU basic        |

🟤 NIVEL 7 — UX & tools

| Status | Componentă        |
| ------ | ----------------- |
| 🔲     | shell avansat     |
| 🔲     | piping            |
| 🔲     | scripting         |
| 🔲     | virtual terminals |
| 🔲     | cursor            |
| 🔲     | scrollback        |
| 🔲     | culori            |
| 🔲     | editor text       |
| 🔲     | tools FS          |
