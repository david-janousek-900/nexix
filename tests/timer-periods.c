/* Production timer dispatch with deterministic elapsed cycles and device stubs. */
#include <assert.h>
#include <stdio.h>
#include "../kernel/arch/x86_64/interrupts.c"
static struct user user;
static struct proc process={.p_user=&user};
static unsigned callbacks;
struct proc *proc_current(void){return &process;}
void callout_tick(uint64_t now){assert(now==ticks);++callbacks;}
void tty_timer(uint64_t now){assert(now==ticks);}
void modem_timer(uint64_t now){assert(now==ticks);}
void packet_timer(uint64_t now){assert(now==ticks);}
static uint64_t preempt_at;
void proc_timer(struct interrupt_frame *frame)
{
    if(frame->cs&3)++user.u_utime;else ++user.u_stime;
    /* Simulate a yield where another context's IRQ replays its own periods. */
    if(preempt_at){uint64_t at=preempt_at;preempt_at=0;timer_advance_to(frame,at);}
}
int main(void)
{
    struct interrupt_frame frame={.cs=3};
    tick_clock=(struct tick_clock){.last=1000,.period=100};
    timer_advance_to(&frame,1550);
    assert(ticks==5 && callbacks==5 && user.u_utime==5 && cpu_user==5);
    timer_advance_to(&frame,1599);assert(ticks==5);
    timer_advance_to(&frame,1600);assert(ticks==6 && callbacks==6);
    frame.cs=8;timer_advance_to(&frame,2000);
    assert(ticks==10 && user.u_stime==4 && cpu_system==4);
    frame.cs=3;frame.vector=0x80;frame.rip=0x100000;
    syscall_timer(&frame,2200,false);
    syscall_timer(&frame,2900,true);
    assert(user.u_utime==8 && cpu_user==8);
    assert(user.u_stime==11 && cpu_system==11);
    assert(frame.cs==3 && frame.vector==0x80 && frame.rip==0x100000);
    /* A preempted 5-period catch-up plus 1 period replayed meanwhile is 6. */
    uint64_t before=ticks;
    preempt_at=3500;timer_advance_to(&frame,3400);
    assert(ticks==before+6 && !preempt_at);
    puts("PASS: delayed IRQ replays all elapsed periods, timers and CPU charges");
}
