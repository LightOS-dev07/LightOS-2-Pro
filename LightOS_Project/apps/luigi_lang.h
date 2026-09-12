#ifndef LUIGI_LANG_H
#define LUIGI_LANG_H
/*
 * apps/luigi_lang.h — Dil Algılama (TR/EN)
 * ============================================
 * Türkçe özel karakterler (0x01-0x06 keyboard.cpp kodları)
 * + kelime listesi skorlama ile otomatik algılama.
 */
#include <stdint.h>

enum LuigiLang { LUIGI_TR=0, LUIGI_EN=1 };

/* Türkçe'ye özel codepoint'ler: 0x01=ğ 0x02=ü 0x03=ş 0x04=ı 0x05=ö 0x06=ç */
static bool luigi_has_tr_codepoint(const char* s){
    for(int i=0;s[i];i++){
        unsigned char c=(unsigned char)s[i];
        if(c>=0x01 && c<=0x06) return true;
    }
    return false;
}

/* Basit lowercase (ASCII only — TR özel karakterler zaten ayrı kontrol edildi) */
static char luigi_lc(char c){
    if(c>='A'&&c<='Z') return (char)(c-'A'+'a');
    return c;
}

static bool luigi_word_eq(const char* a, const char* b, int alen){
    int i=0;
    for(;i<alen;i++){
        if(!b[i]) return false;
        if(luigi_lc(a[i])!=luigi_lc(b[i])) return false;
    }
    return !b[i]; /* b da tam bitmeli */
}

/* Kelime listesi içinde ara (boşlukla ayrılmış metinde tam kelime eşleşmesi) */
static int luigi_count_matches(const char* text, const char* const* words, int wcount){
    int score=0;
    int i=0;
    while(text[i]){
        while(text[i]==' ') i++;
        int start=i;
        while(text[i] && text[i]!=' ') i++;
        int wlen=i-start;
        if(wlen>0){
            for(int w=0; w<wcount; w++){
                if(luigi_word_eq(text+start, words[w], wlen)){ score++; break; }
            }
        }
    }
    return score;
}

static const char* LUIGI_TR_WORDS[] = {
    "selam","merhaba","naber","nasilsin","tesekkur","tesekkurler","sagol",
    "evet","hayir","lutfen","yardim","soru","cevap","nedir","kimsin","sensin",
    "nasil","neden","niye","ne","kim","nerede","ne zaman","kac","kaç",
    "bana","sana","benim","senin","bir","iki","uc","dort","bes","yapabilir",
    "misin","musun","var","yok","istiyorum","acabilir","misin","oyun","ac",
    "kapat","goster","anlat","soyle","yaz","hesapla","kac","tane","merhabalar",
    "gunaydin","iyi","aksamlar","gece","gunler","tamam","ok","peki"
};
static const char* LUIGI_EN_WORDS[] = {
    "hello","hi","hey","how","are","you","thanks","thank","yes","no",
    "please","help","what","who","where","when","why","which","is",
    "the","can","could","would","i","my","your","one","two","three",
    "open","close","show","tell","write","calculate","game","good",
    "morning","evening","night","okay","ok","sure","want","need"
};

static LuigiLang luigi_detect_lang(const char* text, LuigiLang fallback){
    if(luigi_has_tr_codepoint(text)) return LUIGI_TR;

    int tr_score = luigi_count_matches(text, LUIGI_TR_WORDS,
        (int)(sizeof(LUIGI_TR_WORDS)/sizeof(LUIGI_TR_WORDS[0])));
    int en_score = luigi_count_matches(text, LUIGI_EN_WORDS,
        (int)(sizeof(LUIGI_EN_WORDS)/sizeof(LUIGI_EN_WORDS[0])));

    if(tr_score==0 && en_score==0) return fallback;
    return (tr_score >= en_score) ? LUIGI_TR : LUIGI_EN;
}
#endif
