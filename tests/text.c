#include "text.h"
#include <nexix/errno.h>
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
static int releases,release_error;
int files_inode_put(struct inode *inode)
{if(!inode->i_count)__builtin_trap();--inode->i_count;++releases;return release_error;}
_Noreturn void panic(const char *message){(void)message;__builtin_trap();}
int main(void)
{
    struct inode inode={.i_count=1,.i_mode=V7_IFREG|0755};struct text *text=NULL;
    CHECK(text_acquire(&inode,0,&text)==0&&!text&&inode.i_count==1);
    inode.i_count=2;CHECK(text_acquire(&inode,4096,&text)==-NX_ETXTBSY&&!text&&!(inode.i_flag&V7_ITEXT));
    inode.i_count=1;CHECK(text_acquire(&inode,4096,&text)==0&&text_valid(text)&&inode.i_count==2);
    CHECK(text->x_size==4096&&text->x_count==1&&text->x_ccount==1&&(inode.i_flag&V7_ITEXT));
    CHECK(files_inode_put(&inode)==0&&inode.i_count==1);
    text_retain(text);CHECK(text->x_count==2&&text->x_ccount==2);
    CHECK(text_evict(&inode)==0&&(inode.i_flag&V7_ITEXT));
    text_release(text);CHECK(text_valid(text)&&text->x_count==1);
    text_release(text);CHECK(!text_valid(text)&&!inode.i_count&&!(inode.i_flag&V7_ITEXT));
    inode=(struct inode){.i_count=1,.i_mode=V7_IFREG|01755};
    CHECK(text_acquire(&inode,2048,&text)==0&&files_inode_put(&inode)==0);
    text_release(text);CHECK(!text_valid(text)&&inode.i_count==1&&(inode.i_flag&V7_ITEXT));
    ++inode.i_count;struct text *again;
    CHECK(text_acquire(&inode,2048,&again)==0&&again==text&&again->x_count==1&&files_inode_put(&inode)==0);
    text_release(again);CHECK(text_evict(&inode)==0&&!inode.i_count&&!(inode.i_flag&V7_ITEXT));
    struct inode nodes[V7_NTEXT+1];struct text *owners[V7_NTEXT];
    for(unsigned i=0;i<V7_NTEXT;++i){
        nodes[i]=(struct inode){.i_count=1,.i_mode=V7_IFREG|01755};
        CHECK(text_acquire(&nodes[i],1024+i,&owners[i])==0&&files_inode_put(&nodes[i])==0);
    }
    nodes[V7_NTEXT]=(struct inode){.i_count=1,.i_mode=V7_IFREG|0755};
    CHECK(text_acquire(&nodes[V7_NTEXT],4096,&text)==-NX_ENFILE&&!text&&nodes[V7_NTEXT].i_count==1);
    /* Bounded table pressure can evict an unused sticky slot, never an active one. */
    text_release(owners[3]);
    CHECK(text_acquire(&nodes[V7_NTEXT],4096,&text)==0&&files_inode_put(&nodes[V7_NTEXT])==0);
    CHECK(!nodes[3].i_count&&!(nodes[3].i_flag&V7_ITEXT));
    text_release(text);
    for(unsigned i=0;i<V7_NTEXT;++i)if(i!=3)text_release(owners[i]);
    CHECK(text_purge_unused()==0);
    for(unsigned i=0;i<V7_NTEXT;++i)CHECK(!nodes[i].i_count&&!(nodes[i].i_flag&V7_ITEXT));
    struct inode selected[3];struct text *selected_text[3];
    for(unsigned i=0;i<3;++i){
        selected[i]=(struct inode){.i_count=1,.i_dev=i==2?1:i,.i_mode=V7_IFREG|01755};
        CHECK(text_acquire(&selected[i],4096,&selected_text[i])==0&&files_inode_put(&selected[i])==0);
        if(i!=2)text_release(selected_text[i]);
    }
    CHECK(text_purge_device(1)==0&&!selected[1].i_count&&selected[0].i_count==1&&selected[2].i_count==1);
    CHECK(text_valid(selected_text[2])); /* Active text on the selected device stays busy. */
    text_release(selected_text[2]);CHECK(text_purge_device(1)==0&&!selected[2].i_count&&selected[0].i_count==1);
    CHECK(text_purge_device(0)==0&&!selected[0].i_count);
    inode=(struct inode){.i_count=1,.i_mode=V7_IFREG|01755};
    CHECK(text_acquire(&inode,1,&text)==0&&files_inode_put(&inode)==0);text_release(text);
    release_error=-NX_EIO;CHECK(text_purge_unused()==-NX_EIO&&!inode.i_count&&!(inode.i_flag&V7_ITEXT));
    release_error=0;CHECK(text_purge_unused()==0&&releases>V7_NTEXT);
    struct text foreign={0};CHECK(!text_valid(&foreign)&&!text_valid(NULL));
    return 0;
}
