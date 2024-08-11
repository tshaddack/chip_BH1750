/*

compile: cc -o bh1750 bh1750.c

public domain by Shaddack

do what you wish

*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>


#define BH1750_ADDR  0x23  // 0b 010 0011   ADDR=L (default)
#define BH1750_ADDR2 0x5C  // 0b 101 1100   ADDR=H

#define BH1750_POWER_DOWN             0x00
#define BH1750_POWER_UP               0x01
#define BH1750_RESET                  0x07

#define BH1750_ONE_TIME_H_RES_MODE    0x20 /* auto-mode for BH1721 */
#define BH1750_ONE_TIME_H_RES_MODE2   0x21
#define BH1750_CHANGE_INT_TIME_H_BIT  0x40
#define BH1750_CHANGE_INT_TIME_L_BIT  0x60

#define BH1750_RES_MAX 254        // max output in mode 2 =   7417 lux
#define BH1750_RES_DEFAULT 69     // max output in mode 2 =  27306 lux
#define BH1750_RES_LOW 20         // max output in mode 2 =  94206 lux
#define BH1750_RES_MIN 5          // max output in mode 2 = 376826 lux

#define LUX_OVERFLOW 999999.9     // what to report for raw value of 0xFFFF

#define I2C_DEVICE "/dev/i2c-1"   // raspberry pi default


int dev_addr=BH1750_ADDR;

int verbose=0;
int lowlight=0;
int overflowtest=0;
char i2cdevname[256];

void bh_setaddr(int addr){
  if(addr)dev_addr=BH1750_ADDR2;else dev_addr=BH1750_ADDR;
}


void bh_i2cwrite(int file,int val){
  char buf[1];
  buf[0]=val;
  if(verbose)printf("raw write: %02x\n",buf[0]);
  if (write(file,buf,1) != 1) {
    /* ERROR HANDLING: i2c transaction failed */
    perror("Failed to write to the i2c bus");
    fprintf(stderr,"Cannot talk to addr 0x%02x on bus '%s'.\n",dev_addr,i2cdevname);
    exit(1);
  }
}

float readlux_time(int file,int meastime){
  char buf[4]; // we need just 2 but overallocating is a good practice for one-off errors
  int lux_raw;
  float lux;

  if(verbose)fprintf(stderr,"Reading light with meastime=%d\n",meastime);

  bh_i2cwrite(file,BH1750_CHANGE_INT_TIME_H_BIT + (meastime >> 5) );
  bh_i2cwrite(file,BH1750_CHANGE_INT_TIME_L_BIT + (meastime & 0x1F));

  bh_i2cwrite(file,BH1750_ONE_TIME_H_RES_MODE2);
  usleep(600000);

  buf[0]=0xff;buf[1]=0xfe; // make sure we're reading
  if (read(file,buf,2) != 2) {
    /* ERROR HANDLING: i2c transaction failed */
    perror("Failed to read from the i2c bus");
    fprintf(stderr,"Cannot talk to addr 0x%02x on bus '%s'.\n",dev_addr,i2cdevname);
    exit(1);
  }

  if(verbose)fprintf(stderr,"raw read: %02x %02x\n",buf[0],buf[1]);

  lux_raw=(buf[0]<<8) + buf[1];
  if(overflowtest){
    if(verbose)fprintf(stderr,"overflow test: raw value=%d (%04x), forcing raw value to 0xFFFF\n",lux_raw,lux_raw);
    lux_raw=0xFFFF;
  }
  lux=lux_raw/2.4; // mode 2; use 1.2 for mode 1
  lux=lux*69/meastime;
//1.2*(69/meastime)/2;
//  lux=lux_raw/1.2*(69/meastime)/2;
  if(lux_raw>65530)return -1.0; // overflow
  if(verbose)fprintf(stderr,"raw: %d   lux: %.2f\n",lux_raw,lux);

  return lux;
}


float readlux(){
  int file;
  float lux;

  if(verbose)fprintf(stderr,"accessing device 0x%02x on %s\n",dev_addr,i2cdevname);

  if ((file = open(i2cdevname, O_RDWR)) < 0) {
    /* ERROR HANDLING: you can check errno to see what went wrong */
    perror("Failed to open the i2c bus");
    fprintf(stderr,"bus '%s'\n",i2cdevname);
    exit(1);
  }

  if (ioctl(file, I2C_SLAVE, dev_addr) < 0) {
    /* ERROR HANDLING; you can check errno to see what went wrong */
    perror("IOCTL error on i2c bus");
    fprintf(stderr,"Failed to acquire bus access and/or talk to slave at 0x%02x.\n",dev_addr);
    exit(1);
  }

  lux=readlux_time(file,BH1750_RES_MAX);

  // iterate through resolutions, if overflow happens
  if(!lowlight){
    if(lux<0)lux=readlux_time(file,BH1750_RES_DEFAULT);
    if(lux<0)lux=readlux_time(file,BH1750_RES_LOW);
    if(lux<0)lux=readlux_time(file,BH1750_RES_MIN);
  }
  else if(verbose)if(lux<0)fprintf(stderr,"lowlight mode, skipping brighter measurements\n");

  if(lux<0)lux=LUX_OVERFLOW; // OVERFLOW

  close(file);

  return lux;
}


void help(){
  printf("BH1750 sensor read\n");
  printf("Usage: bh1750 [-l] [-a0] [-a1] [-d <dev>] [-O] [-v] \n");
  printf("measure:\n");
  printf("  -l        low light mode (just overflow to 999999 if over 7417 lx)\n");
  printf("  -i        integer mode (no float)\n");
  printf("  -i10      integer mode (no float), 10 times higher value\n");
  printf("I2C:\n");
  printf("  -a0       address ADDR=L (0x%02x, default)\n",BH1750_ADDR);
  printf("  -a1       address ADDR=H (0x%02x)\n",BH1750_ADDR2);
  printf("  -d <dev>  specify I2C device, default %s\n",I2C_DEVICE);
  printf("general:\n");
  printf("  -O        force light register overflow (for test, use with -v)\n");
  printf("  -v        verbose mode\n");
  printf("  -h,--help this help\n");
  printf("\n");
}


int main(int argc,char*argv[]){
  float lux;
  int t;
  int isint=0;

  strcpy(i2cdevname,I2C_DEVICE);

  for(t=1;t<argc;t++){
    // meas
    if(!strcmp(argv[t],"-l")){lowlight=1;continue;}
    if(!strcmp(argv[t],"-i")){isint=1;continue;}
    if(!strcmp(argv[t],"-i10")){isint=2;continue;}
    // i2c
    if(!strcmp(argv[t],"-a1")){bh_setaddr(1);continue;}
    if(!strcmp(argv[t],"-a0")){bh_setaddr(0);continue;}
    if(!strcmp(argv[t],"-d")){if(t+1<argc)strncpy(i2cdevname,argv[t+1],255);i2cdevname[255]=0;t++;continue;}
    // gen
    if(!strcmp(argv[t],"-O")){overflowtest=1;continue;}
    if(!strcmp(argv[t],"-v")){verbose=1;continue;}
    if(!strcmp(argv[t],"-h")){help();exit(0);}
    if(!strcmp(argv[t],"--help")){help();exit(0);}
    // err
    fprintf(stderr,"ERR: unknown parameter: %s\n",argv[t]);
    help();
    exit(1);
  }

  lux=readlux();
  switch(isint){
    case 2: printf("%d\n",(int)10*(lux+0.49));
    case 1: printf("%d\n",(int)(lux+0.49));
    default:printf("%.1f\n",lux);
  }
}



