#ifndef LANG_H
#define LANG_H
#include <stdint.h>

enum LangID { LANG_TR=0, LANG_EN=1 };
extern LangID g_lang;

enum StrID {
    S_QUICKM=0,S_SHUTDOWN,S_RESTART,S_LANGUAGE,
    S_MINIMIZE,S_CLOSE,S_RESTORE,
    S_NOTEPAD,S_CALCULATOR,S_FILES,S_SETTINGS,
    S_TERMINAL,S_PAINT,S_SYSINFO,S_CLOCK,S_BROWSER,S_CALENDAR,S_TASKMAN,S_XAEFGAMES,S_LUIGI,S_HEXED,S_MUSIC,
    S_NOTEPAD_TITLE,S_FILES_TITLE,
    S_ADDR_BAR,S_SIDEBAR_DRIVES,S_DISK_C,S_DISK_D_EMPTY,
    S_STATUS_FILES,S_FOLDER_SYSTEM,S_FOLDER_USERS,
    S_FOLDER_APPS,S_FOLDER_TEMP,
    S_CTX_REFRESH,S_CTX_NEWFOLDER,S_CTX_PROPS,
    S_CALC_TITLE,S_DESKTOP,S_LIGHTOS,
    S_MENU_TITLE,S_MENU_SUB,
    S_COUNT
};

struct LangTable { const char* s[S_COUNT]; };

static const LangTable LANG_STRINGS[2]={
/* TR */{{"QuickM","Kapat","Yeniden Baslat","EN",
    "Kucult","Kapat","Geri Yukle",
    "Not Defteri","Hesap Makinesi","Dosyalar","Ayarlar",
    "Terminal","Paint","Sistem Bilgisi","Saat","LightSurf","Takvim","Gorev Yon.","XaefGAMES","Luigi AI","Hex Editoru","Muzik",
    "Not Defteri","Dosya Gezgini",
    "Adres:","Surucüler","C: LightDisk","D: (Bos)",
    "4 klasor 3 dosya","Sistem","Kullaniclar","Uygulamalar","Gecici",
    "Yenile","Yeni Klasor","Ozellikler",
    "Hesap Makinesi","Masaustu","LightOS",
    "QuickM","Menu"}},
/* EN */{{"QuickM","Shut Down","Restart","TR",
    "Minimize","Close","Restore",
    "Notepad","Calculator","Files","Settings",
    "Terminal","Paint","System Info","Clock","LightSurf","Calendar","Task Manager","XaefGAMES","Luigi AI","Hex Editor","Music",
    "Notepad","File Explorer",
    "Address:","Drives","C: LightDisk","D: (Empty)",
    "4 folders 3 files","System","Users","Apps","Temp",
    "Refresh","New Folder","Properties",
    "Calculator","Desktop","LightOS",
    "QuickM","Menu"}},
};

static const char* LS(StrID id){
    return LANG_STRINGS[(int)g_lang].s[id];
}
#endif
