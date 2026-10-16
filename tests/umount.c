#include "namespace.h"
#include "files.h"
#include "storage.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct v7fs root;
static struct ramdisk devices[8];
static unsigned char bytes[8][256*BLOCK_SIZE];
static struct inode pins[8];
static unsigned writes[8],released[8],releases,purges;
static int busy=-1,release_error,purge_error;
static unsigned faults;
static int (*backing_write[8])(struct block_device *,uint32_t,const void *);
static int64_t now=1709210096;
void files_inode_changed(struct v7fs *fs,const struct v7fs_inode *ip){(void)fs;(void)ip;}
int64_t clock_now(void){return now;}
_Noreturn void panic(const char *message){fprintf(stderr,"panic: %s\n",message);abort();}
int files_inode_put(struct inode *ip){assert(ip->i_count);--ip->i_count;return 0;}
bool files_volume_busy(v7_dev_t dev){return dev==busy;}
int text_purge_device(v7_dev_t dev){(void)dev;++purges;return purge_error;}
int files_mountpoint_put(struct inode *ip)
{
    if(release_error)return release_error;
    if(ip->i_count!=1)return -NX_EBUSY;
    assert(ip->i_flag&V7_IMOUNT);
    assert(namespace_volume(ip->i_dev)); /* Parent must outlive this pin. */
    released[releases++]=ip->i_dev;
    ip->i_flag&=~V7_IMOUNT;--ip->i_count;return 0;
}
static int write_device(struct block_device *d,uint32_t block,const void *data)
{
    unsigned i=(struct ramdisk *)d->context-devices;assert(i<8);++writes[i];
    if(block==V7_SUPERB&&(faults&(1U<<i)))return -NX_EIO;
    return backing_write[i](d,block,data);
}
static void attach(unsigned dev,struct fs_node point,bool ro)
{
    pins[dev]=(struct inode){.i_count=1,.i_dev=point.dev,.i_number=point.ino};
    assert(namespace_mount(dev,point,&pins[dev],ro)==0);
}
int main(int argc,char **argv)
{
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    assert(fread(bytes[0],1,sizeof(bytes[0]),f)==sizeof(bytes[0])&&fgetc(f)==EOF);fclose(f);
    for(unsigned i=0;i<8;++i){
        if(i)memcpy(bytes[i],bytes[0],sizeof(bytes[i]));
        assert(ramdisk_init(&devices[i],bytes[i],sizeof(bytes[i]),i==3)==0);
        backing_write[i]=devices[i].device.write;devices[i].device.write=write_device;
        assert(storage_bind(i,&devices[i].device,i==0)==0);
    }
    assert(v7fs_mount(&root,&devices[0].device,false)==0&&namespace_root(&root)==0);
    assert(namespace_unmount(0)==-NX_EBUSY&&namespace_unmount(7)==-NX_EINVAL);
    struct storage_handle *raw;assert(storage_open(0x0401,true,true,&raw)==0);
    attach(1,(struct fs_node){0,3},false);
    attach(2,(struct fs_node){1,6},false);
    attach(3,(struct fs_node){0,7},true);
    size_t done;unsigned char data[16];
    assert(storage_transfer(raw,0,data,2,true,&done)==-NX_EBUSY&&!done);
    assert(namespace_unmount(1)==-NX_EBUSY&&namespace_pins()==3&&pins[1].i_count==1);
    busy=2;assert(namespace_unmount(2)==-NX_EBUSY&&namespace_volume(2));busy=-1;
    purge_error=-NX_EIO;assert(namespace_unmount(2)==-NX_EIO&&namespace_volume(2));purge_error=0;
    release_error=-NX_EIO;assert(namespace_unmount(2)==-NX_EIO&&namespace_volume(2)&&pins[2].i_count==1);release_error=0;
    faults=1U<<1;unsigned before=writes[2];
    assert(namespace_unmount(2)==-NX_EIO&&namespace_volume(1)->failed&&namespace_volume(2)&&writes[2]>before);
    faults=0;namespace_volume(1)->failed=false;
    faults=1U<<2;assert(namespace_unmount(2)==-NX_EIO&&namespace_volume(2)->failed&&pins[2].i_count==1);
    faults=0;namespace_volume(2)->failed=false;
    assert(namespace_unmount(2)==0&&!namespace_volume(2)&&!pins[2].i_count);
    assert(namespace_unmount(2)==-NX_EINVAL);
    struct v7fs_inode file;assert(v7fs_inode_get(namespace_volume(1),4,&file)==0);
    assert(v7fs_write(namespace_volume(1),4,0,"saved!",6,&done)==0&&done==6);
    file.mode=V7_IFREG|0600;assert(v7fs_metadata(namespace_volume(1),&file)==0);
    assert(namespace_unmount(1)==0&&!namespace_volume(1)&&!pins[1].i_count);
    assert(storage_transfer(raw,(uint64_t)file.addr[0]*BLOCK_SIZE,"rawnew",6,true,&done)==0&&done==6);
    storage_close(raw);
    attach(1,(struct fs_node){0,3},false); /* Reused cache must read changed backing. */
    memset(data,0,sizeof(data));assert(v7fs_read(namespace_volume(1),4,0,data,6,&done)==0&&done==6&&!memcmp(data,"rawnew",6));
    assert(v7fs_inode_get(namespace_volume(1),4,&file)==0&&(file.mode&0777)==0600);
    assert(namespace_unmount(1)==0);
    unsigned ro_writes=writes[3];assert(namespace_unmount(3)==0&&writes[3]==ro_writes);
    /* Parent occupies slot 2; its child reuses earlier slot 1. */
    attach(1,(struct fs_node){0,3},false);
    attach(3,(struct fs_node){0,7},true);
    assert(namespace_unmount(1)==0);
    attach(4,(struct fs_node){3,3},false);
    assert(namespace_unmount(3)==-NX_EBUSY);
    ++pins[4].i_count;assert(namespace_reset()==-NX_EBUSY);--pins[4].i_count;
    unsigned count=releases;assert(namespace_reset()==0&&namespace_pins()==0);
    assert(releases==count+2&&released[count]==3&&released[count+1]==0);
    assert(v7fs_unmount(&root)==0&&namespace_root(NULL)==0);
    for(unsigned i=0;i<8;++i)assert(storage_bind(i,NULL,false)==0);
    assert(purges>5&&now==1709210096);
    puts("PASS: targeted unmount, busy references/children, selective purge errors, sync failure retention, read-only volumes, remount/cache and reused-slot graph teardown");
    return 0;
}
