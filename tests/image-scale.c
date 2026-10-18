/* Mount and mutate a generated large image using the production filesystem. */
#define _POSIX_C_SOURCE 200809L
#include "v7/fs.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
int64_t clock_now(void){return 1709210096;}
void files_inode_changed(struct v7fs *f,const struct v7fs_inode *i){(void)f;(void)i;}
int main(int argc,char **argv)
{
    assert(argc==2);FILE *file=fopen(argv[1],"r+b");assert(file);struct stat st;assert(!fstat(fileno(file),&st));
    uint8_t *bytes=mmap(NULL,st.st_size,PROT_READ|PROT_WRITE,MAP_SHARED,fileno(file),0);assert(bytes!=MAP_FAILED);
    struct ramdisk ram;assert(!ramdisk_init(&ram,bytes,st.st_size,false));struct v7fs fs={0};
    assert(!v7fs_mount(&fs,&ram.device,false) && fs.fsize==131072 && fs.inodes==4096);
    v7_ino_t ino,alias;assert(!v7fs_lookup(&fs,2,"/usr/many/f1599",&ino) && ino>1000);
    assert(!v7fs_lookup(&fs,2,"/usr/alias",&alias) && alias==ino);
    struct v7fs_inode ip;assert(!v7fs_inode_get(&fs,ino,&ip) && ip.addr[0]>65535 && ip.nlink==2 && ip.uid==123 && ip.gid==456);
    char buffer[64]={0};size_t done;
    assert(!v7fs_read(&fs,ino,0,buffer,sizeof(buffer),&done) && done==17 && !memcmp(buffer,"LARGE_INODE_DATA\n",17));
    v7_ino_t parent;assert(!v7fs_lookup(&fs,2,"/usr/many",&parent));
    unsigned free_inodes=v7_disk_u16(fs.super.tinode);uint32_t free_blocks=v7_disk_u32(fs.super.tfree);
    v7_ino_t created[120];
    for(unsigned i=0;i<120;++i){char name[14]={0};snprintf(name,sizeof(name),"new%04u",i);
        assert(!v7fs_create(&fs,parent,name,V7_IFREG|0600,123,456,&created[i]) && created[i]>1000);
        assert(!v7fs_write(&fs,created[i],0,"persist",7,&done) && done==7);
    }
    uint32_t after_create=v7_disk_u32(fs.super.tfree);
    assert(v7_disk_u16(fs.super.tinode)==free_inodes-120);assert(!v7fs_unmount(&fs));memset(&fs,0,sizeof(fs));
    assert(!v7fs_mount(&fs,&ram.device,false));memset(buffer,0,sizeof(buffer));
    assert(!v7fs_read(&fs,created[119],0,buffer,sizeof(buffer),&done) && done==7 && !memcmp(buffer,"persist",7));
    for(unsigned i=0;i<120;++i){char name[14]={0};snprintf(name,sizeof(name),"new%04u",i);v7_ino_t removed;
        assert(!v7fs_unlink(&fs,parent,name,&removed) && removed==created[i]);assert(!v7fs_release_inode(&fs,removed));
    }
    assert(v7_disk_u16(fs.super.tinode)==free_inodes && v7_disk_u32(fs.super.tfree)==after_create+120);
    /* V7 retains directory blocks containing now-empty slots. */
    assert(free_blocks-v7_disk_u32(fs.super.tfree)<=4);free_blocks=v7_disk_u32(fs.super.tfree);
    uint32_t block;unsigned count=0;while(!v7fs_alloc_block(&fs,&block)){assert(block>=fs.isize && block<fs.fsize);++count;}
    assert(count==free_blocks && !v7_disk_u32(fs.super.tfree));
    assert(!v7fs_unmount(&fs));assert(!munmap(bytes,st.st_size));fclose(file);
    puts("PASS: 64 MiB mount, high inode/address reads, >100 inode allocations, remount/reclamation and full chained free-list exhaustion");
}
