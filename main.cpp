#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mosquitto.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <getopt.h>
#include <stdio.h>
#include <regex.h>
#include <wchar.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <ctype.h>
#include <pthread.h>
#include <termios.h>
#include <locale.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "cJSON.h"

#include "ak_ao.h"
#include "ak_ai.h"
#include "ak_thread.h"
#include "ak_vi.h"
#include "1.h"
#include "ak_common.h"
#include "ak_mem.h"
#include "ak_thread.h"
#include "ak_common_graphics.h"
#include "ak_tde.h"
#include "ak_mem.h"
#include "ak_log.h"


/*
*	Freetype2
*/
FT_Face	face;


/*
*	Time 
*/
time_t timep;
struct tm *p;


/*
*	Mode 1 variable
*/
char *temperature = "0";
char *humidity    = "0";
char *smoke    	  = "0";

/*
*	Mode 2 variable
*/
char buf_mod2[25][50];
static char *city,*air,*fx,*fl,*sunrise,*sunset,*ymd,*notice,*wendu_h,*wendu_l;
static char *wendu1_h,*wendu1_l,*ymd1,*type1;
static char *wendu2_h,*wendu2_l,*ymd2,*type2;
static char *wendu3_h,*wendu3_l,*ymd3,*type3;        
static char *nowwendu,*nowtype;		
static char *flu,*sport,*uv;
static int mode2_flag;


/*
*	
*/
int sc[7] = {200,200,0,0,200,200,3};      
int tg[7] = {0,0,200,200,0,0,GP_FORMAT_RGB888};
int bg[7] = {1024,600,0,0,1024,600,3};
static struct ak_tde_layer tde_layer_screen = {0,0,0,0,0,0,0,1}; //screen


/*
*	video
*/
static int frame_num = 10;
static int channel_num = 1;
static char *isp_path = "/etc/config/isp_ar0230_dvp.conf";
static int main_res_id = 4;
static int sub_res_id = 0;
static VI_CHN_ATTR chn_attr = {0};
static VI_CHN_ATTR chn_attr_sub = {0};
static int dev_cnt = 1;

static RECTANGLE_S  res_group[RES_GROUP]=
{{640, 360},			/* 640*360 */
 {640, 480},			/*   VGA   */
 {1280,720},			/*   720P  */
 {960, 1080},			/*   1080i */
 {1920,1080},			/*   1080P */
 {2304,1296},
 {2560,1440},
 {2560,1920}
};


/*
*  audio input
*/
static FILE *fp = NULL;
static int ao_sample_rate = 16000;
static int sample_rate = 16000;
static int volume = 8;
static int save_time = 3000;        // set save time(ms)
static int channel_num_ai = AUDIO_CHANNEL_MONO;
static int channel_num_ao = AUDIO_CHANNEL_MONO;


/*
*  audio output
*/


/*
	get_file_size: 获取文件长度
	return: 成功:AK_SUCCESS 失败:AK_FAILED
*/
static unsigned int get_file_size( char *pc_filename )
{
    struct stat stat_buf;
    if( stat( pc_filename , &stat_buf ) < 0 ){
        return AK_FAILED ;
    }
    return ( unsigned int )stat_buf.st_size ;
}


static int dump_layer_para(const char * name, struct ak_tde_layer* layer)
{
    printf("\nlayer %s :\n  width = %d , height = %d,pos_width = %d, pos_height = %d, pos_left = %d, pos_top = %d , format_param = %d, phyaddr = 0x%x \n\n",
    name, layer->width, layer->height,layer->pos_width, layer->pos_height,
    layer->pos_left, layer->pos_top,layer->format_param, layer->phyaddr);
}

/*
    	ak_gui_open: 打开lcd屏幕设备
    	@p_fb_fix_screeninfo[OUT]:返回屏幕的只读信息
    	@p_fb_var_screeninfo[OUT]:返回屏幕的可配置信息
    	@pv_addr[OUT]:显存的地址
   	 	return: 成功:AK_SUCCESS 失败:AK_FAILED
*/
static int ak_gui_open( struct fb_fix_screeninfo *p_fb_fix_screeninfo, struct fb_var_screeninfo *p_fb_var_screeninfo, void **pv_addr )
{
    if ( fd_gui > 0 ) {
        return AK_SUCCESS;
    }
    else if ( ( fd_gui = open( DEV_GUI, O_RDWR ) ) > 0 ) {

        if (ioctl(fd_gui, FBIOGET_FSCREENINFO, p_fb_fix_screeninfo) < 0) {
            close( fd_gui );
            return AK_FAILED;
        }
        if (ioctl(fd_gui, FBIOGET_VSCREENINFO, p_fb_var_screeninfo) < 0) {
            close( fd_gui );
            return AK_FAILED;
        }
        *pv_addr = osal_fb_mmap_viraddr(p_fb_fix_screeninfo->smem_len, fd_gui);
	//( void * )mmap( 0 , p_fb_fix_screeninfo->smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd_gui, 0 );

        tde_layer_screen.width      = fb_var_screeninfo_param.xres;
        tde_layer_screen.height     = fb_var_screeninfo_param.yres;
        tde_layer_screen.pos_left   = 0;
        tde_layer_screen.pos_top    = 0;
        tde_layer_screen.pos_width  = fb_var_screeninfo_param.xres;
        tde_layer_screen.pos_height = fb_var_screeninfo_param.yres;
        tde_layer_screen.phyaddr    = fb_fix_screeninfo_param.smem_start;
        return AK_SUCCESS;
    }
    else {
        return AK_FAILED;
    }
}


/*
   	 ak_gui_close: 关闭lcd屏幕设备
   	 return: 成功:AK_SUCCESS 失败:AK_FAILED
*/
static int ak_gui_close( void )
{
    if ( fd_gui > 0 ){
        if( p_vaddr_fb != NULL ) {
            osal_fb_munmap_viraddr( p_vaddr_fb, fb_fix_screeninfo_param.smem_len );//munmap( p_vaddr_fb, fb_fix_screeninfo_param.smem_len );
        }
        if( close( fd_gui ) == 0 ) {
            fd_gui = 0 ;
            return AK_SUCCESS;
        }
        else {

            return AK_FAILED;
        }
    }
    else {
        return AK_SUCCESS;
    }
}

int show(int sc[7],int tg[7],int bg[7],char *pc_file_src,char *pc_file_bg)
{
	unsigned int i_filesize_src, i_filesize_bg,i_filesize_src1;
	FILE *pFILE;
	void *p_vaddr_src= NULL, *p_vaddr_bg= NULL;
	
	struct ak_tde_layer tde_layer_src;
	struct ak_tde_layer tde_layer_tgt;
	struct ak_tde_layer tde_layer_bg;
	
	
	//initialize source image
	tde_layer_src.width = 		sc[0];
	tde_layer_src.height = 		sc[1];
	tde_layer_src.pos_left = 	sc[2];
	tde_layer_src.pos_top = 	sc[3];
	tde_layer_src.pos_width = 	sc[4];
	tde_layer_src.pos_height = 	sc[5];
	tde_layer_src.format_param =sc[6];
	
	//intilaize target 
	tde_layer_tgt.width = 		tg[0];
	tde_layer_tgt.height = 		tg[1];
	tde_layer_tgt.pos_left = 	tg[2];
	tde_layer_tgt.pos_top = 	tg[3];
	tde_layer_tgt.pos_width = 	tg[4];
	tde_layer_tgt.pos_height = 	tg[5];
	tde_layer_tgt.format_param =tg[6];
	
	//intialize background 
	tde_layer_bg.width = 		bg[0];
	tde_layer_bg.height = 		bg[1];
	tde_layer_bg.pos_left = 	bg[2];
	tde_layer_bg.pos_top = 		bg[3];
	tde_layer_bg.pos_width = 	bg[4];
	tde_layer_bg.pos_height = 	bg[5];
	tde_layer_bg.format_param = bg[6];

	int i_dmasize_src  = tde_layer_src.width * tde_layer_src.height * af_format_byte[ tde_layer_src.format_param ];      //源图片的设定大小
    	int i_dmasize_bg   = tde_layer_bg.width * tde_layer_bg.height * af_format_byte[ tde_layer_bg.format_param ];         //背景图片的设定大小
	
	if( ak_tde_open( ) != ERROR_TYPE_NO_ERROR ) 
        return 0;
        
	
	
	if ( ( i_dmasize_src == 0 ) || ( i_dmasize_bg == 0 ) ) 
	{
        ak_print_error_ex(MODULE_ID_APP, "FORMAT ERROR. i_dmasize_src= %d i_dmasize_bg= %d\n", i_dmasize_src, i_dmasize_bg );
        goto test_tde_end;
        }
	
	if ( ( DUAL_FB_FIX == AK_TRUE ) && ( DUAL_FB_VAR != 0 ) ) 
	{                 						//如果使用双buffer的话，将buffer设置为使用第1个buffer
        DUAL_FB_VAR = 0;
        ioctl( fd_gui, FBIOPUT_VSCREENINFO, &fb_var_screeninfo_param ) ;
   	 }
	
	i_filesize_src 	 = get_file_size( pc_file_src );                   //获取文件长度
   	i_filesize_bg	 = get_file_size( pc_file_bg );
	
  
	if ( ( i_filesize_src != i_dmasize_src ) || ( i_filesize_bg != i_dmasize_bg ) ) 
	{
        printf( "FILE SIZE NOT FIT FORMAT. i_filesize_src= %d i_dmasize_src= %d i_filesize_bg= %d i_dmasize_bg= %d\n",
        i_filesize_src, i_filesize_bg, i_filesize_bg, i_dmasize_bg );
        goto test_tde_end;
    	}
	
	
	p_vaddr_src 	= ak_mem_dma_alloc( 1, i_dmasize_src );                         	       //分配pmem内存
    	p_vaddr_bg 	= ak_mem_dma_alloc( 1, i_dmasize_bg );

	ak_mem_dma_vaddr2paddr( p_vaddr_bg , ( unsigned long * )&tde_layer_bg.phyaddr );       //获取背景图片dma物理地址
    	ak_mem_dma_vaddr2paddr( p_vaddr_src , ( unsigned long * )&tde_layer_src.phyaddr );     //获取源图片dma物理地址
	
	if( ( i_filesize_src != SIZE_ERROR ) && ( i_filesize_src != 0 ) ) 
	{        								 		//源图片
        	pFILE = fopen( pc_file_src , "rb" );                                     	//将图片内容读入pmem
        	fseek(pFILE, 0, SEEK_SET);
        	fread( ( char * )p_vaddr_src, 1, i_filesize_src, pFILE);
        	fclose( pFILE );
   	 }   
	if( ( i_filesize_bg != SIZE_ERROR ) && ( i_filesize_bg != 0 ) ) 
	{         								 //背景图片
        pFILE = fopen( pc_file_bg , "rb" );                                      //将图片内容读入pmem
        fseek( pFILE, 0, SEEK_SET ) ;
        fread( ( char * )p_vaddr_bg, 1, i_filesize_bg, pFILE);
        fclose( pFILE );
        //ak_tde_opt_scale( &tde_layer_bg, &tde_layer_screen );                   //贴背景图片
    }
   	else
	{
        memset( p_vaddr_fb , 0xff , fb_fix_screeninfo_param.smem_len );
    }
	
	
	tde_layer_tgt.width 	= fb_var_screeninfo_param.xres;                        //屏幕宽
    tde_layer_tgt.height 	= fb_var_screeninfo_param.yres;                        //屏幕高
    tde_layer_tgt.phyaddr 	= fb_fix_screeninfo_param.smem_start;                  //屏幕的fb物理地址直接赋值

	
	
	ak_tde_opt_blit( &tde_layer_src,&tde_layer_tgt);
	
	
	
	//ak_tde_opt_fillrect(&p_tde_layer,0xFF1493);
	//ak_sleep_ms(2000);
	//ak_tde_opt_scale( &tde_layer_bg, &tde_layer_screen );       
	//ak_tde_opt_format( &tde_layer_bg,&tde_layer_screen);
	 
test_tde_end:
    if( p_vaddr_src != NULL ) {
        ak_mem_dma_free( p_vaddr_src );
    }
    if( p_vaddr_bg != NULL ) {
        ak_mem_dma_free( p_vaddr_bg );
    }
    	
	
	//ak_gui_close( );
        ak_tde_close( );
        
   
}
int show_icon(int icon_width,int icon_height,int icon_x,int icon_y,char *pc_file_src)
{
	unsigned int i_filesize_src;
	FILE *pFILE;
	void *p_vaddr_src= NULL;
	
	struct ak_tde_layer tde_layer_src;
	struct ak_tde_layer tde_layer_tgt;
	struct ak_tde_layer tde_layer_bg;
	
	
	//initialize source image
	tde_layer_src.width = 		icon_width;
	tde_layer_src.height = 		icon_height;
	tde_layer_src.pos_left = 	sc[2];
	tde_layer_src.pos_top = 	sc[3];
	tde_layer_src.pos_width = 	icon_width;
	tde_layer_src.pos_height = 	icon_height;
	tde_layer_src.format_param =sc[6];
	
	//intilaize target 
	tde_layer_tgt.width = 		tg[0];
	tde_layer_tgt.height = 		tg[1];
	tde_layer_tgt.pos_left = 	icon_x;
	tde_layer_tgt.pos_top = 	icon_y;
	tde_layer_tgt.pos_width = 	tg[4];
	tde_layer_tgt.pos_height = 	tg[5];
	tde_layer_tgt.format_param =tg[6];
	
	int i_dmasize_src  = tde_layer_src.width * tde_layer_src.height * af_format_byte[ tde_layer_src.format_param ];      //源图片的设定大小
	
	if( ak_tde_open( ) != ERROR_TYPE_NO_ERROR ) 
        return 0;
        
	if ( ( i_dmasize_src == 0 ) ) 
	{
        ak_print_error_ex(MODULE_ID_APP, "FORMAT ERROR. i_dmasize_src= %d \n", i_dmasize_src);
        goto test_tde_end;
    }
	
	if ( ( DUAL_FB_FIX == AK_TRUE ) && ( DUAL_FB_VAR != 0 ) ) 
	{                 						
        DUAL_FB_VAR = 0;
        ioctl( fd_gui, FBIOPUT_VSCREENINFO, &fb_var_screeninfo_param ) ;
   	}
	
	i_filesize_src 	 = get_file_size( pc_file_src );                   //获取文件长度
  
	if ( ( i_filesize_src != i_dmasize_src ) ) 
	{
        printf( "FILE SIZE NOT FIT FORMAT. i_filesize_src= %d i_dmasize_src= %d\n",i_filesize_src, i_dmasize_src);
        goto test_tde_end;
    }
	
	
	p_vaddr_src 	= ak_mem_dma_alloc( 1, i_dmasize_src );                         	       //分配pmem内存
    ak_mem_dma_vaddr2paddr( p_vaddr_src , ( unsigned long * )&tde_layer_src.phyaddr );     //获取源图片dma物理地址
	
	if( ( i_filesize_src != SIZE_ERROR ) && ( i_filesize_src != 0 ) ) 
	{        								 		//源图片
        	pFILE = fopen( pc_file_src , "rb" );                                     	//将图片内容读入pmem
        	fseek(pFILE, 0, SEEK_SET);
        	fread( ( char * )p_vaddr_src, 1, i_filesize_src, pFILE);
        	fclose( pFILE );
   	 }   
	
	
	tde_layer_tgt.width 	= fb_var_screeninfo_param.xres;                        //屏幕宽
    tde_layer_tgt.height 	= fb_var_screeninfo_param.yres;                        //屏幕高
    tde_layer_tgt.phyaddr 	= fb_fix_screeninfo_param.smem_start;                  //屏幕的fb物理地址直接赋值

	
	
	ak_tde_opt_blit( &tde_layer_src,&tde_layer_tgt);
	 
test_tde_end:
    if( p_vaddr_src != NULL ) {
        ak_mem_dma_free( p_vaddr_src );
    }

    	
	
	//ak_gui_close( );
        ak_tde_close( );


}
int show_weather_icon(int icon_x,int icon_y,char *weather_type)
{
	if (strcmp(weather_type,"强沙尘暴") ==0)
	{
		show_icon(90,75,icon_x,icon_y,ssand);
	}
	if (strcmp(weather_type,"晴") == 0)
	{
		show_icon(90,75,icon_x,icon_y,sun);
	}
	if (strcmp(weather_type,"多云") == 0 || strcmp(weather_type,"晴间多云") == 0)
	{
		show_icon(90,75,icon_x,icon_y,cloud);
	}
	if (strcmp(weather_type,"大部多云") == 0)
	{
		show_icon(90,75,icon_x,icon_y,dcloud);
	}
	if (strcmp(weather_type,"阴") == 0)
	{
		show_icon(90,75,icon_x,icon_y,overcast);
	}
	if (strcmp(weather_type,"阵雪") == 0)
	{
		show_icon(90,75,icon_x,icon_y,fsnow);
	}
	if (strcmp(weather_type,"阵雨") == 0)
	{
		show_icon(90,75,icon_x,icon_y,frain);
	}
	if (strcmp(weather_type,"雨夹雪") == 0)
	{
		show_icon(90,75,icon_x,icon_y,rainsnow);
	}
	if (strcmp(weather_type,"小雪") == 0)
	{
		show_icon(90,75,icon_x,icon_y,ssnow);
	}
	if (strcmp(weather_type,"小雨") == 0)
	{
		show_icon(90,75,icon_x,icon_y,srain);
	}		
	if (strcmp(weather_type,"雷阵雨") == 0)
	{
		show_icon(90,75,icon_x,icon_y,flashrain);
	}		
	if (strcmp(weather_type,"中雪") == 0)
	{
		show_icon(90,75,icon_x,icon_y,msnow);
	}		
	if (strcmp(weather_type,"中雨") == 0)
	{
		show_icon(90,75,icon_x,icon_y,mrain);
	}	
	if (strcmp(weather_type,"大雪") == 0)
	{
		show_icon(90,75,icon_x,icon_y,bsnow);
	}		
	if (strcmp(weather_type,"大雨") == 0)
	{
		show_icon(90,75,icon_x,icon_y,brain);
	}	
	if (strcmp(weather_type,"雷阵雨伴有冰雹") == 0)
	{
		show_icon(90,75,icon_x,icon_y,frice);
	}	
	if (strcmp(weather_type,"暴雪") == 0)
	{
		show_icon(90,75,icon_x,icon_y,gsnow);
	}	
	if (strcmp(weather_type,"暴雨") == 0)
	{
		show_icon(90,75,icon_x,icon_y,grain);
	}	
	if (strcmp(weather_type,"浮尘") == 0)
	{
		show_icon(90,75,icon_x,icon_y,flydust);
	}	
	if (strcmp(weather_type,"扬沙") == 0)
	{
		show_icon(90,75,icon_x,icon_y,flysand);
	}	
	if (strcmp(weather_type,"沙尘暴") == 0)
	{
		show_icon(90,75,icon_x,icon_y,sand);
	}	

}
int show_bg(int bg[7],char *pc_file_bg)
{
	unsigned int i_filesize_bg;
	FILE *pFILE;
	void  *p_vaddr_bg= NULL;

	struct ak_tde_layer tde_layer_bg;

	
	//intialize background 
	tde_layer_bg.width = 		bg[0];
	tde_layer_bg.height = 		bg[1];
	tde_layer_bg.pos_left = 	bg[2];
	tde_layer_bg.pos_top = 		bg[3];
	tde_layer_bg.pos_width = 	bg[4];
	tde_layer_bg.pos_height = 	bg[5];
	tde_layer_bg.format_param = bg[6];

    	int i_dmasize_bg   = tde_layer_bg.width * tde_layer_bg.height * af_format_byte[ tde_layer_bg.format_param ];         //背景图片的设定大小
	
	if( ak_tde_open( ) != ERROR_TYPE_NO_ERROR ) 
        return 0;
        
	if ( ( i_dmasize_bg == 0 ) ) 
	{
        ak_print_error_ex(MODULE_ID_APP, "FORMAT ERROR.  i_dmasize_bg= %d\n",i_dmasize_bg );
        goto test_tde_end;
        }
	
	if ( ( DUAL_FB_FIX == AK_TRUE ) && ( DUAL_FB_VAR != 0 ) ) 
	{                 						//如果使用双buffer的话，将buffer设置为使用第1个buffer
        DUAL_FB_VAR = 0;
        ioctl( fd_gui, FBIOPUT_VSCREENINFO, &fb_var_screeninfo_param ) ;
   	 }
	              
   	i_filesize_bg	 = get_file_size( pc_file_bg );
	
  
	if ( ( i_filesize_bg != i_dmasize_bg ) ) 
	{
        
        goto test_tde_end;
    	}
	
	                      	   
    	p_vaddr_bg 	= ak_mem_dma_alloc( 1, i_dmasize_bg );

	ak_mem_dma_vaddr2paddr( p_vaddr_bg , ( unsigned long * )&tde_layer_bg.phyaddr );       //获取背景图片dma物理地址
 
	if( ( i_filesize_bg != SIZE_ERROR ) && ( i_filesize_bg != 0 ) ) 
	{         								 //背景图片
        pFILE = fopen( pc_file_bg , "rb" );                                      //将图片内容读入pmem
        fseek( pFILE, 0, SEEK_SET ) ;
        fread( ( char * )p_vaddr_bg, 1, i_filesize_bg, pFILE);
        fclose( pFILE );
        ak_tde_opt_scale( &tde_layer_bg, &tde_layer_screen );                   //贴背景图片
    }
   	else
	{
        memset( p_vaddr_fb , 0xff , fb_fix_screeninfo_param.smem_len );
    }


	

	 
test_tde_end:
    
    if( p_vaddr_bg != NULL ) {
        ak_mem_dma_free( p_vaddr_bg );
    }
    	
	
	//ak_gui_close( );
        ak_tde_close( );
        
   
}
int pcm_publish(struct mosquitto *mosq)
{
	int ret;
	int pcm_fd;
	int len;
	char buf_pcm[Max_pcm_buf];

	memset(buf_pcm,'\0',sizeof(buf_pcm));
	strcpy(buf_pcm,"open");
	mosquitto_publish(mosq,NULL,"speech",sizeof(buf_pcm),buf_pcm,0,0);
	while(ao_pcm("/mnt/mmc/open_voice.pcm",channel_num_ao) !=1);	
	while(ai_pcm("/mnt/mmc/ui1/",channel_num_ai) !=1);

	pcm_fd = open("/mnt/mmc/ui1/1.pcm",O_RDONLY);
	if (pcm_fd == -1)
	{
		printf("open pcm error");
	}
	while ((len = read(pcm_fd,buf_pcm,Max_pcm_buf)) > 0 )
	{
		mosquitto_publish(mosq,NULL,"speech",sizeof(buf_pcm),buf_pcm,0,0);
	}
	ret = close(pcm_fd);
	if(ret == -1)
	{
		printf("close pcm error");
	}
	sleep(1);
	memset(buf_pcm,'\0',sizeof(buf_pcm));
	strcpy(buf_pcm,"close");
	mosquitto_publish(mosq,NULL,"speech",sizeof(buf_pcm),buf_pcm,0,0);	
	return 1;
}

int yuv_publish(struct mosquitto *mosq)
{
	int ret;
	int yuv_fd;
	int len;
	char buf_yuv[Max_yuv_buf];

	mosquitto_publish(mosq,NULL,"image",strlen("open"),"open",0,0);
	//show_icon(200,250,164,172,jkb);
	ao_pcm("/mnt/mmc/open_video.pcm",channel_num_ao);	
	while(vi("/mnt/") != 1);
	yuv_fd	= open("/mnt/dev0_640x360.yuv",O_RDONLY);
	if (yuv_fd == -1)
	{
		printf("open yuv error");
	}
	while ((len = read(yuv_fd,buf_yuv,Max_yuv_buf)) > 0 )
	{
		mosquitto_publish(mosq,NULL,"image",sizeof(buf_yuv),buf_yuv,2,0);
	}
	ret = close(yuv_fd);
	if(ret == -1)
	{
		printf("close pcm error");
	}
	sleep(1);
	//memset(buf3,'\0',sizeof(buf3));
	//strcpy(buf3,"close");
	mosquitto_publish(mosq,NULL,"image",strlen("close"),"close",2,0);

	return 1;



}


void lcd_put_pixel(int x, int y, unsigned int color)
{
    unsigned char *pen_8 = fbmem+y*line_width+x*pixel_width;
	unsigned char *pen_8_1,*pen_8_2,*pen_8_3;
    unsigned short *pen_16; 
    unsigned int *pen_32;   

    unsigned char red, green, blue;  

	pen_8_1 = pen_8;
	pen_8_2 = pen_8 + 1;
	pen_8_3 = pen_8 + 2;

    pen_16 = (unsigned short *)pen_8;
    pen_32 = (unsigned int *)pen_8;

    switch (fb_var_screeninfo_param.bits_per_pixel)
    {
        case 8:
        {
            *pen_8 = color;
            break;
        }
        case 24:
        {
            /* 565 */
            red   = (color >> 16) & 0xff;
            green = (color >> 8) & 0xff;
            blue  = (color >> 0) & 0xff;

            //color = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
            //*pen_16 = color;
			*pen_8_1 = blue;
			*pen_8_2 = green;
			*pen_8_3 = red;
            break;
        }
        case 32:
        {
            *pen_32 = color;
            break;
        }
        default:
        {
            printf("can't surport %dbpp\n", fb_var_screeninfo_param.bits_per_pixel);
            break;
        }
    }
}

void
draw_bitmap( FT_Bitmap*  bitmap,
             FT_Int      x,
             FT_Int      y,unsigned int color)
{
    FT_Int  i, j, p, q;
    FT_Int  x_max = x + bitmap->width;
    FT_Int  y_max = y + bitmap->rows;

    //printf("x = %d, y = %d\n", x, y);

    for ( i = x, p = 0; i < x_max; i++, p++ )
    {
        for ( j = y, q = 0; j < y_max; j++, q++ )
        {
            if ( i < 0      || j < 0       ||
                i >= fb_var_screeninfo_param.xres || j >= fb_var_screeninfo_param.yres )
            continue;
		
            //image[j][i] |= bitmap->buffer[q * bitmap->width + p];
	    if(bitmap->buffer[q * bitmap->width + p] !=0){
            lcd_put_pixel(i, j,color);}
        }
    }
}


int compute_string_bbox(FT_Face       face, wchar_t *wstr, FT_BBox  *abbox)
{
    int i;
    int error;
    FT_BBox bbox;
    FT_BBox glyph_bbox;
    FT_Vector pen;
    FT_Glyph  glyph;
    FT_GlyphSlot slot = face->glyph;

    /* 初始化 */
    bbox.xMin = bbox.yMin = 32000;
    bbox.xMax = bbox.yMax = -32000;

    /* 指定原点为(0, 0) */
    pen.x = 0;
    pen.y = 0;

    /* 计算每个字符的bounding box */
    /* 先translate, 再load char, 就可以得到它的外框了 */
    for (i = 0; i < wcslen(wstr); i++)
    {
        /* 转换：transformation */
        FT_Set_Transform(face, 0, &pen);

        /* 加载位图: load glyph image into the slot (erase previous one) */
        error = FT_Load_Char(face, wstr[i], FT_LOAD_RENDER);
        if (error)
        {
            printf("FT_Load_Char error\n");
            return -1;
        }

        /* 取出glyph */
        error = FT_Get_Glyph(face->glyph, &glyph);
        if (error)
        {
            printf("FT_Get_Glyph error!\n");
            return -1;
        }
        
        /* 从glyph得到外框: bbox */
        FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_TRUNCATE, &glyph_bbox);

        /* 更新外框 */
        if ( glyph_bbox.xMin < bbox.xMin )
            bbox.xMin = glyph_bbox.xMin;

        if ( glyph_bbox.yMin < bbox.yMin )
            bbox.yMin = glyph_bbox.yMin;

        if ( glyph_bbox.xMax > bbox.xMax )
            bbox.xMax = glyph_bbox.xMax;

        if ( glyph_bbox.yMax > bbox.yMax )
            bbox.yMax = glyph_bbox.yMax;
        
        /* 计算下一个字符的原点: increment pen position */
        pen.x += slot->advance.x;
        pen.y += slot->advance.y;
    }

    /* return string bbox */
    *abbox = bbox;
}


int display_string(FT_Face face, wchar_t *wstr, int lcd_x, int lcd_y,unsigned int color)
{
    int i;
    int error;
    FT_BBox bbox;
    FT_Vector pen;
    FT_Glyph  glyph;
    FT_GlyphSlot slot = face->glyph;

    /* 把LCD坐标转换为笛卡尔坐标 */
    int x = lcd_x;
    int y = fb_var_screeninfo_param.yres - lcd_y;

    /* 计算外框 */
    compute_string_bbox(face, wstr, &bbox);

    /* 反推原点 */
    pen.x = (x - bbox.xMin) * 64; /* 单位: 1/64像素 */
    pen.y = (y - bbox.yMax) * 64; /* 单位: 1/64像素 */

    /* 处理每个字符 */
    for (i = 0; i < wcslen(wstr); i++)
    {
        /* 转换：transformation */
        FT_Set_Transform(face, 0, &pen);

        /* 加载位图: load glyph image into the slot (erase previous one) */
        error = FT_Load_Char(face, wstr[i],  FT_LOAD_RENDER );
        if (error)
        {
            printf("FT_Load_Char error\n");
            return -1;
        }

        /* 在LCD上绘制: 使用LCD坐标 */
        draw_bitmap( &slot->bitmap,
                        slot->bitmap_left,
                        fb_var_screeninfo_param.yres - slot->bitmap_top,color);

        /* 计算下一个字符的原点: increment pen position */
        pen.x += slot->advance.x;
        pen.y += slot->advance.y;
    }

    return 0;
}


int ts_init(void)
{
	//1、打开触摸屏
	ts_fd = open("/dev/input/event0",O_RDWR);
	if(ts_fd < 0)
	{
		perror("open touch srceen failed");
		return -1;
	}
}





void get_pos(int *x,int *y)
{
	struct input_event buf;
	while(1)
	{
		//循环获取触摸屏信息
		read(ts_fd,&buf,sizeof(buf));
		if(buf.type == EV_ABS && buf.code == ABS_MT_POSITION_X) //获取x轴坐标
		{
			*x = 1024 - buf.value;
		}
		if(buf.type == EV_ABS && buf.code == ABS_MT_POSITION_Y) //获取y轴坐标
		{
			*y = buf.value;
		}
		//判断手指松开
		if(buf.value == 0)
			break;
	}
}

static int check_dir(const char *path)
{
    struct stat buf = {0};

    if (NULL == path)
        return 0;

    stat(path, &buf);
    if (S_ISDIR(buf.st_mode)) {
        return 1;
    } else {
        return 0;
    }
}

static int stop_vi(int dev_id)
{
    int ret = -1;   
    int chn_main_id = VIDEO_CHN0;
    int chn_sub_id = VIDEO_CHN1;
    int chn_trd_id = VIDEO_CHN16;

    if (dev_id)
    {
        chn_main_id = VIDEO_CHN2;
        chn_sub_id = VIDEO_CHN3;
        chn_trd_id = VIDEO_CHN17;
    }
    
    ak_vi_disable_chn(chn_main_id);
    ak_vi_disable_chn(chn_sub_id);
#ifdef THIRD_CHN_SUPPORT
    ak_vi_disable_chn(chn_trd_id);
#endif

	ak_vi_disable_dev(dev_id);
    ret = ak_vi_close(dev_id);

    return ret;
}

static int start_vi(int dev_id)
{
    /* 
     * step 0: global value initialize
     */
    int ret = -1;                                //return value
    int width = res_group[main_res_id].width;
    int height = res_group[main_res_id].height;
    int subwidth = res_group[sub_res_id].width;
    int subheight = res_group[sub_res_id].height;
    int chn_main_id = VIDEO_CHN0;
    int chn_sub_id = VIDEO_CHN1;
    int chn_trd_id = VIDEO_CHN16;

  
    if (dev_id)
    {
        chn_main_id = VIDEO_CHN2;
        chn_sub_id = VIDEO_CHN3;
        chn_trd_id = VIDEO_CHN17;
    }



    /* 
     * step 1: open video input device
     */
    ret = ak_vi_open(dev_id);

    /*
     * step 2: load isp config
     */
    ret = ak_vi_load_sensor_cfg(dev_id, isp_path);


    

    /* 
     * step 3: get sensor support max resolution
     */
    RECTANGLE_S res;                //max sensor resolution
    VI_DEV_ATTR    dev_attr;
    memset(&dev_attr, 0, sizeof(VI_DEV_ATTR));
    dev_attr.dev_id = dev_id;
    dev_attr.crop.left = 0;
    dev_attr.crop.top = 0;
    dev_attr.crop.width = width;
    dev_attr.crop.height = height;
    dev_attr.max_width = width;
    dev_attr.max_height = height;
    dev_attr.sub_max_width = subwidth;
    dev_attr.sub_max_height = subheight;

    /* get sensor resolution */
    ret = ak_vi_get_sensor_resolution(dev_id, &res);
    if (ret) {
        
        ak_vi_close(dev_id);
        return ret;
    } else {
        
        dev_attr.crop.width = res.width;
        dev_attr.crop.height = res.height;
    }

 

    ret = ak_vi_set_dev_attr(dev_id, &dev_attr);
    if (ret) {
        
        ak_vi_close(dev_id);
        return ret;
    }


    /*
     * step 5: set main channel attribute
     */
    
    memset(&chn_attr, 0, sizeof(VI_CHN_ATTR));
    chn_attr.chn_id = chn_main_id;
    chn_attr.res.width = width;
    chn_attr.res.height = height;
    chn_attr.frame_depth = DEF_FRAME_DEPTH;
    /*disable frame control*/
    chn_attr.frame_rate = 0;
    ret = ak_vi_set_chn_attr(chn_main_id, &chn_attr);
    if (ret) {
        
        ak_vi_close(dev_id);
        return ret;
    }
    //ak_print_normal_ex(MODULE_ID_APP, "vi device %d main sub channel attribute\n", dev_id);

    //printf("5555");
    /*
     * step 6: set sub channel attribute
     */
    
    memset(&chn_attr_sub, 0, sizeof(VI_CHN_ATTR));
    chn_attr_sub.chn_id = chn_sub_id;
    chn_attr_sub.res.width = subwidth;
    chn_attr_sub.res.height = subheight;
    chn_attr_sub.frame_depth = DEF_FRAME_DEPTH;
    /*disable frame control*/
    chn_attr_sub.frame_rate = 0;
    ret = ak_vi_set_chn_attr(chn_sub_id, &chn_attr_sub);
    if (ret) {
        
        ak_vi_close(dev_id);
        return ret;
    }
    /*
	ret = ak_vi_switch_mode(dev_id,VI_MODE_DAY_OUTDOOR);
	if(ret != 0)
	{
		printf("set day error");


	}
	*/
	/* 
     * step 8: enable vi device 
     */
    ret = ak_vi_enable_dev(dev_id);
    if (ret) {
        
        ak_vi_close(dev_id);
        return ret;
    }

    /* 
     * step 9: enable vi main channel 
     */
    ret = ak_vi_enable_chn(chn_main_id);
    if(ret)
    {
       
        ak_vi_close(dev_id);
        return ret;
    }

    /* 
     * step 10: enable vi sub channel 
     */
    ret = ak_vi_enable_chn(chn_sub_id);
    if(ret)
    {
        
        ak_vi_close(dev_id);
        return ret;
    }


    return ret;
}

static void save_yuv_data(int chn_id, const char *path, int index, 
        struct video_input_frame *frame, VI_CHN_ATTR *attr)
{
    FILE *fd = NULL;
    unsigned int len = 0;
    unsigned char *buf = NULL;
    struct ak_date date;
    char time_str[32] = {0};
    char file_path[255] = {0};
    int dev_id;

    if ((VIDEO_CHN0 == chn_id) || (VIDEO_CHN1 == chn_id) || (VIDEO_CHN16 == chn_id))
    {
        dev_id = 0;
    }
    else 
    {
        dev_id = 1;
    }

   

    /* construct file name */
    ak_get_localdate(&date);
    ak_date_to_string(&date, time_str);
    if((chn_id == VIDEO_CHN0) || (chn_id == VIDEO_CHN2))
        sprintf(file_path, "%sdev%d_%dx%d.yuv", path, dev_id, attr->res.width, attr->res.height);
    else if ((chn_id == VIDEO_CHN1) || (chn_id == VIDEO_CHN3))
        sprintf(file_path, "%sdev%d_%dx%d.yuv", path, dev_id, attr->res.width, attr->res.height);
    else if ((chn_id == VIDEO_CHN16) || (chn_id == VIDEO_CHN17))
        sprintf(file_path, "%sdev%d_%dx%d.yuv", path, dev_id, attr->res.width/2, attr->res.height/2);

    /* 
     * open appointed file to save YUV data
     * save main channel yuv here
     */
    fd = fopen(file_path, "w+b");
    if (fd) {
        buf = frame->vi_frame.data;
        len = frame->vi_frame.len;
        do {
            len -= fwrite(buf+frame->vi_frame.len-len, 1, len, fd);
        } while (len != 0);
        
        fclose(fd);
    } 
}
static void vi_capture_loop(int dev_cnt, int number, const char *path,
        VI_CHN_ATTR *attr, VI_CHN_ATTR *attr_sub)
{
    int count = 0;
    int i = 0;
    int chn_id = 0;
    int flag_0 = 0;
    int flag_1 = 0;
    struct video_input_frame frame;

    //ak_print_normal(MODULE_ID_APP, "capture start\n");

    /*
     * To get frame by loop
     */
    while (count <  number * dev_cnt) 
    {
        chn_id = channel_num;
        flag_0 = 0;
        flag_1 = 0;
        
        while (flag_0 + flag_1 < dev_cnt)
        {
            memset(&frame, 0x00, sizeof(frame));

            /* to get frame according to the channel number */
            int ret = ak_vi_get_frame(chn_id, &frame);

            if (!ret) {
                /* 
                 * Here, you can implement your code to use this frame.
                 * Notice, do not occupy it too long.
                 */
                

                if ((chn_id == VIDEO_CHN0) || (chn_id == VIDEO_CHN2))
                    save_yuv_data(chn_id, path, count, &frame,attr);
                else if ((chn_id == VIDEO_CHN1) || (chn_id == VIDEO_CHN3))
                    save_yuv_data(chn_id, path, count, &frame,attr_sub);
    			else 
                    save_yuv_data(chn_id, path, count, &frame,attr_sub);

                /* 
                 * in this context, this frame was useless,
                 * release frame data
                 */
                ak_vi_release_frame(chn_id, &frame);
                count++;

                if (chn_id == channel_num)
                {
                    flag_0 = 1;

                    if (2 == dev_cnt)
                    {
                        if (VIDEO_CHN0 == channel_num)
                            chn_id = VIDEO_CHN2;
                        else if (VIDEO_CHN1 == channel_num)
                            chn_id = VIDEO_CHN3;
                        else
                            chn_id = VIDEO_CHN17; 
                    }
                }
                else
                {
                    flag_1 = 1;
                }
            } else {

                /* 
                 *    If getting too fast, it will have no data,
                 *    just take breath.
                 */
               
                ak_sleep_ms(10);
            } 
                
        }
    }

    //ak_print_normal(MODULE_ID_APP,"capture finish\n\n");
}

int vi(const char *save_path)
{



    int dev_id = VIDEO_DEV0;

    int ret = -1; 

    ret = start_vi(VIDEO_DEV0);
    if (ret)
    {

    }
    
    vi_capture_loop(dev_cnt, frame_num, save_path, &chn_attr, &chn_attr_sub);
    
    stop_vi(VIDEO_DEV0);

    printf("ok");
	return 1;

}

static void create_pcm_file_name(const char *path, char *file_path,
                                        int sample_rate, int channel_num, int save_time)
{
    if (0 == check_dir(path)) 
    {
        return;
    }

    char time_str[20] = {0};
    struct ak_date date;

    /* get the file path */
    ak_get_localdate(&date);
    ak_date_to_string(&date, time_str);
    sprintf(file_path, "%s%s_%d_%d_%d.pcm", path, time_str, sample_rate, channel_num, save_time);
}

static int open_pcm_file(int sample_rate, int channel_num, const char *path, FILE **fp, int save_time)
{
	/* create the pcm file name */
    char file_path[255];
    sprintf(file_path, "%s%d.pcm", path,1);

    /* open file */
    *fp = fopen(file_path, "w+b");
	printf(file_path);
    if (NULL == *fp) 
    {
        //ak_print_normal(MODULE_ID_APP, "open pcm file: %s error\n", file_path);
		return -1;
    } 
    else 
    {
        //ak_print_normal(MODULE_ID_APP, "open pcm file: %s OK\n", file_path);
		return 0;
    }
}

static void close_pcm_file(FILE *fp)
{
    if (NULL != fp) 
    {
        fclose(fp);
        fp = NULL;
    }
    printf("ok");
}

static void setup_default_audio_argument(void *audio_args, char args_type)
{
    struct ak_audio_nr_attr     default_ai_nr_attr      = {-40, 0, 1};
    struct ak_audio_agc_attr    default_ai_agc_attr     = {24576, 4, 0, 80, 0, 1};
    struct ak_audio_aec_attr    default_ai_aec_attr     = {0, 1024, 1024, 0, 512, 1};
    struct ak_audio_aslc_attr   default_ai_aslc_attr    = {9830, 0, 0};

    struct ak_audio_nr_attr     default_ao_nr_attr      = {-40, 0, 1};
    struct ak_audio_aslc_attr   default_ao_aslc_attr    = {9830, 0, 0};
    
    switch(args_type)
    {
    case 1:
        *(struct ak_audio_nr_attr*)audio_args = default_ai_nr_attr;
        break;
    case 2:
        *(struct ak_audio_agc_attr*)audio_args = default_ai_agc_attr;
        break;
    case 3:
        *(struct ak_audio_aec_attr*)audio_args = default_ai_aec_attr;
        break;
    case 4:
        *(struct ak_audio_aslc_attr*)audio_args = default_ai_aslc_attr;
        break;
    case 5:
        *(struct ak_audio_nr_attr*)audio_args = default_ao_nr_attr;
        break;
    case 6:
        *(struct ak_audio_aslc_attr*)audio_args = default_ao_aslc_attr;
        break;
    default:
        break;
    }

    return;
}

static void ai_capture_loop(int ai_handle_id, const char *path, int save_time)
{
    unsigned long long start_ts = 0;// use to save capture start time
    unsigned long long end_ts = 0;    // the last frame time
    struct frame frame = {0};
    int ret = AK_FAILED;

    

    while (1) 
    {
        /* get the pcm data frame */
        ret = ak_ai_get_frame(ai_handle_id, &frame, 0);
        if (ret) 
        {
            if (ERROR_AI_NO_DATA == ret)
            {  
                ak_sleep_ms(10);
                continue;
				 
            }
            else 
            {  
                break;
            }
        }
        
        if (!frame.data || frame.len <= 0)
        {
            ak_sleep_ms(10);
            continue;
        }

        if (NULL != fp) 
        {
            if(fwrite(frame.data, frame.len, 1, fp) < 0) 
            {
                ak_ai_release_frame(ai_handle_id, &frame);
                //ak_print_normal(MODULE_ID_APP, "write file error.\n");
                break;
            }
        }

        /* save the begin time */
        if (0 == start_ts) 
        {
            start_ts = frame.ts;
            end_ts = frame.ts;
        }
        end_ts = frame.ts;

        ak_ai_release_frame(ai_handle_id, &frame);

        /* time is up */
        if ((end_ts - start_ts) >= save_time) 
        {
            //ak_print_normal(MODULE_ID_APP, "*** timp is up, start_ts = %lld, end_ts = %lld, save_time = %d ***\n\n",
                      //  start_ts, end_ts, save_time);
            break;
        }
    }
    ak_print_normal(MODULE_ID_APP, "*** capture finish ***\n\n");
}

int ai_pcm(const char *path,int channel_num)
{
	open_pcm_file(sample_rate,channel_num,path,&fp,save_time);

    int ret = -1;

    struct ak_audio_in_param param;
    memset(&param, 0, sizeof(struct ak_audio_in_param)); //参数清零

    param.pcm_data_attr.sample_rate = sample_rate;                // set sample rate
    param.pcm_data_attr.sample_bits = AK_AUDIO_SMPLE_BIT_16;    // sample bits only support 16 bit
    param.pcm_data_attr.channel_num = channel_num;    // channel number
    param.dev_id = DEV_ADC;


    int ai_handle_id = -1;
    if (ak_ai_open(&param, &ai_handle_id)) //打开音频设备文件，并赋予ai_handle_id音频输入模块句柄，0~2
    {
        //ak_print_normal(MODULE_ID_APP, "*** ak_ai_open failed. ***\n");

    }

    ret = ak_ai_set_source(ai_handle_id, AI_SOURCE_MIC);     //设置音频信息的具体来源
    if (ret) 
    {
        //ak_print_normal(MODULE_ID_APP, "*** ak_ai_set_source failed. ***\n");
        ak_ai_close(ai_handle_id);      //关闭文件

    }

    ret = ak_ai_set_gain(ai_handle_id, 5);    //模拟音量调整
    if (ret) 
    {
        //ak_print_normal(MODULE_ID_APP, "*** set ak_ai_set_volume failed. ***\n");
        ak_ai_close(ai_handle_id);

    }

    ak_ai_set_volume(ai_handle_id, volume);    //设置音量

    struct ak_audio_nr_attr nr_attr;
    setup_default_audio_argument(&nr_attr, 1);
    ak_ai_set_nr_attr(ai_handle_id, &nr_attr);

    struct ak_audio_agc_attr agc_attr;
    setup_default_audio_argument(&agc_attr, 2);
    ak_ai_set_agc_attr(ai_handle_id, &agc_attr);

    ret = ak_ai_clear_frame_buffer(ai_handle_id);

    if (ret) 
    {
        //ak_print_error(MODULE_ID_APP, "*** set ak_ai_clear_frame_buffer failed. ***\n");
        ak_ai_close(ai_handle_id);

    }
	printf("<---------------------------------------------->\n");
	printf("                  录音开始\n");
	printf("<---------------------------------------------->\n");
	show_icon(150,150,874,450,yyb);
    ret = ak_ai_start_capture(ai_handle_id);    //开始捕捉
    if (ret) 
    {
        //ak_print_error(MODULE_ID_APP, "*** ak_ai_start_capture failed. ***\n");
        ak_ai_close(ai_handle_id);

    }

    ai_capture_loop(ai_handle_id, path, save_time); //进行捕捉

    ret = ak_ai_stop_capture(ai_handle_id);
    if (ret) 
    {
        //ak_print_error(MODULE_ID_APP, "*** ak_ai_stop_capture failed. ***\n");
        ak_ai_close(ai_handle_id);

    }

    ret = ak_ai_close(ai_handle_id);
    if (ret) 
    {
        ak_print_normal(MODULE_ID_APP, "*** ak_ai_close failed. ***\n");

    }


	close_pcm_file(fp);
	show_icon(150,150,874,450,yya);
	return 1;
}


static void setup_default_audio_argument_ao(void *audio_args, char args_type)
{
    struct ak_audio_nr_attr     default_ai_nr_attr      = {-40, 0, 1};
    struct ak_audio_agc_attr    default_ai_agc_attr     = {24576, 4, 0, 20, 0, 1};
    struct ak_audio_aec_attr    default_ai_aec_attr     = {0, 1024, 1024, 0, 512, 1};
    struct ak_audio_aslc_attr   default_ai_aslc_attr    = {9830, 0, 0};

    struct ak_audio_nr_attr     default_ao_nr_attr      = {-40, 0, 1};
    struct ak_audio_aslc_attr   default_ao_aslc_attr    = {9830, 0, 0};
    
    switch(args_type)
    {
    case 1:
        *(struct ak_audio_nr_attr*)audio_args = default_ai_nr_attr;
        break;
    case 2:
        *(struct ak_audio_agc_attr*)audio_args = default_ai_agc_attr;
        break;
    case 3:
        *(struct ak_audio_aec_attr*)audio_args = default_ai_aec_attr;
        break;
    case 4:
        *(struct ak_audio_aslc_attr*)audio_args = default_ai_aslc_attr;
        break;
    case 5:
        *(struct ak_audio_nr_attr*)audio_args = default_ao_nr_attr;
        break;
    case 6:
        *(struct ak_audio_aslc_attr*)audio_args = default_ao_aslc_attr;
        break;
    default:
        break;
    }

    return;
}

static FILE* open_pcm_file_ao(const char *pcm_file)
{
    FILE *fp = fopen(pcm_file, "r");
    if(NULL == fp) {
        printf("open pcm file err\n");
        return NULL;
    }
    
    return fp;
}

static void write_da_pcm(int handle_id, FILE *fp, unsigned int channel_num,
                unsigned int volume)
{
    int read_len = 0;
    int send_len = 0;
    int total_len = 0;
    int data_len = PCM_READ_LEN;
    if (AK_AUDIO_SAMPLE_RATE_44100 <= sample_rate &&  AUDIO_CHANNEL_STEREO == channel_num) {
        data_len = PCM_READ_LEN << 1;
    }
    unsigned char data[data_len];
    ak_ao_set_gain(handle_id, 6);

    while(1) 
    {
        memset(data, 0x00, sizeof(data));
        read_len = fread(data, sizeof(char), sizeof(data), fp);

        if(read_len > 0) 
        {
            /* send frame and play */
            if (ak_ao_send_frame(handle_id, data, read_len, &send_len)) 
            {
                ak_print_error_ex(MODULE_ID_APP, "write pcm to DA error!\n");
                break;
            }
            total_len += send_len;

            ak_sleep_ms(10);
        } else if(0 == read_len) {
            /* read to the end of file */
            //ak_print_normal(MODULE_ID_APP, "\n\t read to the end of file\n");

            break;
        } else {
            //ak_print_error(MODULE_ID_APP, "read, %s\n", strerror(errno));
            break;
        }

    }
    //ak_print_normal(MODULE_ID_APP, "%s exit\n", __func__);
}


int ao_pcm(const char *pcm_file,unsigned int channel_num)
{

    FILE *fp = NULL;
    fp = open_pcm_file_ao(pcm_file);
    if(NULL == fp) {
        printf("open file error\n");

    }
    int ret = 0;
 
    struct ak_audio_out_param ao_param;
    ao_param.pcm_data_attr.sample_bits = 16;
    ao_param.pcm_data_attr.channel_num = channel_num;
    ao_param.pcm_data_attr.sample_rate = ao_sample_rate;
    ao_param.dev_id = DEV_DAC;

    int ao_handle_id = -1;
    if(ak_ao_open(&ao_param, &ao_handle_id)) {
        ret = -1;

    }

    ak_ao_set_volume(ao_handle_id, volume);
    struct ak_audio_nr_attr nr_attr;
    setup_default_audio_argument_ao(&nr_attr, 6);    
    ak_ao_set_nr_attr(ao_handle_id, &nr_attr);
    
    write_da_pcm(ao_handle_id, fp, channel_num, volume);
    
    if(NULL != fp) {
        fclose(fp);
        fp = NULL;
    }

    ak_ao_close(ao_handle_id);
    ao_handle_id = -1;

	return 1;
}

int get_file_line(char *pInputName, char *pOutputBuf, int cnt)
{
	FILE * fp;
	int i=0;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
 
	fp = fopen(pInputName, "r");
	if (fp == NULL)
	 return -1;
 
	if(cnt<=0)
		 return -2;
		 
	while ((read = getline(&line, &len, fp)) != -1) {
		++i;
		if(i>=cnt)
			break;
	}
 
	if (line)
	{
		memcpy(pOutputBuf,line,strlen(line));
		free(line);
		return 0; 
	}
 
	return -3;
}

void save(char *name)
{
	FILE *fp1,*fp2;
	char buf[MAX_LINE];
	int ret;
	int len;
	int flag=0;
	//open file 
	fp1 = fopen("weather.txt","r");
	if(fp1 == NULL)
		printf("open weather.txt error\n");
	fp2 = fopen(name,"w+");
	if(fp2 == NULL)
		printf("open %s error\n",name);
	
	//read line
	while(fgets(buf,MAX_LINE,fp1)!= NULL)
	{
		len = strlen(buf);
		if (buf[0] == '{')
			flag = 1;
		if (len <= 2 && buf[len-1] != '}')
			{
				flag = 0;
			}
		if(flag)
		{
			fputs(buf,fp2);
		}

	}
	fclose(fp1);
	fclose(fp2);
	
}

char *getfileall(char *fname)
{
     FILE *fp;
    char *str;
    char txt[1000];
    int filesize;

    if ((fp=fopen(fname,"r"))==NULL)
    {
        printf("打开文件%s错误\n",fname);
        return NULL;
    }
    //将文件指针移到末尾
    fseek(fp,0,SEEK_END);
    filesize = ftell(fp);//通过ftell函数获得指针到文件头的偏移字节数。
     
    str=(char *)malloc(filesize);//动态分配str内存
//    str=malloc(filesize);//动态分配str内存
    str[0]=0;//字符串置空
//    memset(str,filesize*sizeof(char),0);//清空数组,字符串置空第二种用法
    rewind(fp);
     
    while((fgets(txt,1000,fp))!=NULL)
	{//循环读取1000字节,如果没有数据则退出循环
        strcat(str,txt);//拼接字符串
    }
    //fclose(fp);
    return str;
}

int init_tty(int fd)
{
    struct termios old_flags,new_flags;
    bzero(&new_flags,sizeof(new_flags));


    tcgetattr(fd,&old_flags);


    cfmakeraw(&new_flags);


    new_flags.c_cflag |= CLOCAL | CREAD ;


    cfsetispeed(&new_flags,B115200);
    cfsetospeed(&new_flags,B115200);


    new_flags.c_cflag &= ~CSIZE;
    new_flags.c_cflag |= CS8;


    new_flags.c_cflag &= ~PARENB;


    new_flags.c_cflag &= ~CSTOPB;


    new_flags.c_cc[VTIME] = 0;
    new_flags.c_cc[VMIN] = 1;


    tcflush(fd,TCIFLUSH);


    tcsetattr(fd,TCSANOW,&new_flags);

    return 0;

}

int get_wchar_size(const char *str)
{
    int len = strlen(str);
    int size =0;
    int i;
	for(i=0;i<len;i++)
	{
		if(str[size] >=0 && str[size] <= 127)
			size += sizeof(wchar_t);
		else
			{
				size += sizeof(wchar_t);
				   i += 2;
			}
		
	}
	return size;

}

int UTF8ToUnicode(const char *pmbs, wchar_t *pwcs, int size)
{
    int cnt = 0;
    // 这里 size-- 是预先除去尾零所需位置
    if (pmbs != NULL && pwcs != NULL && size-- > 0) {
        while (*pmbs != 0 && size > 0) {
            unsigned char ch = *pmbs;
            if (ch > 0x7FU) {
                int cwch = 0;
                while (ch & 0x80U) {
                    ch <<= 1;
                    cwch++;
                }
                *pwcs = *pmbs++ & (0xFFU >> cwch);
                while (--cwch > 0) {
                    *pwcs <<= 6;
                    *pwcs |= (*pmbs++ & 0x3FU);
                }
            } else {
                *pwcs = *pmbs++;
            }
            pwcs++;
            size--;
            cnt++;
        }
        *pwcs = 0;
        cnt++;
    }
    return cnt;
}



//myhttp
int myhttp_gets(char *domain,char *parameter1,char *parameter2,char *parameter3,char *name1,char *name2,char *name3)
{
	FILE *fp = NULL;
	int sockfd, ret, i, h;
	char *domain_head = "Host: ";
	char *domain_end  = "\r\n";
	char *parameter_head ="GET ";
	char *parameter_end =" HTTP/1.1\r\n";
	struct sockaddr_in servaddr;
	char str1[4096],buf[BUFSIZE];
	struct hostent *host;
	fd_set   t_set1;
	struct timeval  tv;
	//char *ipdomain = "api.seniverse.com";

	//create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) 
	{
			printf("创建网络连接失败,本线程即将终止---socket error!\n");
            exit(0);
	}
	
	if((host = gethostbyname(domain)) == NULL)
	{
		printf("get ip false");
		return -1;
	}
	//printf("%s",host->h_addr_list[0]);
	
	inet_ntop(host->h_addrtype,host->h_addr_list[0],buf,sizeof(buf));
	printf("%s",buf);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
	servaddr.sin_addr.s_addr=inet_addr(buf);
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
	{
		printf("连接到服务器失败,connect error!\n");
        exit(0);
    }
    printf("与远端建立了连接\n");
	
	memset(str1, 0,4096);
	strcat(str1, parameter_head);
	strcat(str1, parameter1);
	strcat(str1, parameter_end);
	strcat(str1, domain_head);
	strcat(str1, domain);
	strcat(str1, domain_end);
	strcat(str1, "\r\n");

	
	ret = write(sockfd,str1,strlen(str1));
	if (ret < 0)
	{
        printf("发送失败！错误代码是%d，错误信息是'%s'\n",errno, strerror(errno));
        exit(0);
    }
	else
	{
        printf("消息发送成功，共发送了%d个字节！\n\n", ret);
    }
	FD_ZERO(&t_set1);
    FD_SET(sockfd, &t_set1);
	sleep(2);
	tv.tv_sec= 0;
	tv.tv_usec= 0;
	h= 0;
	//printf("--------------->1");
	h= select(sockfd +1, &t_set1, NULL, NULL, &tv);
	//printf("--------------->2\n");
	if (h < 0)
	{
        close(sockfd);
        printf("在读取数据报文时SELECT检测到异常，该异常导致线程终止！\n");
        return -1;
    };
	if (h > 0)
	{
        memset(buf, 0, 5000);
        i= read(sockfd, buf, 5000);
        if (i==0)
		{
            close(sockfd);
            printf("读取数据报文时发现远端关闭，该线程终止！\n");
            return -1;
        }
	   
	   	
		//printf("%s\n", buf);
	   	fp = fopen("./weather.txt","w+");
		fputs(buf,fp);
		fclose(fp);
		save(name1);
		remove("./weather.txt");
	}

	memset(str1, 0,4096);
	strcat(str1, parameter_head);
	strcat(str1, parameter2);
	strcat(str1, parameter_end);
	strcat(str1, domain_head);
	strcat(str1, domain);
	strcat(str1, domain_end);
	strcat(str1, "\r\n");

	
	ret = write(sockfd,str1,strlen(str1));
	if (ret < 0)
	{
        printf("发送失败！错误代码是%d，错误信息是'%s'\n",errno, strerror(errno));
        exit(0);
    }
	else
	{
        printf("消息发送成功，共发送了%d个字节！\n\n", ret);
    }
	FD_ZERO(&t_set1);
    FD_SET(sockfd, &t_set1);
	sleep(2);
	tv.tv_sec= 0;
	tv.tv_usec= 0;
	h= 0;
	//printf("--------------->1");
	h= select(sockfd +1, &t_set1, NULL, NULL, &tv);
	//printf("--------------->2\n");
	if (h < 0)
	{
        close(sockfd);
        printf("在读取数据报文时SELECT检测到异常，该异常导致线程终止！\n");
        return -1;
    };
	if (h > 0)
	{
        memset(buf, 0, 5000);
        i= read(sockfd, buf, 5000);
        if (i==0)
		{
            close(sockfd);
            printf("读取数据报文时发现远端关闭，该线程终止！\n");
            return -1;
        }
	   
	   	
		//printf("%s\n", buf);
	   	fp = fopen("./weather.txt","w+");
		fputs(buf,fp);
		fclose(fp);
		save(name2);
		remove("./weather.txt");
	}
	

	memset(str1, 0,4096);
	strcat(str1, parameter_head);
	strcat(str1, parameter3);
	strcat(str1, parameter_end);
	strcat(str1, domain_head);
	strcat(str1, domain);
	strcat(str1, domain_end);
	strcat(str1, "\r\n");

	
	ret = write(sockfd,str1,strlen(str1));
	if (ret < 0)
	{
        printf("发送失败！错误代码是%d，错误信息是'%s'\n",errno, strerror(errno));
        exit(0);
    }
	else
	{
        printf("消息发送成功，共发送了%d个字节！\n\n", ret);
    }
	FD_ZERO(&t_set1);
    FD_SET(sockfd, &t_set1);
	sleep(2);
	tv.tv_sec= 0;
	tv.tv_usec= 0;
	h= 0;
	//printf("--------------->1");
	h= select(sockfd +1, &t_set1, NULL, NULL, &tv);
	//printf("--------------->2\n");
	if (h < 0)
	{
        close(sockfd);
        printf("在读取数据报文时SELECT检测到异常，该异常导致线程终止！\n");
        return -1;
    };
	if (h > 0)
	{
        memset(buf, 0, 5000);
        i= read(sockfd, buf, 5000);
        if (i==0)
		{
            close(sockfd);
            printf("读取数据报文时发现远端关闭，该线程终止！\n");
            return -1;
        }
	   
	   	
		//printf("%s\n", buf);
	   	fp = fopen("./weather.txt","w+");
		fputs(buf,fp);
		fclose(fp);
		save(name3);
		remove("./weather.txt");
	}
	close(sockfd);
	
	
}

//mqtt callback mode 1
void my_connect_callback(struct mosquitto *mosq, void *obj, int rc)
{
	printf("Call the function: on_connect\n");
   	if(rc)
	{
		printf("on_connect error!\n");
        exit(1);
    }
	else
	{	
		//subscribe
        if(mosquitto_subscribe(mosq, NULL, "temperature", 2))
		{
            printf("Set the topic temperature error!\n");
			exit(1);
        }
        if(mosquitto_subscribe(mosq, NULL, "humidity", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "smoke", 2))
		{
            printf("Set the topic smoke error!\n");
			exit(1);
        }
		
    }

}

//mqtt callback mode2 
void my_connect_callback_mode2(struct mosquitto *mosq, void *obj, int rc)
{
	printf("Call the function: on_connect\n");
   	if(rc)
	{
		printf("on_connect error!\n");
        exit(1);
    }
	else
	{	
		//subscribe
		if(mosquitto_subscribe(mosq, NULL, "city", 2))
		{
            printf("Set the topic city error!\n");
			exit(1);
        }
		/*
		if(mosquitto_subscribe(mosq, NULL, "weather", 2))
		{
            printf("Set the topic city error!\n");
			exit(1);
        }
		*/
		if(mosquitto_subscribe(mosq, NULL, "fx", 2))
		{
            printf("Set the topic city error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "fl", 2))
		{
            printf("Set the topic city error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "air", 2))
		{
            printf("Set the topic city error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "hum", 2))
		{
            printf("Set the topic city error!\n");
			exit(1);
        }
		/*
		if(mosquitto_subscribe(mosq, NULL, "ymd", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "notice", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "wendu_h", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "wendu_l", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		*/
		if(mosquitto_subscribe(mosq, NULL, "wendu1", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "ymd1", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "type1", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "wendu2", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "ymd2", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "type2", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "wendu3", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "ymd3", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "type3", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "nowwendu", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "nowtype", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		/*
		if(mosquitto_subscribe(mosq, NULL, "flu", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		if(mosquitto_subscribe(mosq, NULL, "sport", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		*/
		if(mosquitto_subscribe(mosq, NULL, "uv", 2))
		{
            printf("Set the topic humidity error!\n");
			exit(1);
        }
		
    }

}

//mode 3
//mqtt callback mode 1
void my_connect_callback_mode3(struct mosquitto *mosq, void *obj, int rc)
{
	printf("Call the function: on_connect\n");
   	if(rc)
	{
		printf("on_connect error!\n");
        exit(1);
    }

}
void my_disconnect_callback(struct mosquitto *mosq, void *obj, int rc)
{
	printf("Call the function: my_disconnect_callback\n");
	//run = 0;
}

void my_subscribe_callback(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	printf("Call the function: on_subscribe\n");
}
void my_subscribe_callback_mode2(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	printf("Call the function: on_subscribe\n");
}

void mode2_show(void)
{
	wchar_t wcity[5],wwendu[5],wair[5],wfx[5],wfl[5],wtype[5],*wsunrise,*wsunset,wweek[10],wymd[20],wnotice[100],wyr[20],wwenduhl[10];
	wchar_t *wendu_flag = L"℃ ";
	wchar_t *maohao = L"：";
	wchar_t *notice_flag = L"Notice";
	wchar_t *air_flag = L"空气质量";
	wchar_t *weilai    = L"未来三日天气：";
	char yr[] = "08月02日";
	char wenduhl[] = "26~35℃";
	//printf("%c\n",ymd[5]);
	
	yr[0] = *(ymd1 + 5);
	yr[1] = *(ymd1 + 6);
	yr[5] = *(ymd1 + 8);
	yr[6] = *(ymd1 + 9);
	UTF8ToUnicode(yr,wyr,sizeof(wyr)/sizeof(wchar_t));
	
	wenduhl[0] = *(wendu1_l + 0);
	wenduhl[1] = *(wendu1_l + 1);
	wenduhl[3] = *(wendu1_h + 0);		
	wenduhl[4] = *(wendu1_h + 1);	
	UTF8ToUnicode(wenduhl,wwenduhl,sizeof(wwenduhl)/sizeof(wchar_t));

	UTF8ToUnicode(city,wcity,sizeof(wcity)/sizeof(wchar_t));
	UTF8ToUnicode(nowwendu,wwendu,sizeof(wwendu)/sizeof(wchar_t));
	UTF8ToUnicode(nowtype,wtype,sizeof(wtype)/sizeof(wchar_t));
	UTF8ToUnicode(notice,wnotice,sizeof(wnotice)/sizeof(wchar_t));
	UTF8ToUnicode(fx,wfx,sizeof(wfx)/sizeof(wchar_t));
	UTF8ToUnicode(fl,wfl,sizeof(wfl)/sizeof(wchar_t));
	UTF8ToUnicode(air,wair,sizeof(wair)/sizeof(wchar_t));
	UTF8ToUnicode(ymd,wymd,sizeof(wymd)/sizeof(wchar_t));

	show_bg(bg,pc_file_bg2);
	FT_Set_Pixel_Sizes(face, 55, 0);
	display_string(face, wcity, 80, 60,0xFF0000);
	FT_Set_Pixel_Sizes(face, 100, 0);
	display_string(face, wwendu, 120, 160,0xFFFFFF);
	display_string(face, wendu_flag, 220, 150,0xFFFFFF);
	FT_Set_Pixel_Sizes(face, 39, 0);
	display_string(face, wtype, 150, 350,0xFFFFFF);
	
	FT_Set_Pixel_Sizes(face, 40, 0);
	display_string(face, wfx, 100, 290,0xFFFFFF);
	display_string(face, wfl, 225, 290,0xFFFFFF);
	
	FT_Set_Pixel_Sizes(face, 42, 0);
	display_string(face, air_flag, 25, 420,0xFFFFFF);
	display_string(face, maohao, 200, 430,0xFFFFFF);
	display_string(face, wair, 220, 420,0xFFFFFF);
	
	FT_Set_Pixel_Sizes(face, 30, 0);
	display_string(face, notice_flag, 25, 510,0xFFFFFF);
	display_string(face, maohao, 200, 520,0xFFFFFF);
	display_string(face, wnotice, 220, 510,0xFFFFFF);
	
	FT_Set_Pixel_Sizes(face, 35, 0);
	display_string(face, weilai, 447, 150,0xFFFFFF);

	FT_Set_Pixel_Sizes(face, 55, 0);
	display_string(face, wymd, 447, 60,0xFFFFFF);
	//display_string(face, wweek, 750, 55,0xFFFFFF);
	
	FT_Set_Pixel_Sizes(face, 40, 0);
	display_string(face, wyr, 450, 220,0xFFFFFF);
	yr[0] = *(ymd2 + 5);
	yr[1] = *(ymd2 + 6);
	yr[5] = *(ymd2 + 8);
	yr[6] = *(ymd2 + 9);
	UTF8ToUnicode(yr,wyr,sizeof(wyr)/sizeof(wchar_t));
	display_string(face, wyr, 620, 220,0xFFFFFF);
	yr[0] = *(ymd3 + 5);
	yr[1] = *(ymd3 + 6);
	yr[5] = *(ymd3 + 8);
	yr[6] = *(ymd3 + 9);
	UTF8ToUnicode(yr,wyr,sizeof(wyr)/sizeof(wchar_t));
	display_string(face, wyr, 800, 220,0xFFFFFF);

	FT_Set_Pixel_Sizes(face, 25, 0);
	display_string(face, wwenduhl, 450, 300,0xFFFFFF);
	wenduhl[0] = *(wendu2_l + 0);
	wenduhl[1] = *(wendu2_l + 1);
	wenduhl[3] = *(wendu2_h + 0);		
	wenduhl[4] = *(wendu2_h + 1);	
	UTF8ToUnicode(wenduhl,wwenduhl,sizeof(wwenduhl)/sizeof(wchar_t));
	display_string(face, wwenduhl, 620, 300,0xFFFFFF);
	wenduhl[0] = *(wendu3_l + 0);
	wenduhl[1] = *(wendu3_l + 1);
	wenduhl[3] = *(wendu3_h + 0);		
	wenduhl[4] = *(wendu3_h + 1);	
	UTF8ToUnicode(wenduhl,wwenduhl,sizeof(wwenduhl)/sizeof(wchar_t));
	display_string(face, wwenduhl, 800, 300,0xFFFFFF);

}

void mode2_mqtt(char *payload)
{
	FILE *fp_mode2;
	static wchar_t wcity[5];
	fp_mode2 = fopen("2.txt","rw");
		if (fp_mode2 == NULL)
		{
			printf("open file error");
		}
		fwrite(payload,sizeof(payload),1,fp_mode2);
		printf("write finish\n");
		int i = 0;
		while (fgets(buf_mod2[i],1024,fp_mode2) != NULL)
		{
			
			if (i == 24)
			{
				break;
			}
			
			i++;
		}
		fclose(fp_mode2);
		city = buf_mod2[0];
		air  = buf_mod2[1];
		fl   = buf_mod2[2];
		fx	 = buf_mod2[3];
		notice = buf_mod2[4];
		wendu_h = buf_mod2[5];
		wendu_l = buf_mod2[6];
		nowwendu = buf_mod2[7];
		nowtype = buf_mod2[8];
		ymd = buf_mod2[9];
		ymd1 = buf_mod2[10];
		wendu1_h = buf_mod2[11];
		wendu1_l = buf_mod2[12];
		type1 = buf_mod2[13];
		ymd2 = buf_mod2[14];
		wendu2_h = buf_mod2[15];
		wendu2_l = buf_mod2[16];
		type2 = buf_mod2[17];
		ymd3 = buf_mod2[18];
		wendu3_h = buf_mod2[19];
		wendu3_l = buf_mod2[20];
		type3 = buf_mod2[21];
		flu = buf_mod2[22];
		sport = buf_mod2[23];
		uv = buf_mod2[24];
		
		printf("city: %s\n",city);
		printf("uv: %s\n",uv);
		
		show_bg(bg,pc_file_bg2);
		UTF8ToUnicode(city,wcity,sizeof(wcity)/sizeof(wchar_t));
		display_string(face,wcity,80,60,0xFF0000);

}
//mode 1 
void my_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
	//static int mode2_flag;

	char *city,*air,*fx,*fl,*sunrise,*sunset,*ymd,*notice,*wendu_h,*wendu_l;
    char *wendu1,*ymd1,*type1;
    char *wendu2,*ymd2,*type2;
    char *wendu3,*ymd3,*type3;
        
	char *nowwendu,*nowtype;
		
	char *flu,*sport,*uv;



	static char *tem,*hum,*smk;
	static wchar_t wtem[3],whum[3],wsmk[6];
	//static wchar_t wcity[5];
	
	static wchar_t wuv[5],wcity[5],wwendu[5],wair[5],wfx[5],wfl[5],wtype[5],*wsunrise,*wsunset,wweek[10],wymd[50],wnotice[100],wyr[20],wwenduhl[10];
	static wchar_t *wendu_flag = L"℃ ";
	static wchar_t *maohao = L"：";
	static wchar_t *notice_flag = L"Notice";
	static wchar_t *air_flag = L"空气质量";
	static wchar_t *weilai    = L"未来三日天气：";
	
	static char yr[] = "08月02日";
	static char wenduhl[] = "26~35℃";
	

	//printf("Call the function: on_message\n");
    //printf("Recieve a message of %s : %s\n", (char *)msg->topic, (char *)msg->payload);
	if(strcmp(msg->topic,"temperature") == 0)
	{
		temperature = msg->payload;
		printf("tempreature:%s\n",temperature);
		//if(strcmp(tem,temperature) != 0)
		//{
			tem = temperature;
			printf("tem:%s,temperature:%s\n",tem,temperature);
			UTF8ToUnicode(tem,wtem,sizeof(wtem)/sizeof(wchar_t));
			show_bg(bg,pc_file_bg1);
			FT_Set_Pixel_Sizes(face, 40, 0);
			display_string(face, wtem, 317, 152,0x020100);
			display_string(face, whum, 317, 309,0x020100);
			display_string(face, wsmk, 312, 470,0x020100);
		//}
	}
	if(strcmp(msg->topic,"humidity") == 0)
	{	
		humidity = msg->payload;
		printf("humidity:%s\n",humidity);
		//if(strcmp(hum,humidity) != 0)
		//{
			hum = humidity;
			printf("hum:%s,humidity:%s\n",hum,humidity);
			UTF8ToUnicode(hum,whum,sizeof(whum)/sizeof(wchar_t));
			show_bg(bg,pc_file_bg1);
			FT_Set_Pixel_Sizes(face, 40, 0);
			display_string(face, wtem, 317, 152,0x020100);
			display_string(face, whum, 317, 309,0x020100);
			display_string(face, wsmk, 312, 470,0x020100);
		//}
	}
	if(strcmp(msg->topic,"smoke") == 0)
	{	
		smoke = msg->payload;
		printf("smoke:%s\n",smoke);
		//if(strcmp(hum,humidity) != 0)
		//{
			smk = smoke;
			printf("smk:%s,smoke:%s\n",smk,smoke);
			UTF8ToUnicode(smk,wsmk,sizeof(wsmk)/sizeof(wchar_t));
			show_bg(bg,pc_file_bg1);
			FT_Set_Pixel_Sizes(face, 40, 0);
			display_string(face, wtem, 317, 152,0x020100);
			display_string(face, whum, 317, 309,0x020100);
			display_string(face, wsmk, 312, 470,0x020100);
		//}
	}

	
	
	if(strcmp(msg->topic,"city") == 0)
	{
		city = msg->payload;
		printf("city:%s\n",city);
		mode2_flag |=  (1<<0);
		UTF8ToUnicode(city,wcity,sizeof(wcity)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 55, 0);
		show_bg(bg,pc_file_bg2);
		display_string(face, wcity, 321, 55,0x595758);
		/*
		FT_Set_Pixel_Sizes(face, 42, 0);
		display_string(face, maohao, 200, 430,0);
		*/
		/*
		if (strcmp(city,msg->payload) != 0)
		{
			city = msg->payload;

			printf("city:%s\n",city);
		}
		*/
		
	}
	if(strcmp(msg->topic,"fl") == 0)
	{
		fl = msg->payload;
		strcat(fl,"级");
		printf("fl:%s\n",fl);
		mode2_flag |=  (1<<1);
		UTF8ToUnicode(fl,wfl,sizeof(wfl)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 35, 0);
		display_string(face, wfl, 350, 460,0x595758);

	}

	if(strcmp(msg->topic,"fx") == 0)
	{
		fx = msg->payload;
		printf("fx:%s\n",fx);
		mode2_flag |=  (1<<2);
		UTF8ToUnicode(fx,wfx,sizeof(wfx)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 35, 0);
		display_string(face, wfx, 273, 460,0x595758);

	}
	if(strcmp(msg->topic,"air") == 0)
	{
		air = msg->payload;
		printf("air:%s\n",air);
		mode2_flag |=  (1<<3);
		UTF8ToUnicode(air,wair,sizeof(wair)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 32, 0);
		display_string(face, wair, 410, 520,0x595758);
	}
	if(strcmp(msg->topic,"hum") == 0)
	{
		hum = msg->payload;
		printf("humidity:%s\n",hum);
		mode2_flag |=  (1<<4);
		UTF8ToUnicode(hum,whum,sizeof(wair)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 30, 0);
		display_string(face, whum, 344, 400,0x595758);

	}
	if(strcmp(msg->topic,"wendu1") == 0)
	{
		wendu1 = msg->payload;
		printf("wendu1:%s\n",wendu1);
		mode2_flag |=  (1<<5);

		wenduhl[0] = *(wendu1 + 0);
		wenduhl[1] = *(wendu1 + 1);
		wenduhl[3] = *(wendu1 + 3);		
		wenduhl[4] = *(wendu1 + 4);
		UTF8ToUnicode(wenduhl,wwenduhl,sizeof(wwenduhl)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 22, 0);
		display_string(face, wwenduhl, 550, 302,0);
		/*
		wenduhl[0] = *(wendu1_l + 0);
		wenduhl[1] = *(wendu1_l + 1);
		wenduhl[3] = *(wendu1_h + 0);		
		wenduhl[4] = *(wendu1_h + 1);	
		UTF8ToUnicode(wenduhl,wwenduhl,sizeof(wwenduhl)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 25, 0);
		display_string(face, wwenduhl, 450, 300,0xFFFFFF);
		*/

	}
	/*
	if(strcmp(msg->topic,"wendu1_l") == 0)
	{
		wendu1_l = msg->payload;
		printf("wendu1_l:%s\n",wendu1_l);
		mode2_flag |=  (1<<6);
	}
	*/
	if(strcmp(msg->topic,"ymd1") == 0)
	{
		ymd1 = msg->payload;
		printf("ymd1:%s\n",ymd1);
		mode2_flag |=  (1<<6);
		yr[0] = *(ymd1 + 0);
		yr[1] = *(ymd1 + 1);
		yr[5] = *(ymd1 + 3);
		yr[6] = *(ymd1 + 4);
		UTF8ToUnicode(yr,wymd,sizeof(wymd)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 27, 0);
		display_string(face, wymd, 531, 332,0x595758);
	}
	if(strcmp(msg->topic,"type1") == 0)
	{
		type1 = msg->payload;
		printf("type1:%s\n",type1);
		mode2_flag |=  (1<<7);
		//show_icon(90,75,152,107,sun);
		show_weather_icon(531,225,type1);
	}
	if(strcmp(msg->topic,"wendu2") == 0)
	{
		wendu2 = msg->payload;
		printf("wendu2_h:%s\n",wendu2_h);
		mode2_flag |=  (1<<8);
		wenduhl[0] = *(wendu2 + 0);
		wenduhl[1] = *(wendu2 + 1);
		wenduhl[3] = *(wendu2 + 3);		
		wenduhl[4] = *(wendu2 + 4);
		UTF8ToUnicode(wenduhl,wwenduhl,sizeof(wwenduhl)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 22, 0);
		display_string(face, wwenduhl, 781, 302,0);
	}
	/*
	if(strcmp(msg->topic,"wendu2_l") == 0)
	{
		wendu2_l = msg->payload;
		printf("wendu2_l:%s\n",wendu2_l);
		mode2_flag |=  (1<<10);
	}
	*/
	if(strcmp(msg->topic,"ymd2") == 0)
	{
		ymd2 = msg->payload;
		printf("ymd2:%s\n",ymd2);
		mode2_flag |=  (1<<9);
		yr[0] = *(ymd2 + 0);
		yr[1] = *(ymd2 + 1);
		yr[5] = *(ymd2 + 3);
		yr[6] = *(ymd2 + 4);
		UTF8ToUnicode(yr,wymd,sizeof(wymd)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 27, 0);
		display_string(face, wymd, 761, 332,0x595758);
	}
	if(strcmp(msg->topic,"type2") == 0)
	{
		type2 = msg->payload;
		printf("type2:%s\n",type2);
		mode2_flag |=  (1<<10);
		show_weather_icon(761,225,type2);
	}
	if(strcmp(msg->topic,"wendu3") == 0)
	{
		wendu3 = msg->payload;
		printf("wendu3_h:%s\n",wendu3_h);
		mode2_flag |=  (1<<11);
		wenduhl[0] = *(wendu3 + 0);
		wenduhl[1] = *(wendu3 + 1);
		wenduhl[3] = *(wendu3 + 3);		
		wenduhl[4] = *(wendu3 + 4);
		UTF8ToUnicode(wenduhl,wwenduhl,sizeof(wwenduhl)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 22, 0);
		display_string(face, wwenduhl, 660, 421,0);
	}
	/*
	if(strcmp(msg->topic,"wendu3_l") == 0)
	{
		wendu3_l = msg->payload;
		printf("wendu3_l:%s\n",wendu3_l);
		mode2_flag |=  (1<<14);
	}
	*/
	if(strcmp(msg->topic,"ymd3") == 0)
	{
		ymd3 = msg->payload;
		printf("ymd3:%s\n",ymd3);
		mode2_flag |=  (1<<12);
		yr[0] = *(ymd3 + 0);
		yr[1] = *(ymd3 + 1);
		yr[5] = *(ymd3 + 3);
		yr[6] = *(ymd3 + 4);
		UTF8ToUnicode(yr,wymd,sizeof(wymd)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 27, 0);
		display_string(face, wymd, 641, 451,0x595758);
	}
	if(strcmp(msg->topic,"type3") == 0)
	{
		type3 = msg->payload;
		printf("type3:%s\n",type3);
		mode2_flag |=  (1<<13);
		show_weather_icon(641,344,type3);
	}
	if(strcmp(msg->topic,"nowwendu") == 0)
	{
		nowwendu = msg->payload;
		printf("nowwendu:%s\n",nowwendu);
		mode2_flag |=  (1<<14);
		UTF8ToUnicode(nowwendu,wwendu,sizeof(wwendu)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 88, 0);
		display_string(face, wwendu, 219, 205,0x595758);

	}
	if(strcmp(msg->topic,"nowtype") == 0)
	{
		nowtype = msg->payload;
		printf("nowtype:%s\n",nowtype);
		mode2_flag |=  (1<<15);
		UTF8ToUnicode(nowtype,wtype,sizeof(wtype)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 40, 0);
		display_string(face, wtype, 327, 126,0x595758);
		show_weather_icon(204,104,nowtype);

	}
	if(strcmp(msg->topic,"uv") == 0)
	{
		uv = msg->payload;
		printf("uv:%s\n",uv);
		mode2_flag |=  (1<<16);
		printf("%d",mode2_flag);
		UTF8ToUnicode(uv,wuv,sizeof(wuv)/sizeof(wchar_t));
		FT_Set_Pixel_Sizes(face, 32, 0);
		display_string(face, wuv, 405, 337,0x595758);
	}
	if(mode2_flag == 0x00010000)
	{
		printf("receive 1 time all data\n");
		
		mode2_flag = 0;
		//close mqtt
		mosquitto_disconnect(mosq);
		mosquitto_loop_stop(mosq, false);
		//mosquitto_destroy(mosq);
    	//mosquitto_lib_cleanup();

		//while(mosquitto_loop_stop(mosq, false) !=0 );
		//while(mosquitto_disconnect(mosq) !=0);
		//mosquitto_destroy(mosq);
    	//while(mosquitto_lib_cleanup() !=0);

	}
    if(0 == strcmp(msg->payload, "quit"))
	{
        	mosquitto_disconnect(mosq);
    }

}
//mode 3
void my_message_callback_mode3(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
	char *people;
	if(strcmp(msg->topic,"people") == 0)
	{	
		people = msg->payload;
		if(strcmp(people,"1") == 0)
		{
			printf("receive people 1\n");
			while(yuv_publish(mosq) != 1);

		}
	}


}
//mode 2
void my_message_callback_mode2(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
	/*
	static wchar_t wcity[5],wwendu[5],wair[5],wfx[5],wfl[5],wtype[5],*wsunrise,*wsunset,wweek[10],wymd[20],wnotice[100],wyr[20],wwenduhl[10];
	static wchar_t *wendu_flag = L"℃ ";
	static wchar_t *maohao = L"：";
	static wchar_t *notice_flag = L"Notice";
	static wchar_t *air_flag = L"空气质量";
	static wchar_t *weilai    = L"未来三日天气：";
	
	static char yr[] = "08月02日";
	static char wenduhl[] = "26~35℃";
	*/
	
	
	if(strcmp(msg->topic,"city") == 0)
	{
		if (strcmp(city,msg->payload) != 0)
		{
			city = msg->payload;

			printf("city:%s\n",city);
		}
	}
	/*
	if(strcmp(msg->topic,"air") == 0)
	{
		if (strcmp(air,msg->payload) != 0)
		{
			air = msg->payload;
			printf("air:%s\n",air);
		}
	}

	if(strcmp(msg->topic,"fx") == 0)
	{
		if (strcmp(fx,msg->payload) != 0)
		{
			fx = msg->payload;
			printf("fx:%s\n",fx);
		}
	}

	if(strcmp(msg->topic,"fl") == 0)
	{
		if (strcmp(fl,msg->payload) != 0)
		{
			fl = msg->payload;
			printf("fl:%s\n",fl);
		}
	}

	if(strcmp(msg->topic,"ymd") == 0)
	{
		if (strcmp(ymd,msg->payload) != 0)
		{
			ymd = msg->payload;
			printf("ymd:%s\n",ymd);
		}
	}

	if(strcmp(msg->topic,"notice") == 0)
	{
		if (strcmp(notice,msg->payload) != 0)
		{
			notice = msg->payload;
			printf("notice:%s\n",notice);
		}
	}

	if(strcmp(msg->topic,"wendu_h") == 0)
	{
		if (strcmp(wendu_h,msg->payload) != 0)
		{
			wendu_h = msg->payload;
			printf("wendu_h:%s\n",wendu_h);
		}
	}

	if(strcmp(msg->topic,"wendu_l") == 0)
	{
		if (strcmp(wendu_l,msg->payload) != 0)
		{
			wendu_l = msg->payload;
			printf("wendu_l:%s\n",wendu_l);
		}
	}

	if(strcmp(msg->topic,"wendu1_h") == 0)
	{
		if (strcmp(wendu1_h,msg->payload) != 0)
		{
			wendu1_h = msg->payload;
			printf("wendu1_h:%s\n",wendu1_h);
		}
	}

	if(strcmp(msg->topic,"wendu1_l") == 0)
	{
		if (strcmp(wendu1_l,msg->payload) != 0)
		{
			wendu1_l = msg->payload;
			printf("wendu1_l:%s\n",wendu1_l);
		}
	}

	if(strcmp(msg->topic,"ymd1") == 0)
	{
		if (strcmp(ymd1,msg->payload) != 0)
		{
			ymd1 = msg->payload;
			printf("ymd1:%s\n",ymd1);
		}
	}

	if(strcmp(msg->topic,"type1") == 0)
	{
		if (strcmp(type1,msg->payload) != 0)
		{
			type1 = msg->payload;
			printf("city:%s\n",type1);
		}
	}

	if(strcmp(msg->topic,"wendu2_h") == 0)
	{
		if (strcmp(wendu2_h,msg->payload) != 0)
		{
			wendu2_h = msg->payload;
			printf("wendu2_h:%s\n",wendu2_h);
		}
	}

	if(strcmp(msg->topic,"wendu2_l") == 0)
	{
		if (strcmp(wendu2_l,msg->payload) != 0)
		{
			wendu2_l = msg->payload;
			printf("wendu2_l:%s\n",wendu2_l);
		}
	}

	if(strcmp(msg->topic,"ymd2") == 0)
	{
		if(strcmp(ymd2,msg->payload) != 0)
		{
			ymd2 = msg->payload;
			printf("ymd2:%s\n",ymd2);
		}
	}	

	if(strcmp(msg->topic,"type2") == 0)
	{
		if (strcmp(type2,msg->payload) != 0)
		{
			type2 = msg->payload;
			printf("type2:%s\n",type2);
		}
	}

	if(strcmp(msg->topic,"wendu3_h") == 0)
	{
		if (strcmp(wendu3_h,msg->payload) != 0)
		{
			wendu3_h = msg->payload;
			printf("wendu3_h:%s\n",wendu3_h);
		}
	}

	if(strcmp(msg->topic,"wendu3_l") == 0)
	{
		if (strcmp(wendu3_l,msg->payload) != 0)
		{
			wendu3_l = msg->payload;
			printf("wendu3_l:%s\n",wendu3_l);
		}
	}

	if(strcmp(msg->topic,"ymd3") == 0)
	{
		if(strcmp(ymd3,msg->payload) != 0)
		{
			ymd3 = msg->payload;
			printf("ymd3:%s\n",ymd3);
		}
	}

	if(strcmp(msg->topic,"type3") == 0)
	{
		if (strcmp(type3,msg->payload) != 0)
		{
			type3 = msg->payload;
			printf("type3:%s\n",type3);
		}
	}

	if(strcmp(msg->topic,"nowwendu") == 0)
	{
		if (strcmp(nowwendu,msg->payload) != 0)
		{
			nowwendu = msg->payload;
			printf("nowwendu:%s\n",nowwendu);
		}
	}

	if(strcmp(msg->topic,"nowtype") == 0)
	{
		if (strcmp(nowtype,msg->payload) != 0)
		{
			nowtype = msg->payload;
			printf("nowtype:%s\n",nowtype);
		}
	}

	if(strcmp(msg->topic,"flu") == 0)
	{
		if (strcmp(flu,msg->payload) != 0)
		{
			flu = msg->payload;
			printf("flu:%s\n",flu);
		}
	}

	if(strcmp(msg->topic,"sport") == 0)
	{
		if (strcmp(sport,msg->payload) != 0)
		{
			sport = msg->payload;
			printf("sport:%s\n",sport);
		}
	}

	if(strcmp(msg->topic,"uv") == 0)
	{
		if (strcmp(uv,msg->payload) != 0)
		{
			uv = msg->payload;
			printf("uv:%s\n",uv);
		}
	}
	*/

    if(0 == strcmp(msg->payload, "quit"))
	{
        	mosquitto_disconnect(mosq);
    }
	
}

wchar_t *to_wchar(wchar_t **ppDest, const char *pSrc)
{

    int len_char = 0;
	int len_wchar = 0;
    int ret = 0;
   // setlocale(LC_ALL,"zh_CN.utf8");
    len_char = strlen(pSrc)*sizeof(char);
	len_wchar = get_wchar_size(pSrc);
	
    
    /*
    len = strlen(pSrc) + 1;
    len = len*2;
    */
    if (len_char <= 1) 
	return *ppDest;

    *ppDest = (wchar_t*)malloc (len_wchar);


    ret = mbstowcs(*ppDest, pSrc,len_wchar);

    return *ppDest;
}

void *mode1_mqtt(void *arg)
{
	char *tem,*hum;
	wchar_t wtem[3],whum[3];
	tem = temperature;
	hum = humidity;	
	
	UTF8ToUnicode(tem,wtem,sizeof(wtem)/sizeof(wchar_t));
	UTF8ToUnicode(hum,whum,sizeof(whum)/sizeof(wchar_t));
	show_bg(bg,pc_file_bg1);
	display_string(face, wtem, 150, 150,0x8F8F8F);
	display_string(face, whum, 450, 150,0x8F8F8F);
	while(1)
	{
		ak_sleep_ms(1000);
		if(strcmp(tem,temperature) != 0)
		{
			tem = temperature;
			printf("tem:%s,temperature:%s\n",tem,temperature);
			UTF8ToUnicode(tem,wtem,sizeof(wtem)/sizeof(wchar_t));
			show_bg(bg,pc_file_bg1);
			display_string(face, wtem, 150, 150,0x8F8F8F);
			display_string(face, whum, 450, 150,0x8F8F8F);
		}
		if(strcmp(hum,humidity) != 0)
		{
			hum = humidity;
			printf("hum:%s,humidity:%s\n",hum,humidity);
			UTF8ToUnicode(hum,whum,sizeof(whum)/sizeof(wchar_t));
			show_bg(bg,pc_file_bg1);
			display_string(face, wtem, 150, 150,0x8F8F8F);
			display_string(face, whum, 450, 150,0x8F8F8F);
		}
		
		
	
	}






}
void *mode1_rev(void *arg)
{
    int recv_fd = *(int *)arg;
    char *p2;
    char tem1[2];
    char shidu[2];
    char gas;
    wchar_t *temperature_in;
    wchar_t *shidu_in;
    wchar_t *gas_in;
    msg = calloc(1,BUFSIZE);
    int cont = -1;
    while(1)
    {
        bzero(msg,BUFSIZE);
        cont = read(recv_fd,msg,BUFSIZE);
	//msg = "20a20a20";
	tem1[0] = msg[0];
	tem1[1] = msg[1];
	shidu[0] = msg[2];
	shidu[1] = msg[3];
	gas = msg[4];
	if(gas == 0)
		gas_in = L"正常";	
	else
		gas_in = L"异常";	
	   
	to_wchar(&temperature_in, tem1);
	to_wchar(&shidu_in, shidu);
	//to_wchar(&gas_in, gas);
	
	show_bg(bg,pc_file_bg1);
	display_string(face, temperature_in, 150, 150,0x8F8F8F);
	display_string(face, shidu_in, 450, 150,0x8F8F8F);
	display_string(face, gas_in, 750, 150,0x8F8F8F);
	ak_sleep_ms(3000);

    }
}

void mode_1(void)
{	
	char *light_on = "LED_ON";
	char *light_off = "LED_OFF";
	//char *open_curtain = "c";
	//char *close_curtain = "d";
	static int curtain_flag;
	int ret;
	show_bg(bg,pc_file_bg1);
	struct mosquitto *mosq;
	ret = mosquitto_lib_init();
    if(ret)
	{
		printf("Initialize lib error!\n");
        //return -1;
   	}
	mosq =  mosquitto_new("anyka",true, NULL);
	if(mosq == NULL)
	{        
		printf("New sub error!\n");
        mosquitto_lib_cleanup();
        //return -1;
    }
	
	//set callback function
    mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_disconnect_callback_set(mosq, my_disconnect_callback);
    mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);
    mosquitto_message_callback_set(mosq, my_message_callback);
	
	//connect to an MQTT broker
    ret = mosquitto_connect_async(mosq, HOST, MQTTPORT, KEEP_ALIVE);
	if(ret)
	{
		printf("Connect server error!\n");
		mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        //return -1;
    }
	//open mqtt thread
	ret = mosquitto_loop_start(mosq);
    if(ret)
	{
        printf("Start loop error!\n");
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        //return -1;
    }
    //start
    printf("Start!\n");
	
	ts_init();
	//pthread_t tid;
	//pthread_create(&tid,NULL,mode1_mqtt,NULL);
	
	/*
	//tty 
	recv_fd = open(DEV_TTY0, O_RDWR | O_NOCTTY);
	init_tty(recv_fd);
	pthread_t tid;
  	pthread_create(&tid,NULL,mode1_rev,(void *)&recv_fd);
	*/
	//display_string(face,wstr, 156, 159,0xdbd2d2);
	while(1)
	{
		get_pos(&x,&y);	
		if(x>810&&x<880&&y>180&&y<250)
		{
			close(ts_fd);		
			ao_pcm("/mnt/mmc/notice.pcm",channel_num_ao);
			mosquitto_publish(mosq,NULL,"SW_LED",strlen(light_on)+1,light_on,0,0);	
			//write(send_fd,light_on,1);
			ts_init();
			continue;

		}
		if(x>730&&x<800&&y>180&&y<250)
		{
			close(ts_fd);
			ao_pcm("/mnt/mmc/notice.pcm",channel_num_ao);	
			//write(send_fd,light_off,1);
			mosquitto_publish(mosq,NULL,"SW_LED",strlen(light_off)+1,light_off,0,0);	
			ts_init();
			continue;
		}
		if(x>735&&x<885&&y>372&&y<472)
		{
			close(ts_fd);
			if(curtain_flag)
			{
				ao_pcm("/mnt/mmc/close_curtain.pcm",channel_num_ao);
				show_icon(150,100,735,372,curtain_close);
				mosquitto_publish(mosq,NULL,"CURTAIN",strlen("OFF")+1,"OFF",0,0);	
				ts_init();
				curtain_flag = 0;
				continue;
			}
			ao_pcm("/mnt/mmc/open_curtain.pcm",channel_num_ao);	
			//write(send_fd,light_off,1);
			show_icon(150,100,735,372,curtain_open);
			mosquitto_publish(mosq,NULL,"CURTAIN",strlen("ON")+1,"ON",0,0);
			ts_init();
			curtain_flag = 1;

			continue;
		}
		/*
		if(x>780&&x<880&&y>360&&y<410)
		{
			close(ts_fd);
			ao_pcm("/mnt/mmc/close_curtain.pcm",channel_num_ao);	
			mosquitto_publish(mosq,NULL,"close_curtain",strlen(close_curtain)+1,light_off,0,2);
			//write(send_fd,light_off,1);
			ts_init();
			continue;
		}
		*/
		if(x>900&&x<990&&y>500&&y<580)
		{
			close(ts_fd);
			while(pcm_publish(mosq) != 1);
			ts_init();
			continue;
		}

		if(x>0&&x<100&&y>512&&y<580)
		{	
			//exit
			//close mqtt
			mosquitto_disconnect(mosq);
			mosquitto_loop_stop(mosq, false);
			mosquitto_destroy(mosq);
    		mosquitto_lib_cleanup();
			//close pthread
			//pthread_cancel(tid);
			//close ts
			close(ts_fd);
			//close tty
			//close(recv_fd);
			break;
		}
	
	}

	
	flag_return = 1;
	

}

void mode_2(void)
{
	int ret;
    struct mosquitto *mosq;
	//Initialize 
	mode2_flag = 0;
    ret = mosquitto_lib_init();
    if(ret)
	{
        printf("Init lib error!\n");
        //return -1;
    }

    //create a new pub instance
    mosq =  mosquitto_new("anyka", true, NULL);
    if(mosq == NULL)
    {
        printf("New pub_test error!\n");
        mosquitto_lib_cleanup();
        //return -1;
    }

    //set callback function
    mosquitto_connect_callback_set(mosq, my_connect_callback_mode2);
	mosquitto_disconnect_callback_set(mosq, my_disconnect_callback);
    mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);
    mosquitto_message_callback_set(mosq, my_message_callback);

    //connect to server
    ret = mosquitto_connect_async(mosq, HOST, MQTTPORT, KEEP_ALIVE);
    if(ret)
	{
        printf("Connect server error!\n");
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        //return -1;
    }

    int loop = mosquitto_loop_start(mosq); 
	if(loop != MOSQ_ERR_SUCCESS)
	{
	    printf("mosquitto loop error\n");
		//return 1;
	}

    //start
    printf("Start!\n");
	//show_bg(bg,pc_file_bg2);
















	/*
	show_bg(bg,pc_file_bg2);
	wchar_t wcity[5],wwendu[5],wair[5],wfx[5],wfl[5],wtype[5],*wsunrise,*wsunset,wweek[10],wymd[20],wnotice[100],wyr[20],wwenduhl[10];
	wchar_t *wendu_flag = L"℃ ";
	wchar_t *maohao = L"：";
	wchar_t *notice_flag = L"Notice";
	wchar_t *air_flag = L"空气质量";
	wchar_t *weilai    = L"未来三日天气：";
	char yr[] = "08月02日";
	char wenduhl[] = "26~35℃";

	
	yr[0] = *(ymd1 + 5);
	yr[1] = *(ymd1 + 6);
	yr[5] = *(ymd1 + 8);
	yr[6] = *(ymd1 + 9);
	UTF8ToUnicode(yr,wyr,sizeof(wyr)/sizeof(wchar_t));
	
	wenduhl[0] = *(wendu1_l + 0);
	wenduhl[1] = *(wendu1_l + 1);
	wenduhl[3] = *(wendu1_h + 0);		
	wenduhl[4] = *(wendu1_h + 1);	
	UTF8ToUnicode(wenduhl,wwenduhl,sizeof(wwenduhl)/sizeof(wchar_t));

	UTF8ToUnicode(city,wcity,sizeof(wcity)/sizeof(wchar_t));
	UTF8ToUnicode(nowwendu,wwendu,sizeof(wwendu)/sizeof(wchar_t));
	UTF8ToUnicode(nowtype,wtype,sizeof(wtype)/sizeof(wchar_t));
	UTF8ToUnicode(notice,wnotice,sizeof(wnotice)/sizeof(wchar_t));
	UTF8ToUnicode(fx,wfx,sizeof(wfx)/sizeof(wchar_t));
	UTF8ToUnicode(fl,wfl,sizeof(wfl)/sizeof(wchar_t));
	UTF8ToUnicode(air,wair,sizeof(wair)/sizeof(wchar_t));
	UTF8ToUnicode(ymd,wymd,sizeof(wymd)/sizeof(wchar_t));
	

	
	FT_Set_Pixel_Sizes(face, 55, 0);
	display_string(face, wcity, 80, 60,0xFF0000);
	FT_Set_Pixel_Sizes(face, 100, 0);
	display_string(face, wwendu, 120, 160,0xFFFFFF);
	display_string(face, wendu_flag, 220, 150,0xFFFFFF);
	FT_Set_Pixel_Sizes(face, 39, 0);
	display_string(face, wtype, 150, 350,0xFFFFFF);
	
	FT_Set_Pixel_Sizes(face, 40, 0);
	display_string(face, wfx, 100, 290,0xFFFFFF);
	display_string(face, wfl, 225, 290,0xFFFFFF);
	
	FT_Set_Pixel_Sizes(face, 42, 0);
	display_string(face, air_flag, 25, 420,0xFFFFFF);
	display_string(face, maohao, 200, 430,0xFFFFFF);
	display_string(face, wair, 220, 420,0xFFFFFF);
	
	FT_Set_Pixel_Sizes(face, 30, 0);
	display_string(face, notice_flag, 25, 510,0xFFFFFF);
	display_string(face, maohao, 200, 520,0xFFFFFF);
	display_string(face, wnotice, 220, 510,0xFFFFFF);
	
	FT_Set_Pixel_Sizes(face, 35, 0);
	display_string(face, weilai, 447, 150,0xFFFFFF);

	FT_Set_Pixel_Sizes(face, 55, 0);
	display_string(face, wymd, 447, 60,0xFFFFFF);
	//display_string(face, wweek, 750, 55,0xFFFFFF);
	
	FT_Set_Pixel_Sizes(face, 40, 0);
	display_string(face, wyr, 450, 220,0xFFFFFF);
	yr[0] = *(ymd2 + 5);
	yr[1] = *(ymd2 + 6);
	yr[5] = *(ymd2 + 8);
	yr[6] = *(ymd2 + 9);
	UTF8ToUnicode(yr,wyr,sizeof(wyr)/sizeof(wchar_t));
	display_string(face, wyr, 620, 220,0xFFFFFF);
	yr[0] = *(ymd3 + 5);
	yr[1] = *(ymd3 + 6);
	yr[5] = *(ymd3 + 8);
	yr[6] = *(ymd3 + 9);
	UTF8ToUnicode(yr,wyr,sizeof(wyr)/sizeof(wchar_t));
	display_string(face, wyr, 800, 220,0xFFFFFF);

	FT_Set_Pixel_Sizes(face, 25, 0);
	display_string(face, wwenduhl, 450, 300,0xFFFFFF);
	wenduhl[0] = *(wendu2_l + 0);
	wenduhl[1] = *(wendu2_l + 1);
	wenduhl[3] = *(wendu2_h + 0);		
	wenduhl[4] = *(wendu2_h + 1);	
	UTF8ToUnicode(wenduhl,wwenduhl,sizeof(wwenduhl)/sizeof(wchar_t));
	display_string(face, wwenduhl, 620, 300,0xFFFFFF);
	wenduhl[0] = *(wendu3_l + 0);
	wenduhl[1] = *(wendu3_l + 1);
	wenduhl[3] = *(wendu3_h + 0);		
	wenduhl[4] = *(wendu3_h + 1);	
	UTF8ToUnicode(wenduhl,wwenduhl,sizeof(wwenduhl)/sizeof(wchar_t));
	display_string(face, wwenduhl, 800, 300,0xFFFFFF);
	*/
	ts_init();
	//ak_mem_free(tem_20);
	/*
	free(tem_20);
	free(city);
	free(wind);
	free(weather);
	free(advice);
	free(sun);
	free(riqi);
	free(weilai);
	free(date3);
	free(tem_3);
	*/
	while(1)
	{
		get_pos(&x,&y);
		if(x>0&&x<100&&y>512&&y<580)
		{	
			printf("close");
			close(ts_fd);
			//close mqtt
			//mosquitto_disconnect(mosq);
			//mosquitto_loop_stop(mosq, false);
			mosquitto_destroy(mosq);
			mosquitto_lib_cleanup();
			break;
		}
	}

	flag_return = 1;

}

void mode_3(void)
{	
	int ret;
	int yuv_fd;
	int len;
	char buf3[Max_yuv_buf];
	struct mosquitto *mosq;
	ret = mosquitto_lib_init();
    if(ret)
	{
		printf("Initialize lib error!\n");
        //return -1;
   	}
	mosq =  mosquitto_new("anyka",true, NULL);
	if(mosq == NULL)
	{        
		printf("New sub error!\n");
        mosquitto_lib_cleanup();
        //return -1;
    }
	
	//set callback function
    mosquitto_connect_callback_set(mosq, my_connect_callback_mode3);
	mosquitto_disconnect_callback_set(mosq, my_disconnect_callback);
    mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);
    mosquitto_message_callback_set(mosq, my_message_callback_mode3);

	//connect to an MQTT broker
    ret = mosquitto_connect_async(mosq, HOST, MQTTPORT, KEEP_ALIVE);
	if(ret)
	{
		printf("Connect server error!\n");
		mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        //return -1;
    }
	//open mqtt thread
	ret = mosquitto_loop_start(mosq);
    if(ret)
	{
        printf("Start loop error!\n");
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        //return -1;
    }
    //start
    printf("Start!\n");

	show_bg(bg,pc_file_bg3);
	ts_init();
	while(1)
	{
		get_pos(&x,&y);	
		if(x>420&&x<630&&y>317&&y<376)
		{
			close(ts_fd);
			//memset(buf3,'\0',sizeof(buf3));
			//strcpy(buf3,"open");
			if(mosquitto_subscribe(mosq, NULL, "people", 2))
			{
            	printf("Set the topic humidity error!\n");
				exit(1);
       		}
			show_icon(200,250,164,172,jkb);
			/*
			mosquitto_publish(mosq,NULL,"image",strlen("open"),"open",0,0);
			show_icon(200,250,164,172,jkb);
			ao_pcm("/mnt/mmc/open_video.pcm",channel_num_ao);	
			while(vi("/mnt/") != 1);
			yuv_fd	= open("/mnt/dev0_640x360.yuv",O_RDONLY);
			if (yuv_fd == -1)
			{
				printf("open yuv error");
			}
			while ((len = read(yuv_fd,buf3,Max_yuv_buf)) > 0 )
			{
				mosquitto_publish(mosq,NULL,"image",sizeof(buf3)+1,buf3,2,0);

			};
			ret = close(yuv_fd);
			if(ret == -1)
			{
				printf("close pcm error");
			}

			sleep(1);
			//memset(buf3,'\0',sizeof(buf3));
			//strcpy(buf3,"close");
			mosquitto_publish(mosq,NULL,"image",strlen("close"),"close",2,0);	
			*/

			ts_init();
			continue;

		}
		if(x>689&&x<869&&y>317&&y<376)
		{
			close(ts_fd);
			ao_pcm("/mnt/mmc/close_video.pcm",channel_num_ao);
			mosquitto_unsubscribe(mosq,NULL,"people");
			show_icon(200,250,164,172,jka);	
			//vi("/mnt/");
			ts_init();
			continue;

		}	
		if(x>0&&x<100&&y>512&&y<580)
		{	
			close(ts_fd);
			mosquitto_disconnect(mosq);
			mosquitto_loop_stop(mosq, false);
			mosquitto_destroy(mosq);
    		mosquitto_lib_cleanup();
			break;
		}
	}

	flag_return = 1;


}

void mode_4(void)
{

	show_bg(bg,pc_file_bg4);
	ts_init();
	while(1)
	{
		get_pos(&x,&y);
		if(x>0&&x<100&&y>512&&y<580)
		{	
			close(ts_fd);
			break;
		}
	}

	flag_return = 1;


}
/*
void mode_4(void)
{
	int ret;
	int pcm_fd;
	int len;
	//FILE *fq = NULL;
	char buf3[Max_pcm_buf];
	struct mosquitto *mosq;
	ret = mosquitto_lib_init();
    if(ret)
	{
		printf("Initialize lib error!\n");
        //return -1;
   	}
	mosq =  mosquitto_new("anyka",true, NULL);
	if(mosq == NULL)
	{        
		printf("New sub error!\n");
        mosquitto_lib_cleanup();
        //return -1;
    }
	
	//set callback function
    mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_disconnect_callback_set(mosq, my_disconnect_callback);
    mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);
    mosquitto_message_callback_set(mosq, my_message_callback);
	
	//connect to an MQTT broker
    ret = mosquitto_connect_async(mosq, HOST, MQTTPORT, KEEP_ALIVE);
	if(ret)
	{
		printf("Connect server error!\n");
		mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        //return -1;
    }
	//open mqtt thread
	ret = mosquitto_loop_start(mosq);
    if(ret)
	{
        printf("Start loop error!\n");
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        //return -1;
    }
    //start
    printf("Start!\n");
	show_bg(bg,pc_file_bg4);
	ts_init();
	while(1)
	{
		get_pos(&x,&y);
		if(x>230&&x<440&&y>445&&y<530)
		{
			close(ts_fd);
			memset(buf3,'\0',sizeof(buf3));
			strcpy(buf3,"open");
			mosquitto_publish(mosq,NULL,"speech",strlen(buf3)+1,buf3,2,0);

			while(ao_pcm("/mnt/mmc/open_voice.pcm",channel_num_ao) !=1);	
			while(ai_pcm("/mnt/mmc/ui1/",channel_num_ai) !=1);

			pcm_fd = open("/mnt/mmc/ui1/1.pcm",O_RDONLY);
			if (pcm_fd == -1)
			{
				printf("open pcm error");
			}
			while ((len = read(pcm_fd,buf3,Max_pcm_buf)) > 0 )
			{
				mosquitto_publish(mosq,NULL,"speech",sizeof(buf3)+1,buf3,2,0);

			};
			ret = close(pcm_fd);
			if(ret == -1)
			{
				printf("close pcm error");
			}
			

			sleep(1);
			memset(buf3,'\0',sizeof(buf3));
			strcpy(buf3,"close");
			mosquitto_publish(mosq,NULL,"speech",strlen(buf3)+1,buf3,2,0);	
			//fclose(fq);
			//vi("/mnt/");
			ts_init();
			continue;

		}
		if(x>590&&x<790&&y>445&&y<530)
		{
			close(ts_fd);
			while(ao_pcm("/mnt/mmc/close_voice.pcm",channel_num_ao) !=1);	
			while(ao_pcm("/mnt/mmc/ui1/1.pcm",channel_num_ao) !=1);
			//vi("/mnt/");
			ts_init();
			continue;

		}		
			
		if(x>890&&x<980&&y>490&&y<570)
		{	
			close(ts_fd);
			mosquitto_disconnect(mosq);
			mosquitto_loop_stop(mosq, false);
			mosquitto_destroy(mosq);
    		mosquitto_lib_cleanup();
			break;
		}
	}

	flag_return = 1;


}
*/

int main(void)

{	
	//initilaize text 
	FT_Library    library;
	char *wday[] = {"星期日","星期一","星期二","星期三","星期四","星期五","星期六"};
	int error;
	FT_BBox bbox;
	char nyr[20];
	char buf_mode1[20];
	ts_init();
	wchar_t *wstr = L"25°C";
	wchar_t *dil = L":";
	wchar_t page0_date[20];
	wchar_t page0_week[10];
	wchar_t hour[2] ={0};
	wchar_t min[2] ={0};
	wchar_t sec[2] ={0};
	char hour1[4];
	char min1[4];
	char sec1[4];
	//printf("locale is : %s\n",setlocale(LC_ALL,"utf-8"));
	//open fb
	if( ak_gui_open( &fb_fix_screeninfo_param, &fb_var_screeninfo_param, &p_vaddr_fb) != AK_SUCCESS ) 
	{
        ak_print_error_ex(MODULE_ID_APP, "fb open error.\n" );
        return 0;
    }


	line_width  = fb_var_screeninfo_param.xres * fb_var_screeninfo_param.bits_per_pixel / 8;
    pixel_width = fb_var_screeninfo_param.bits_per_pixel / 8;
    screen_size = fb_var_screeninfo_param.xres * fb_var_screeninfo_param.yres * fb_var_screeninfo_param.bits_per_pixel / 8;
	printf("line_width: %d\n",line_width);
	printf("pixel_width: %d\n",pixel_width);
    fbmem = (unsigned char *)mmap(NULL , screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_gui, 0);
	error = FT_Init_FreeType( &library );              
   	error = FT_New_Face( library, "/mnt/mmc/text/fzyt.TTF", 0, &face );

	
	FT_Set_Pixel_Sizes(face, 50, 0);
	
	sdk_run_config config = {0};
    config.mem_trace_flag = SDK_RUN_NORMAL;
	config.isp_tool_server_flag = 0;
    ak_sdk_init( &config );
	
	time(&timep);
	p = localtime(&timep);
	printf("%d:%d:%d\n",p->tm_hour,p->tm_min,p->tm_sec);
	printf("month: %d\n",1+p->tm_mon);
	printf("year: %d\n",1900+p->tm_year);
	printf("day: %d\n",p->tm_mday);
	printf("week: %s\n",wday[p->tm_wday]);
	//weekday = wday[p->tm_wday];
	memset(nyr,0,20);
	sprintf(buf_mode1,"%d",1900+p->tm_year);
	//buf_mode1 = (char*)(1900+p->tm_year);
	printf("%s\n",buf_mode1);
	printf("1\n");
	//itoa(1900+p->tm_year,buf_mode1,10);
	strcat(nyr,buf_mode1);
	printf("1\n");
	strcat(nyr,"年");
	if(1+p->tm_mon < 10)
	{	
		strcat(nyr,"0");
	}
	
	sprintf(buf_mode1,"%d",1+p->tm_mon);
	
	//itoa(1+p->tm_mon,buf_mode1,10);
	strcat(nyr,buf_mode1);
	strcat(nyr,"月");
	if(p->tm_mday < 10)
	{	
		strcat(nyr,"0");
	}
	sprintf(buf_mode1,"%d",p->tm_mday);
	//itoa(p->tm_mday,buf_mode1,10);
	strcat(nyr,buf_mode1);
	strcat(nyr,"日");
	printf("nyr: %s\n",nyr);
	show_bg(bg,pc_file_bg);
	UTF8ToUnicode(nyr,page0_date,sizeof(page0_date)/sizeof(wchar_t));
	UTF8ToUnicode(wday[p->tm_wday],page0_week,sizeof(page0_week)/sizeof(wchar_t));
	display_string(face, page0_date, 80, 426,0x00FFFFFF);	//0x708090
	display_string(face, page0_week, 167, 490,0x00FFFFFF);	//0x8F8F8F
	//ai_pcm("/mnt/",channel_num_ai);
	//ao_pcm("/mnt/1.pcm",channel_num_ao);
	while(1)
	{
		if(flag_return ==1)
		{	

			show_bg(bg,pc_file_bg);
			FT_Set_Pixel_Sizes(face, 50, 0);
			display_string(face, page0_date, 80, 426,0x00FFFFFF);	//0x708090
			display_string(face, page0_week, 167, 490,0x00FFFFFF);	//0x8F8F8F
			ts_init();
			flag_return =0;
		}
		else
		{
			show_bg(bg,pc_file_bg);
			//show(sc,tg,bg,qing,pc_file_bg);
			//show(sc,tg,bg,duoyun,pc_file_bg);
			//show(sc,tg,bg,yujiaxue,pc_file_bg);
			/*
			time(&timep);
			p = localtime(&timep);
			sprintf(hour1,"%d",p->tm_hour);
			sprintf(min1,"%d",p->tm_min);
			sprintf(sec1,"%d",p->tm_sec);
			FT_Set_Pixel_Sizes(face, 50, 0);
			UTF8ToUnicode(hour1,hour,sizeof(hour)/sizeof(wchar_t));
			UTF8ToUnicode(min1,min,sizeof(min)/sizeof(wchar_t));
			UTF8ToUnicode(sec1,sec,sizeof(sec)/sizeof(wchar_t));
			display_string(face, hour, 480, 500,0xFFFFFF);
			display_string(face, dil, 530, 500,0xFFFFFF); //:
			display_string(face, min, 540, 500,0xFFFFFF);
			display_string(face, dil, 590, 500,0xFFFFFF); //:
			display_string(face, sec, 600, 500,0xFFFFFF);
			*/
			display_string(face, page0_date, 80, 426,0x00FFFFFF);	//0x708090
			display_string(face, page0_week, 167, 490,0x00FFFFFF);	//0x8F8F8F
			//show_icon(150,100,50,50,curtain_open);

		}
		get_pos(&x,&y);	
		printf("(%d,%d)\n",x,y);
		if(x>350&&x<650&&y>140&&y<215)
		{	//show(sc,tg,bg,pc_file_src,pc_file_bg);
			//show_bg(bg,pc_file_bg);
			close(ts_fd);
			mode_1();
			continue;
		}
		if(x>570&&x<880&&y>367&&y<440)
		{	
			close(ts_fd);
			mode_2();
			continue;
		}
		if(x>460&&x<770&y>250&&y<326)
		{	
			close(ts_fd);
			mode_3();
			continue;
		}
		if(x>680&&x<990&&y>480&&y<556)
		{	
			close(ts_fd);
			mode_4();
			continue;
		}
		}				

	
	return 0;
	
}
