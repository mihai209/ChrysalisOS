#pragma once
#include "../../video/surface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Încarcă un fișier BMP de la calea specificată și îl desenează pe suprafață.
   Returnează 0 la succes, -1 la eroare. */
int fly_load_bmp_to_surface(surface_t* surf, const char* path);

#ifdef __cplusplus
}
#endif
