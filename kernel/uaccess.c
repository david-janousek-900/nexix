#include "uaccess.h"
#include "process.h"
#include <nexix/errno.h>

static bool accessible(const struct address_space *space, uintptr_t address,
                       size_t length, bool write)
{
    if (!space || !space->root) return false;
    if (!length) return true;
    if (address < USER_BASE || address >= USER_LIMIT || length > USER_LIMIT - address)
        return false;
    while (length) {
        unsigned permissions;
        if (!vm_translate(space, address, NULL, &permissions) || !(permissions & VM_USER) ||
            (write && !(permissions & VM_WRITE))) return false;
        size_t chunk = PAGE_SIZE - (address & (PAGE_SIZE - 1));
        if (chunk > length) chunk = length;
        length -= chunk;
        address += chunk;
    }
    return true;
}
bool user_access_ok(const struct address_space *space, uintptr_t address, size_t length, bool write)
{
    uintptr_t flags = proc_irq_save();
    bool result = accessible(space, address, length, write);
    proc_irq_restore(flags);
    return result;
}
static int transfer(const struct address_space *space, uintptr_t address,
                    void *buffer, size_t length, bool write, bool instruction)
{
    uintptr_t flags = proc_irq_save();
    if (!accessible(space, address, length, write && !instruction)) {
        proc_irq_restore(flags);
        return -NX_EFAULT;
    }
    if(instruction){
        uintptr_t cursor=address;size_t remaining=length;
        while(remaining){
            unsigned permissions;
            if(!vm_translate(space,cursor,NULL,&permissions)||!(permissions&(VM_WRITE|VM_EXEC))){
                proc_irq_restore(flags);return -NX_EFAULT;
            }
            size_t chunk=PAGE_SIZE-(cursor&(PAGE_SIZE-1));
            if(chunk>remaining)chunk=remaining;
            cursor+=chunk;remaining-=chunk;
        }
    }
    uint8_t *kernel = buffer;
    while (length) {
        uintptr_t physical;
        if (!vm_translate(space, address, &physical, NULL))
            panic("user mapping changed during serialized copy");
        size_t chunk = PAGE_SIZE - (address & (PAGE_SIZE - 1));
        if (chunk > length) chunk = length;
        uint8_t *user = (uint8_t *)physical;
        for (size_t i = 0; i < chunk; ++i) {
            if (write) user[i] = kernel[i];
            else kernel[i] = user[i];
        }
        kernel += chunk;
        address += chunk;
        length -= chunk;
    }
    proc_irq_restore(flags);
    return 0;
}
int copyin(const struct address_space *space, uintptr_t source, void *destination, size_t length)
{ return transfer(space, source, destination, length, false, false); }
int copyout(const struct address_space *space, uintptr_t destination, const void *source, size_t length)
{ return transfer(space, destination, (void *)source, length, true, false); }
int copyout_instruction(const struct address_space *space, uintptr_t destination,
                        const void *source,size_t length)
{ return transfer(space,destination,(void *)source,length,true,true); }
int copyinstr(const struct address_space *space, uintptr_t source, char *destination,
              size_t capacity, size_t *copied)
{
    uintptr_t flags = proc_irq_save();
    size_t count = 0;
    int result = -NX_E2BIG;
    while (count < capacity) {
        if (!accessible(space, source, 1, false)) { result = -NX_EFAULT; break; }
        uintptr_t physical;
        if (!vm_translate(space, source, &physical, NULL)) panic("user string mapping changed");
        size_t chunk = PAGE_SIZE - (source & (PAGE_SIZE - 1));
        if (chunk > capacity - count) chunk = capacity - count;
        const char *bytes = (const char *)physical;
        for (size_t i = 0; i < chunk; ++i) {
            char byte = bytes[i];
            destination[count++] = byte;
            if (!byte) { result = 0; goto done; }
        }
        source += chunk;
    }
done:
    if (copied) *copied = count;
    proc_irq_restore(flags);
    return result;
}
