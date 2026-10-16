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
static struct inode pins[7];
static int busy_device=-1;
void files_inode_changed(struct v7fs *fs,const struct v7fs_inode *ip){(void)fs;(void)ip;}
int64_t clock_now(void){return 1709210096;}
int files_inode_put(struct inode *ip){assert(ip->i_count==1);--ip->i_count;return 0;}
_Noreturn void panic(const char *message){fprintf(stderr,"panic: %s\n",message);abort();}
int text_purge_device(v7_dev_t dev){(void)dev;return 0;}
int files_mountpoint_put(struct inode *ip)
{assert(ip->i_count==1&&(ip->i_flag&V7_IMOUNT));ip->i_flag&=~V7_IMOUNT;--ip->i_count;return 0;}
bool files_volume_busy(v7_dev_t dev){return dev==busy_device;}
static struct fs_node lookup(struct fs_node cwd,struct fs_node jail,const char *path)
{
    struct fs_node node;int error=namespace_lookup(cwd,jail,path,0,0,&node);
    if(error)fprintf(stderr,"lookup %s from %u:%u root %u:%u: %d\n",path,cwd.dev,cwd.ino,jail.dev,jail.ino,error);
    assert(error==0);return node;
}
static int bad_read(struct block_device *d,uint32_t b,void *p){(void)d;(void)b;(void)p;return -NX_EIO;}
int main(int argc,char **argv)
{
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    assert(fread(bytes[0],1,sizeof(bytes[0]),f)==sizeof(bytes[0])&&fgetc(f)==EOF);fclose(f);
    for(unsigned i=0;i<8;++i){
        if(i)memcpy(bytes[i],bytes[0],sizeof(bytes[i]));
        assert(ramdisk_init(&devices[i],bytes[i],sizeof(bytes[i]),false)==0);
        assert(storage_bind(i,&devices[i].device,i==0)==0);
    }
    assert(v7fs_mount(&root,&devices[0].device,false)==0&&namespace_root(&root)==0);
    struct fs_node base={0,V7_ROOTINO};
    assert(node_equal(lookup(base,base,"/inner/.."),base));
    pins[0]=(struct inode){.i_count=1,.i_dev=0,.i_number=3};
    assert(namespace_mount(1,(struct fs_node){0,3},&pins[0],false)==0);
    assert(namespace_pins()==1&&(pins[0].i_flag&V7_IMOUNT));
    assert(node_equal(lookup(base,base,"/inner/data"),(struct fs_node){1,4}));
    assert(node_equal(lookup(base,base,"/inner/.."),base));
    pins[1]=(struct inode){.i_count=1,.i_dev=1,.i_number=3};
    assert(namespace_mount(2,(struct fs_node){1,3},&pins[1],false)==0);
    struct fs_node jail={1,2},nested={2,2};
    assert(node_equal(lookup(base,base,"/inner/inner/data"),(struct fs_node){2,4}));
    assert(node_equal(lookup(base,base,"/inner/inner/.."),jail));
    assert(node_equal(lookup(base,base,"/inner/inner/../.."),base));
    assert(node_equal(lookup(nested,jail,"../../../.."),jail));
    assert(node_equal(lookup(nested,jail,"/inner/data"),(struct fs_node){2,4}));
    assert(namespace_mount(1,(struct fs_node){0,6},&pins[2],false)==-NX_EBUSY);
    assert(namespace_mount(3,(struct fs_node){0,3},&pins[2],false)==-NX_EBUSY);
    struct fs_node result;
    assert(namespace_lookup(base,base,"/inner/data/",0,0,&result)==-NX_ENOTDIR);
    assert(namespace_lookup(base,base,"/inner/missing",0,0,&result)==-NX_ENOENT);
    struct v7fs_inode dir;assert(v7fs_inode_get(namespace_volume(1),2,&dir)==0);
    dir.mode=V7_IFDIR|0700;assert(v7fs_metadata(namespace_volume(1),&dir)==0);
    assert(namespace_lookup(base,base,"/inner/data",123,123,&result)==-NX_EACCES);
    dir.mode=V7_IFDIR|0755;assert(v7fs_metadata(namespace_volume(1),&dir)==0);
    struct storage_handle *h;
    assert(storage_open(1,false,true,&h)==-NX_EBUSY);
    pins[2]=(struct inode){.i_count=1,.i_dev=0,.i_number=6};
    memset(bytes[3]+BLOCK_SIZE,0,BLOCK_SIZE);
    assert(namespace_mount(3,(struct fs_node){0,6},&pins[2],false)==-NX_EIO);
    assert(pins[2].i_count==1&&namespace_pins()==2&&!namespace_mountpoint((struct fs_node){0,6}));
    assert(storage_bind(3,&devices[3].device,false)==0);
    memcpy(bytes[3],bytes[0],sizeof(bytes[3]));
    devices[3].device.read=bad_read;
    assert(namespace_mount(3,(struct fs_node){0,6},&pins[2],false)==-NX_EIO);
    assert(storage_bind(3,&devices[3].device,false)==0);
    assert(ramdisk_init(&devices[3],bytes[3],sizeof(bytes[3]),true)==0);
    assert(namespace_mount(3,(struct fs_node){0,6},&pins[2],false)==-NX_EROFS);
    assert(namespace_mount(3,(struct fs_node){0,6},&pins[2],true)==0);
    assert(namespace_volume(3)->read_only);
    struct v7fs_inode data;assert(v7fs_inode_get(namespace_volume(3),4,&data)==0);
    data.mode=V7_IFREG|0600;assert(v7fs_metadata(namespace_volume(3),&data)==-NX_EROFS);
    for(unsigned i=4;i<8;++i){
        struct fs_node point=i==4?(struct fs_node){1,2}:(struct fs_node){0,i+3};
        pins[i-1]=(struct inode){.i_count=1,.i_dev=point.dev,.i_number=point.ino};
        assert(namespace_mount(i,point,&pins[i-1],false)==0);
    }
    assert(namespace_pins()==7);
    assert(node_equal(lookup(base,base,"/inner/data"),(struct fs_node){4,4}));
    assert(node_equal(lookup(base,base,"/inner/.."),base));
    assert(namespace_mount(8,(struct fs_node){0,11},&pins[6],false)==-NX_EBUSY);
    assert(namespace_root(NULL)==-NX_EBUSY);
    busy_device=2;assert(namespace_reset()==-NX_EBUSY&&namespace_pins()==7);busy_device=-1;
    ++pins[1].i_count;assert(namespace_reset()==-NX_EBUSY&&namespace_pins()==7);--pins[1].i_count;
    assert(namespace_sync()==0&&namespace_reset()==0&&namespace_pins()==0);
    for(unsigned i=0;i<7;++i)assert(!pins[i].i_count&&!(pins[i].i_flag&V7_IMOUNT));
    assert(node_equal(lookup(base,base,"/data"),(struct fs_node){0,4}));
    assert(namespace_lookup(base,base,"/inner/data",0,0,&result)==-NX_ENOENT);
    assert(v7fs_unmount(&root)==0&&namespace_root(NULL)==0);
    for(unsigned i=0;i<8;++i)assert(storage_bind(i,NULL,false)==0);
    puts("PASS: mount crossings, nested dotdot, chroot, permissions, device/inode identity, errors, bounded mounts and teardown");
    return 0;
}
