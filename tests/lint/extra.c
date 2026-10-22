long big;
int narrow() { int i; i = big; return(i); }
int dead(n) { if (n) return(n); abort(); }
int quietdead(n) { if (n) return(n); abort(); /* NOTREACHED */ }
int sw(n) { switch (n) { case 1: return(1); break; } return(0); }
int unreach(n) { goto out; n++; out: return(n); }
int use() { return(narrow() + dead(1) + quietdead(2) + sw(3) + unreach(4)); }
