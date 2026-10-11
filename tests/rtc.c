#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "clock.h"
#include <nexix/syscall.h>
static uint8_t registers[128], selected;
static unsigned reads, changing;
static uint64_t ticks;
void outb(uint16_t port, uint8_t value) { assert(port == 0x70); assert(value < 128); selected = value; }
uint8_t inb(uint16_t port)
{
    assert(port == 0x71); ++reads;
    if (selected == 0 && changing) { ++registers[0]; registers[0] %= 60; if(changing == 1) changing = 0; }
    return registers[selected];
}
uint64_t timer_ticks(void) { return ticks; }
static void setup(void)
{
    memset(registers, 0, sizeof(registers)); reads = changing = 0;
    registers[0x0b] = 2; registers[0x0d] = 0x80;
    registers[7] = 1; registers[8] = 1; registers[9] = 0x26; registers[0x32] = 0x20;
}
static void expect(int64_t value)
{
    int64_t result = -1; assert(rtc_read_epoch(&result)); assert(result == value);
}
static void invalid(void)
{
    int64_t result = -1; assert(!rtc_read_epoch(&result)); assert(result == -1); assert(reads < 400000);
}
int main(void)
{
    setup(); expect(1767225600); /* 2026-01-01 UTC */
    registers[0x0b] = 6; registers[9] = 26; registers[0x32] = 20; expect(1767225600);
    setup(); registers[0x0b] = 0; registers[4] = 0x12; expect(1767225600);
    registers[4] = 0x92; expect(1767268800); /* noon */
    registers[4] = 0x81; expect(1767272400); /* 13:00 */
    setup(); registers[0x32] = 0; expect(1767225600);
    registers[0x32] = 0xff; expect(1767225600);
    setup(); registers[9] = 0x24; registers[8] = 2; registers[7] = 0x29; expect(1709164800);
    registers[9] = 0x23; invalid();
    setup(); registers[9] = 0; registers[0x32] = 0x21; registers[8] = 2; registers[7] = 0x29; invalid();
    setup(); registers[0] = 0x6a; invalid();
    setup(); registers[4] = 0x24; invalid();
    setup(); registers[8] = 0; invalid();
    setup(); registers[0x0d] = 0; invalid();
    setup(); registers[0x0a] = 0x80; invalid();
    setup(); registers[0x0b] |= 0x80; invalid();
    setup(); changing = 2; invalid();
    setup(); changing = 1; expect(1767225601);
    setup(); ticks = 250; assert(clock_init()); assert(clock_now() == 1767225600);
    ticks += NX_CLK_TCK; assert(clock_now() == 1767225601);
    assert(clock_set(123)); assert(clock_now() == 123);
    assert(!clock_set(-1)); assert(!clock_set(INT64_MAX)); assert(clock_now() == 123);
    ticks += NX_CLK_TCK; assert(clock_now() == 124);
    puts("PASS: RTC formats, UTC calendar, century fallback, invalid/stuck/unstable RTC, clock ticks and stime");
}
