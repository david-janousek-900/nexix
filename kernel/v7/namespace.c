/* Native V7 mount table, iget mount crossing and namei dotdot traversal. */
#include "namespace.h"
#include "kinfo.h"
#include "files.h"
#include "storage.h"
#include "text.h"
#include <nexix/errno.h>
struct volume {
    struct v7fs *fs;
    struct v7fs owned;
    v7_dev_t dev;
    struct fs_node point;
    struct inode *pin;
    struct storage_handle *handle;
};
static struct volume mounts[NAMESPACE_MOUNTS];
static void clear_volume(struct volume *v)
{ volatile uint8_t *p=(void *)v;for(size_t i=0;i<sizeof(*v);++i)p[i]=0; }
struct v7fs *namespace_volume(v7_dev_t dev)
{
    for(unsigned i=0;i<NAMESPACE_MOUNTS;++i)
        if(mounts[i].fs&&mounts[i].dev==dev)return mounts[i].fs;
    return NULL;
}
int namespace_root(struct v7fs *fs)
{
    for(unsigned i=1;i<NAMESPACE_MOUNTS;++i)if(mounts[i].fs)return -NX_EBUSY;
    if(mounts[0].fs)mounts[0].fs->inode_changed=NULL;
    clear_volume(&mounts[0]);mounts[0].fs=fs;
    if(fs)fs->inode_changed=files_inode_changed;
    return 0;
}
bool namespace_mountpoint(struct fs_node node)
{
    for(unsigned i=1;i<NAMESPACE_MOUNTS;++i)
        if(mounts[i].fs&&node_equal(node,mounts[i].point))return true;
    return false;
}
static struct fs_node cross(struct fs_node node)
{
    for(unsigned depth=0;depth<NAMESPACE_MOUNTS;++depth){
        bool found=false;
        for(unsigned i=1;i<NAMESPACE_MOUNTS;++i)
            if(mounts[i].fs&&node_equal(node,mounts[i].point)){
                node=(struct fs_node){mounts[i].dev,V7_ROOTINO};found=true;break;
            }
        if(!found)break;
    }
    return node;
}
int namespace_lookup(struct fs_node cwd,struct fs_node root,const char *path,uint16_t uid,uint16_t gid,struct fs_node *result)
{
    if(!path||!result)return -NX_ENOENT;
    struct fs_node current=*path=='/'?root:cwd;
    while(*path){
        struct v7fs *fs=namespace_volume(current.dev);
        if(!fs)return -NX_ENODEV;
        struct v7fs_inode dir;int error=v7fs_inode_get(fs,current.ino,&dir);
        if(error)return error;
        bool slash=false;while(*path=='/'){++path;slash=true;}
        if(slash&&(dir.mode&V7_IFMT)!=V7_IFDIR)return -NX_ENOTDIR;
        if(!*path)break;
        if((dir.mode&V7_IFMT)!=V7_IFDIR)return -NX_ENOTDIR;
        if(!v7fs_access(&dir,uid,gid,1))return -NX_EACCES;
        char name[V7_DIRSIZ+1]={0};unsigned n=0;
        while(*path&&*path!='/'){if(n<V7_DIRSIZ)name[n++]=*path;++path;}
        bool dotdot=n==2&&name[0]=='.'&&name[1]=='.';
        if(dotdot&&node_equal(current,root))continue;
        if(dotdot&&current.ino==V7_ROOTINO){
            /* V7 can mount on an unreferenced mounted root. Walk through
             * those layers before resolving the mount-point directory's .. . */
            for(unsigned depth=0;depth<NAMESPACE_MOUNTS && current.ino==V7_ROOTINO;++depth){
                bool found=false;
                for(unsigned i=1;i<NAMESPACE_MOUNTS;++i)
                    if(mounts[i].fs&&mounts[i].dev==current.dev){
                        current=mounts[i].point;found=true;break;
                    }
                if(!found||node_equal(current,root))break;
            }
            if(node_equal(current,root))continue;
            fs=namespace_volume(current.dev);
            /* Search was checked on the visible mounted root. The hidden
             * covered directory is not another permission boundary. */
            error=v7fs_lookup(fs,current.ino,"..",&current.ino);
            if(error)return error;
            goto resolved_dotdot;
        }
        /* rootino=0 disables the single-volume helper's implicit root clamp;
         * the namespace performs the process-root clamp above. */
        error=v7fs_lookup_root_user(fs,current.ino,0,name,&current.ino,uid,gid);
        if(error)return error;
resolved_dotdot:
        current=cross(current);
    }
    struct v7fs *fs=namespace_volume(current.dev);struct v7fs_inode node;
    if(!fs)return -NX_ENODEV;
    int error=v7fs_inode_get(fs,current.ino,&node);if(error)return error;
    *result=current;return 0;
}
int namespace_mount(v7_dev_t dev,struct fs_node point,struct inode *pin,bool read_only)
{
    struct volume *slot=NULL;
    for(unsigned i=0;i<NAMESPACE_MOUNTS;++i){
        if(mounts[i].fs&&mounts[i].dev==dev)return -NX_EBUSY;
        if(i&&mounts[i].fs&&node_equal(point,mounts[i].point))return -NX_EBUSY;
        if(i&&!mounts[i].fs&&!slot)slot=&mounts[i];
    }
    if(!slot)return -NX_EBUSY;
    /* The global root is permanently held, like V7 rootdir. */
    if(node_equal(point,(struct fs_node){0,V7_ROOTINO}))return -NX_EBUSY;
    struct storage_handle *handle=NULL;int error=storage_mount_open(dev,read_only,&handle);
    if(error)return error;
    for(unsigned i=0;i<NAMESPACE_MOUNTS;++i)
        if(mounts[i].fs&&mounts[i].fs->device==handle->device){storage_mount_close(handle);return -NX_EBUSY;}
    clear_volume(slot);slot->dev=dev;slot->point=point;slot->pin=pin;slot->handle=handle;
    error=v7fs_mount(&slot->owned,handle->device,read_only);
    struct v7fs_inode root;
    if(!error)error=v7fs_inode_get(&slot->owned,V7_ROOTINO,&root);
    if(!error&&(root.mode&V7_IFMT)!=V7_IFDIR)error=-NX_ENOTDIR;
    if(error){
        if(slot->owned.mounted)(void)v7fs_unmount(&slot->owned);
        storage_mount_close(handle);clear_volume(slot);return error;
    }
    slot->fs=&slot->owned;slot->fs->inode_changed=files_inode_changed;
    pin->i_flag|=V7_IMOUNT;return 0;
}
unsigned namespace_pins(void)
{unsigned n=0;for(unsigned i=1;i<NAMESPACE_MOUNTS;++i)n+=mounts[i].fs!=NULL;return n;}
bool namespace_quiescent(void)
{
    for(unsigned i=1;i<NAMESPACE_MOUNTS;++i)
        if(mounts[i].fs&&mounts[i].pin->i_count!=1)return false;
    return true;
}
int namespace_sync(void)
{
    int error=0;
    for(unsigned i=0;i<NAMESPACE_MOUNTS;++i)if(mounts[i].fs){
        int e=v7fs_sync(mounts[i].fs);if(!error)error=e;
    }
    return error;
}
static bool children(v7_dev_t dev)
{
    for(unsigned i=1;i<NAMESPACE_MOUNTS;++i)
        if(mounts[i].fs && mounts[i].point.dev==dev)return true;
    return false;
}
static int release_volume(struct volume *volume)
{
    /* All fallible disk work precedes removal of the mount-point pin. */
    int error=files_mountpoint_put(volume->pin);if(error)return error;
    if(v7fs_unmount(volume->fs))panic("invalid mounted volume release");
    storage_mount_close(volume->handle);clear_volume(volume);return 0;
}
int namespace_unmount(v7_dev_t dev)
{
    struct volume *volume=NULL;
    for(unsigned i=0;i<NAMESPACE_MOUNTS;++i)
        if(mounts[i].fs && mounts[i].dev==dev){volume=&mounts[i];break;}
    if(!volume)return -NX_EINVAL;
    if(volume==&mounts[0])return -NX_EBUSY;
    int error=text_purge_device(dev);if(error)return error;
    /* V7 xumount -> update -> busy-inode scan. Nexix retains the mount on
     * any synchronous update error, including an error on another volume. */
    error=namespace_sync();if(error)return error;
    if(children(dev)||files_volume_busy(dev))return -NX_EBUSY;
    return release_volume(volume);
}
int namespace_reset(void)
{
    if(!namespace_quiescent())return -NX_EBUSY;
    for(unsigned i=1;i<NAMESPACE_MOUNTS;++i)
        if(mounts[i].fs && files_volume_busy(mounts[i].dev))return -NX_EBUSY;
    int error=namespace_sync();if(error)return error;
    /* Slot numbers cease to reflect ancestry after individual unmounts and
     * slot reuse. Remove leaves of the mount graph rather than reverse slots. */
    while(namespace_pins()){
        bool released=false;
        for(unsigned i=1;i<NAMESPACE_MOUNTS;++i)
            if(mounts[i].fs && !children(mounts[i].dev)){
                error=release_volume(&mounts[i]);if(error)return error;
                released=true;break;
            }
        if(!released)return -NX_EBUSY;
    }
    return 0;
}

bool mounts_info(unsigned i,struct nx_kinfo_mount *r)
{
    if(i>=NAMESPACE_MOUNTS || !mounts[i].fs)return false;
    const struct volume *v=&mounts[i];const struct v7fs *fs=v->fs;
    *r=(struct nx_kinfo_mount){.slot=i+1,.dev=v->dev,
        .flags=(!i?NX_KINFO_MOUNT_ROOT:0) | (fs->read_only?NX_KINFO_MOUNT_RDONLY:0) |
               (fs->failed?NX_KINFO_MOUNT_FAILED:0),
        .point_dev=v->point.dev,.point_ino=i?v->point.ino:V7_ROOTINO,
        .fs_blocks=fs->fsize,.device_blocks=fs->device?fs->device->blocks:0,
        .inodes=fs->inodes,.cache_capacity=V7FS_CACHE};
    for(unsigned j=0;j<V7FS_CACHE;++j)r->cache_used+=fs->cache[j].valid;
    return true;
}
