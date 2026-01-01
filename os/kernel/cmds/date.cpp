#include "../terminal.h"
#include "../time/clock.h"

/* helpers locale — fără libc */

static void append_char(char* buf, int& i, char c)
{
    buf[i++] = c;
    buf[i] = 0;
}

static void append_2d(char* buf, int& i, int v)
{
    append_char(buf, i, '0' + (v / 10) % 10);
    append_char(buf, i, '0' + (v % 10));
}

static void append_4d(char* buf, int& i, int v)
{
    append_char(buf, i, '0' + (v / 1000) % 10);
    append_char(buf, i, '0' + (v / 100) % 10);
    append_char(buf, i, '0' + (v / 10) % 10);
    append_char(buf, i, '0' + (v % 10));
}

extern "C" void cmd_date(const char* args)
{
    (void)args; // momentan nu folosim argumente

    datetime t;
    time_get_local(&t);

    char buf[64];
    int i = 0;

    // format: YYYY-MM-DD HH:MM:SS
    append_4d(buf, i, t.year);
    append_char(buf, i, '-');
    append_2d(buf, i, t.month);
    append_char(buf, i, '-');
    append_2d(buf, i, t.day);

    append_char(buf, i, ' ');
    append_2d(buf, i, t.hour);
    append_char(buf, i, ':');
    append_2d(buf, i, t.minute);
    append_char(buf, i, ':');
    append_2d(buf, i, t.second);

    append_char(buf, i, '\n');

    terminal_writestring(buf);
}
