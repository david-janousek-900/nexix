/* Native V7 fio/sys2/sys3/pipe ownership and descriptor semantics. */
#include "files.h"
#include "kinfo.h"
#include "tty.h"
#include "signals.h"
#include "uaccess.h"
#include "exec.h"
#include "text.h"
#include "storage.h"
#include "tape.h"
#include "memdev.h"
#include "output.h"
#include "modem.h"
#include "namespace.h"
#include "accounting.h"
#include <nexix/syscall.h>
#include <nexix/errno.h>
#include <nexix/mpx.h>
#define NPIPE 32
#define FILE_STORAGE 0100 /* Native private-device discriminator. */
#define FILE_TAPE 0200
struct pipe_buffer { bool active; unsigned readers, writers, head, size; uint8_t bytes[NX_PIPE_BUF]; };
static struct pipe_buffer pipes[NPIPE];
static bool modem_file(const struct file *f)
{return f->f_inode && (f->f_inode->i_mode&V7_IFMT)==V7_IFCHR && modem_matches(f->f_inode->i_un.i_rdev);}
static bool output_file(const struct file *f)
{return f->f_inode && (f->f_inode->i_mode&V7_IFMT)==V7_IFCHR && output_matches(f->f_inode->i_un.i_rdev);}
static struct file table[V7_NFILE];
static bool closing[V7_NFILE];
static struct inode inodes[V7_NINODE];
static bool release_pending[V7_NINODE];
static struct v7fs *root;
static bool root_exec;
static void zero(void *p, size_t n) { uint8_t *b = p; for (size_t i = 0; i < n; ++i) b[i] = 0; }
static bool valid_inode(const struct inode *p)
{ for (unsigned i = 0; i < V7_NINODE; ++i) if (p == &inodes[i]) return p->i_count != 0; return false; }
static bool valid_file(const struct file *p)
{ for (unsigned i = 0; i < V7_NFILE; ++i) if (p == &table[i]) return p->f_count != 0; return false; }
unsigned files_live(void)
{
    unsigned n = 0;
    for (unsigned i = 0; i < V7_NFILE; ++i) n += table[i].f_count != 0;
    for (unsigned i = 0; i < V7_NINODE; ++i) n += inodes[i].i_count != 0;
    for (unsigned i = 0; i < NPIPE; ++i) n += pipes[i].active;
    return n;
}
bool files_volume_busy(v7_dev_t dev)
{
    for(unsigned i=0;i<V7_NINODE;++i)
        if(inodes[i].i_count && inodes[i].i_dev==dev &&
           !((inodes[i].i_flag&V7_IMOUNT)&&inodes[i].i_count==1))return true;
    return false;
}
int files_reset_volumes(void)
{
    int error=text_purge_unused();if(error)return error;
    return namespace_reset();
}
int files_mount(struct v7fs *fs)
{
    int error=text_purge_unused();if(error)return error;
    if (files_live()!=namespace_pins() || !namespace_quiescent()) return -NX_EBUSY;
    if (fs && !fs->mounted) return -NX_ENODEV;
    error=namespace_reset();if(error)return error;
    error=storage_bind(0,fs?fs->device:NULL,fs!=NULL);
    if(error)return error;
    error=namespace_root(fs);if(error)return error;
    root = fs; root_exec = false; return 0;
}
bool files_refs_valid(const struct user *u)
{
    if ((u->u_cdir && !valid_inode(u->u_cdir)) || (u->u_rdir && !valid_inode(u->u_rdir)) || u->u_pdir) return false;
    for (unsigned i = 0; i < V7_NOFILE; ++i)
        if (u->u_ofile[i] && !valid_file(u->u_ofile[i])) return false;
    return true;
}
static struct fs_node inode_node(const struct inode *inode)
{ return (struct fs_node){inode->i_dev,inode->i_number}; }
static struct v7fs *node_volume(struct fs_node node)
{ return namespace_volume(node.dev); }
static int node_inode_get(struct fs_node node,struct v7fs_inode *disk)
{ return v7fs_inode_get(node_volume(node),node.ino,disk); }
static struct inode *inode_get(struct fs_node ino, const struct v7fs_inode *disk)
{
    struct inode *slot = NULL;
    for (unsigned i = 0; i < V7_NINODE; ++i) {
        if (inodes[i].i_count && node_equal(inode_node(&inodes[i]),ino)) { ++inodes[i].i_count; return &inodes[i]; }
        if (!inodes[i].i_count && !slot) slot = &inodes[i];
    }
    if (!slot) return NULL;
    *slot = (struct inode){.i_count = 1, .i_dev=ino.dev, .i_number = ino.ino, .i_mode = disk->mode,
        .i_nlink = disk->nlink, .i_uid = disk->uid, .i_gid = disk->gid, .i_size = disk->size};
    if((disk->mode&V7_IFMT)==V7_IFCHR || (disk->mode&V7_IFMT)==V7_IFBLK)slot->i_un.i_rdev=disk->addr[0];
    return slot;
}
int files_inode_put(struct inode *inode)
{
    if (!valid_inode(inode)) panic("invalid inode release");
    if (--inode->i_count) return 0;
    struct v7fs_inode disk;
    int error = node_inode_get(inode_node(inode), &disk);
    if (!error && !disk.nlink) error = v7fs_release_inode(node_volume(inode_node(inode)), inode->i_number);
    unsigned slot=inode-inodes;
    if(error){
        /* Retain cleanup ownership even when exit/text release cannot return
         * the error. sync retries it and reports continued failure. */
        inode->i_count=1;release_pending[slot]=true;return error;
    }
    release_pending[slot]=false;
    zero(inode, sizeof(*inode)); return 0;
}
static int retry_releases(void)
{
    int error=0;
    for(unsigned i=0;i<V7_NINODE;++i)if(release_pending[i]){
        release_pending[i]=false;
        int e=files_inode_put(&inodes[i]);if(!error)error=e;
    }
    return error;
}
int files_mountpoint_put(struct inode *inode)
{
    if(!valid_inode(inode)||!(inode->i_flag&V7_IMOUNT))return -NX_EINVAL;
    if(inode->i_count!=1)return -NX_EBUSY;
    /* A mounted-on inode cannot be unlinked. Preflight its disk record before
     * dropping the pin so a parent-device error cannot half-detach the mount.
     * Metadata is write-through; a linked, final reference needs no disk write. */
    struct v7fs_inode disk;int error=node_inode_get(inode_node(inode),&disk);
    if(error)return error;
    if(!disk.nlink)return -NX_EIO;
    zero(inode,sizeof(*inode));return 0;
}
static struct file *file_alloc(void)
{
    for (unsigned i = 0; i < V7_NFILE; ++i) if (!table[i].f_count && !closing[i]) {
        zero(&table[i], sizeof(table[i])); table[i].f_count = 1; return &table[i];
    }
    return NULL;
}
static void mpx_close(struct file *);
static int file_put(struct file *f)
{
    if (!valid_file(f)) panic("invalid open file release");
    if (--f->f_count) return 0;
    /* A closing slot is unavailable until sleeping device teardown finishes. */
    unsigned slot=(unsigned)(f-table);closing[slot]=true;
    int error = 0;
    if(f->f_flag&V7_FMP) mpx_close(f);
    else if (f->f_private && !(f->f_flag & V7_FPIPE)) {
        if(f->f_flag & FILE_STORAGE) storage_close(f->f_private);
        else if(f->f_flag & FILE_TAPE) error=tape_close(f->f_private);
        else if(output_file(f)) error=output_close(f->f_private);
        else if(modem_file(f))modem_close(f->f_private);
        else tty_close(f->f_private);
    }
    if (f->f_inode) { int e=files_inode_put(f->f_inode);if(!error)error=e; }
    if (f->f_flag & V7_FPIPE) {
        struct pipe_buffer *p = f->f_private;
        if (f->f_flag & V7_FREAD) --p->readers; else --p->writers;
        proc_wakeup(p);
        if (!p->readers && !p->writers) zero(p, sizeof(*p));
    }
    zero(f, sizeof(*f));closing[slot]=false;return error;
}
static int close_fd(struct user *u, unsigned fd)
{
    if (fd >= V7_NOFILE || !u->u_ofile[fd]) return -NX_EBADF;
    struct file *f = u->u_ofile[fd]; u->u_ofile[fd] = NULL; u->u_pofile[fd] = 0;
    return file_put(f);
}
void files_fork(struct user *u)
{
    for (unsigned i = 0; i < V7_NOFILE; ++i) if (u->u_ofile[i]) ++u->u_ofile[i]->f_count;
    if (u->u_cdir) ++u->u_cdir->i_count;
    if (u->u_rdir) ++u->u_rdir->i_count;
}
void files_exit(struct user *u)
{
    for (unsigned i = 0; i < V7_NOFILE; ++i) if (u->u_ofile[i]) (void)close_fd(u, i);
    if (u->u_cdir) { (void)files_inode_put(u->u_cdir); u->u_cdir = NULL; }
    if (u->u_rdir) { (void)files_inode_put(u->u_rdir); u->u_rdir = NULL; }
    text_release(u->u_procp->p_textp);u->u_procp->p_textp=NULL;
    u->u_ttyp = NULL; u->u_ttyd = V7_NODEV;
}
void files_exec(struct user *u)
{
    for (unsigned i = 0; i < V7_NOFILE; ++i)
        if (u->u_ofile[i] && (u->u_pofile[i] & V7_EXCLOSE)) (void)close_fd(u, i);
}
static int fd_slot(struct user *u, unsigned start)
{ for (unsigned i = start; i < V7_NOFILE; ++i) if (!u->u_ofile[i]) return i; return -NX_EMFILE; }
static struct fs_node cwd(struct user *u) { return u->u_cdir ? inode_node(u->u_cdir) : (struct fs_node){0,V7_ROOTINO}; }
static struct fs_node rootdir(struct user *u) { return u->u_rdir ? inode_node(u->u_rdir) : (struct fs_node){0,V7_ROOTINO}; }
static int path_copy(uintptr_t address, char path[256])
{ return copyinstr(&proc_current()->p_vm, address, path, 256, NULL); }
static int lookup_path(struct user *u, const char *path, struct fs_node *ino)
{ return namespace_lookup(cwd(u), rootdir(u), path, u->u_uid, u->u_gid, ino); }
bool files_exec_enabled(void) { return root_exec; }
int files_mount_root(struct v7fs *fs)
{
    if (!fs) return -NX_EINVAL;
    int error = files_mount(fs);
    if (!error) root_exec = true;
    return error;
}
static struct inode *cached_inode(struct fs_node ino)
{
    for(unsigned i=0;i<V7_NINODE;++i)if(inodes[i].i_count&&node_equal(inode_node(&inodes[i]),ino))return &inodes[i];
    return NULL;
}
/* V7 access() reports busy text before permission bits. This check has no
 * side effects, so an unpermitted writer still cannot evict sticky text. */
static int text_busy_check(struct fs_node ino)
{
    struct inode *inode=cached_inode(ino);
    if(inode && (tape_image_busy(inode)||output_image_busy(inode)))return -NX_EBUSY;
    return inode&&(inode->i_flag&V7_ITEXT)&&text_active(inode)?-NX_ETXTBSY:0;
}
static int text_write_check(struct fs_node ino)
{
    struct inode *inode=cached_inode(ino);
    if(inode && (tape_image_busy(inode)||output_image_busy(inode)))return -NX_EBUSY;
    if(!inode||!(inode->i_flag&V7_ITEXT))return 0;
    int error=text_evict(inode);if(error)return error;
    return inode->i_count&&(inode->i_flag&V7_ITEXT)?-NX_ETXTBSY:0;
}
/* V7 reports ETXTBSY before permission; this probe evicts nothing. */
static int image_text(struct fs_node ino,struct exec_image *image)
{
    struct v7fs_inode disk;int error=node_inode_get(ino,&disk);if(error)return error;
    struct inode *inode=inode_get(ino,&disk);if(!inode)return -NX_ENFILE;
    error=text_acquire(inode,image->text,&image->owner);
    (void)files_inode_put(inode);return error;
}
int files_image_text(v7_ino_t ino,struct exec_image *image)
{ return image_text((struct fs_node){0,ino},image); }
int files_load_exec(const char *path, const struct exec_args *args, struct exec_image *image)
{
    if (!root_exec || !proc_current()) return -NX_ENODEV;
    struct user *u = proc_current()->p_user;
    struct fs_node ino;
    int error = lookup_path(u, path, &ino);
    if (error) return error;
    struct v7fs_inode node;
    error = node_inode_get(ino, &node);
    if (error) return error;
    /* Check access using the caller's credentials, before any set-ID change. */
    if ((node.mode & V7_IFMT) != V7_IFREG || !(node.mode & 0111) ||
        !v7fs_access(&node, u->u_uid, u->u_gid, 1)) return -NX_EACCES;
    error = elf_load_inode(node_volume(ino), ino.ino, args, image);
    if (!error) error=image_text(ino,image);
    if (error) { exec_image_release(image); return error; }
    image->mode=node.mode; image->uid=node.uid; image->gid=node.gid;
    return error;
}
static int parent_path(struct user *u, const char *path, struct fs_node *parent, char name[V7_DIRSIZ])
{
    char prefix[256]; unsigned length = 0, split = 0;
    while (path[length]) { if (path[length] == '/') split = length + 1; ++length; }
    if (!length || path[length - 1] == '/') return -NX_EISDIR;
    zero(name, V7_DIRSIZ);
    for (unsigned i = split; i < length && i - split < V7_DIRSIZ; ++i) name[i - split] = path[i];
    if (u->u_uid && name[0] == '.' && (!name[1] || (name[1] == '.' && !name[2]))) return -NX_EPERM;
    if (split) { for (unsigned i = 0; i < split; ++i) prefix[i] = path[i]; prefix[split] = 0; }
    else { prefix[0] = '.'; prefix[1] = 0; }
    int error = lookup_path(u, prefix, parent);
    if (error) return error;
    struct v7fs_inode dir;
    error = node_inode_get(*parent, &dir);
    if (error) return error;
    if ((dir.mode & V7_IFMT) != V7_IFDIR) return -NX_ENOTDIR;
    return v7fs_access(&dir, u->u_uid, u->u_gid, 3) ? 0 : -NX_EACCES;
}
int files_account_open(uintptr_t address,struct inode **out)
{
    *out=NULL;if(!root)return -NX_ENODEV;
    struct user *u=proc_current()->p_user;
    char path[256];int error=path_copy(address,path);if(error)return error;
    struct fs_node node;error=lookup_path(u,path,&node);if(error)return error;
    struct v7fs_inode disk;error=node_inode_get(node,&disk);if(error)return error;
    if((disk.mode&V7_IFMT)!=V7_IFREG)return -NX_EACCES;
    /* Like V7 sysacct, enabling does not test mode bits, read-only or ITEXT.
     * Writes enforce those storage constraints without impersonating the exiter. */
    *out=inode_get(node,&disk);return *out?0:-NX_ENFILE;
}
/* A loaded tape pins its regular inode and excludes ordinary writers. */
int files_tape_backing(uintptr_t address,bool write,struct inode **out)
{
    *out=NULL;if(!root)return -NX_ENODEV;
    struct user *u=proc_current()->p_user;char path[256];
    int error=path_copy(address,path);if(error)return error;
    struct fs_node node;error=lookup_path(u,path,&node);if(error)return error;
    struct v7fs_inode disk;error=node_inode_get(node,&disk);if(error)return error;
    if((disk.mode&V7_IFMT)!=V7_IFREG)return -NX_EINVAL;
    if(!v7fs_access(&disk,u->u_uid,u->u_gid,write?6:4))return -NX_EACCES;
    if(write && node_volume(node)->read_only)return -NX_EROFS;
    error=text_write_check(node);if(error)return error;
    struct inode *inode=inode_get(node,&disk);if(!inode)return -NX_ENFILE;
    for(unsigned i=0;i<V7_NFILE;++i)if(table[i].f_count && table[i].f_inode==inode && (table[i].f_flag&V7_FWRITE)){
        (void)files_inode_put(inode);return -NX_EBUSY;
    }
    *out=inode;return 0;
}
int files_tape_read(struct inode *inode,uint64_t offset,void *bytes,size_t size)
{
    if(!valid_inode(inode)||offset>INT64_MAX)return -NX_EINVAL;
    size_t done=0;int error=v7fs_read(node_volume(inode_node(inode)),inode->i_number,(int64_t)offset,bytes,size,&done);
    return error?error:done!=size?-NX_EIO:0;
}
int files_tape_write(struct inode *inode,uint64_t offset,const void *bytes,size_t size)
{
    if(!valid_inode(inode)||offset>INT64_MAX)return -NX_EINVAL;
    size_t done=0;int error=v7fs_write(node_volume(inode_node(inode)),inode->i_number,(int64_t)offset,bytes,size,&done);
    return error?error:done!=size?-NX_EIO:0;
}
int files_account_append(struct inode *inode,const void *bytes,size_t size)
{
    if(!valid_inode(inode))return -NX_EINVAL;
    struct fs_node node=inode_node(inode);
    int error=text_write_check(node);if(error)return error; /* Evict unused sticky text; protect active text. */
    return v7fs_append_record(node_volume(node),node.ino,bytes,size);
}
int files_core_begin(struct inode **result)
{
    *result=NULL;
    if(!root||!proc_current())return -NX_ENODEV;
    struct user *u=proc_current()->p_user;
    if(u->u_uid!=u->u_ruid||u->u_gid!=u->u_rgid)return -NX_EPERM;
    struct fs_node ino;int error=lookup_path(u,"core",&ino);
    if(error==-NX_ENOENT){
        struct fs_node parent;char name[V7_DIRSIZ];
        error=parent_path(u,"core",&parent,name);
        bool free=false;for(unsigned i=0;i<V7_NINODE;++i)free|=!inodes[i].i_count;
        if(!error&&!free)error=-NX_ENFILE;
        if(!error){ino.dev=parent.dev;error=v7fs_create(node_volume(parent),parent.ino,name,0666&~u->u_cmask,u->u_uid,u->u_gid,&ino.ino);}
    }
    struct v7fs_inode disk;
    if(!error && node_volume(ino)->read_only)error=-NX_EROFS;
    if(!error)error=node_inode_get(ino,&disk);
    if(!error&&(disk.mode&V7_IFMT)!=V7_IFREG)error=-NX_EACCES;
    if(!error&&!v7fs_access(&disk,u->u_uid,u->u_gid,2))error=-NX_EACCES;
    if(!error)error=text_write_check(ino);
    if(error)return error;
    struct inode *inode=inode_get(ino,&disk);if(!inode)return -NX_ENFILE;
    error=v7fs_truncate(node_volume(ino),ino.ino);
    if(error){(void)files_inode_put(inode);return error;}
    *result=inode;return 0;
}
int files_core_write(struct inode *inode,uint64_t offset,const void *bytes,size_t size)
{
    if(!valid_inode(inode)||offset>INT64_MAX)return -NX_EINVAL;
    if((tape_image_busy(inode)||output_image_busy(inode)))return -NX_EBUSY;
    size_t done=0;int error=v7fs_write(node_volume(inode_node(inode)),inode->i_number,(int64_t)offset,bytes,size,&done);
    return error?error:done!=size?-NX_EIO:0;
}
#include "mpx.inc"

static int open_path(struct user *u, uintptr_t address, uint64_t flags, uint64_t mode)
{
    uint64_t allowed = NX_O_ACCMODE | NX_O_CREAT | NX_O_TRUNC | NX_O_EXCL | NX_O_APPEND | NX_O_CLOEXEC;
    if ((flags & ~allowed) || (flags & NX_O_ACCMODE) == 3 ||
        ((flags & NX_O_TRUNC) && !(flags & NX_O_ACCMODE))) return -NX_EINVAL;
    int fd = fd_slot(u, 0); if (fd < 0) return fd;
    char path[256]; int error = path_copy(address, path); if (error) return error;
    if (!root) return -NX_ENODEV;
    struct file *f = file_alloc(); if (!f) return -NX_ENFILE;
    struct fs_node ino;
    error = lookup_path(u, path, &ino);
    bool created = false;
    if (error == -NX_ENOENT && (flags & NX_O_CREAT)) {
        struct fs_node parent; char name[V7_DIRSIZ];
        error = parent_path(u, path, &parent, name);
        bool available = false;
        for (unsigned i = 0; i < V7_NINODE; ++i) available |= !inodes[i].i_count;
        if (!error && !available) error = -NX_ENFILE;
        if (!error){ino.dev=parent.dev; error = v7fs_create(node_volume(parent), parent.ino, name, mode & ~u->u_cmask & 0777, u->u_uid, u->u_gid, &ino.ino);}
        created = !error;
    } else if (!error && (flags & NX_O_CREAT) && (flags & NX_O_EXCL)) error = -NX_EEXIST;
    struct v7fs_inode disk={0};
    if (!error) error = node_inode_get(ino, &disk);
    unsigned access = flags & NX_O_ACCMODE;
    if(!error&&access&&(disk.mode&V7_IFMT)==V7_IFREG)
        error=node_volume(ino)->read_only?-NX_EROFS:text_busy_check(ino);
    if (!error && !created && !v7fs_access(&disk, u->u_uid, u->u_gid, access == 0 ? 4 : access == 1 ? 2 : 6)) error = -NX_EACCES;
    /* Only a permitted writer may evict unused sticky text. */
    if(!error&&access&&(disk.mode&V7_IFMT)==V7_IFREG){
        error=node_volume(ino)->read_only?-NX_EROFS:text_write_check(ino);
    }
    if (!error && access && (disk.mode & V7_IFMT) == V7_IFDIR) error = -NX_EISDIR;
    unsigned type = error ? 0 : disk.mode & V7_IFMT;
    if (!error && type != V7_IFREG && type != V7_IFDIR && type != V7_IFCHR && type != V7_IFBLK) error = -NX_ENODEV;
    if (!error && access && type == V7_IFREG && node_volume(ino)->read_only) error = -NX_EROFS;
    if (!error) {
        f->f_inode = inode_get(ino, &disk);
        if (!f->f_inode) error = -NX_ENFILE;
    }
    if (!error && (flags & NX_O_TRUNC) && type == V7_IFREG) error = v7fs_truncate(node_volume(ino), ino.ino);
    bool tape=(type==V7_IFCHR || type==V7_IFBLK) && tape_matches(disk.addr[0],type==V7_IFCHR);
    if(!error && tape){
        error=tape_open(disk.addr[0],type==V7_IFCHR,access,&f->f_private);
        if(!error)f->f_flag=FILE_TAPE;
    }
    bool storage = !tape && (type == V7_IFBLK || (type == V7_IFCHR && storage_raw_device(disk.addr[0])));
    if(!error && storage){
        struct storage_handle *handle;
        error=storage_open(disk.addr[0],type==V7_IFCHR,access!=0,&handle);
        if(!error){f->f_private=handle;f->f_flag=FILE_STORAGE;}
    }
    bool multiplexor=type==V7_IFCHR && (disk.addr[0]>>8)==NX_MX_MAJOR;
    if(!error && multiplexor)error=mpx_open(f,path,u);
    bool output=type==V7_IFCHR && output_matches(disk.addr[0]);
    if(!error && output){
        error=output_open(disk.addr[0],access,&f->f_private);
    }
    bool memory=type==V7_IFCHR && memdev_matches(disk.addr[0]);
    if(!error && memory)error=memdev_open(disk.addr[0],u);
    bool modem=type==V7_IFCHR && modem_matches(disk.addr[0]);
    if(!error && modem)error=modem_open(disk.addr[0],u,&f->f_private);
    if (!error && !storage && !tape && !multiplexor && !memory && !modem && !output && type == V7_IFCHR) {
        struct tty *tty;
        error = tty_open(disk.addr[0], u, &tty);
        if (!error) f->f_private = tty;
    }
    if (error) { (void)file_put(f); return error; }
    f->f_flag |= access == 0 ? V7_FREAD : access == 1 ? V7_FWRITE : V7_FREAD | V7_FWRITE;
    f->f_open_flags |= flags & (NX_O_ACCMODE | NX_O_APPEND);
    u->u_ofile[fd] = f; u->u_pofile[fd] = (flags & NX_O_CLOEXEC) ? V7_EXCLOSE : 0;
    return fd;
}
static int make_pipe(struct user *u, uintptr_t address)
{
    struct proc *process = proc_current();
    if (!user_access_ok(&process->p_vm, address, 8, true)) return -NX_EFAULT;
    int a = fd_slot(u, 0); if (a < 0) return a;
    int b = fd_slot(u, a + 1); if (b < 0) return b;
    struct pipe_buffer *pipe = NULL;
    for (unsigned i = 0; i < NPIPE; ++i) if (!pipes[i].active) { pipe = &pipes[i]; break; }
    if (!pipe) return -NX_ENFILE;
    struct file *reader = file_alloc(); if (!reader) return -NX_ENFILE;
    struct file *writer = file_alloc();
    if (!writer) { (void)file_put(reader); return -NX_ENFILE; }
    *pipe = (struct pipe_buffer){.active = true, .readers = 1, .writers = 1};
    reader->f_flag = V7_FREAD | V7_FPIPE; reader->f_private = pipe;
    writer->f_flag = V7_FWRITE | V7_FPIPE; writer->f_private = pipe; writer->f_open_flags = NX_O_WRONLY;
    u->u_ofile[a] = reader; u->u_ofile[b] = writer; u->u_pofile[a] = u->u_pofile[b] = 0;
    int32_t pair[] = {a, b};
    if (copyout(&process->p_vm, address, pair, sizeof(pair))) panic("pipe output mapping changed");
    return 0;
}
static int64_t pipe_io(struct file *f, uintptr_t address, size_t count, bool write)
{
    struct pipe_buffer *p = f->f_private;
    struct address_space *vm = &proc_current()->p_vm;
    if (!count) return 0;
    size_t done = 0;
    uint8_t bytes[512];
    if (!write) {
        while (!p->size && p->writers) if (proc_sleep_interruptible(p, 40)) return -NX_EINTR;
        if (count > p->size) count = p->size;
        while (done < count) {
            size_t n = count - done; if (n > sizeof(bytes)) n = sizeof(bytes);
            for (size_t i = 0; i < n; ++i) bytes[i] = p->bytes[(p->head + i) % NX_PIPE_BUF];
            if (copyout(vm, address + done, bytes, n)) panic("pipe read mapping changed");
            p->head = (p->head + n) % NX_PIPE_BUF; p->size -= n; done += n;
        }
        proc_wakeup(p); return done;
    }
    bool atomic = count <= NX_PIPE_BUF;
    while (done < count) {
        while (p->readers && (atomic ? NX_PIPE_BUF - p->size < count : p->size == NX_PIPE_BUF))
            if (proc_sleep_interruptible(p, 40)) return done ? (int64_t)done : -NX_EINTR;
        if (!p->readers) { signal_post(proc_current(), NX_SIGPIPE); return done ? (int64_t)done : -NX_EPIPE; }
        size_t n = count - done;
        if (n > NX_PIPE_BUF - p->size) n = NX_PIPE_BUF - p->size;
        if (n > sizeof(bytes)) n = sizeof(bytes);
        if (copyin(vm, address + done, bytes, n)) panic("pipe write mapping changed");
        for (size_t i = 0; i < n; ++i) p->bytes[(p->head + p->size + i) % NX_PIPE_BUF] = bytes[i];
        p->size += n; done += n; proc_wakeup(p);
        /* Once the atomic write has room, its remaining chunks cannot block. */
        atomic = false;
    }
    return done;
}
static int64_t io(struct user *u, uint64_t fd, uintptr_t address, uint64_t count, bool write)
{
    if (fd >= V7_NOFILE || !u->u_ofile[fd] || !(u->u_ofile[fd]->f_flag & (write ? V7_FWRITE : V7_FREAD))) return -NX_EBADF;
    if (count > INT64_MAX) return -NX_EINVAL;
    struct address_space *vm = &proc_current()->p_vm;
    if (!user_access_ok(vm, address, count, !write)) return -NX_EFAULT;
    struct file *f = u->u_ofile[fd];
    if (f->f_flag & V7_FMP) {
        if(write || count){
            int error=v7fs_touch(node_volume(inode_node(f->f_inode)),f->f_inode->i_number,
                write?V7FS_MTIME|V7FS_CTIME:V7FS_ATIME);
            if(error)return error;
        }
        return mpx_io(f,address,count,write);
    }
    if (f->f_flag & V7_FPIPE) return pipe_io(f, address, count, write);
    if(f->f_flag & FILE_TAPE){
        if(count>(uint64_t)(INT64_MAX-f->f_offset))return -NX_EINVAL;
        int error=0;
        if(count)error=v7fs_touch(node_volume(inode_node(f->f_inode)),f->f_inode->i_number,write?V7FS_MTIME|V7FS_CTIME:V7FS_ATIME);
        if(error)return error;
        int64_t n=tape_io(f->f_private,vm,address,count,write,f->f_offset);
        if(n>0)f->f_offset+=n;
        return n;
    }
    if(f->f_private && (f->f_flag & FILE_STORAGE)){
        bool raw=(f->f_inode->i_mode & V7_IFMT)==V7_IFCHR;
        if(raw && count && ((address|count)&1))return -NX_EFAULT;
        if(!count)return 0;
        int error=v7fs_touch(node_volume(inode_node(f->f_inode)),f->f_inode->i_number,write?V7FS_MTIME|V7FS_CTIME:V7FS_ATIME);
        if(error)return error;
        uint64_t start=raw?((uint64_t)f->f_offset & ~(uint64_t)(BLOCK_SIZE-1)):(uint64_t)f->f_offset;
        size_t done=0;uint8_t bytes[BLOCK_SIZE];
        while(done<count){
            size_t n=count-done;if(n>sizeof(bytes))n=sizeof(bytes);size_t moved=0;
            if(write && copyin(vm,address+done,bytes,n))return done?(int64_t)done:-NX_EFAULT;
            error=storage_transfer(f->f_private,start+done,bytes,n,write,&moved);
            if(!write && moved && copyout(vm,address+done,bytes,moved))panic("storage output mapping changed");
            /* Raw transfers start on a block boundary (V7 physio), but as in V7
             * rdwr the file offset advances by the bytes moved from where it was. */
            done+=moved;f->f_offset+=(int64_t)moved;
            if(error||moved<n)break;
        }
        return done?(int64_t)done:error;
    }
    if (f->f_private) {
        /* V7 readi/writei also stamp character-device inodes. Pipes have no
         * persistent inode. Keep the established zero-count TTY behavior. */
        if (write || count) {
            int error = v7fs_touch(node_volume(inode_node(f->f_inode)), f->f_inode->i_number,
                write ? V7FS_MTIME | V7FS_CTIME : V7FS_ATIME);
            if (error) return error;
        }
        if(output_file(f))return output_io(f->f_private,vm,address,count,write);
        if(modem_file(f))return modem_io(f->f_private,vm,address,count,write);
        return tty_io(f->f_private, vm, address, count, write);
    }
    struct v7fs_inode inode;
    int error = node_inode_get(inode_node(f->f_inode), &inode);
    if (error) return error;
    if ((inode.mode & V7_IFMT) == V7_IFCHR && memdev_matches(inode.addr[0])) {
        error=memdev_open(inode.addr[0],u);if(error)return error;
        if (write || count) {
            error = v7fs_touch(node_volume(inode_node(f->f_inode)), inode.number, write ? V7FS_MTIME | V7FS_CTIME : V7FS_ATIME);
            if (error) return error;
        }
        /* Reject an offset overflow before any memory is transferred. */
        if(f->f_offset<0 || (uint64_t)count>(uint64_t)(INT64_MAX-f->f_offset))return -NX_EINVAL;
        int64_t moved=memdev_io(inode.addr[0],u,vm,f->f_offset,address,count,write);
        if(moved>0)f->f_offset+=moved;
        return moved;
    }
    if (write && (f->f_open_flags & NX_O_APPEND)) f->f_offset = inode.size;
    size_t done = 0; uint8_t bytes[512];
    while (done < count) {
        size_t n = count - done; if (n > sizeof(bytes)) n = sizeof(bytes);
        size_t moved = 0;
        if (write) {
            if (copyin(vm, address + done, bytes, n)) panic("file input mapping changed");
            error = v7fs_write(node_volume(inode_node(f->f_inode)), inode.number, f->f_offset, bytes, n, &moved);
        } else {
            error = v7fs_read(node_volume(inode_node(f->f_inode)), inode.number, f->f_offset, bytes, n, &moved);
            if (moved && copyout(vm, address + done, bytes, moved)) panic("file output mapping changed");
        }
        done += moved; f->f_offset += moved;
        if (error || moved < n) break;
    }
    return done ? (int64_t)done : error;
}
static int64_t seek(struct file *f, int64_t offset, uint64_t whence)
{
    if ((f->f_flag & V7_FPIPE) || (f->f_private && !(f->f_flag & FILE_STORAGE) && !((f->f_flag&FILE_TAPE)&&tape_seekable(f->f_private)))) return -NX_ESPIPE;
    int64_t base = 0;
    if (whence == NX_SEEK_CUR) base = f->f_offset;
    else if (whence == NX_SEEK_END) {
        struct v7fs_inode inode; int error = node_inode_get(inode_node(f->f_inode), &inode);
        if (error) return error;
        base = inode.size;
    } else if (whence != NX_SEEK_SET) return -NX_EINVAL;
    if (offset < -base || offset > INT64_MAX - base) return -NX_EINVAL;
    f->f_offset = base + offset; return f->f_offset;
}
static int stat_inode(struct fs_node ino, struct nx_stat *st)
{
    struct v7fs_inode i; int error = node_inode_get(ino, &i); if (error) return error;
    *st = (struct nx_stat){.dev=ino.dev, .ino = ino.ino, .mode = mpx_mode(ino,i.mode), .nlink = i.nlink, .uid = i.uid, .gid = i.gid,
        .rdev = ((i.mode & V7_IFMT) == V7_IFCHR || (i.mode & V7_IFMT) == V7_IFBLK) ? i.addr[0] : 0,
        .size = i.size, .atime = i.atime, .mtime = i.mtime, .ctime = i.ctime}; return 0;
}
static int device_path(struct user *u,uintptr_t address,v7_dev_t *dev)
{
    char path[256];int error=path_copy(address,path);if(error)return error;
    struct fs_node node;error=lookup_path(u,path,&node);if(error)return error;
    struct v7fs_inode disk;error=node_inode_get(node,&disk);if(error)return error;
    if((disk.mode&V7_IFMT)!=V7_IFBLK)return -NX_ENOTBLK;
    *dev=disk.addr[0];
    return storage_block_device(*dev)?0:-NX_ENXIO;
}
static int mount_path(struct user *u,const uintptr_t args[6])
{
    if(!proc_superuser(u))return -NX_EPERM;
    if(!root)return -NX_ENODEV;
    v7_dev_t dev=0;int error=device_path(u,args[0],&dev);if(error)return error;
    char point_path[256];error=path_copy(args[1],point_path);if(error)return error;
    struct fs_node point;error=lookup_path(u,point_path,&point);if(error)return error;
    struct v7fs_inode disk;error=node_inode_get(point,&disk);if(error)return error;
    /* V7 accepts ordinary files as well as directories as mount points. */
    if((disk.mode&V7_IFMT)!=V7_IFREG && (disk.mode&V7_IFMT)!=V7_IFDIR)return -NX_EBUSY;
    struct inode *pin=inode_get(point,&disk);if(!pin)return -NX_ENFILE;
    if(pin->i_count!=1)error=-NX_EBUSY;
    else if((disk.mode&V7_IFMT)==V7_IFREG){
        struct fs_node parent;char name[V7_DIRSIZ];
        error=parent_path(u,point_path,&parent,name);
        if(!error)error=namespace_mount_file(dev,point,parent,pin,!!(args[2]&1));
    }else error=namespace_mount(dev,point,pin,!!(args[2]&1));
    if(error)(void)files_inode_put(pin);
    return error;
}
int64_t files_syscall(uint64_t number, const uintptr_t args[6])
{
    struct proc *p = proc_current(); struct user *u = p->p_user;
    uint64_t fd = args[0];
    if(number==NX_SYS_MPX)return mpx_call(u,args[0],args[1]);
    if(number==NX_SYS_MOUNT)return mount_path(u,args);
    if(number==NX_SYS_UMOUNT){
        if(!proc_superuser(u))return -NX_EPERM;
        if(!root)return -NX_ENODEV;
        v7_dev_t dev=0;int error=device_path(u,args[0],&dev);
        return error?error:namespace_unmount(dev);
    }
    if (number == NX_SYS_SYNC) {
        int error=retry_releases(),sync_error=namespace_sync();
        return error?error:sync_error;
    } /* Write-through cache plus V7 superblock time. */
    if (number == NX_SYS_MKNOD) {
        if (!proc_superuser(u)) return -NX_EPERM;
        if (args[1] > UINT16_MAX || args[2] > UINT16_MAX) return -NX_EINVAL;
        char path[256], name[V7_DIRSIZ]; struct fs_node parent;v7_ino_t ino;
        int error = path_copy(args[0], path); if (error) return error;
        if (!root) return -NX_ENODEV;
        error = parent_path(u, path, &parent, name);
        uint16_t mode = args[1];
        if (!(mode & V7_IFMT)) mode |= V7_IFREG;
        return error ? error : v7fs_mknod(node_volume(parent), parent.ino, name, mode & ~(u->u_cmask & 0777), u->u_uid, u->u_gid, args[2], &ino);
    }
    if (number == NX_SYS_ACCESS || number == NX_SYS_CHMOD || number == NX_SYS_CHOWN || number == NX_SYS_UTIME) {
        char path[256]; int error = path_copy(args[0], path); if (error) return error;
        if (!root) return -NX_ENODEV;
        bool access = number == NX_SYS_ACCESS;
        if (access && args[1] > 7) return -NX_EINVAL;
        if (number == NX_SYS_CHOWN && (args[1] > UINT16_MAX || args[2] > UINT16_MAX)) return -NX_EINVAL;
        struct fs_node ino;
        error = namespace_lookup(cwd(u), rootdir(u), path, access ? u->u_ruid : u->u_uid, access ? u->u_rgid : u->u_gid, &ino);
        struct v7fs_inode disk;
        if (!error) error = node_inode_get(ino, &disk);
        if (error) return error;
        if (access) {
            if ((args[1] & 2) && node_volume(ino)->read_only) return -NX_EROFS;
            if ((args[1] & 2) && (error = text_busy_check(ino))) return error;
            if (!v7fs_access(&disk, u->u_ruid, u->u_rgid, args[1])) return -NX_EACCES;
            return (args[1] & 2) ? text_write_check(ino) : 0;
        }
        if ((number==NX_SYS_CHOWN || u->u_uid!=disk.uid) && !proc_superuser(u))return -NX_EPERM;
        if (number == NX_SYS_CHMOD) {
            disk.mode = (disk.mode & ~07777) | (args[1] & (u->u_uid ? 06777 : 07777));
        } else if (number == NX_SYS_CHOWN) { disk.uid = args[1]; disk.gid = args[2]; }
        else {
            int64_t times[2];
            error = copyin(&p->p_vm, args[1], times, sizeof(times));
            if (error) return error;
            if (times[0] < 0 || times[0] > UINT32_MAX || times[1] < 0 || times[1] > UINT32_MAX) return -NX_EINVAL;
            disk.atime = times[0]; disk.mtime = times[1];
        }
        error = v7fs_metadata(node_volume(ino), &disk);
        if (!error) for (unsigned i = 0; i < V7_NINODE; ++i)
            if (inodes[i].i_count && node_equal(inode_node(&inodes[i]),ino)) {
                inodes[i].i_mode = disk.mode; inodes[i].i_uid = disk.uid; inodes[i].i_gid = disk.gid;
            }
        if(!error&&number==NX_SYS_CHMOD&&!(disk.mode&V7_ISVTX)){
            struct inode *inode=cached_inode(ino);if(inode)error=text_evict(inode);
        }
        return error;
    }
    if (number == NX_SYS_OPEN) return open_path(u, args[0], args[1], args[2]);
    if (number == NX_SYS_CREAT) return open_path(u, args[0], NX_O_WRONLY | NX_O_CREAT | NX_O_TRUNC, args[1]);
    if (number == NX_SYS_READ || number == NX_SYS_WRITE) return io(u, fd, args[1], args[2], number == NX_SYS_WRITE);
    if (number == NX_SYS_PIPE) return make_pipe(u, args[0]);
    if (number == NX_SYS_CHDIR || number == NX_SYS_CHROOT || number == NX_SYS_UNLINK || number == NX_SYS_LINK || number == NX_SYS_STAT) {
        if (number == NX_SYS_CHROOT && !proc_superuser(u)) return -NX_EPERM;
        char path[256]; int error = path_copy(args[0], path); if (error) return error;
        if (!root) return -NX_ENODEV;
        struct fs_node ino;
        if (number == NX_SYS_UNLINK) {
            struct fs_node parent; char name[V7_DIRSIZ];
            error = parent_path(u, path, &parent, name);
            if(!error){
                struct v7fs_inode disk;
                ino.dev=parent.dev;
                char component[V7_DIRSIZ+1]={0};for(unsigned i=0;i<V7_DIRSIZ;++i)component[i]=name[i];
                error=v7fs_lookup_root_user(node_volume(parent),parent.ino,0,component,&ino.ino,u->u_uid,u->u_gid);
                if(!error && namespace_mountpoint(ino))error=-NX_EBUSY;
                if(!error)error=node_inode_get(ino,&disk);
                if(!error && (disk.mode&V7_IFMT)==V7_IFDIR && !proc_superuser(u))error=-NX_EPERM;
                if(!error){
                    struct inode *inode=cached_inode(ino);
                    if(inode)error=text_evict(inode);
                    if(!error&&disk.nlink==1&&inode&&inode->i_count&&(inode->i_flag&V7_ITEXT))error=-NX_ETXTBSY;
                }
            }
            if (!error) error = v7fs_unlink_privileged(node_volume(parent), parent.ino, name, &ino.ino, !u->u_uid);
            if (error) return error;
            for (unsigned i = 0; i < V7_NINODE; ++i) if (inodes[i].i_count && node_equal(inode_node(&inodes[i]),ino)) return 0;
            struct v7fs_inode disk; error = node_inode_get(ino, &disk);
            return error ? error : disk.nlink ? 0 : v7fs_release_inode(node_volume(ino), ino.ino);
        }
        error = lookup_path(u, path, &ino); if (error) return error;
        if (number == NX_SYS_LINK) {
            struct v7fs_inode source;
            error = node_inode_get(ino, &source); if (error) return error;
            if ((source.mode & V7_IFMT) == V7_IFDIR && !proc_superuser(u)) return -NX_EPERM;
            char target[256], name[V7_DIRSIZ]; struct fs_node parent;
            error = path_copy(args[1], target); if (error) return error;
            error = parent_path(u, target, &parent, name);
            if(!error && ino.dev!=parent.dev)error=-NX_EXDEV;
            return error ? error : v7fs_link_privileged(node_volume(ino), ino.ino, parent.ino, name, !u->u_uid);
        }
        if (number == NX_SYS_STAT) {
            struct nx_stat st; error = stat_inode(ino, &st);
            return error ? error : copyout(&p->p_vm, args[1], &st, sizeof(st));
        }
        struct v7fs_inode disk; error = node_inode_get(ino, &disk);
        if (error) return error;
        if ((disk.mode & V7_IFMT) != V7_IFDIR) return -NX_ENOTDIR;
        if (!v7fs_access(&disk, u->u_uid, u->u_gid, 1)) return -NX_EACCES;
        struct inode *dir = inode_get(ino, &disk); if (!dir) return -NX_ENFILE;
        struct inode **slot = number == NX_SYS_CHROOT ? &u->u_rdir : &u->u_cdir;
        if (*slot) (void)files_inode_put(*slot);
        *slot = dir; return 0;
    }
    if (fd >= V7_NOFILE || !u->u_ofile[fd]) return -NX_EBADF;
    struct file *f = u->u_ofile[fd];
    if (number == NX_SYS_IOCTL || number == NX_SYS_STTY || number == NX_SYS_GTTY) {
        uint64_t request = number == NX_SYS_IOCTL ? args[1] : number == NX_SYS_STTY ? NX_TIOCSETP : NX_TIOCGETP;
        uintptr_t argument = number == NX_SYS_IOCTL ? args[2] : args[1];
        if (request == NX_FIOCLEX || request == NX_FIONCLEX) {
            u->u_pofile[fd] = request == NX_FIOCLEX ? V7_EXCLOSE : 0;
            return 0;
        }
        if(f->f_flag&FILE_TAPE){
            return tape_ioctl(f->f_private,&p->p_vm,request,argument,&f->f_offset);
        }
        if(f->f_flag&V7_FMP)return mpx_ioctl(f,&p->p_vm,request,argument);
        if(output_file(f))return output_ioctl(f->f_private,&p->p_vm,request,argument);
        if(modem_file(f))return modem_ioctl(f->f_private,&p->p_vm,request,argument);
        if (!f->f_private || (f->f_flag & (V7_FPIPE|V7_FMP)) || (f->f_flag & FILE_STORAGE)) return -NX_ENOTTY;
        return tty_ioctl(f->f_private, &p->p_vm, request, argument);
    }
    if (number == NX_SYS_CLOSE) return close_fd(u, fd);
    if (number == NX_SYS_LSEEK) return seek(f, args[1], args[2]);
    if (number == NX_SYS_DUP || number == NX_SYS_DUP2) {
        int target;
        if (number == NX_SYS_DUP) target = fd_slot(u, 0);
        else {
            if (args[1] >= V7_NOFILE) return -NX_EBADF;
            target = args[1]; if ((uint64_t)target == fd) return target;
        }
        if (target < 0) return target;
        ++f->f_count;
        if (u->u_ofile[target]) (void)close_fd(u, target);
        u->u_ofile[target] = f; u->u_pofile[target] = 0; return target;
    }
    if (number == NX_SYS_FCNTL) {
        switch (args[1]) {
        case NX_F_GETFD: return !!(u->u_pofile[fd] & V7_EXCLOSE);
        case NX_F_SETFD:
            if (args[2] & ~NX_FD_CLOEXEC) return -NX_EINVAL;
            u->u_pofile[fd] = args[2] ? V7_EXCLOSE : 0; return 0;
        case NX_F_GETFL: return f->f_open_flags;
        case NX_F_SETFL:
            if (args[2] & ~(NX_O_ACCMODE | NX_O_APPEND)) return -NX_EINVAL;
            f->f_open_flags = (f->f_open_flags & NX_O_ACCMODE) | (args[2] & NX_O_APPEND); return 0;
        default: return -NX_EINVAL;
        }
    }
    if (number == NX_SYS_FSTAT) {
        struct nx_stat st = {0}; int error = 0;
        if (f->f_flag & V7_FPIPE) { st.mode = 0010000 | 0600; st.size = ((struct pipe_buffer *)f->f_private)->size; st.nlink = 1; }
        else error = stat_inode(inode_node(f->f_inode), &st);
        return error ? error : copyout(&p->p_vm, args[1], &st, sizeof(st));
    }
    return -NX_ENOSYS;
}

/* Public IDs are table slots plus one, never private addresses. */
unsigned files_info_inode_id(const struct inode *ip)
{
    for(unsigned i=0;i<V7_NINODE;++i)if(ip==&inodes[i]&&ip->i_count)return i+1;
    return 0;
}
bool files_info(unsigned i,struct nx_kinfo_file *r)
{
    if(i>=V7_NFILE || !table[i].f_count)return false;
    const struct file *f=&table[i];
    zero(r,sizeof(*r));r->slot=i+1;r->refs=f->f_count;
    r->flags=f->f_flag & ~(FILE_STORAGE|FILE_TAPE);r->open_flags=f->f_open_flags;
    r->offset=f->f_offset;r->inode_id=files_info_inode_id(f->f_inode);
    if(r->inode_id){r->dev=f->f_inode->i_dev;r->ino=f->f_inode->i_number;}
    r->kind=(f->f_flag&V7_FPIPE)?NX_KINFO_FILE_PIPE:
        (f->f_flag&FILE_STORAGE)?NX_KINFO_FILE_STORAGE:
        (f->f_flag&FILE_TAPE)?NX_KINFO_FILE_TAPE:
        (f->f_flag&V7_FMP)?NX_KINFO_FILE_MPX:
        modem_file(f)?NX_KINFO_FILE_MODEM:
        output_file(f)?NX_KINFO_FILE_OUTPUT:
        f->f_private?NX_KINFO_FILE_TTY:
        f->f_inode && (f->f_inode->i_mode&V7_IFMT)==V7_IFCHR && memdev_matches(f->f_inode->i_un.i_rdev)?
        NX_KINFO_FILE_MEMORY:NX_KINFO_FILE_REGULAR;
    return true;
}
bool inodes_info(unsigned i,struct nx_kinfo_inode *r)
{
    if(i>=V7_NINODE || !inodes[i].i_count)return false;
    const struct inode *ip=&inodes[i];zero(r,sizeof(*r));
    r->slot=i+1;r->refs=ip->i_count;r->dev=ip->i_dev;r->ino=ip->i_number;
    r->mode=mpx_mode(inode_node(ip),ip->i_mode);r->uid=ip->i_uid;r->gid=ip->i_gid;r->flags=ip->i_flag;
    r->nlink=ip->i_nlink;r->size=ip->i_size;
    unsigned type=ip->i_mode&V7_IFMT;
    if(type==V7_IFCHR || type==V7_IFBLK)r->rdev=ip->i_un.i_rdev;
    return true;
}

void files_inode_changed(struct v7fs *fs,const struct v7fs_inode *disk)
{
    for(unsigned i=0;i<V7_NINODE;++i){
        struct inode *ip=&inodes[i];
        if(!ip->i_count || ip->i_number!=disk->number || node_volume(inode_node(ip))!=fs)continue;
        ip->i_mode=disk->mode;ip->i_nlink=disk->nlink;ip->i_uid=disk->uid;
        ip->i_gid=disk->gid;ip->i_size=disk->size;
        unsigned type=disk->mode&V7_IFMT;
        if(type==V7_IFCHR || type==V7_IFBLK)ip->i_un.i_rdev=disk->addr[0];
    }
}
