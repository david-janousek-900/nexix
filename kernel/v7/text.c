/* Resident ownership counterpart of V7 text.c; no swap/shared physical pages. */
#include "text.h"
#include "kinfo.h"
#include "serial.h"
#include "interrupts.h"
#include <nexix/errno.h>
static struct text texts[V7_NTEXT];
bool text_valid(const struct text *text)
{
    for(unsigned i=0;i<V7_NTEXT;++i)
        if(text==&texts[i])return text->x_iptr&&text->x_count&&text->x_ccount==text->x_count;
    return false;
}
static int discard(struct text *text)
{
    struct inode *inode=text->x_iptr;
    inode->i_flag&=~V7_ITEXT;
    *text=(struct text){0};
    return files_inode_put(inode);
}
int text_purge_unused(void)
{
    int error=0;
    for(unsigned i=0;i<V7_NTEXT;++i)if(texts[i].x_iptr&&!texts[i].x_count){
        int result=discard(&texts[i]);if(!error)error=result;
    }
    return error;
}
int text_purge_device(v7_dev_t dev)
{
    int error=0;
    for(unsigned i=0;i<V7_NTEXT;++i)
        if(texts[i].x_iptr && texts[i].x_iptr->i_dev==dev && !texts[i].x_count){
            int result=discard(&texts[i]);if(!error)error=result;
        }
    return error;
}
bool text_active(const struct inode *inode)
{
    for(unsigned i=0;i<V7_NTEXT;++i)
        if(texts[i].x_iptr==inode&&texts[i].x_count)return true;
    return false;
}
int text_evict(struct inode *inode)
{
    for(unsigned i=0;i<V7_NTEXT;++i)
        if(texts[i].x_iptr==inode&&!texts[i].x_count)return discard(&texts[i]);
    return 0;
}
int text_acquire(struct inode *inode,size_t size,struct text **out)
{
    *out=NULL;
    if(!size)return 0;
    /* V7 getxfile checks all open references, including read-only ones,
     * unless this inode already belongs to a text entry. */
    if(!(inode->i_flag&V7_ITEXT)&&inode->i_count!=1)return -NX_ETXTBSY;
    for(unsigned i=0;i<V7_NTEXT;++i)if(texts[i].x_iptr==inode){
        if(texts[i].x_count==UINT32_MAX)return -NX_ENFILE;
        ++texts[i].x_count;++texts[i].x_ccount;*out=&texts[i];return 0;
    }
    struct text *slot=NULL;
    for(unsigned i=0;i<V7_NTEXT;++i)if(!texts[i].x_iptr){slot=&texts[i];break;}
    if(!slot){
        int error=text_purge_unused();if(error)return error;
        for(unsigned i=0;i<V7_NTEXT;++i)if(!texts[i].x_iptr){slot=&texts[i];break;}
    }
    if(!slot)return -NX_ENFILE;
    ++inode->i_count;inode->i_flag|=V7_ITEXT;
    *slot=(struct text){.x_iptr=inode,.x_size=size,.x_count=1,.x_ccount=1};
    *out=slot;return 0;
}
void text_retain(struct text *text)
{
    if(!text_valid(text)||text->x_count==UINT32_MAX)panic("invalid text retain");
    ++text->x_count;++text->x_ccount;
}
void text_release(struct text *text)
{
    if(!text)return;
    if(!text_valid(text))panic("invalid text release");
    --text->x_count;--text->x_ccount;
    if(!text->x_count&&!(text->x_iptr->i_mode&V7_ISVTX))(void)discard(text);
}

unsigned text_info_id(const struct text *text)
{
    for(unsigned i=0;i<V7_NTEXT;++i)if(text==&texts[i]&&text->x_iptr)return i+1;
    return 0;
}
bool text_info(unsigned i,struct nx_kinfo_text *r)
{
    if(i>=V7_NTEXT || !texts[i].x_iptr)return false;
    const struct text *t=&texts[i];
    *r=(struct nx_kinfo_text){.slot=i+1,.refs=t->x_count,.resident_refs=t->x_ccount,
        .flags=t->x_flag,.size=t->x_size,.dev=t->x_iptr->i_dev,.ino=t->x_iptr->i_number};
    return true;
}
