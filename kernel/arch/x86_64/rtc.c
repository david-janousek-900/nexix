/* MC146818-compatible RTC on the initial QEMU PC platform. Boot only,
 * with maskable interrupts disabled; never access CMOS from an IRQ handler.
 * Register 0x32 is the PC/QEMU century byte. If unavailable (0/0xff),
 * interpret the two-digit year as 2000..2099. */
#include "clock.h"
#include "io.h"

static uint8_t read_register(uint8_t reg)
{
    outb(0x70, reg); /* Leave NMI enabled. */
    io_wait();
    return inb(0x71);
}
struct snapshot { uint8_t second, minute, hour, day, month, year, century, mode; };
static bool snapshot(struct snapshot *s)
{
    if (read_register(0x0a) & 0x80) return false;
    s->mode = read_register(0x0b);
    s->second = read_register(0); s->minute = read_register(2);
    s->hour = read_register(4); s->day = read_register(7);
    s->month = read_register(8); s->year = read_register(9);
    s->century = read_register(0x32);
    return !(read_register(0x0a) & 0x80) && !(s->mode & 0x80) &&
        s->mode == read_register(0x0b) && (read_register(0x0d) & 0x80);
}
static int decode(uint8_t value, bool binary)
{
    if (binary) return value;
    if ((value & 15) > 9 || (value >> 4) > 9) return -1;
    return (value >> 4) * 10 + (value & 15);
}
static bool leap(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
bool rtc_read_epoch(int64_t *epoch)
{
    struct snapshot a, b;
    bool stable = false;
    /* Bounded even for absent hardware, stuck UIP, or continuous updates. */
    for (unsigned attempt = 0; attempt < 10000; ++attempt) {
        if (!snapshot(&a) || !snapshot(&b)) continue;
        const uint8_t *x = (const uint8_t *)&a, *y = (const uint8_t *)&b;
        stable = true;
        for (unsigned i = 0; i < sizeof(a); ++i) if (x[i] != y[i]) stable = false;
        if (stable) break;
    }
    if (!stable) return false;
    bool binary = b.mode & 4;
    int second = decode(b.second, binary), minute = decode(b.minute, binary);
    int hour = decode(b.hour & 0x7f, binary), day = decode(b.day, binary);
    int month = decode(b.month, binary), year = decode(b.year, binary);
    int century = b.century == 0 || b.century == 0xff ? 20 : decode(b.century, binary);
    if (!(b.mode & 2)) {
        if (hour < 1 || hour > 12) return false;
        hour = hour % 12 + ((b.hour & 0x80) ? 12 : 0);
    } else if (b.hour & 0x80) return false;
    if (year < 0 || year > 99 || century < 19 || century > 99) return false;
    year += century * 100;
    static const unsigned lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (year < 1970 || month < 1 || month > 12 || day < 1 ||
        day > (int)(lengths[month-1] + (month == 2 && leap(year))) ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
        return false;
    int64_t days = 0;
    for (int y = 1970; y < year; ++y) days += 365 + leap(y);
    for (int m = 1; m < month; ++m) days += lengths[m-1] + (m == 2 && leap(year));
    *epoch = ((days + day - 1) * 24 + hour) * 3600 + minute * 60 + second;
    return true;
}
