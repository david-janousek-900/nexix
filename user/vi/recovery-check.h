/* Included after the release's identical native struct header definitions. */
static int nx_vi_valid_header(int fd)
{
    struct stat st;
    size_t check_index,check_count,check_name;
    if(fstat(fd,&st)<0 || !S_ISREG(st.st_mode) || st.st_uid!=H.Uid ||
       st.st_size<(off_t)sizeof(H) || H.Flines<0 ||
       H.Flines>=(bbloc)(LBLKS*(BUFSIZ/sizeof(line)))-1 ||
       H.Blocks[0]!=HBLKS || H.Blocks[1]!=HBLKS+1)return 0;
    for(check_name=0;check_name<sizeof(H.Savedfile) && H.Savedfile[check_name];check_name++){}
    if(check_name==sizeof(H.Savedfile))return 0;
    check_count=((size_t)H.Flines+1)*sizeof(line);
    check_count=(check_count+BUFSIZ-1)/BUFSIZ;
    for(check_index=0;check_index<check_count;check_index++)if(H.Blocks[check_index]<0 || (off_t)H.Blocks[check_index]*BUFSIZ>=st.st_size)return 0;
    return 1;
}
