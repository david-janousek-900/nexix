int main(void)
{
    int stops[]={1,0};
    yysvec[1].yystoff=yycrank+1;yysvec[2].yystoff=yycrank+300;
    yysvec[3].yystoff=yycrank;yysvec[3].yystops=stops;
    for(int byte=1;byte<256;++byte){
        yycrank[1+byte]=(struct yywork){1,3};
        yycrank[300+byte]=(struct yywork){2,3};
    }
    for(int byte=1;byte<256;++byte){
        next_byte=byte;available=1;read_calls=0;yysptr=yysbuf;yyprevious=YYNEWLINE;
        int result=yylook();if(result!=1 || yyleng!=1 || (unsigned char)yytext[0]!=byte){fprintf(stderr,"byte=%d result=%d length=%d actual=%d previous=%d\n",byte,result,yyleng,(unsigned char)yytext[0],yyprevious);return 1;}
        assert(yyprevious==byte && yylook()==0);
    }
    puts("PASS: extended lex driver accepts all 255 non-NUL bytes with signed char and native pointers");
}
