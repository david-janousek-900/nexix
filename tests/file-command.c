#include "command-fixture.h"
int main(void)
{
 putfile("/tmp/empty","");putfile("/tmp/text","xyz\n");putfile("/tmp/c","int x;\n");putfile("/tmp/fort","function x\n");putfile("/tmp/asm",".globl x\n");putfile("/tmp/data","\200\201");putfile("/tmp/ar","!<arch>\n");
 command(CMD("/bin/file","/bin/sh"),0,"/bin/sh:\tELF 64-bit LSB executable, x86-64\n","");
 command(CMD("/bin/file","/lib/crt0.o"),0,"/lib/crt0.o:\tELF 64-bit LSB relocatable, x86-64\n","");
 command(CMD("/bin/file","/tmp/empty","/tmp/text","/tmp/c","/tmp/fort","/tmp/asm","/tmp/data","/tmp/ar"),0,"/tmp/empty:\tempty\n/tmp/text:\tascii text\n/tmp/c:\tc program text\n/tmp/fort:\tfortran program text\n/tmp/asm:\tassembler program text\n/tmp/data:\tdata\n/tmp/ar:\tarchive\n","");
 command(CMD("/bin/file","/tmp","/dev/null","/missing"),0,"/tmp:\tdirectory\n/dev/null:\tcharacter special (3/0)\n/missing:\tcannot stat\n","");
 putfile("/tmp/list","/tmp/text");command(CMD("/bin/file","-f","/tmp/list"),1,"/tmp/text:\tascii text\n","");
 command(CMD("/bin/file","-f"),2,"usage: file [-f list] file ...\n","");
 putfile("/tmp/short","\177ELF");command(CMD("/bin/file","/tmp/short"),0,"/tmp/short:\tELF (truncated or unsupported)\n","");
 puts("FILE_OK");return 0;
}
