#ifndef LUIGI_MATH_H
#define LUIGI_MATH_H
/*
 * apps/luigi_math.h — Gerçek Matematik İfade Çözücü
 * =====================================================
 * Recursive-descent parser: + - * / % ^ ( )
 * Fonksiyonlar: sqrt abs floor ceil
 * Sabitler: pi e
 * 
 * Not: ^ sadece tam sayı üs destekler (bare-metal'de pow() yok)
 * sqrt: Newton-Raphson ile (libm yok)
 * Çıktı: özel double->string formatlayıcı (printf yok)
 */
#include <stdint.h>

/* ── Newton-Raphson sqrt ── */
static double lm_sqrt(double x){
    if(x<0) return -1.0; /* hata işareti */
    if(x==0) return 0.0;
    double guess=x>1?x/2:1.0;
    for(int i=0;i<40;i++){
        double ng=0.5*(guess+x/guess);
        if(ng==guess) break;
        guess=ng;
    }
    return guess;
}
static double lm_abs(double x){ return x<0?-x:x; }
static double lm_floor(double x){
    long long i=(long long)x;
    if(x<0 && (double)i!=x) i--;
    return (double)i;
}
static double lm_ceil(double x){
    long long i=(long long)x;
    if(x>0 && (double)i!=x) i++;
    return (double)i;
}
static double lm_pow_int(double base,long long exp){
    if(exp==0) return 1.0;
    bool neg=exp<0; if(neg) exp=-exp;
    double r=1.0;
    while(exp>0){
        if(exp&1) r*=base;
        base*=base; exp>>=1;
    }
    return neg ? 1.0/r : r;
}

/* ── Double → string (printf yok) ── */
/* maxdec basamak hassasiyeti, sonda sıfırları kırpar */
static void lm_dtoa(double v, char* out, int maxdec=6){
    int oi=0;
    if(v<0){ out[oi++]='-'; v=-v; }
    /* Tam kısım */
    long long ip=(long long)v;
    double frac=v-(double)ip;
    /* Tam kısmı yaz (ters çevirip düz koy) */
    char tmp[24]; int ti=0;
    if(ip==0){ tmp[ti++]='0'; }
    while(ip>0){ tmp[ti++]=(char)('0'+ip%10); ip/=10; }
    for(int k=ti-1;k>=0;k--) out[oi++]=tmp[k];
    /* Ondalık kısım */
    if(maxdec>0){
        out[oi++]='.';
        int written=0;
        char dec[16];
        for(int i=0;i<maxdec;i++){
            frac*=10;
            int d=(int)frac;
            if(d>9)d=9; if(d<0)d=0;
            dec[i]=(char)('0'+d);
            frac-=d;
        }
        /* Sondaki sıfırları kırp */
        int last=maxdec-1;
        while(last>0 && dec[last]=='0') last--;
        for(int i=0;i<=last;i++){ out[oi++]=dec[i]; written++; }
        if(written==0){ oi--; } /* tüm ondalık sıfırsa noktayı da kaldır */
    }
    out[oi]=0;
}

/* ── Tokenizer + Parser ── */
struct LuigiMathResult {
    bool   ok;
    double value;
    char   error[48];
};

struct LuigiMathParser {
    const char* s;
    int pos, len;
    bool error_flag;
    char error_msg[48];

    void init(const char* expr){
        s=expr; pos=0; len=0;
        while(s[len]) len++;
        error_flag=false; error_msg[0]=0;
    }
    void skip_ws(){ while(pos<len && (s[pos]==' '||s[pos]=='\t')) pos++; }
    char peek(){ skip_ws(); return pos<len ? s[pos] : 0; }
    char advance(){ skip_ws(); return pos<len ? s[pos++] : 0; }

    bool match_word(const char* w){
        skip_ws();
        int wl=0; while(w[wl]) wl++;
        if(pos+wl>len) return false;
        for(int i=0;i<wl;i++) if(s[pos+i]!=w[i]) return false;
        /* Kelime sonrası alfanumerik olmamalı (örn "sqrt2" değil "sqrt(2)") */
        pos+=wl; return true;
    }

    void err(const char* m){
        if(!error_flag){
            error_flag=true;
            int i=0; while(m[i]&&i<47){error_msg[i]=m[i];i++;} error_msg[i]=0;
        }
    }

    /* Grammar:
       expr   := term (('+'|'-') term)*
       term   := factor (('*'|'/'|'%') factor)*
       factor := unary ('^' unary)?      (sağdan-sola değil, basit sol-sağ — limit doc'lu)
       unary  := ('-' unary) | primary
       primary:= number | const | func '(' expr ')' | '(' expr ')'
    */
    double parse_expr(){
        double v=parse_term();
        for(;;){
            char c=peek();
            if(c=='+'){ advance(); v+=parse_term(); }
            else if(c=='-'){ advance(); v-=parse_term(); }
            else break;
        }
        return v;
    }
    double parse_term(){
        double v=parse_factor();
        for(;;){
            char c=peek();
            if(c=='*'){ advance(); v*=parse_factor(); }
            else if(c=='/'){
                advance(); double d=parse_factor();
                if(d==0){ err("divide by zero"); return 0; }
                v/=d;
            }
            else if(c=='%'){
                advance(); double d=parse_factor();
                if(d==0){ err("divide by zero"); return 0; }
                long long a=(long long)v, b=(long long)d;
                v=(double)(a - (a/b)*b); /* tam sayı mod */
            }
            else break;
        }
        return v;
    }
    double parse_factor(){
        double v=parse_unary();
        if(peek()=='^'){
            advance();
            double e=parse_unary();
            /* Sadece tam sayı üs desteklenir */
            long long ei=(long long)e;
            if((double)ei!=e){ err("only integer exponents supported"); return v; }
            v=lm_pow_int(v,ei);
        }
        return v;
    }
    double parse_unary(){
        skip_ws();
        if(peek()=='-'){ advance(); return -parse_unary(); }
        if(peek()=='+'){ advance(); return parse_unary(); }
        return parse_primary();
    }
    double parse_primary(){
        skip_ws();
        if(error_flag) return 0;
        char c=peek();
        if(c=='('){
            advance();
            double v=parse_expr();
            if(peek()==')') advance(); else err("missing )");
            return v;
        }
        /* Fonksiyonlar */
        if(match_word("sqrt")){
            if(peek()!='('){ err("expected ( after sqrt"); return 0; }
            advance(); double v=parse_expr();
            if(peek()==')') advance(); else err("missing )");
            if(v<0){ err("sqrt of negative number"); return 0; }
            return lm_sqrt(v);
        }
        if(match_word("abs")){
            if(peek()!='('){ err("expected ( after abs"); return 0; }
            advance(); double v=parse_expr();
            if(peek()==')') advance(); else err("missing )");
            return lm_abs(v);
        }
        if(match_word("floor")){
            if(peek()!='('){ err("expected ( after floor"); return 0; }
            advance(); double v=parse_expr();
            if(peek()==')') advance(); else err("missing )");
            return lm_floor(v);
        }
        if(match_word("ceil")){
            if(peek()!='('){ err("expected ( after ceil"); return 0; }
            advance(); double v=parse_expr();
            if(peek()==')') advance(); else err("missing )");
            return lm_ceil(v);
        }
        /* Sabitler */
        if(match_word("pi")) return 3.14159265358979323846;
        if(match_word("e"))  return 2.71828182845904523536;

        /* Sayı */
        if((c>='0'&&c<='9') || c=='.'){
            double v=0; bool any=false;
            while(peek()>='0'&&peek()<='9'){ v=v*10+(advance()-'0'); any=true; }
            if(peek()=='.'){
                advance();
                double frac=0.1;
                while(peek()>='0'&&peek()<='9'){ v+=(advance()-'0')*frac; frac*=0.1; any=true; }
            }
            if(!any) err("invalid number");
            return v;
        }
        err("unexpected character");
        return 0;
    }
};

static LuigiMathResult luigi_eval_math(const char* expr){
    LuigiMathResult r; r.ok=true; r.value=0; r.error[0]=0;
    LuigiMathParser p; p.init(expr);
    double v=p.parse_expr();
    p.skip_ws();
    if(!p.error_flag && p.pos<p.len) p.err("unexpected trailing characters");
    if(p.error_flag){
        r.ok=false;
        int i=0; while(p.error_msg[i]&&i<47){ r.error[i]=p.error_msg[i]; i++; } r.error[i]=0;
    } else {
        r.value=v;
    }
    return r;
}

/* Bir string'in matematik ifadesi olup olmadığını sezgisel kontrol et */
static bool luigi_looks_like_math(const char* s){
    int digit=0, op=0, alpha=0, total=0;
    for(int i=0;s[i];i++){
        char c=s[i];
        if(c==' ') continue;
        total++;
        if(c>='0'&&c<='9') digit++;
        else if(c=='+'||c=='-'||c=='*'||c=='/'||c=='^'||c=='%'||c=='('||c==')'||c=='.') op++;
        else if((c>='a'&&c<='z')||(c>='A'&&c<='Z')) alpha++;
    }
    if(total==0) return false;
    /* En az bir rakam + (operatör veya fonksiyon kelimesi) olmalı */
    bool has_func = false;
    const char* funcs[]={"sqrt","abs","floor","ceil","pi",nullptr};
    for(int i=0;funcs[i];i++){
        const char* f=funcs[i]; int j=0,k=0;
        for(j=0; s[j]; j++){
            k=0; while(f[k]&&s[j+k]==f[k])k++;
            if(!f[k]){ has_func=true; break; }
        }
        if(has_func) break;
    }
    if(digit==0 && !has_func) return false;
    if(op==0 && !has_func) return false;
    /* Çok fazla harf varsa (kelime cümlesi) matematik değildir */
    if(alpha > digit+op+4 && !has_func) return false;
    return true;
}
#endif
