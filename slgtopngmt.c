/*
 * copyright 2009 Rafael Richard
 * 
 * Slug to PNG (multithreaded)
 * Converts Lowerance SLG file echogram data to PNG files based on input parameters.
 * Uses libpng14 
 *  
 * This software may be freely redistributed under the terms
 * of the GPL3 license.
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <math.h>

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PNG_DEBUG 3
#include "/usr/local/include/libpng14/png.h"
#include "palettedata.h"

#define packed_data __attribute__((__packed__))

//#define DISPLAY_TESTDATA
#define VERBOSE 1
#define OUTPUT_SLG_DATA_STDIO 0
#define PROCESS_SLG_DATA 0

/* sonar defaults */
#define FILE_HEADER_SIZE 8
#define SONAR_SIZE 2610
#define PAGE_SIZE (SONAR_SIZE)
#define PAGE_HEADER_SIZE 50
#define ECHO_GRAM_SIZE 2560

/* structures */
typedef struct {
  double lat;
  double lon;
} latlon;

typedef struct {
  unsigned char red;
  unsigned char blue;
  unsigned char green;
  
} rgbcolor;

typedef struct {
  int width;
  int height;
  png_byte color_type;
  png_byte bit_depth;
  png_bytep *row_pointers;

} img_data_info;

/* SLG Data structures */
typedef struct {
     int sonar_page_count;
     int sonar_size;
} generic_sonar_info;

typedef struct {
     int page_size;
     char bytedata[4];
} file_header;

typedef struct {
     int flags;
     float depth_limit_bottom;
     float depth_hard;
     float tempr;
     unsigned long position_latitude;
     unsigned long position_longitude;
     char raw[26];
} page_data;  

typedef struct {
     int flags;
     float depth_limit_bottom;
     float depth_hard;
     float temprf;
     float temprc;
     double lat;
     double lon;
     int ordinal;
} processed_page_data;

typedef struct {
     char bytedata[ECHO_GRAM_SIZE];
} echogram_data;

typedef struct {
     page_data page_header;
     echogram_data echo_data;
} sonar_page;

typedef struct {
     char buff[SONAR_SIZE];
} raw_sonar_page;  

/* Thread Specific Data struct*/
typedef struct {
     int total_pages_to_process;      // total pages to process
     int sonar_page_count;            // total page count in file 
     int sonar_page_offset;           // page offset into file to start processing  
     int sonar_data_offset;           // offset into file in bytes
     int sonar_size;                  // page sonar size
     int sonar_offset;                // 2800*$sonar_size;
     int total_pages_processed;
     int verbose;
     int thread;
     int temprscan;
     float mintempr;
     float maxtempr;
     processed_page_data *page_data;
     FILE* slgfile;
     char* inputfile;
     char* file_prepend;
} thread_section_data;

// thread mutex
pthread_mutex_t td_mutex = PTHREAD_MUTEX_INITIALIZER;


void *section_process_thread(void*);
void write_png_file(char* file_name, void *data, img_data_info img_data);
void abort_(const char * s, ...);
double latconvert( long lat_in);
double lonconvert( long lon_in);


int main(int argc, char **argv){
     
     int i, j, k, x, y;
     /* process command line options */
     int clOffset = 0;
     int clVerbose = 0;
     int clTemprscan = 0;
     int clPageCount = 5000;
     int clMaxImgPages = 500;
     int clOutputDataFile = 0;
     char clOutputDataFilename[] = "dataout.out"; 
     char clSLGInputFilename[] = "lg.slg";
     /* command line slg filename */
     char *filename = clSLGInputFilename;
     /* CSV data file */
     char *dataoutfile = clOutputDataFilename;
     /* file prepend string init to empty */
     char fileprepend[64]={0};
  
     //printf("%d", sizeof(word));
  
     for(i=0;i<argc;++i){
  
          if(strstr(argv[i], "-h")){
             printf("-h                        Help\n");
             printf("-v                        Verbose\n");
             printf("-t [pages]                Total echogram pages to process\n");
             printf("-s [offset]               Start offest into SLG file\n");
             printf("-d [filename]             Create Data CSV file\n");
             printf("-f [filename]             SLG filename to process\n");
             printf("-x [pages]                Multiple PNG output files\n");
             printf("-p [fileprepend]          Prepend to output image files\n");
             printf("\n\n");
             exit(0);
          }
  
          /* -v verbose   */
          if(strstr(argv[i], "-v")){
          clVerbose = VERBOSE;
          }
  
          /* -t total pages to process */
          if(strstr(argv[i], "-t")){
               if(argv[i+1]!=NULL){
               printf("\n-cnt ");
               printf("%s\n", argv[i+1]);
               clPageCount = atoi(argv[i+1]);
               }
          }
  
          /* -s page start offset into SLG file to start */
          if(strstr(argv[i], "-s")){
             if(argv[i+1]!=NULL){
               printf("\n-ps ");
               printf("%s \n", argv[i+1]);
               clOffset = atoi(argv[i+1]);
             }
          }
    
          /* -d ouput data as CSV file */
          if(strstr(argv[i], "-d")){
               if(argv[i+1]!=NULL){
                    printf("\n-d ");
                    printf("%s \n", argv[i+1]);
                    dataoutfile = argv[i+1];
                    clOutputDataFile = 1;
               }
          }
    
          /* -f SLG File Name */
          if(strstr(argv[i], "-f")){
               if(argv[i+1]!=NULL){
                    printf("\n-f ");
                    printf("%s \n", argv[i+1]);
                    filename = argv[i+1];
               }
          }
    
          /* -x Max Echogram Pages per Image */
          if(strstr(argv[i], "-x")){
               if(argv[i+1]!=NULL){
                    printf("\n-m ");
                    printf("%s \n", argv[i+1]);
                    clMaxImgPages = atoi(argv[i+1]);
               }
          }
    
          /* -p filename prepend */
          if(strstr(argv[i], "-p")){
               if(argv[i+1]!=NULL&&strlen(argv[i+1])<64){
                    printf("\n-p ");
                    printf("%s \n", argv[i+1]);
                    strcpy(fileprepend, argv[i+1]);
                    //fileprepend = argv[i+1];
               }
          }
    
     } /* for */
 
     /* AtoI Test*/
     /*
     char *s = "-100";
     int a;
     a^=a; 
     while(*s) a=(a<<3)+(a<<1)+*s++-'0'; 

     printf("%d\n", a); 

     exit(0);
     */

     /* FILES */
     FILE *fpOutfile;   // Datafile output
     FILE *fp;          // SLG Fiel
     // open CSV data file 
     if(clOutputDataFile){
          fpOutfile = fopen(dataoutfile, "w");
          if (!fpOutfile)
               abort_("Data CSV File %s could not be opened for reading", dataoutfile);
     }
  
     /* open slg file */
     fp = fopen(filename, "rb");
     if (!fp)
		abort_("SLG File %s could not be opened for reading", filename);
	 
	/* echo gram page stats & settings */
     int total_pages_to_process = clPageCount; // total pages to process
     int sonar_page_count = 0;                 // total page count in file 
     int sonar_page_offset = clOffset;         // page offset into file to start processing  
     int sonar_data_offset = 0;                // offset into file in bytes
     int sonar_size = SONAR_SIZE;              // page sonar size
     int sonar_offset = 0;                     // 2800*$sonar_size;
     int total_pages_processed;
	int total_bytes;
	int temprscan=1;                          // clTemprscan; 
	
	float maxtemp = 0;
     float mintemp = 0;
  
     /* do scan for temp data */
     void* pg_ptr;
     page_data* pd_ptr;
     int total_pages_read = 0;
     processed_page_data* page_data_store_ptr;  
  
     if(temprscan){
          pg_ptr = malloc(sizeof(page_data));
          pd_ptr = (page_data*)pg_ptr;

          page_data_store_ptr =  malloc(sizeof(processed_page_data)*total_pages_to_process);
    
          if(pg_ptr&&page_data_store_ptr){

               float temprC = 0;
               float temprF = 0;
               double lat, lon;
               int theFlags;

               float tempinit = 0;

               fseek(fp, sonar_data_offset+(sizeof(file_header))+sizeof(page_data), SEEK_SET);
    
               for(i=0;i<total_pages_to_process;++i){

                    fseek(fp, (sonar_size)-sizeof(page_data), SEEK_CUR);
                    total_pages_read = fread(pg_ptr, sizeof(page_data), 1, fp);

                    theFlags = (pd_ptr->flags)>>16;

                    if(theFlags==0x2c11||theFlags==0x6d14){
                    temprC = pd_ptr->tempr;
                    temprF = (1.8*pd_ptr->tempr)+32;
                    if(tempinit){
                         if(temprF>maxtemp)maxtemp=temprF;
                         if(temprF<mintemp)mintemp=temprF;
                    }else
                    {
                         maxtemp=temprF;
                         mintemp=temprF;
                         tempinit = 1;
                    }
                    }else
                    {
                         temprC = -100;
                         temprF = -100;
                    }

                    /* GPS data present */
                    if(theFlags==0x6d14){
                     
                         lat = latconvert(pd_ptr->position_latitude);
                         lon = lonconvert(pd_ptr->position_longitude);
                     
                    }else
                    {
                         lat = 0;
                         lon = 0;
                    }

                    if(page_data_store_ptr){
                         page_data_store_ptr[i].ordinal = i;
                         page_data_store_ptr[i].flags = theFlags;
                         page_data_store_ptr[i].temprf = temprF;
                         page_data_store_ptr[i].temprc = temprC;
                         page_data_store_ptr[i].lat = lat;
                         page_data_store_ptr[i].lon = lon;
                         page_data_store_ptr[i].depth_hard = pd_ptr->depth_hard;
                         page_data_store_ptr[i].depth_limit_bottom =  pd_ptr->depth_limit_bottom;
                    }

                    /*
                    printf("%#010x, %x, %f, %f, %f, %f, %f\n", (i+sonar_page_offset), theFlags,  pd_ptr->depth_limit_bottom,
                                pd_ptr->depth_hard, temprF, lat, lon);    
                    */

               }
      
      /*  Over Process */
#ifdef DISPLAY_TESTDATA       
               for(i=0;i<total_pages_to_process;++i){
                    if(i<50)
                         printf("%d, %#010x, %x, %f, %07.2f, %f, %f, %f, %f\n",
                           page_data_store_ptr[i].ordinal,
                           i,
                           page_data_store_ptr[i].flags ,
                           page_data_store_ptr[i].temprf ,
                           page_data_store_ptr[i].temprc ,
                           page_data_store_ptr[i].lat,
                           page_data_store_ptr[i].lon,
                           page_data_store_ptr[i].depth_hard ,
                           page_data_store_ptr[i].depth_limit_bottom 
                           );
               }
      
               printf("Max %f, %f Min", maxtemp, mintemp);  
#endif      
    
          //free(page_data_store_ptr);
               free(pg_ptr);
          }
    
     }
  
     //fclose(fp);
     //exit(0);
     int sonar_structure_size = sizeof(raw_sonar_page);
     if(clVerbose)printf("\nSonar Page Size: %i\n", sonar_structure_size);
  
     /* sonar data stuctures */
     file_header fileheader;
  
     /* get SLG file information */
     struct stat fileinfo;
     int data_file_size;

     if(0!=stat(filename, &fileinfo)){
          if(clVerbose)printf("\nCould not determine file size.\n\n");
          abort_("Could not determine file size of SLG File %s", filename);
     }
    
     data_file_size = fileinfo.st_size;
  
     if(clVerbose)printf("Start of processing for : %s        size: %d\n", filename, (int)fileinfo.st_size); 
    
     /* calc file sonar page count  */
     if(data_file_size>(sonar_size*2)){
          sonar_page_count = (data_file_size / sonar_size)-1;
          if(clVerbose)printf("Sonar Pages: %d\n", sonar_page_count);
     }else
     {
          abort_("Insufficient Sonar Data in SLG file");
     }

     if(clVerbose)printf("\nProcessing %d pages\n", total_pages_to_process);
  
     /* calc page and data offset into sonar file */
     int total_data_size = total_pages_to_process * sonar_size;
     sonar_data_offset = sonar_page_offset * SONAR_SIZE;
     
     if(total_data_size>data_file_size)
          abort_("Sonar data offset past end of data file.");
     
     if(sonar_page_offset>=(sonar_page_count-10))
          abort_("Sonar Page offset past end of total sonar page.");
     
     if((sonar_page_offset+total_pages_to_process)>sonar_page_count)
          total_pages_to_process = sonar_page_count - sonar_page_offset;

     /* Read SLG file header */
     fseek(fp, sonar_data_offset+8, SEEK_SET);
     total_bytes = fread(&fileheader, sizeof(fileheader), 1, fp);
  
     /* Output header bytes   */
     total_bytes = sizeof(fileheader);
     unsigned char *pFileHeader = (unsigned char*) &fileheader;
     if(clVerbose){
          printf("SLG File Header\n");
          for(i=0;i<total_bytes;i++){
               printf("%02x", *pFileHeader++);
               printf(" ");
          }
     }
 
     /* Create Thread Specific structures */
     #define THREADS 4
     thread_section_data thread_data[THREADS];
     int thread_cnt = THREADS;
     int threads_started=0;
     int threadret[THREADS];
     pthread_t threads[THREADS];
     thread_section_data* ptr_tdata;
     int pages_per_thread;
  
     /* reduce data for threads */
     int total_imgs;
     int thread_rounds;
     if(clMaxImgPages>0){
          /* divide by max img size specified */
          total_imgs = total_pages_to_process/clMaxImgPages;
          thread_rounds = total_imgs/thread_cnt;
          pages_per_thread = clMaxImgPages;
          /* check if we have enough thread rounds for all data  */
          if(total_imgs%thread_cnt!=0)thread_rounds++;

     }else
     {   /* divide among max threads */
          pages_per_thread = total_pages_to_process/thread_cnt;
          total_imgs=thread_cnt;
     }
     
     int thread_compensator = 0;
     int cnt_img=0;
     int page_offset = clOffset;
     
     /* Thread Launcher */
     for(j=0;j<total_imgs;){
      
          /* compensate for need for fewer threads during this thread round */
          if(j+thread_cnt>total_imgs){
               thread_compensator = thread_cnt-((j+thread_cnt)-total_imgs);
               page_offset=clOffset+(pages_per_thread*j);
               //thread_compensator = 0;
          }else
          {
               thread_compensator = thread_cnt;
          }
          /* create and launch threads with the thread's assigned data */
          threads_started=0;
          for(i=0;i<thread_compensator;++i){

               ptr_tdata = &thread_data[i];
               ptr_tdata->total_pages_to_process = pages_per_thread;  // total pages to process
               ptr_tdata->sonar_page_count = sonar_page_count;   // total page count in file 
               ptr_tdata->sonar_page_offset = page_offset+(pages_per_thread*i); // page offset into file to start processing  
               ptr_tdata->sonar_data_offset = sonar_data_offset+(ptr_tdata->sonar_page_offset*sonar_size);    // offset into file in bytes
               ptr_tdata->sonar_size = sonar_size;     // page sonar size
               ptr_tdata->sonar_offset = sonar_offset; // 2800*$sonar_size;
               ptr_tdata->total_pages_processed = total_pages_processed;   
               ptr_tdata->verbose = clVerbose;
               ptr_tdata->thread = cnt_img++;
               ptr_tdata->temprscan = temprscan;
               ptr_tdata->maxtempr = maxtemp;
               ptr_tdata->mintempr = mintemp;
               ptr_tdata->page_data = page_data_store_ptr;
               ptr_tdata->slgfile = fp;
               ptr_tdata->inputfile = filename;
               ptr_tdata->file_prepend = fileprepend;
               threadret[i] = pthread_create(&threads[i], NULL, section_process_thread, ptr_tdata);
               if(threadret[i]==0)threads_started++;

               #ifdef DISPLAY_TESTDATA          
               printf("t %d\n",i);
               printf("sp cnt %d\n",ptr_tdata->sonar_page_count);
               printf("sp offset %d\n", ptr_tdata->sonar_page_offset);
               printf("sp data offset %d\n", ptr_tdata->sonar_data_offset);
               #endif
          }
       
          /* wait for threads */
          for(i=0;i<threads_started;++i){
               pthread_join(threads[i], NULL);
          }
      
          j+=thread_cnt;
          page_offset=pages_per_thread*j;
      
     }
  
     if(page_data_store_ptr)free(page_data_store_ptr);
     fclose(fp);
     if(clOutputDataFile)fclose(fpOutfile);
  
     return 0;
} /* main */

void* section_process_thread( void* ptr_data){

     int i, j, k, x, y;
     char filename[64] = {0};
     char stemp[128] = {0};
     FILE* fp;
     FILE* fpOutfile;
     int clOutputDataFile = 0;
     thread_section_data* td = (thread_section_data*) ptr_data;
  
     /* echo gram page stats & settings */
     int total_pages_to_process = td->total_pages_to_process;      // total pages to process
     int sonar_page_count = td->sonar_page_count;                  // total page count in file 
     int sonar_page_offset = td->sonar_page_offset;                // page offset into file to start processing  
     int sonar_data_offset = td->sonar_data_offset;                // offset into file in bytes
     int sonar_size = td->sonar_size;                              // page sonar size
     int sonar_offset = td->sonar_offset;                          // 2800*$sonar_size;
     int clVerbose = td->verbose;
     int temprscan = td->temprscan;

     float maxtemp=td->maxtempr;
     float mintemp=td->mintempr;
     float temprange = maxtemp-mintemp;
     float tempfactor=1;
     float lastgoodtemp;
  
     processed_page_data* page_data_store_ptr = td->page_data;
     fp = td->slgfile;
     page_data_store_ptr = page_data_store_ptr + sonar_page_offset;

     /* test data output */
#ifdef DISPLAY_TESTDATA
     if(0)
     //if(td->thread==0)
     {
          for(i=0;i<total_pages_to_process;++i){
               printf("%d, %d, %d, %x, %2.2f, %07.2f, %f, %f, %f, %f\n",
                     page_data_store_ptr[i].ordinal,
                     (i+sonar_page_offset),
                     i,
                     page_data_store_ptr[i].flags ,
                     page_data_store_ptr[i].temprf ,
                     page_data_store_ptr[i].temprc ,
                     page_data_store_ptr[i].lat,
                     page_data_store_ptr[i].lon,
                     page_data_store_ptr[i].depth_hard ,
                     page_data_store_ptr[i].depth_limit_bottom 
                     );

          }
     }
#endif  

     //fp = fopen(td->inputfile, "rb");

     int total_pages_processed;
     int total_pages_read=0;
     int total_bytes;
     /* image settings */
     int brightness_compensation = -245;
     int reduction_factor = 2;
     float reduction_factors[]={20.0, 16.0, 8.0, 5.7, 4.0, 4.15, 3.15, (16/7), 2.0, 2.0};

     /* add size of SLG File header for offset position */
     sonar_data_offset += sizeof(file_header);

     /* allocate mem for raw sonar page data */
     void* pSonarInput = malloc(SONAR_SIZE * (total_pages_to_process+1));
     if(!pSonarInput)
          abort_("Failed to allocate memory for page data.");
     
     /* allocate mem for echogream image data */
     void *pNewEchoData = malloc(((ECHO_GRAM_SIZE/reduction_factor)*sizeof(rgbcolor))*total_pages_to_process);
     if(!pNewEchoData)
          abort_("Failed to allocate memory for echo data.");

     /* allocate mem for img row ptrs */
     png_bytep *pImg_row_ptrs = malloc(sizeof(png_bytep) * ECHO_GRAM_SIZE);
     if(!pImg_row_ptrs)
          abort_("Failed to allocate memory for img row pointers.");
  
     /* Setup Array for image rows */
     for(i=0;i<(ECHO_GRAM_SIZE/reduction_factor);i++){
          pImg_row_ptrs[i] = pNewEchoData+(total_pages_to_process*sizeof(rgbcolor)*i);
     }

     memset(pNewEchoData, 200, total_pages_to_process*(ECHO_GRAM_SIZE/reduction_factor)*sizeof(rgbcolor));

     /* Calcualate total_pages_to_process */
     total_pages_read=0;

     /* LOCK */
     // pthread_mutex_lock(&td_mutex); 

     /* seek offset into SLG file page start */
     fseek(fp, sonar_data_offset, SEEK_SET);
     total_pages_read = fread(pSonarInput, SONAR_SIZE, total_pages_to_process, fp);

     // pthread_mutex_unlock(&td_mutex);

     if(!total_pages_read){
          printf("Error reading file - %i of %i total sonar pages read.", i, total_pages_to_process);
          abort_("Error reading file");
     }
     if(clVerbose)
          printf("%i Total Bytes\n",(total_pages_read*SONAR_SIZE));

     latlon latlonConv;
     raw_sonar_page* pPageRaw = (raw_sonar_page*) pSonarInput;
     page_data* pPage;
  
     /* depth break */ 
     int factor_offset;

     /* Calculate color pallete for Tempr */
     rgbcolor palette[512];
     int palette_num = 0;
     create_palette(palette, 255);

     /* temp precalc */
     float palhold, palhold1, palhold2, palhold3;
     tempfactor = (maxtemp-maxtemp)/255;
     /* seek first valid temp */
     for(i=0;i<total_pages_to_process;++i){
          if(page_data_store_ptr[i].temprf>0){
               palhold1 = page_data_store_ptr[i].temprf-mintemp;
               break;
          }
     }
     
     /* process page loop */
     for(i=0;i<total_pages_to_process;i++){
          pPage = (page_data*)pPageRaw;

          /* 2c11 and 6d14 temp   6d14 latlon */
          int theFlags = (pPage->flags)>>16;
          float temprC = 0;
          float temprF = 0;
          double lat, lon;

          if(PROCESS_SLG_DATA){
          
               if(theFlags==0x2c11||theFlags==0x6d14){
                    temprC = pPage->tempr;
                    temprF = (1.8*pPage->tempr)+32;
               }else
               {
                    temprC = -100;
                    temprF = -100;
               }
          
               /* Check if GPS data present */
               if(theFlags==0x6d14){
                    lat = latconvert(pPage->position_latitude);
                    lon = lonconvert(pPage->position_longitude);
               }
          
               /* Output SLG Data */
               if(OUTPUT_SLG_DATA_STDIO){
                    printf("\nPAGE  %i",i);
                    printf("\nFlags: %x",theFlags);
                    printf("\nDepth Limit: %f",pPage->depth_limit_bottom);
                    printf("\nDepth: %f",pPage->depth_hard);
                    printf("\nTemp: %fC   %fF",temprC, temprF);
                    printf("\nLon: %f",lon);
                    printf("\nLat: %f",lat);
                    printf("\n%i\n", sizeof(page_data));
                    printf("\n%i\n", sizeof(float));
               }

               /* Write SLG data to CSV file */
               if(clOutputDataFile){
                    fprintf(fpOutfile,"%#010x, %x, %f, %f, %f, %f, %f\n", i, theFlags,  pPage->depth_limit_bottom,
                           pPage->depth_hard, temprF, lat, lon);

                    //fprintf(fpOutfile,"%#010x, %x, %#010x, %#010x, %#010x, %#010x, %#010x\n", i, theFlags, 
                    //          (unsigned int) pPage->depth_limit_bottom,
                    //          (unsigned int) pPage->depth_hard, (unsigned int) temprF, (unsigned int) lat, (unsigned int) lon);
               }
          }


          float dbreak = pPage->depth_limit_bottom;

          /* determine depth break factor */
          if(dbreak<10)factor_offset = 0;
          if(dbreak<20&&dbreak>=10)factor_offset = 1;
          if(dbreak<30&&dbreak>=20)factor_offset = 2;
          if(dbreak<40&&dbreak>=30)factor_offset = 3;
          if(dbreak<50&&dbreak>=40)factor_offset = 4;
          if(dbreak<60&&dbreak>=50)factor_offset = 5;
          if(dbreak<70&&dbreak>=60)factor_offset = 6;
          if(dbreak<80&&dbreak>=70)factor_offset = 7;
          if(dbreak<90&&dbreak>=80)factor_offset = 8;
          if(dbreak>=90)factor_offset = 9;

          /* setup for writing image to memory */
          rgbcolor pixel;
          unsigned int ImgDataOffset=0;
          png_bytep *pRow = pImg_row_ptrs;
      
          rgbcolor *pImgdata = (rgbcolor*) pNewEchoData;
          pImgdata+=i;
          
          char *pEchoData = (char*) pPageRaw;
          pEchoData += offsetof(sonar_page, echo_data);

          /* calc palette value
           *  calc color pos in palette
          */
          if(page_data_store_ptr[i].temprf>0){
               palhold1 = page_data_store_ptr[i].temprf-mintemp;
          }
      
          palhold2 = palhold1/temprange;
          palhold3 = 255*palhold2;
      
#ifdef DISPLAY_TESTDATA      
          if(td->thread==0)
               printf("%d %f %f %d\n", i, palhold1, palhold2, (int)palhold3); 
#endif      

          /* compensate start of echo gram for differing pageheader sizes */
          if(theFlags==0x6d14||theFlags==0x6d04){
               pEchoData+=20;
          }
          /* mov echo gram data */
          float factor_apply = 1;
          
          /* apply single factor or from table */
          if(1){
               /* apply from factor array */
               factor_apply = reduction_factors[factor_offset];
          }else
          {    /* apply from using single factor */
               factor_apply = reduction_factor;
          }
      
          for(j=0;j<(ECHO_GRAM_SIZE/factor_apply);j++){   
               /* read pixel value from echo gram */
               unsigned char coloravg;
               unsigned char echopixel = *pEchoData; 

               /* Calculate reduction by averaging pixels */
               /*
               for(k=-1;k<reduction_factor-1;k++){
                    coloravg+= *(pEchoData+k);
               }

               /* Calculate brightness and contrast */
               /* 
               (1-brightnessPercent/100.)*(maxPixelValue - minPixelValue) + minPixelValue
               (1-contrastPercent/100.)*(maxPixelValue - minPixelValue)
               coloravg/=reduction_factor;
               */
               
               unsigned char color = abs(echopixel+brightness_compensation);
               pixel.red = color;
               pixel.green = color;
               pixel.blue = color;
          
               /* apply temp color to bottom of image */
               if(j>(ECHO_GRAM_SIZE/factor_apply)-30)
               {              
                    pixel.red = palette[(int)palhold3].red;
                    pixel.green = palette[(int)palhold3].green;
                    pixel.blue = palette[(int)palhold3].blue;
               }

               /* write pixel of echo gram data to img */
               *pImgdata = pixel; 
               pImgdata+= total_pages_to_process; // next line  (column)     
               pEchoData+=(int)factor_apply;
          }

          palette_num++;
          if(palette_num>255)palette_num=0;

          pPageRaw++;

     } /* process page loop */

     /* Completion Stats  */
     total_pages_processed = i;
     if(clVerbose){
          printf("%d Total Pages Processed\n",i);
     }

     /* Raw Image data for PNG write function */
     img_data_info img_data;
     img_data.width = total_pages_processed;
     img_data.height = (ECHO_GRAM_SIZE/reduction_factor);
     img_data.color_type = PNG_COLOR_TYPE_RGB;
     img_data.bit_depth = 8;
     img_data.row_pointers = pImg_row_ptrs;
  
     /* write file */
     sprintf(stemp, "_%d.png", td->thread);
     strcpy(filename, td->file_prepend);
     strcat(filename, "_output");
     strcat(filename, stemp);
  
#ifdef DISPLAY_TESTDATA   
     printf("\n-> %s\n", filename);
#endif

     //pthread_mutex_lock(&td_mutex);
     write_png_file(filename, pNewEchoData, img_data);
     //pthread_mutex_unlock(&td_mutex);

     // Clean up    
     free(pSonarInput);
     free(pNewEchoData);
     free(pImg_row_ptrs);
}

void abort_(const char * s, ...){
     
	va_list args;
	va_start(args, s);
	vfprintf(stderr, s, args);
	fprintf(stderr, "\n");
	va_end(args);
	abort();
}

void write_png_file(char* file_name, void *data, img_data_info img_data){
     
     int x, y;
     int width, height;
     png_byte color_type;
     png_byte bit_depth;
     png_structp png_ptr;
     png_infop info_ptr;
     int number_of_passes;
     png_bytep * row_pointers;
	  
     /* create file */
	FILE *fp=fopen(file_name, "wb");
	if(!fp)
		abort_("[write_png_file] File %s could not be opened for writing", file_name);

	/* initialize stuff */
	png_ptr=png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	
	if(!png_ptr)
		abort_("Failed png_create_write_struct");

	info_ptr=png_create_info_struct(png_ptr);
	if(!info_ptr)
		abort_("Failed png_create_info_struct failed");

	if(setjmp(png_jmpbuf(png_ptr)))
		abort_("Failed Error during init_io");

	png_init_io(png_ptr, fp);

	/* write png header */
	if(setjmp(png_jmpbuf(png_ptr)))
		abort_("[write_png_file] Error during writing header");

     /* set data info */
     width = img_data.width;
	height = img_data.height;
	color_type = img_data.color_type;
	bit_depth = img_data.bit_depth;
     row_pointers = (png_bytep*) img_data.row_pointers;

	png_set_IHDR(png_ptr, info_ptr, width, height,
		     bit_depth, color_type, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	/* write bytes */
	if(setjmp(png_jmpbuf(png_ptr)))
		abort_("Error during writing info structure");

	png_write_image(png_ptr, row_pointers);

	if(setjmp(png_jmpbuf(png_ptr)))
		abort_("Error during write png");

	png_write_end(png_ptr, NULL);

     fclose(fp);
}

int create_palette(rgbcolor palette[], int palette_colors){
  
     int i, j, k, l;

     char rval = 255;
     char gval = 255;
     char bval = 0;
     rgbcolor palhold[255];

     /* create warm tempr colors */
     j = 96;
     for(i=0;i<127;i++){
          j+=1;
          if(j>192)j+=1;
          palhold[i].red=rval;  //  Red
          palhold[i].green=bval;     // Green
          palhold[i].blue=j;  // Blue
     }
  
     /* create cool tempr coolers */
     j=255;
     k=255;
     l=0;
     for(i=127;i<255;i++){
          j-=2;
          l+=2;

          if(l>255)l=255;
          if(l<0)l=0;

          if(j>255)l=255;
          if(j<0)l=0;

          if(k>255)l=255;
          if(k<0)l=0;

          palhold[i].red=j;  //  Red
          palhold[i].green=l;     // Green
          palhold[i].blue=k;  // Blue
     }
  
  
     /* reverse */
     int c1=255;
     for(i=0;i<255;i++){   
          palette[i].red=palhold[c1].red;  //  Red
          palette[i].green=palhold[c1].green;     // Green
          palette[i].blue=palhold[c1].blue;  // Blue
          c1--;
     }
     /*
     for(i=0;i<255;i++)
     {
          palette[i].red = color_grad[i*3];
          palette[i].blue = color_grad[(i*3)+1];
          palette[i].green = color_grad[(i*3)+2];
     }
     */
}

double latconvert( long lat_in){
    
     double lat = 0;
     double pi = 3.1415926535898;
     lat =  180/pi*(2*atan(exp((lat_in)/6356752.3142))-pi/2);
        
     return lat;
}

double lonconvert( long lon_in){
     double lon = 0;
     double pi = 3.1415926535898;
     lon =  (180.0/pi);
     lon = lon *(lon_in);
     lon = lon/6356752.3142;
         
     return lon;
}


