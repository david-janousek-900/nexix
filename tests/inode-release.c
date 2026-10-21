/* Final inode errors keep ownership even for callers with no error return. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "../kernel/v7/files.c"
static struct v7fs volume;
static int failure;
static unsigned freed;
struct v7fs *namespace_volume(v7_dev_t dev){(void)dev;return &volume;}
int v7fs_inode_get(struct v7fs *fs,v7_ino_t number,struct v7fs_inode *disk)
{(void)fs;(void)number;if(failure)return failure;*disk=(struct v7fs_inode){.nlink=0};return 0;}
int v7fs_release_inode(struct v7fs *fs,v7_ino_t number)
{(void)fs;(void)number;++freed;return 0;}
_Noreturn void panic(const char *message){(void)message;abort();}
int main(void)
{
    inodes[0]=(struct inode){.i_count=1,.i_number=3};
    failure=-NX_EIO;
    assert(files_inode_put(&inodes[0])==-NX_EIO);
    assert(inodes[0].i_count==1 && release_pending[0] && !freed);
    assert(retry_releases()==-NX_EIO && release_pending[0] && !freed);
    failure=0;assert(!retry_releases());
    assert(!inodes[0].i_count && !release_pending[0] && freed==1);
    assert(!retry_releases() && freed==1);
    puts("PASS: failed final inode cleanup retains ownership and retries once");
}
