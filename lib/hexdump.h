/*
 * Copyright (c) 2026 ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */

static u8 _hdbuf[17];

static void hexdump(u8 *data, u32 size)
{
    for(int i = 0; i<size;i+=16)
    {
        for (int shift = 28; shift >= 0; shift -= 4) {
            printf("%x", (i >> shift) & 0xF);
        }
        printf(": ");

        for(int j = 0;j<16 && (i+j)<size;j++) {
            if(data[i+j]<0x10)printf("0");
            printf("%x ",data[i+j]);
        }
        printf(" |");
        for(int j = 0;j<16 && (i+j)<size;j++) {
            if(data[i+j]<0x20 || data[i+j]>=0x7F)printf(".");
            else printf("%c",data[i+j]);
        }
        printf("|\n");
    }
}
