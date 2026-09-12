#ifndef CLOCK_H
#define CLOCK_H
/*
 * kernel/clock.h — RTC (Real Time Clock) okuyucu
 * CMOS RTC: port 0x70 (index), 0x71 (data)
 */
#include <stdint.h>

static uint8_t rtc_read(uint8_t reg){
    __asm__ volatile("outb %0,%1"::"a"(reg),"Nd"((uint16_t)0x70));
    uint8_t v;
    __asm__ volatile("inb %1,%0":"=a"(v):"Nd"((uint16_t)0x71));
    return v;
}
static int rtc_updating(){
    __asm__ volatile("outb %0,%1"::"a"((uint8_t)0x0A),"Nd"((uint16_t)0x70));
    uint8_t v;
    __asm__ volatile("inb %1,%0":"=a"(v):"Nd"((uint16_t)0x71));
    return (v>>7)&1;
}
static uint8_t bcd2bin(uint8_t b){ return (b>>4)*10+(b&0xF); }

struct RtcTime {
    uint8_t hour, min, sec;
    uint8_t day, month;
    uint16_t year;
};

static RtcTime rtc_get(){
    /* Update geçiş sırasında okuma yapma */
    while(rtc_updating());
    uint8_t statusB = 0;
    {
        __asm__ volatile("outb %0,%1"::"a"((uint8_t)0x0B),"Nd"((uint16_t)0x70));
        __asm__ volatile("inb %1,%0":"=a"(statusB):"Nd"((uint16_t)0x71));
    }
    bool bcd = !(statusB & 0x04);

    RtcTime t;
    t.sec   = rtc_read(0x00);
    t.min   = rtc_read(0x02);
    t.hour  = rtc_read(0x04);
    t.day   = rtc_read(0x07);
    t.month = rtc_read(0x08);
    uint8_t yr = rtc_read(0x09);

    if(bcd){ t.sec=bcd2bin(t.sec); t.min=bcd2bin(t.min);
             t.hour=bcd2bin(t.hour); t.day=bcd2bin(t.day);
             t.month=bcd2bin(t.month); yr=bcd2bin(yr); }

    t.year = (uint16_t)(yr < 70 ? 2000+yr : 1900+yr);
    return t;
}

/* 2-digit string yardımcısı */
static void rtc_2d(uint8_t v, char* buf){
    buf[0]='0'+v/10; buf[1]='0'+v%10; buf[2]=0;
}

#endif
