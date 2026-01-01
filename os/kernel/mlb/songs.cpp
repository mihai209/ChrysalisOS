#include <stdint.h>

/* structura unei note */
struct note_t {
    uint16_t freq;
    uint16_t duration_ms;
};

/* ===== SONGS ===== */

static const note_t song_beep[] = {
    {440, 300},
    {0,   100},
    {660, 300},
    {0,   100},
    {880, 400},
};

static const note_t song_alarm[] = {
    {880, 200},
    {0, 100},
    {880, 200},
    {0, 100},
    {880, 400},
};

struct song_entry {
    const char* name;
    const note_t* notes;
    int count;
};

const song_entry mlb_songs[] = {
    { "beep",  song_beep,  sizeof(song_beep)/sizeof(note_t) },
    { "alarm", song_alarm, sizeof(song_alarm)/sizeof(note_t) },
};

const int mlb_song_count = sizeof(mlb_songs) / sizeof(mlb_songs[0]);
