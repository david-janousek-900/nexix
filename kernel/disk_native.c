#include "disk.h"
#include "ide.h"
static struct block_device auxiliary[3];
static bool initialized;
void disk_native_init(void)
{
    if(initialized)return;
    initialized=true;
    for(unsigned unit=0;unit<3;++unit){
        if(ide_init_unit(&auxiliary[unit],unit+1))continue;
        for(unsigned kind=0;kind<NX_DISK_KINDS;++kind){
            if((kind==NX_DISK_RF && unit) || (kind==NX_DISK_HP && unit>=2))continue;
            (void)disk_bind(kind,unit,&auxiliary[unit]);
        }
    }
}
