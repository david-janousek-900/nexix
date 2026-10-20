#include "vga.h"
#include "io.h"
#include <stdbool.h>
#define WIDTH 80U
#define HEIGHT 25U
static volatile uint16_t *screen;
static unsigned row, column;
static uint8_t attribute=7;
static unsigned escape,parameters[8],parameter_count,sequence_length;
static bool invalid_sequence;
static uint16_t blank(void){return (uint16_t)attribute<<8|' ';}
static void erase(unsigned first,unsigned last)
{for(unsigned i=first;i<=last && i<WIDTH*HEIGHT;++i)screen[i]=blank();}
static unsigned parameter(unsigned n,unsigned fallback)
{return n<parameter_count && parameters[n]?parameters[n]:fallback;}
static void control(uint8_t command)
{
    unsigned n=parameter(0,1),position=row*WIDTH+column;
    switch(command){
    case 'H':case 'f':row=parameter(0,1)-1;column=parameter(1,1)-1;
        if(row>=HEIGHT)row=HEIGHT-1;
        if(column>=WIDTH)column=WIDTH-1;
        break;
    case 'A':row=n>row?0:row-n;break;
    case 'B':row=n>=HEIGHT-row?HEIGHT-1:row+n;break;
    case 'C':column=n>=WIDTH-column?WIDTH-1:column+n;break;
    case 'D':column=n>column?0:column-n;break;
    case 'J':
        if(parameters[0]==0)erase(position,WIDTH*HEIGHT-1);
        else if(parameters[0]==1)erase(0,position);
        else if(parameters[0]==2)erase(0,WIDTH*HEIGHT-1);
        break;
    case 'K':
        if(parameters[0]==0)erase(position,(row+1)*WIDTH-1);
        else if(parameters[0]==1)erase(row*WIDTH,position);
        else if(parameters[0]==2)erase(row*WIDTH,(row+1)*WIDTH-1);
        break;
    case 'm':
        for(unsigned i=0;i<parameter_count;++i){
            if(parameters[i]==0 || parameters[i]==27)attribute=7;
            else if(parameters[i]==7)attribute=0x70;
            else if(parameters[i]==1)attribute|=8;
            else if(parameters[i]==22)attribute&=~8U;
        }
        break;
    }
}
static bool sequence(uint8_t c)
{
    if(c==27){escape=1;return true;}
    if(!escape)return false;
    if(escape==1){
        escape=0;
        if(c=='['){escape=2;parameter_count=1;sequence_length=0;invalid_sequence=false;
            for(unsigned i=0;i<8;++i)parameters[i]=0;}
        return true;
    }
    if(++sequence_length>64)invalid_sequence=true;
    if(c>='0' && c<='9'){
        unsigned *value=&parameters[parameter_count-1];
        if(*value>999)invalid_sequence=true;
        if(!invalid_sequence)*value=*value*10+(c-'0');
    }else if(c==';'){
        if(parameter_count==8)invalid_sequence=true;
        else ++parameter_count;
    }else if(c>=0x40 && c<=0x7e){
        if(!invalid_sequence)control(c);
        escape=0;
    }else invalid_sequence=true;
    return true;
}
static void cursor(void)
{
    unsigned position = row * WIDTH + column;
    outb(0x3d4, 14); outb(0x3d5, position >> 8);
    outb(0x3d4, 15); outb(0x3d5, position & 255);
}
static void advance(void)
{
    if (column == WIDTH) { column = 0; ++row; }
    if (row < HEIGHT) return;
    for (unsigned i = 0; i < WIDTH * (HEIGHT - 1); ++i)
        screen[i] = screen[i + WIDTH];
    for (unsigned i = WIDTH * (HEIGHT - 1); i < WIDTH * HEIGHT; ++i)
        screen[i] = blank();
    row = HEIGHT - 1;
}
void vga_init(volatile uint16_t *buffer)
{
    screen = buffer; row = column = 0;attribute=7;escape=0;
    for (unsigned i = 0; i < WIDTH * HEIGHT; ++i) screen[i] = blank();
    outb(0x3d4, 10); outb(0x3d5, 14);
    outb(0x3d4, 11); outb(0x3d5, 15);
    cursor();
}
void vga_write(const uint8_t *bytes, size_t count)
{
    if (!screen) return;
    for (size_t i = 0; i < count; ++i) {
        uint8_t c = bytes[i];
        if(sequence(c))continue;
        if (c == '\r') column = 0;
        else if (c == '\n') ++row;
        else if (c == '\b') { if (column) --column; }
        else if (c == '\t') {
            /* HT is cursor motion, not output of spaces. Full-screen programs
             * rely on existing cells surviving a tab, including at the margin. */
            unsigned next=(column|7U)+1;
            column=next<WIDTH?next:WIDTH-1;
        } else if (c >= 32 && c != 127) {
            screen[row * WIDTH + column++] = ((uint16_t)attribute<<8) | c;
        }
        advance();
    }
    cursor();
}
