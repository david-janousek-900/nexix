/* Native V7 fio/sys2/sys3/pipe ownership and descriptor semantics. */
#include "files.h"
#include "tty.h"
#include "signals.h"
#include "uaccess.h"
#include "exec.h"
#include "text.h"
#include <nexix/syscall.h>
#include <nexix/errno.h>
#define NPIPE 32
struct pipe_buffer { bool active; unsigned readers, writers, head, size; uint8_t bytes[NX_PIPE_BUF]; };
static struct pipe_buffer pipes[NPIPE];
static struct file table[V7_NFILE];
static struct inode inodes[V7_NINODE];
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
int files_mount(struct v7fs *fs)
{
    int error=text_purge_unused();if(error)return error;
    if (files_live()) return -NX_EBUSY;
    if (fs && !fs->mounted) return -NX_ENODEV;
    root = fs; root_exec = false; return 0;
}
bool files_refs_valid(const struct user *u)
{
    if ((u->u_cdir && !valid_inode(u->u_cdir)) || (u->u_rdir && !valid_inode(u->u_rdir)) || u->u_pdir) return false;
    for (unsigned i = 0; i < V7_NOFILE; ++i)
        if (u->u_ofile[i] && !valid_file(u->u_ofile[i])) return false;
    return true;
}
static struct inode *inode_get(v7_ino_t ino, const struct v7fs_inode *disk)
{
    struct inode *slot = NULL;
    for (unsigned i = 0; i < V7_NINODE; ++i) {
        if (inodes[i].i_count && inodes[i].i_number == ino) { ++inodes[i].i_count; return &inodes[i]; }
        if (!inodes[i].i_count && !slot) slot = &inodes[i];
    }
    if (!slot) return NULL;
    *slot = (struct inode){.i_count = 1, .i_number = ino, .i_mode = disk->mode,
        .i_nlink = disk->nlink, .i_uid = disk->uid, .i_gid = disk->gid, .i_size = disk->size};
    return slot;
}
int files_inode_put(struct inode *inode)
{
    if (!valid_inode(inode)) panic("invalid inode release");
    if (--inode->i_count) return 0;
    struct v7fs_inode disk;
    int error = v7fs_inode_get(root, inode->i_number, &disk);
    if (!error && !disk.nlink) error = v7fs_release_inode(root, inode->i_number);
    zero(inode, sizeof(*inode)); return error;
}
static struct file *file_alloc(void)
{
    for (unsigned i = 0; i < V7_NFILE; ++i) if (!table[i].f_count) {
        zero(&table[i], sizeof(table[i])); table[i].f_count = 1; return &table[i];
    }
    return NULL;
}
static int file_put(struct file *f)
{
    if (!valid_file(f)) panic("invalid open file release");
    if (--f->f_count) return 0;
    int error = 0;
    if (f->f_private && !(f->f_flag & V7_FPIPE)) tty_close(f->f_private);
    if (f->f_inode) error = files_inode_put(f->f_inode);
    if (f->f_flag & V7_FPIPE) {
        struct pipe_buffer *p = f->f_private;
        if (f->f_flag & V7_FREAD) --p->readers; else --p->writers;
        proc_wakeup(p);
        if (!p->readers && !p->writers) zero(p, sizeof(*p));
    }
    zero(f, sizeof(*f)); return error;
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
static v7_ino_t cwd(struct user *u) { return u->u_cdir ? u->u_cdir->i_number : V7_ROOTINO; }
static v7_ino_t rootdir(struct user *u) { return u->u_rdir ? u->u_rdir->i_number : V7_ROOTINO; }
static int path_copy(uintptr_t address, char path[256])
{ return copyinstr(&proc_current()->p_vm, address, path, 256, NULL); }
static int lookup_path(struct user *u, const char *path, v7_ino_t *ino)
{ return v7fs_lookup_root_user(root, cwd(u), rootdir(u), path, ino, u->u_uid, u->u_gid); }
bool files_exec_enabled(void) { return root_exec; }
int files_mount_root(struct v7fs *fs)
{
    if (!fs) return -NX_EINVAL;
    int error = files_mount(fs);
    if (!error) root_exec = true;
    return error;
}
static struct inode *cached_inode(v7_ino_t ino)
{
    for(unsigned i=0;i<V7_NINODE;++i)if(inodes[i].i_count&&inodes[i].i_number==ino)return &inodes[i];
    return NULL;
}
static int text_write_check(v7_ino_t ino)
{
    struct inode *inode=cached_inode(ino);
    if(!inode||!(inode->i_flag&V7_ITEXT))return 0;
    int error=text_evict(inode);if(error)return error;
    return inode->i_count&&(inode->i_flag&V7_ITEXT)?-NX_ETXTBSY:0;
}
int files_image_text(v7_ino_t ino,struct exec_image *image)
{
    struct v7fs_inode disk;int error=v7fs_inode_get(root,ino,&disk);if(error)return error;
    struct inode *inode=inode_get(ino,&disk);if(!inode)return -NX_ENFILE;
    error=text_acquire(inode,image->text,&image->owner);
    (void)files_inode_put(inode);return error;
}
int files_load_exec(const char *path, const struct exec_args *args, struct exec_image *image)
{
    if (!root_exec || !proc_current()) return -NX_ENODEV;
    struct user *u = proc_current()->p_user;
    v7_ino_t ino;
    int error = lookup_path(u, path, &ino);
    if (error) return error;
    struct v7fs_inode node;
    error = v7fs_inode_get(root, ino, &node);
    if (error) return error;
    /* Check access using the caller's credentials, before any set-ID change. */
    if ((node.mode & V7_IFMT) != V7_IFREG || !(node.mode & 0111) ||
        !v7fs_access(&node, u->u_uid, u->u_gid, 1)) return -NX_EACCES;
    error = elf_load_inode(root, ino, args, image);
    if (!error) error=files_image_text(ino,image);
    if (error) { exec_image_release(image); return error; }
    image->mode=node.mode; image->uid=node.uid; image->gid=node.gid;
    return error;
}
static int parent_path(struct user *u, const char *path, v7_ino_t *parent, char name[V7_DIRSIZ])
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
    error = v7fs_inode_get(root, *parent, &dir);
    if (error) return error;
    if ((dir.mode & V7_IFMT) != V7_IFDIR) return -NX_ENOTDIR;
    return v7fs_access(&dir, u->u_uid, u->u_gid, 3) ? 0 : -NX_EACCES;
}
int files_core_begin(struct inode **result)
{
    *result=NULL;
    if(!root||!proc_current())return -NX_ENODEV;
    if(root->read_only)return -NX_EROFS;
    struct user *u=proc_current()->p_user;
    if(u->u_uid!=u->u_ruid||u->u_gid!=u->u_rgid)return -NX_EPERM;
    v7_ino_t ino;int error=lookup_path(u,"core",&ino);
    if(error==-NX_ENOENT){
        v7_ino_t parent;char name[V7_DIRSIZ];
        error=parent_path(u,"core",&parent,name);
        bool free=false;for(unsigned i=0;i<V7_NINODE;++i)free|=!inodes[i].i_count;
        if(!error&&!free)error=-NX_ENFILE;
        if(!error)error=v7fs_create(root,parent,name,0666&~u->u_cmask,u->u_uid,u->u_gid,&ino);
    }
    struct v7fs_inode disk;
    if(!error)error=v7fs_inode_get(root,ino,&disk);
    if(!error&&(disk.mode&V7_IFMT)!=V7_IFREG)error=-NX_EACCES;
    if(!error)error=text_write_check(ino);
    if(!error&&!v7fs_access(&disk,u->u_uid,u->u_gid,2))error=-NX_EACCES;
    if(error)return error;
    struct inode *inode=inode_get(ino,&disk);if(!inode)return -NX_ENFILE;
    error=v7fs_truncate(root,ino);
    if(error){(void)files_inode_put(inode);return error;}
    *result=inode;return 0;
}
int files_core_write(struct inode *inode,uint64_t offset,const void *bytes,size_t size)
{
    if(!valid_inode(inode)||offset>INT64_MAX)return -NX_EINVAL;
    size_t done=0;int error=v7fs_write(root,inode->i_number,(int64_t)offset,bytes,size,&done);
    return error?error:done!=size?-NX_EIO:0;
}
static int open_path(struct user *u, uintptr_t address, uint64_t flags, uint64_t mode)
{
    uint64_t allowed = NX_O_ACCMODE | NX_O_CREAT | NX_O_TRUNC | NX_O_EXCL | NX_O_APPEND | NX_O_CLOEXEC;
    if ((flags & ~allowed) || (flags & NX_O_ACCMODE) == 3 ||
        ((flags & NX_O_TRUNC) && !(flags & NX_O_ACCMODE))) return -NX_EINVAL;
    int fd = fd_slot(u, 0); if (fd < 0) return fd;
    char path[256]; int error = path_copy(address, path); if (error) return error;
    if (!root) return -NX_ENODEV;
    struct file *f = file_alloc(); if (!f) return -NX_ENFILE;
    v7_ino_t ino;
    error = lookup_path(u, path, &ino);
    bool created = false;
    if (error == -NX_ENOENT && (flags & NX_O_CREAT)) {
        v7_ino_t parent; char name[V7_DIRSIZ];
        error = parent_path(u, path, &parent, name);
        bool available = false;
        for (unsigned i = 0; i < V7_NINODE; ++i) available |= !inodes[i].i_count;
        if (!error && !available) error = -NX_ENFILE;
        if (!error) error = v7fs_create(root, parent, name, mode & ~u->u_cmask & 0777, u->u_uid, u->u_gid, &ino);
        created = !error;
    } else if (!error && (flags & NX_O_CREAT) && (flags & NX_O_EXCL)) error = -NX_EEXIST;
    struct v7fs_inode disk;
    if (!error) error = v7fs_inode_get(root, ino, &disk);
    unsigned access = flags & NX_O_ACCMODE;
    if(!error&&access&&(disk.mode&V7_IFMT)==V7_IFREG){
        error=root->read_only?-NX_EROFS:text_write_check(ino);
    }
    if (!error && !created && !v7fs_access(&disk, u->u_uid, u->u_gid, access == 0 ? 4 : access == 1 ? 2 : 6)) error = -NX_EACCES;
    if (!error && access && (disk.mode & V7_IFMT) == V7_IFDIR) error = -NX_EISDIR;
    unsigned type = error ? 0 : disk.mode & V7_IFMT;
    if (!error && type != V7_IFREG && type != V7_IFDIR && type != V7_IFCHR) error = -NX_ENODEV;
    if (!error && access && type == V7_IFREG && root->read_only) error = -NX_EROFS;
    if (!error) {
        f->f_inode = inode_get(ino, &disk);
        if (!f->f_inode) error = -NX_ENFILE;
    }
    if (!error && (flags & NX_O_TRUNC) && type == V7_IFREG) error = v7fs_truncate(root, ino);
    if (!error && type == V7_IFCHR && disk.addr[0] != NX_NULL_DEV) {
        struct tty *tty;
        error = tty_open(disk.addr[0], u, &tty);
        if (!error) f->f_private = tty;
    }
    if (error) { (void)file_put(f); return error; }
    f->f_flag = access == 0 ? V7_FREAD : access == 1 ? V7_FWRITE : V7_FREAD | V7_FWRITE;
    f->f_open_flags = flags & (NX_O_ACCMODE | NX_O_APPEND);
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
    if (f->f_flag & V7_FPIPE) return pipe_io(f, address, count, write);
    if (f->f_private) {
        /* V7 readi/writei also stamp character-device inodes. Pipes have no
         * persistent inode. Keep the established zero-count TTY behavior. */
        if (write || count) {
            int error = v7fs_touch(root, f->f_inode->i_number,
                write ? V7FS_MTIME | V7FS_CTIME : V7FS_ATIME);
            if (error) return error;
        }
        return tty_io(f->f_private, vm, address, count, write);
    }
    struct v7fs_inode inode;
    int error = v7fs_inode_get(root, f->f_inode->i_number, &inode);
    if (error) return error;
    if ((inode.mode & V7_IFMT) == V7_IFCHR && inode.addr[0] == NX_NULL_DEV) {
        if (write || count) {
            error = v7fs_touch(root, inode.number, write ? V7FS_MTIME | V7FS_CTIME : V7FS_ATIME);
            if (error) return error;
        }
        return write ? (int64_t)count : 0;
    }
    if (write && (f->f_open_flags & NX_O_APPEND)) f->f_offset = inode.size;
    size_t done = 0; uint8_t bytes[512];
    while (done < count) {
        size_t n = count - done; if (n > sizeof(bytes)) n = sizeof(bytes);
        size_t moved = 0;
        if (write) {
            if (copyin(vm, address + done, bytes, n)) panic("file input mapping changed");
            error = v7fs_write(root, inode.number, f->f_offset, bytes, n, &moved);
        } else {
            error = v7fs_read(root, inode.number, f->f_offset, bytes, n, &moved);
            if (moved && copyout(vm, address + done, bytes, moved)) panic("file output mapping changed");
        }
        done += moved; f->f_offset += moved;
        if (error || moved < n) break;
    }
    return done ? (int64_t)done : error;
}
static int64_t seek(struct file *f, int64_t offset, uint64_t whence)
{
    if ((f->f_flag & V7_FPIPE) || f->f_private) return -NX_ESPIPE;
    int64_t base = 0;
    if (whence == NX_SEEK_CUR) base = f->f_offset;
    else if (whence == NX_SEEK_END) {
        struct v7fs_inode inode; int error = v7fs_inode_get(root, f->f_inode->i_number, &inode);
        if (error) return error;
        base = inode.size;
    } else if (whence != NX_SEEK_SET) return -NX_EINVAL;
    if (offset < -base || offset > INT64_MAX - base) return -NX_EINVAL;
    f->f_offset = base + offset; return f->f_offset;
}
static int stat_inode(v7_ino_t ino, struct nx_stat *st)
{
    struct v7fs_inode i; int error = v7fs_inode_get(root, ino, &i); if (error) return error;
    *st = (struct nx_stat){.ino = ino, .mode = i.mode, .nlink = i.nlink, .uid = i.uid, .gid = i.gid,
        .rdev = ((i.mode & V7_IFMT) == V7_IFCHR || (i.mode & V7_IFMT) == V7_IFBLK) ? i.addr[0] : 0,
        .size = i.size, .atime = i.atime, .mtime = i.mtime, .ctime = i.ctime}; return 0;
}
int64_t files_syscall(uint64_t number, const uintptr_t args[6])
{
    struct proc *p = proc_current(); struct user *u = p->p_user;
    uint64_t fd = args[0];
    if (number == NX_SYS_SYNC) return root ? v7fs_sync(root) : 0; /* Write-through cache plus V7 superblock time. */
    if (number == NX_SYS_MKNOD) {
        if (u->u_uid) return -NX_EPERM;
        if (args[1] > UINT16_MAX || args[2] > UINT16_MAX) return -NX_EINVAL;
        char path[256], name[V7_DIRSIZ]; v7_ino_t parent, ino;
        int error = path_copy(args[0], path); if (error) return error;
        if (!root) return -NX_ENODEV;
        error = parent_path(u, path, &parent, name);
        uint16_t mode = args[1];
        if (!(mode & V7_IFMT)) mode |= V7_IFREG;
        return error ? error : v7fs_mknod(root, parent, name, mode & ~(u->u_cmask & 0777), u->u_uid, u->u_gid, args[2], &ino);
    }
    if (number == NX_SYS_ACCESS || number == NX_SYS_CHMOD || number == NX_SYS_CHOWN || number == NX_SYS_UTIME) {
        char path[256]; int error = path_copy(args[0], path); if (error) return error;
        if (!root) return -NX_ENODEV;
        bool access = number == NX_SYS_ACCESS;
        if (access && args[1] > 7) return -NX_EINVAL;
        if (number == NX_SYS_CHOWN && (args[1] > UINT16_MAX || args[2] > UINT16_MAX)) return -NX_EINVAL;
        v7_ino_t ino;
        error = v7fs_lookup_root_user(root, cwd(u), rootdir(u), path, &ino, access ? u->u_ruid : u->u_uid, access ? u->u_rgid : u->u_gid);
        struct v7fs_inode disk;
        if (!error) error = v7fs_inode_get(root, ino, &disk);
        if (error) return error;
        if (access) {
            if ((args[1] & 2) && root->read_only) return -NX_EROFS;
            if(args[1]&2){error=text_write_check(ino);if(error)return error;}
            return v7fs_access(&disk, u->u_ruid, u->u_rgid, args[1]) ? 0 : -NX_EACCES;
        }
        if (u->u_uid && (number == NX_SYS_CHOWN || u->u_uid != disk.uid)) return -NX_EPERM;
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
        error = v7fs_metadata(root, &disk);
        if (!error) for (unsigned i = 0; i < V7_NINODE; ++i)
            if (inodes[i].i_count && inodes[i].i_number == ino) {
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
        if (number == NX_SYS_CHROOT && u->u_uid) return -NX_EPERM;
        char path[256]; int error = path_copy(args[0], path); if (error) return error;
        if (!root) return -NX_ENODEV;
        v7_ino_t ino;
        if (number == NX_SYS_UNLINK) {
            v7_ino_t parent; char name[V7_DIRSIZ];
            error = parent_path(u, path, &parent, name);
            if(!error){
                struct v7fs_inode disk;
                error=lookup_path(u,path,&ino);
                if(!error)error=v7fs_inode_get(root,ino,&disk);
                if(!error){
                    struct inode *inode=cached_inode(ino);
                    if(inode)error=text_evict(inode);
                    if(!error&&disk.nlink==1&&inode&&inode->i_count&&(inode->i_flag&V7_ITEXT))error=-NX_ETXTBSY;
                }
            }
            if (!error) error = v7fs_unlink_privileged(root, parent, name, &ino, !u->u_uid);
            if (error) return error;
            for (unsigned i = 0; i < V7_NINODE; ++i) if (inodes[i].i_count && inodes[i].i_number == ino) return 0;
            struct v7fs_inode disk; error = v7fs_inode_get(root, ino, &disk);
            return error ? error : disk.nlink ? 0 : v7fs_release_inode(root, ino);
        }
        error = lookup_path(u, path, &ino); if (error) return error;
        if (number == NX_SYS_LINK) {
            struct v7fs_inode source;
            error = v7fs_inode_get(root, ino, &source); if (error) return error;
            if ((source.mode & V7_IFMT) == V7_IFDIR && u->u_uid) return -NX_EPERM;
            char target[256], name[V7_DIRSIZ]; v7_ino_t parent;
            error = path_copy(args[1], target); if (error) return error;
            error = parent_path(u, target, &parent, name);
            return error ? error : v7fs_link_privileged(root, ino, parent, name, !u->u_uid);
        }
        if (number == NX_SYS_STAT) {
            struct nx_stat st; error = stat_inode(ino, &st);
            return error ? error : copyout(&p->p_vm, args[1], &st, sizeof(st));
        }
        struct v7fs_inode disk; error = v7fs_inode_get(root, ino, &disk);
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
        if (!f->f_private || (f->f_flag & V7_FPIPE)) return -NX_ENOTTY;
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
        else error = stat_inode(f->f_inode->i_number, &st);
        return error ? error : copyout(&p->p_vm, args[1], &st, sizeof(st));
    }
    return -NX_ENOSYS;
}
