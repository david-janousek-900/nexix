#include <assert.h>
#include <stdio.h>
#include "vga.h"
static uint16_t cells[2002];
static unsigned reg, position;
void outb(uint16_t port, uint8_t value)
{
    if(port==0x3d4)reg=value;
    if(port==0x3d5 && reg==14)position=(position&255)|(value<<8);
    if(port==0x3d5 && reg==15)position=(position&0xff00)|value;
}
static void write_text(const char *s)
{for(;*s;++s)vga_write((const uint8_t *)s,1);}
int main(void)
{
    cells[0]=cells[2001]=0xbeef;
    vga_init(cells+1);
    for(unsigned i=1;i<=2000;++i)assert(cells[i]==0x0720);
    write_text("abc\bX\rY");
    assert(cells[1]==0x0759 && cells[3]==0x0758);
    write_text("\tZ");assert(cells[9]==0x075a && position==9);
    vga_init(cells+1);
    for(unsigned i=0;i<80;++i)write_text("A");
    assert(position==80);
    for(unsigned i=0;i<24;++i)write_text("\r\nB");
    assert(position==1921 && cells[1921]==0x0742);
    write_text("\r\bC");assert(position==1921 && cells[1921]==0x0743);
    assert(cells[0]==0xbeef && cells[2001]==0xbeef);
    puts("PASS: VGA controls, scrolling, cursor and bounds");
}
