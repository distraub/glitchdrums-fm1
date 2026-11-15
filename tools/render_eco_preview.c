/* SPDX-License-Identifier: GPL-3.0-only */
#include "engine.h"
#include "output.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <string.h>
static GDProject p;static GDEngine e;
static void u32(FILE *f,uint32_t x){for(unsigned i=0;i<4;++i)fputc((x>>(8*i))&255,f);}
static void u16(FILE *f,uint16_t x){fputc(x&255,f);fputc(x>>8,f);}
int main(int argc,char **argv){assert(argc==2);
 for(unsigned lane=6;lane<8;++lane){gd_defaults(&p);p.reserved=10;memset(&e,0,sizeof e);GDOutput out={65536};char file[1024];snprintf(file,sizeof file,"%s-%s.wav",argv[1],lane==6?"glitch":"analog");FILE *f=fopen(file,"wb");assert(f);unsigned count=4*44100;
 fwrite("RIFF",1,4,f);u32(f,36+count*4);fwrite("WAVEfmt ",1,8,f);u32(f,16);u16(f,1);u16(f,2);u32(f,44100);u32(f,176400);u16(f,4);u16(f,16);fwrite("data",1,4,f);u32(f,count*4);
 double energy=0;int peak=0;unsigned first=count;const int pitch[4]={0,7,12,-12};
 for(unsigned i=0;i<count;++i){if(i%44100==0)gd_hit(&e,&p,lane,100,pitch[i/44100],-1,0);int32_t l,r;gd_render_sample(&e,&p,&l,&r);gd_output_frame(&out,&l,&r,2048);int preview_gain=lane==6?16:6;l*=preview_gain;r*=preview_gain;assert(l>=-28000&&l<=28000&&r>=-28000&&r<=28000);u16(f,(uint16_t)l);u16(f,(uint16_t)r);energy+=(double)l*l+(double)r*r;int a=l<0?-l:l,b=r<0?-r:r;if(a>peak)peak=a;if(b>peak)peak=b;if(first==count&&(a>8||b>8))first=i;}
 fclose(f);printf("%s peak=%d rms=%.1f onset=%u frames\n",file,peak,sqrt(energy/(count*2)),first);
 }return 0;}
