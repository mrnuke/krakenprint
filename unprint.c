/* $Id$ */
/*
 * Attempt to simulate a printer to facilitate driver testing.  Is this
 * useful?
 *
 * Copyright 2000 Eric Sharkey <sharkey@superk.physics.sunysb.edu>
 *
 * GPL yada yada yada
 */

#include<stdio.h>
#include<stdlib.h>

typedef struct {
  unsigned char unidirectional;
  unsigned char microweave;
  int page_management_units; /* dpi */
  int relative_horizontal_units; /* dpi */
  int absolute_horizontal_units; /* dpi, assumed to be >= relative */
  int relative_vertical_units; /* dpi */
  int absolute_vertical_units; /* dpi, assumed to be >= relative */
  int top_margin; /* dots */
  int bottom_margin; /* dots */
  int page_length; /* dots */
  int dotsize;
  int current_color;
  int xposition; /* dots */
  int yposition; /* dots */
} pstate_t;

#define MAX_INKS 6
typedef struct {
   unsigned char *line[MAX_INKS];
   int startx[MAX_INKS];
   int stopx[MAX_INKS];
} line_type;

unsigned char buf[256*256];
unsigned char minibuf[256];
unsigned short bufsize;
unsigned char ch;
unsigned short sh;
int i,m,n,c;
FILE *fp_r,*fp_w;

pstate_t pstate;

line_type **page=NULL;

/* Color Codes:
   color    Epson   Sequential
   Black    0       0
   Magenta  1       1
   Cyan     2       2
   Yellow   4       3
   L.Mag.   17      4
   L.Cyan   18      5
 */
#define seqcolor(c) (((c)&3)+((c)&20)?3:0)  /* Intuitive, huh? */
#define epcolor(c)  ({0,1,2,4,17,18}[c])
  

void *mycalloc(size_t count,size_t size){
  void *p;
  if (p=calloc(count,size))
    return(p);

  fprintf(stderr,"Buy some RAM, dude!");
  exit(-1);
}

void *mymalloc(size_t size){
  void *p;
  if (p=malloc(size))
    return(p);

  fprintf(stderr,"Buy some RAM, dude!");
  exit(-1);
}

void update_page(unsigned char *buf,int bufsize,int m,int n,int color,int bpp,int density) {

  int y,skip;

  skip=pstate.relative_horizontal_units/density;

  if (skip==0) {
    fprintf(stderr,"Warning!  Attempting to print at density higher than units.\n");
  }
 
  if (!page) {
    fprintf(stderr,"Warning!  Attempting to print before setting up page!\n");
    /* Let's hope that we've at least initialized the printer with
     * with an ESC @ and allocate the default page.  Otherwise, we'll
     * have unpredictable results.  But, that's a pretty acurate statement
     * for a real printer, too!  */
    page=(line_type **)mycalloc(pstate.bottom_margin-pstate.top_margin,
                               sizeof(line_type *));
  }
  for (y=pstate.yposition;y<pstate.yposition+m;y++) {
    if (!(page[y])) {
      page[y]=(line_type *) mycalloc(sizeof(line_type),1);
    }
    if (!page[y]->line[seqcolor(color)]) {
       page[y]->line[seqcolor(color)]=(unsigned char *)
          mycalloc(sizeof(unsigned char),bufsize*skip);
       page[y]->startx[seqcolor(color)]=pstate.xposition;
       page[y]->stopx[seqcolor(color)]=pstate.xposition+n*skip;
       memcpy(page[y]->line[seqcolor(color)],buf,bufsize);
    } else {
       fprintf(stderr,"FIXME: double printing not yet supported.\n");
    }
  }
}

main(int argc,char *argv[]){

int currentcolor,currentbpp,density;

    if(argc == 1){
        fp_r = stdin;
        fp_w = stdout;
    }
    else if(argc == 2){
        fp_r = fopen(argv[1],"r");
        fp_w = stdout;
    }
    else {
        if(*argv[1] == '-'){
            fp_r = stdin;
            fp_w = fopen(argv[2],"w");
        }
        else {
            fp_r = fopen(argv[1],"r");
            fp_w = fopen(argv[2],"w");
        }
    }

#define get1(error) if (!fread(&ch,1,1,fp_r)) {fprintf(stderr,error);exit(-1);}
#define get2(error) {if(!fread(minibuf,1,2,fp_r)){\
                       fprintf(stderr,error);exit(-1);}\
                       sh=minibuf[0]+minibuf[1]*256;}
#define getn(n,error) if (!fread(buf,1,n,fp_r)){fprintf(stderr,error);exit(-1);}

    while (fread(&ch,1,1,fp_r)){
      if (ch!=0x1b) {
        fprintf(stderr,"Corrupt file?  No ESC found.\n");
        continue;
      }
      get1("Corrupt file.  No command found.\n");
      fprintf(stderr,"Got a %X.\n",ch);
      switch (ch) {
        case '@': /* initialize printer */
            pstate.unidirectional=0;
            pstate.microweave=0;
            pstate.page_management_units=360;
            pstate.relative_horizontal_units=180;
            pstate.absolute_horizontal_units=60;
            pstate.relative_vertical_units=360;
            pstate.absolute_vertical_units=360;
            pstate.top_margin=120;
            pstate.bottom_margin=
              pstate.page_length=22*360; /* 22 inches is default ??? */
            break;
        case 'U': /* turn unidirectional mode on/off */
            get1("Error reading unidirectionality.\n");
            if ((ch<=2)||((ch>=0x30)&&(ch<=0x32))) {
              pstate.unidirectional=ch;
            }
            break;
        case 'i': /* transfer raster image */
            get1("Error reading color.\n");
            currentcolor=ch;
            get1("Error reading compression mode!\n");
            c=ch;
            get1("Error reading bpp!\n");
            currentbpp=ch;
            get2("Error reading number of horizontal dots!\n");
            n=sh;
            get2("Error reading number of vertical dots!\n");
            m=sh;
            density=pstate.relative_horizontal_units;
            ch='!'; /* make sure ch!='.' and fall through */
        case '.': /* transfer raster image */
            if (ch=='.') {
              get1("Error reading compression mode!\n");
              c=ch;
              if (c>2) {
                fprintf(stderr,"Warning!  Unknown compression mode.\n");
                break;
              }
              get1("Error reading vertical density!\n");
              get1("Error reading horizontal density!\n");
              density=3600/ch;
              get1("Error reading number of vertical dots!\n");
              m=ch;
              get2("Error reading number of horizontal dots!\n");
              n=sh;
              currentbpp=1;
              currentcolor=pstate.current_color;
            }
            switch (c) {
              case 0:  /* uncompressed */
                bufsize=m*((n*currentbpp+7)/8);
                getn(bufsize,"Error reading raster data!\n");
                update_page(buf,bufsize,m,n,currentcolor,currentbpp,density);
                break;
              case 1:  /* run length encoding */
                for (i=0;i<(m*((n*currentbpp+7)/8));) {
                  get1("Error reading counter!\n");
                  if (ch<128) {
                    bufsize=ch+1;
                    getn(bufsize,"Error reading RLE raster data!\n");
                  } else {
                    bufsize=257-(int)ch;
                    get1("Error reading compressed RLE raster data!\n");
                    memset(buf,ch,bufsize);
                  }
                  i+=bufsize;
                  update_page(buf,bufsize,m,n,currentcolor,currentbpp,density);
                }
                break;
              case 2: /* TIFF compression */
                fprintf(stderr,"TIFF mode not yet supported!\n");
                /* FIXME: write TIFF stuff */
                break;
              default: /* unknown */
                fprintf(stderr,"Unknown compression mode.\n");
                break;
            }
            break;
        case 0x5C: /* set relative horizontal position */
            get2("Error reading relative horizontal position.\n");
            if (pstate.xposition+(signed short)sh<0) {
              fprintf(stderr,"Warning! Attempt to move to -X region ignored.\n");
            } else  /* FIXME: Where is the right margin??? */
              pstate.xposition+=(signed short)sh;
            break;
        case '$': /* set absolute horizontal position */
            get2("Error reading absolute horizontal position.\n");
            pstate.xposition=sh*(pstate.relative_horizontal_units/
                                pstate.absolute_horizontal_units);
            break;
        case 0x6: /* flush buffers */
            /* Woosh.  Consider them flushed. */
            break;
        case 0x19: /* control paper loading */
            get1("Error reading paper control byte.\n");
            /* paper? */
            break;
        case 'r': /* select printing color */
            get1("Error reading color.\n");
            if ((ch<=4)&&(ch!=3)) {
              pstate.current_color=ch;
            } else {
              fprintf(stderr,"Invalid color %d.\n",ch);
            }
        case '(': /* commands with a payload */
            get1("Corrupt file.  Incomplete extended command.\n");
            get2("Corrupt file.  Error reading buffer size.\n");
            bufsize=sh;
            fprintf(stderr,"Command %X bufsize %d.\n",ch,bufsize);
            getn(bufsize,"Corrupt file.  Error reading data buffer.\n");
            switch (ch) {
              case 'G': /* select graphics mode */
                /* FIXME: this is supposed to have more side effects */
                pstate.microweave=0;
                break;
              case 'U': /* set page units */
                switch (bufsize) {
                  case 1:
                    pstate.page_management_units=
                    pstate.absolute_horizontal_units=
                    pstate.relative_vertical_units=
                    pstate.absolute_vertical_units=3600/buf[0];
                    break;
                  case 5:
                    pstate.page_management_units=(buf[4]*256+buf[3])/buf[0];
                    pstate.relative_vertical_units=
                    pstate.absolute_vertical_units=(buf[4]*256+buf[3])/buf[1];
                    pstate.relative_horizontal_units=
                    pstate.absolute_horizontal_units=(buf[4]*256+buf[3])/buf[2];
                    break;
                }
                break;
              case 'i': /* set MicroWeave mode */
                if (bufsize!=1) {
                  fprintf(stferr,"Malformed microweave setting command.\n");
                } else {
                  switch (buf[0]) {
                    0x00:
                    0x30:pstate.microweave=0;
                        break;
                    0x01:
                    0x31:pstate.microweave=1;
                         break;
                    default:fprintf(stderr,"Unknown Microweave mode 0x%X.\n",
                                    buf[0]);

                  }
                }
                break;
              case 'e': /* set dot size */
                break;
              case 'c': /* set page format */
                if (page) {
                  fprintf(stderr,"Setting the page format in the middle of printing a page is not supported.\n");
                  exit(-1);
                }
                if (bufsize==4) {
                  pstate.top_margin=buf[1]*256+buf[0];
                  pstate.bottom_margin=buf[3]*256+buf[2];
                  pstate.yposition=0;
                  if (pstate.top_margin+pstate.bottom_margin>
                       pstate.page_length) {
                    pstate.page_length=pstate.top_margin+pstate.bottom_margin;
                  }
                  page=(line_type **)mycalloc(pstate.bottom_margin-
                                  pstate.top_margin,sizeof(line_type *));
                  /* FIXME: what is cut sheet paper??? */
                } else {
                  fprintf(stderr,"Malformed page format.  Ignored.\n");
                }
                break;
              case 'V': /* set absolute vertical position */
                break;
              case 'v': /* set relative vertical position */
                break;
              case 'S': /* set paper dimensions */
                break;
              case 'r': /* select color */
                break;
              case '/': /* set relative horizontal position */
                break;
              case '$': /* set absolute horizontal position */
                break;
              case 'C': /* set page length */
                break;
              default:
                fprintf(stderr,"Warning: Unknown command ESC ( 0x%X.\n",ch);
            }
            break;
          default:
            fprintf(stderr,"Warning: Unknown command ESC 0x%X.\n",ch);
      }
    }

}
