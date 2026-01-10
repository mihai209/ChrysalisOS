#include "bmp.h"
#include "../../mem/kmalloc.h"
#include "../../cmds/fat.h" /* Pentru fat32_read_file */
#include "../../drivers/serial.h"

/* Structuri BMP (packed) */
#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;      /* 'BM' */
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;   /* Offset către datele pixelilor */
} BITMAPFILEHEADER;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BITMAPINFOHEADER;
#pragma pack(pop)

extern void serial(const char *fmt, ...);
extern int fat32_read_file_offset(const char* path, void* buf, uint32_t size, uint32_t offset);
extern int fat32_read_file(const char* path, void* buf, uint32_t max_size);

int fly_load_bmp_to_surface(surface_t* surf, const char* path) {
    if (!surf || !path) return -1;

    /* 1. Alocăm un buffer temporar pentru a citi header-ul și a afla dimensiunea */
    /* Citim primii 54 bytes (FileHeader + InfoHeader standard) */
    uint8_t header_buf[54];
    if (fat32_read_file(path, header_buf, 54) < 54) {
        serial("[BMP] Error: Could not read BMP header for %s\n", path);
        return -1;
    }

    BITMAPFILEHEADER* fileHeader = (BITMAPFILEHEADER*)header_buf;
    BITMAPINFOHEADER* infoHeader = (BITMAPINFOHEADER*)(header_buf + sizeof(BITMAPFILEHEADER));

    if (fileHeader->bfType != 0x4D42) { /* 'BM' */
        serial("[BMP] Error: Not a valid BMP file (Magic: %x)\n", fileHeader->bfType);
        return -1;
    }

    uint32_t fileSize = fileHeader->bfSize;
    uint32_t dataOffset = fileHeader->bfOffBits;
    int32_t width = infoHeader->biWidth;
    int32_t height = infoHeader->biHeight;
    uint16_t bpp = infoHeader->biBitCount;

    serial("[BMP] Loading %s: %dx%d, %d bpp, Size: %u\n", path, width, height, bpp, fileSize);

    if (bpp != 24 && bpp != 32) {
        serial("[BMP] Error: Only 24 and 32 bpp BMPs are supported.\n");
        return -1;
    }

    /* 2. Citim secvențial rând cu rând pentru a economisi memorie */
    /* Padding la 4 bytes per rând */
    int rowSize = ((width * bpp + 31) / 32) * 4;
    
    uint8_t* rowBuffer = (uint8_t*)kmalloc(rowSize);
    if (!rowBuffer) {
        serial("[BMP] Error: Out of memory for row buffer.\n");
        return -1;
    }

    /* Iterăm prin rândurile imaginii (în fișier) */
    /* BMP stochează de obicei bottom-up. Rândul 0 din fișier este rândul de jos al imaginii. */
    int absHeight = (height > 0) ? height : -height;

    for (int i = 0; i < absHeight; i++) {
        /* Citim rândul i din fișier */
        uint32_t filePos = dataOffset + i * rowSize;
        if (fat32_read_file_offset(path, rowBuffer, rowSize, filePos) != rowSize) {
            serial("[BMP] Error reading row %d\n", i);
            break;
        }

        /* Calculăm poziția pe ecran */
        /* Dacă height > 0 (bottom-up), rândul 0 din fișier este ultimul rând de pe ecran */
        int screenY = (height > 0) ? (absHeight - 1 - i) : i;
        
        if (screenY >= (int)surf->height) continue;

        for (int x = 0; x < width; x++) {
            if (x >= (int)surf->width) break;

            uint8_t* px = rowBuffer + (x * (bpp / 8));
            uint32_t color = 0;

            if (bpp == 24) {
                /* BGR -> ARGB */
                uint8_t b = px[0];
                uint8_t g = px[1];
                uint8_t r = px[2];
                color = 0xFF000000 | (r << 16) | (g << 8) | b;
            } else if (bpp == 32) {
                /* BGRA -> ARGB */
                uint8_t b = px[0];
                uint8_t g = px[1];
                uint8_t r = px[2];
                uint8_t a = px[3]; /* Alpha channel might be ignored or used */
                color = 0xFF000000 | (r << 16) | (g << 8) | b; 
                /* Notă: Multe BMP-uri 32bpp au alpha 0, deci forțăm FF la alpha pentru vizibilitate */
            }

            /* Desenăm direct în bufferul suprafeței */
            surf->pixels[screenY * surf->width + x] = color;
        }
    }

    kfree(rowBuffer);
    return 0;
}
