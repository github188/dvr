#ifndef __dvr_h__
#define __dvr_h__

// #define DBG

// standard headers
#include <time.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <fnmatch.h>
#include <termios.h>
#include <stdarg.h>

#include <sys/vfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/times.h>

#include "../cfg.h"

// memory allocation
void *mem_alloc(int size);
void mem_free(void *pmem);
void *mem_addref(void *pmem);
int mem_refcount(void *pmem);
int mem_check(void *pmem);
int mem_size(void * pmem);
void mem_cpy32(void *dest, const void *src, size_t count);
int mem_available();
void mem_init();
void mem_uninit();

#include "crypt.h"
#include "genclass.h"
#include "cfg.h"

#define DVRVERSION0		(2)
#define	DVRVERSION1		(0)
#define DVRVERSION2		(8)
#define	DVRVERSION3		(521)

// used types
typedef unsigned long int DWORD;
typedef unsigned short int WORD;
typedef unsigned char BYTE;

#define MAXCHANNEL		16

// application variables, functions

#define APPQUIT 	(0)
#define APPUP   	(1)
#define APPDOWN 	(2)
#define APPRESTART	(3)

extern pthread_mutex_t mutex_init ;
extern char dvrconfigfile[];
extern int app_state;
extern int system_shutdown;
extern int g_lowmemory;
extern char g_hostname[] ;
extern unsigned char g_filekey[] ;

int  dvr_log(char *str, ...);
void dvr_lock();
void dvr_unlock();

// lock variable
void dvr_lock( void * lockvar, int delayus=10000 ) ;
void dvr_unlock( void * lockvar );

unsigned dvr_random();

// recording support
class rec_index {
protected:
	struct rec_index_item {
		int onoff ;
		int onofftime ;
	} * idx_array ;
	array <struct rec_index_item> m_array ;

public:
	rec_index(){};
	~rec_index(){};
	void empty(){
		m_array.empty();
	}
	void addopen(int opentime);
	void addclose(int closetime);
	void savefile( char * filename );
	void readfile( char * filename );
	int getsize() { 
		return m_array.size(); 
	}
	int getidx( int n, int * ponoff, int * ptime ) {
		if( n<0 || n>=m_array.size() )
			return 0;
		*ponoff=m_array[n].onoff ;
		*ptime=m_array[n].onofftime ;
		return 1 ;
	}
};

// time service
struct dvrtime {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int milliseconds;
	int tz;
};

// dvr .264 file structure
// .264 file struct
struct hd_file {
	DWORD flag;					//      "4HKH", 0x484b4834
	DWORD res1[6];
	DWORD width_height;			// lower word: width, high word: height
	DWORD res2[2];
};

struct hd_frame {
	DWORD flag;					// 1
	DWORD serial;
	DWORD timestamp;
	DWORD res1;
	DWORD d_frames;				// sub frames number, lower 8 bits
	DWORD width_height;			// lower word: width, higher word: height
	DWORD res3[6];
	DWORD d_type;				// lower 8 bits; 3  keyframe, 1, audio, 4: B frame
	DWORD res5[3];
	DWORD framesize;
};

#define HD264_FRAMEWIDTH(hd)  	  ( ((hd).width_height) & 0x0ffff)
#define HD264_FRAMEHEIGHT(hd)     ( (((hd).width_height)>>16)&0x0ffff)
#define HD264_FRAMESUBFRAME(hd)   ( ((hd).d_frames)&0x0ff)
#define HD264_FRAMETYPE(hd)       ( ((hd).d_type)&0x0ff)

struct hd_subframe {
	DWORD d_flag ;				// lower 16 bits as flag, audio: 0x1001  B frame: 0x1005
	DWORD res1[3];
	DWORD framesize;			// 0x50 for audio
};

#define HD264_SUBFLAG(subhd)      ( ((subhd).d_flag)&0x0ffff )

#define FRAMETYPE_UNKNOWN	(0)
#define FRAMETYPE_KEYVIDEO	(1)
#define FRAMETYPE_VIDEO		(2)
#define FRAMETYPE_AUDIO		(3)
#define FRAMETYPE_JPEG		(4)
#define FRAMETYPE_264FILEHEADER	(10)

struct cap_frame {
	int channel;
	int framesize;
	int frametype;
//    struct dvrtime frametime ;      // only available on key frames
	char *framedata;
};

extern char Dvr264Header[];

/*
	key file format
		1st_frame_milliseconds, 1st_frame_offset\n
		2nd_frame_milliseconds, 2nd_frame_offset\n
		...
		Nth_frame_milliseconds, Nth_frame_offset\n
*/
// key frame index support
struct dvr_key_t {
    int ktime;				// key frame time, in milliseconds, from start of file(file time in filename)
	int koffset ;			// key frame offset
} ;

// dvrfile
class dvrfile {
  protected:
	FILE * m_handle;
	int	   m_fileencrypt ;		// if file is encrypted?
	int    m_initialsize;
    char * m_filebuf ;

	string m_filename ;
	int    m_openmode ;			// 0: read, 1: write

    struct dvrtime m_filetime ;	// file start time
    int   m_filesize ;
	int   m_filelen ;			// file length in seconds
    DWORD m_filestamp ;			// first frame time stamp
	int	  m_filestart ;			// first frame offset
        
    int   m_syncsize ;

    int   m_framepos ;
    int   m_framesize ;
    int   m_frametime ;
    int   m_frametype ;
    int   m_frame_kindex ;
        
    // current frame info
	DWORD m_timestamp ;			// current time stamp ;
    DWORD m_fileheader ;			// header flag
        
	array <struct dvr_key_t> m_keyarray ;

    int nextframe();
	int prevframe();

  public:
	 dvrfile() ;
	~dvrfile();
	int open(const char *filename, char *mode = "r", int initialsize = 0, int encrypt=0);
	void close();
    int writeheader(void * buffer, size_t headersize) ;
    int readheader(void * buffer, size_t headersize) ;
	int read(void *buffer, size_t buffersize);
	int write(void *buffer, size_t buffersize);
	int repair();							// repair a .264 file
    int repairpartiallock() ;               // repair a partial locked file
	int tell();
	int seek(int pos, int from = SEEK_SET);
    int truncate( int tsize );
	// seek to specified time,
	//   return 1 if seek inside the file.
	//	return 0 if seek outside the file, pointer set to begin or end of file
	int seek( dvrtime * seekto );
    int frameinfo();                // read frame info
        
	dvrtime frametime();			// return current frame time
	int frametype();                // return current frame type
	int framesize();                // return current frame size
        
	int prevkeyframe();
	int nextkeyframe();
	int readframe(struct cap_frame *pframe);
	int readframe(void * framebuf, size_t bufsize);
	int writeframe(void * buffer, size_t size, int keyframe, dvrtime * frametime);
	int isopen() {
		return m_handle != NULL;
	}
    int isencrypt() {
        return m_fileencrypt ;
    }
	void writekey();
	void readkey() ;
	int isframeencrypt() {
	  return m_fileencrypt;
	}
	DWORD getfileheader() ;

	static int rename(const char * oldfilename, const char * newfilename); // rename .264 filename as well .k file
	static int remove(const char * filename);		// remove .264 file as well .k file
};

void file_sync();
void file_init();
void file_uninit();

// capture
// capture card structure

struct  DvrChannel_attr {
    int         Enable ;
    char        CameraName[64] ;
    int         Resolution;				// 0: CIF, 1: 2CIF, 2:DCIF, 3:4CIF, 4:QCIF
    int         RecMode;				// 0: Continue, 1: Trigger by sensor, 2: triger by motion, 3, trigger by sensor/motion, 4: Schedule
    int         PictureQuality;			// 0: lowest, 10: Highest
    int         FrameRate;
    int         Sensor[4] ;
    int         SensorEn[4] ;
    int         SensorOSD[4] ;
    int         PreRecordTime ;			// pre-recording time in seconds
    int         PostRecordTime ;		// post-recording time in seconds
    int         RecordAlarm ;			// Recording Signal
    int         RecordAlarmEn ;
    int         RecordAlarmPat ;		// 0: OFF, 1: ON, 2: 0.5s Flash, 3: 2s Flash, 4, 4s Flash
    int         VideoLostAlarm ;		// Video signal lost alarm
    int         VideoLostAlarmEn ;
    int         VideoLostAlarmPat ;
    int         MotionAlarm ;			// Motion Detect alarm
    int         MotionAlarmEn ;
    int         MotionAlarmPat ;
    int         MotionSensitivity;		
    int         GPS_enable ;			// enable gps options
    int         ShowGPS ;
    int         GPSUnit ;				// GPS unit display, 0: English, 1: Metric
    int         BitrateEn ;				// Enable Bitrate Control
    int         BitMode;				// 0: VBR, 1: CBR
    int         Bitrate;
    int         ptz_addr ;
    int         structsize;             // structure size
    int         brightness;             // 1-9, 0: use default value
    int         contrast;               // 1-9, 0: use default value
    int         saturation;             // 1-9, 0: use default value
    int         hue;                    // 1-9, 0: use default value
    int         key_interval ;          // key frame interval, default 100
    int         b_frames ;              // b frames number, default 2
    int         p_frames ;              // p frames number, not used
    int         disableaudio ;          // 1: no audio
    int         showgpslocation ;       // show gps location on osd
    int         showpeak ;              // show gforce peak on osd
} ;

extern int cap_channels;

void cap_start();
void cap_stop();
void cap_restart(int channel);
void cap_init();
void cap_uninit();

#define CAP_TYPE_UNKNOWN	(-1)
#define CAP_TYPE_HIKLOCAL	(0)
#define CAP_TYPE_HIKIP		(1)

struct hik_osd_type {
    int brightness ;
    int translucent ;
    int twinkle ;
    int lines ;
    WORD osdline[8][128] ;
} ;

class capture {
  protected:
    struct  DvrChannel_attr	m_attr ;	// channel attribute
	int m_type ;				// channel type, 0: local capture card, 1: eagle ip camera
	int m_channel;				// channel number
  	int m_enable ;				// 1: enable, 0:disable
	int m_motion ;				// motion status ;
	int m_signal ;				// signal ok.
	int m_oldsignal ;			// signal ok.
    unsigned long m_streambytes; // total stream bytes.

    int m_started;
    int m_working ;             // channel is working
    int m_remoteosd ;           // get OSD from network
        
    unsigned int m_sensorosd ;      // bit maps for sensor osd
        
    int m_motionalarm ;
    int m_motionalarm_pattern ;
    int m_signallostalarm ;
    int m_signallostalarm_pattern ;

  public:
	capture(int channel) ;
    virtual ~capture(){};

	void loadconfig() ;
    void getattr(struct DvrChannel_attr * pattr) {
        memcpy( pattr, &m_attr, sizeof(m_attr) );
    }
    void setattr(struct DvrChannel_attr * pattr) {
        if( pattr->structsize == sizeof(struct DvrChannel_attr) ) {
            memcpy(&m_attr, pattr, sizeof(m_attr) );
        }
    }
	int getchannel() {
		return m_channel;
	}
	int enabled(){
		return m_enable;
	}
    int isworking(){
      if (m_started)
        return --m_working>0 ;
      else
	return 1;
    }
	int type(){
		return m_type ;
	}
    unsigned long streambytes() {
        return m_streambytes ;
    }
    void onframe(cap_frame * pcapframe);	// send frames to network or to recording
	char * getname(){ 
		return m_attr.CameraName ;
	}
	int getptzaddr() {
		return m_attr.ptz_addr;
	}

	virtual void update(int updosd);	// periodic update procedure, updosd: require to update OSD

    void setremoteosd() { m_remoteosd=1; } 
	void updateOSD ();          		// to update OSD text
   	virtual void setosd( struct hik_osd_type * posd )=0;

	void alarm();                   	// update alarm relate to capture channel
	virtual void start(){};				// start capture
	virtual void stop(){};				// stop capture
	virtual void restart(){				// restart capture
		stop();
		if( m_enable ) {
			start();
		}
	}
    // to capture one jpeg frame
    virtual int captureJPEG(int quality, int pic)
    {
        return 0 ;                     // not supported
    }
    virtual void docaptureJPEG()
    {
    }
    virtual int getsignal(){return m_signal;}	// get signal available status, 1:ok,0:signal lost
    virtual int getmotion(){return m_motion;}	// get motion detection status
};

extern capture ** cap_channel;


int  eagle32_init();
void eagle32_uninit();
// jpeg capture
void eagle_captureJPEG();

class eagle_capture : public capture {
  protected:
 
	int m_state;				// capture state: 0=stop, 1=running. 2=req_stop
//	pthread_t capthread;

	// Hik Eagle32 parameter
	int m_hikhandle;				// hikhandle = hikchannel+1
	int m_chantype ;
	int m_dspdatecounter ;  

	int m_headerlen ;
  	char m_header[256] ;
	char m_motiondata[256] ;
	int m_motionupd;				// motion status changed ;
    // jpeg capture 
    int m_jpeg_mode;            // -1: no capture, >=0 : capture resolution
    int m_jpeg_quality ;        // capture quality


  public:
	eagle_capture(int channel, int hikchannel);
    ~eagle_capture();

	int gethandle(){
		return m_hikhandle;
	}
	
	int state() {
		return m_state;
	};

	void streamcallback( void * buf, int size, int frame_type);

	int getresolution(){
		return m_attr.Resolution;
	}
   	virtual void setosd( struct hik_osd_type * posd );

	// virtual function implements
	virtual void update(int updosd);	// periodic update procedure, updosd: require to update OSD
	virtual void start();
	virtual void stop();
    // to capture one jpeg frame
    virtual int captureJPEG(int quality, int pic) ;
    virtual void docaptureJPEG() ;
    virtual int getsignal();        // get signal available status, 1:ok,0:signal lost
};

class ipeagle32_capture : public capture {
protected:
	int m_sockfd ;		//  control socket for ip camera
	int m_streamfd ;	//  stream socket for ip camera
	string m_ip ;		//  remote camera ip address
	int m_port ;		//  remote camera port
	int m_ipchannel ; 	//  remote camera channel
	struct hd_file m_hd264 ;	// .264 file header
	pthread_t m_streamthreadid ;	// streaming thread id
	int m_state;		//  thread running state, 0: stop, 1: running, 2: started but not connected.
	int m_updcounter ;
	struct cap_frame m_jpegframe;
public:
	ipeagle32_capture(int channel);
    virtual ~ipeagle32_capture();

	void streamthread();
	int  connect();

// virtual functions
	virtual void update(int updosd);	// periodic update procedure, updosd: require to update OSD
	virtual void start();				// start capture
	virtual void stop();				// stop capture
   	virtual void setosd( struct hik_osd_type * posd );
	virtual int captureJPEG(int quality, int pic) ;
	virtual void docaptureJPEG() ;
} ;

// ptz service
void ptz_init();
void ptz_uninit();
int ptz_msg( int channel, DWORD command, int param );

struct dayinfoitem {
	int ontime  ;		// seconds of the day
	int offtime ;		// seconds of the day
} ;
	
void time_init();
void time_uninit();
void time_settimezone(char * timezone);
int  time_gettimezone(char * timezone);
void time_inittimezone();

// struct used for synchronize dvr time. (sync time with ip camera)
struct dvrtimesync {
	unsigned int tag ;
	struct dvrtime dvrt ;
	char tz[128] ;
} ;

time_t time_now(struct dvrtime *dvrt);
time_t time_utctime(struct dvrtime *dvrt);
int time_setlocaltime(struct dvrtime *dvrt);
int time_setutctime(struct dvrtime *dvrt);
int time_adjtime(struct dvrtime *dvrt);

time_t time_dvrtime_init( struct dvrtime *dvrt,
						int year=2000,
						int month=1, 
						int day=1,
						int hour=0,
						int minute=0,
						int second=0,
						int milliseconds=0 );
time_t time_dvrtime_zerohour( struct dvrtime *dvrt );
time_t time_dvrtime_nextday( struct dvrtime *dvrt );

int  time_dvrtime_diff( struct dvrtime *t1, struct dvrtime *t2) ;	// difference in seconds
int  time_dvrtime_diffms( struct dvrtime *t1, struct dvrtime *t2 ); // difference in milliSecond
void  time_dvrtime_add( struct dvrtime *t, int seconds);				// add seconds to t
void  time_dvrtime_addms( struct dvrtime *t, int ms);				// add milliseconds to t

// convert between dvrtime and time_t
struct dvrtime * time_dvrtimelocal( time_t t, struct dvrtime * dvrt);
struct dvrtime * time_dvrtimeutc( time_t t, struct dvrtime * dvrt);
time_t time_timelocal( struct dvrtime * dvrt);
time_t time_timeutc( struct dvrtime * dvrt);
// get a replace hik card time stamp
DWORD time_hiktimestamp() ;

// appliction up time in seconds
int time_uptime();

int time_readrtc(struct dvrtime * dvrt);
int time_setrtc();

inline int operator > ( struct dvrtime & t1, struct dvrtime & t2 )
{
    if(      t1.year   != t2.year )   return (t1.year   > t2.year);
    else if( t1.month  != t2.month )  return (t1.month  > t2.month);
    else if( t1.day    != t2.day )    return (t1.day    > t2.day);
    else if( t1.hour   != t2.hour )   return (t1.hour   > t2.hour);
    else if( t1.minute != t2.minute ) return (t1.minute > t2.minute);
    else if( t1.second != t2.second ) return (t1.second > t2.second);
    else return (t1.milliseconds > t2.milliseconds);
}

inline int operator < ( struct dvrtime & t1, struct dvrtime & t2 )
{
    if(      t1.year   != t2.year )   return (t1.year   < t2.year);
    else if( t1.month  != t2.month )  return (t1.month  < t2.month);
    else if( t1.day    != t2.day )    return (t1.day    < t2.day);
    else if( t1.hour   != t2.hour )   return (t1.hour   < t2.hour);
    else if( t1.minute != t2.minute ) return (t1.minute < t2.minute);
    else if( t1.second != t2.second ) return (t1.second < t2.second);
    else return (t1.milliseconds > t2.milliseconds);
}

inline int operator == ( struct dvrtime & t1, struct dvrtime & t2 )
{
    return (t1.year         == t2.year     &&
            t1.month        == t2.month    &&
            t1.day          == t2.day      &&
            t1.hour         == t2.hour     &&
            t1.minute       == t2.minute   &&
            t1.second       == t2.second   &&
            t1.milliseconds == t2.milliseconds );
    
}

inline int operator != (struct dvrtime & t1, struct dvrtime & t2 )
{
    return (t1.year         != t2.year   ||
            t1.month        != t2.month  ||
            t1.day          != t2.day    ||
            t1.hour         != t2.hour   ||
            t1.minute       != t2.minute ||
            t1.second       != t2.second ||
            t1.milliseconds != t2.milliseconds );
};

struct dvrtime operator + ( struct dvrtime & t, int seconds ) ;
struct dvrtime operator + ( struct dvrtime & t, double seconds ) ;
double operator - ( struct dvrtime &t1, struct dvrtime &t2 ) ;

// recording service
#define DEFMAXFILESIZE (100000000)
#define DEFMAXFILETIME (24*60*60)

extern string rec_basedir;
extern int rec_busy ;
extern int rec_pause;           // pause recording temperary
extern int rec_watchdog ;      // recording watchdog
void rec_init();
void rec_uninit();
void rec_onframe(struct cap_frame *pframe);
void rec_record(int channel);
void rec_prerecord(int channel);
void rec_postrecord(int channel);
void rec_lockstart(int channel);
void rec_lockstop(int channel);
int  rec_state(int channel);
void rec_lock(time_t locktime);
void rec_unlock();
void rec_break();
void rec_update();
void rec_alarm();
void rec_start();
void rec_stop();
struct nfileinfo {
    int channel;
    struct dvrtime filetime ;
    int filelength ;
    char extension[4] ;                         // ".k" or ".264"
    int  filesize ;                             // option, can be 0
} ;
FILE * rec_opennfile(int channel, struct nfileinfo * nfi);

// disk and .264 file directories
// get .264 file list by day and channel
extern struct dvrtime disk_earliesttime ;
int f264length(const char *filename);				// get file length from file name
int f264locklength(const char *filename);			// get file lock length from file name
int f264time(const char *filename, struct dvrtime *dvrt);
char *basefilename(const char *fullpath);

class f264name : public string {
public:
        int operator < ( f264name & s2 ) {
            struct dvrtime t1, t2 ;
            f264time( getstring(), &t1 );
            f264time( s2.getstring(), &t2 );
            return t1<t2 ;
        }
        f264name & operator =(const char *str) {
            setstring(str);
            return *this;
        }
};

void disk_listday(array <f264name> & list, struct dvrtime * day, int channel);
void disk_getdaylist(array <int> & daylist);
int disk_unlockfile( dvrtime * begin, dvrtime * end );
void disk_check();
int disk_renew(char * newfilename);
void disk_init(int);
void disk_uninit(int);
int do_disk_check();

// live video service
struct net_fifo {
    struct net_fifo *next;
    char *buf;
    int bufsize;
    int loc;
};

class live {
    protected:
        int m_channel;
        net_fifo * m_fifo ;
        void * m_framebuf ;
        int    m_framesize;
        
    public:
        live( int channel );
        ~live();
        void getstreamdata(void ** getbuf, int * getsize, int * frametype);
        void onframe(cap_frame * pframe);
};


// playback service

void play_init();
void play_uninit();

class playback {
    protected:
        int m_channel;
        
        array <int> m_daylist ;			// available data day list
        int m_day ;						// current day, in bcd format
        array <f264name> m_filelist ;	// day file list
        int m_curfile ;					// current opened file, index of m_filelist
        
        dvrfile m_file ;				// current opened file, always opened
        struct dvrtime m_streamtime ;	// current frame time
        
        // pre-read buffer
        void * m_framebuf ;				// pre-read buffer
        int  m_framesize ;
        int  m_frametype ;
        
        int opennextfile();
        int close();
        void readframe();
        
    public:
        playback( int channel ) ;
        ~playback();
        int seek( struct dvrtime * seekto );
        void getstreamdata(void ** getbuf, int * getsize, int * frametype);
        void preread();
        int getstreamtime(dvrtime * dvrt) {
            *dvrt=m_streamtime ;
            return 1;
        }
        void getdayinfo(array <struct dayinfoitem> &dayinfo, struct dvrtime * pday);
        void getlockinfo(array <struct dayinfoitem> &dayinfo, struct dvrtime * pday);
        DWORD getmonthinfo(dvrtime * month);				// return day info in bits, bit 0 as day 1
        int  getdaylist( int * * daylist ) ;
        DWORD getcurrentfileheader();
};

// network service

#define DVRPORT 15111
#define REQDVREX	0x7986a348
#define REQDVRTIME	0x7986a349
#define DVRSVREX	0x95349b63
#define DVRSVRTIME	0x95349b64
#define REQSHUTDOWN	0x5d26f7a3
#define REQMSG		0x5373cb4a
#define DVRMSG		0x774f9a31

struct sockad {
    struct sockaddr addr;
    char padding[128];
    socklen_t addrlen;
};

// maximum live fifo size
extern int net_livefifosize;
// network active
extern int net_active ;		
// trigger a new network wait cycle
void net_trigger();
int net_sendok(int fd, int tout);
int net_recvok(int fd, int tout);
void net_message();
int net_listen(int port, int socktype);
int net_connect(char *ip, int port);
void net_clean(int sockfd);
int net_send(int sockfd, void * data, int datasize);
int net_recv(int sockfd, void * data, int datasize);
int net_onframe(cap_frame * pframe);
// initialize network
void net_init();
void net_uninit();


// dvr net service
struct dvr_req {
    int reqcode;
    int data;
    int reqsize;
};

struct dvr_ans {
    int anscode;
    int data;
    int anssize;
};

struct channel_fifo {
    int channel;
};

enum reqcode_type { REQOK =	1, 
    REQREALTIME, REQREALTIMEDATA, 
    REQCHANNELINFO, REQCHANNELDATA,
    REQSWITCHINFO, REQSHAREINFO, 
    REQPLAYBACKINFO, REQPLAYBACK,
    REQSERVERNAME,
    REQAUTH, REQGETSYSTEMSETUP, 
    REQSETSYSTEMSETUP, REQGETCHANNELSETUP,
    REQSETCHANNELSETUP, REQKILL, 
    REQSETUPLOAD, REQGETFILE, REQPUTFILE,
    REQHOSTNAME, REQRUN,
    REQRENAME, REQDIR, REQFILE, 
    REQGETCHANNELSTATE, REQSETTIME, REQGETTIME,
    REQSETLOCALTIME, REQGETLOCALTIME, 
    REQSETSYSTEMTIME, REQGETSYSTEMTIME,
    REQSETTIMEZONE, REQGETTIMEZONE,
    REQFILECREATE, REQFILEREAD, 
    REQFILEWRITE, REQFILESETPOINTER, REQFILECLOSE,
    REQFILESETEOF, REQFILERENAME, REQFILEDELETE,
    REQDIRFINDFIRST, REQDIRFINDNEXT, REQDIRFINDCLOSE,
    REQFILEGETPOINTER, REQFILEGETSIZE, REQGETVERSION, 
    REQPTZ, REQCAPTURE,
    REQKEY, REQCONNECT, REQDISCONNECT,
    REQCHECKID, REQLISTID, REQDELETEID, REQADDID, REQCHPASSWORD,
    REQSHAREPASSWD, REQGETTZIENTRY,
    REQECHO,
    
    REQ2BEGIN =	200, 
    REQSTREAMOPEN, REQSTREAMOPENLIVE, REQSTREAMCLOSE, 
    REQSTREAMSEEK, REQSTREAMGETDATA, REQSTREAMTIME,
    REQSTREAMNEXTFRAME, REQSTREAMNEXTKEYFRAME, REQSTREAMPREVKEYFRAME,
    REQSTREAMDAYINFO, REQSTREAMMONTHINFO, REQLOCKINFO, REQUNLOCKFILE, REQSTREAMDAYLIST,
    REQ2RES2,                                          // reserved, don't use
    REQ2ADJTIME,
    REQ2SETLOCALTIME, REQ2GETLOCALTIME, REQ2SETSYSTEMTIME, REQ2GETSYSTEMTIME,
    REQ2GETZONEINFO, REQ2SETTIMEZONE, REQ2GETTIMEZONE,
    REQSETLED,
    REQSETHIKOSD,                                               // Set OSD for hik's cards
    REQ2GETCHSTATE,
    REQ2GETSETUPPAGE,
    REQ2GETSTREAMBYTES,
    REQ2GETSTATISTICS,
    REQ2KEYPAD,
    REQ2PANELLIGHTS,
    REQ2GETJPEG,
    REQ2STREAMGETDATAEX,
    REQ2SYSTEMSETUPEX,
    REQ2GETGPS,
        
    REQ3BEGIN = 300,
    REQSENDDATA,
    REQGETDATA,
    REQCHECKKEY,
    REQUSBKEYPLUGIN, // plug-in a new usb key. (send by plug-in autorun program)
     
    REQ5BEGIN = 500,                                            // For eagle32 system
    REQNFILEOPEN,
    REQNFILECLOSE,
    REQNFILEREAD,
    
    REQMAX
};

enum anscode_type { ANSERROR =1, ANSOK, 
    ANSREALTIMEHEADER, ANSREALTIMEDATA, ANSREALTIMENODATA,
    ANSCHANNELHEADER, ANSCHANNELDATA, 
    ANSSWITCHINFO,
    ANSPLAYBACKHEADER, ANSSYSTEMSETUP, ANSCHANNELSETUP, ANSSERVERNAME,
    ANSGETCHANNELSTATE,
    ANSGETTIME, ANSTIMEZONEINFO, ANSFILEFIND, ANSGETVERSION, ANSFILEDATA,
    ANSKEY, ANSCHECKID, ANSLISTID, ANSSHAREPASSWD, ANSTZENTRY,
    ANSECHO,
    
    ANS2BEGIN =	200, 
    ANSSTREAMOPEN, ANSSTREAMDATA,ANSSTREAMTIME,
    ANSSTREAMDAYINFO, ANSSTREAMMONTHINFO, ANSLOCKINFO, ANSSTREAMDAYLIST,
    ANS2RES1, ANS2RES2, ANS2RES3,                        	// reserved, don't use
    ANS2TIME, ANS2ZONEINFO, ANS2TIMEZONE, ANS2CHSTATE, ANS2SETUPPAGE, ANS2STREAMBYTES,
    ANS2JPEG, ANS2STREAMDATAEX, ANS2SYSTEMSETUPEX, ANS2GETGPS,
    
    ANS3BEGIN = 300,
    ANSSENDDATA,
    ANSGETDATA,
    
    ANS5BEGIN = 500,					// file base copying
    ANSNFILEOPEN,
    ANSNFILEREAD,
    
    ANSMAX
};


enum conn_type { CONN_NORMAL = 0, CONN_REALTIME, CONN_LIVESTREAM, CONN_JPEG  };

#define closesocket(s) close(s)

class dvrsvr {
    protected:
        static dvrsvr *m_head;
        dvrsvr *m_next;				// dvr list
        int m_sockfd;				// socket
        struct net_fifo *m_fifo;
        int m_fifosize ;			// size of fifo
        int m_active;               // active streaming socket
        
        playback * m_playback ;
        live	 * m_live ;
        
        // dvr related struct
        int m_conntype;				// 0: normal, 1: realtime video stream, 2: answering
        int m_connchannel;
        
        struct dvr_req m_req;
        char *m_recvbuf;
        int m_recvlen;
        int m_recvloc;
        
        FILE * m_nfilehandle ;
        
    public:
        dvrsvr(int fd);
        virtual ~dvrsvr();
        static dvrsvr *head() {
            return m_head;
        } 
        
        dvrsvr *next() {
            return m_next;
        }
        int isfifo() {
            return m_fifo != NULL;
        }
        int socket() {
            return m_sockfd;
        }
        int  read();
        int  write();
        void send_fifo(char *buf, int bufsize);
		virtual void Send(void *buf, int bufsize);
        void close();
        int isclose();
        int onframe(cap_frame * pframe);
        
        // request handlers
        void onrequest();
        void DefaultReq();
        

        virtual void ReqOK();
        virtual void ReqRealTime();
        virtual void ChannelInfo();
        virtual void DvrServerName();
        virtual void GetSystemSetup();
        virtual void SetSystemSetup();
        virtual void GetChannelSetup();
        virtual void SetChannelSetup();
        virtual void ReqKill();
        virtual void SetUpload();
        virtual void HostName();
        virtual void FileRename();
        virtual void FileCreate();
        virtual void FileRead();
        virtual void FileWrite();
        virtual void FileClose();
        virtual void FileSetPointer();
        virtual void FileGetPointer();
        virtual void FileGetSize();
        virtual void FileSetEof();
        virtual void FileDelete();
        virtual void DirFindFirst();
        virtual void DirFindNext();
        virtual void DirFindClose();
        virtual void GetChannelState();
        virtual void SetDVRSystemTime();
        virtual void GetDVRSystemTime();
        virtual void SetDVRLocalTime();
        virtual void GetDVRLocalTime();
        virtual void SetDVRTimeZone();
        virtual void GetDVRTimeZone();
        virtual void GetDVRTZIEntry();
        virtual void GetVersion();
        virtual void ReqEcho();
        virtual void ReqPTZ();
        virtual void ReqAuth();
        virtual void ReqKey();
        virtual void ReqDisconnect();
        virtual void ReqCheckId();
        virtual void ReqListId();
        virtual void ReqDeleteId();
        virtual void ReqAddId();
        virtual void ReqSharePasswd();
        virtual void ReqStreamOpen();
        virtual void ReqStreamOpenLive();
        virtual void ReqStreamClose();
        virtual void ReqStreamSeek();
        virtual void ReqNextFrame();
        virtual void ReqNextKeyFrame();
        virtual void ReqPrevKeyFrame();
        virtual void ReqStreamGetData();
        virtual void ReqStreamTime();
        virtual void ReqStreamDayInfo();
        virtual void ReqStreamMonthInfo();
        virtual void ReqStreamDayList();
        virtual void ReqLockInfo();
        virtual void ReqUnlockFile();
        virtual void Req2SetLocalTime();
        virtual void Req2GetLocalTime();
        virtual void Req2AdjTime();
        virtual void Req2SetSystemTime();
        virtual void Req2GetSystemTime();
        virtual void Req2GetZoneInfo();
        virtual void Req2SetTimeZone();
        virtual void Req2GetTimeZone();
        virtual void ReqSetled();
        virtual void ReqSetHikOSD();
        virtual void Req2GetChState();
        virtual void Req2GetSetupPage();
        virtual void Req2GetStreamBytes();
        virtual void ReqNfileOpen();
        virtual void ReqNfileClose();
        virtual void ReqNfileRead();
        virtual void Req2GetJPEG();        
        virtual void Req2GetGPS();                
};

// DVR client side support

// open live stream from remote dvr
int dvr_openlive(int sockfd, int channel, struct hd_file * hd264);

// get remote dvr system setup
// return 1:success
//        0:failed
int dvr_getsystemsetup(int sockfd, struct system_stru * psys);

// set remote dvr system setup
// return 1:success
//        0:failed
int dvr_setsystemsetup(int sockfd, struct system_stru * psys);

// get channel setup of remote dvr
// return 1:success
//        0:failed
int dvr_getchannelsetup(int sockfd, int channel, struct DvrChannel_attr * pchannelattr);

// set channel setup of remote dvr
// return 1:success
//        0:failed
int dvr_setchannelsetup (int sockfd, int channel, struct DvrChannel_attr * pchannelattr);

// set channel OSD
// return 1:success
//        0:failed
int dvr_sethikosd(int sockfd, int channel, struct hik_osd_type * posd);

// get channel status
//    return value:
//			bit 0: signal lost
//          bit 1: motion detected
int dvr_getchstate(int sockfd, int ch);

// set remote dvr time
// return 1: success, 0:failed
int dvr_settime(int sockfd, struct dvrtime * dvrt);

// get remote dvr time
// return 1: success, 0:failed
int dvr_gettime(int sockfd, struct dvrtime * dvrt);

// get remote dvr time in utc
// return 1: success, 0:failed
int dvr_getutctime(int sockfd, struct dvrtime * dvrt);

// get remote dvr time
// return 1: success, 0:failed
int dvr_setutctime(int sockfd, struct dvrtime * dvrt);

// adjust remote dvr time
// return 1: success, 0:failed
int dvr_adjtime(int sockfd, struct dvrtime * dvrt);

// set remote dvr time zone
// return 1: success, 0: failed
int dvr_settimezone(int sockfd, char * tzenv);

// open new clip file on remote dvr
int dvr_nfileopen(int sockfd, int channel, struct nfileinfo * nfi);

// close nfile on remote dvr
// return 
//       1: success
//       0: failed
int dvr_nfileclose(int sockfd, int nfile);


// read nfile from remote dvr
// return actual readed bytes
int dvr_nfileread(int sockfd, int nfile, void * buf, int size);


// received UDP message
#define MAXMSGSIZE (32000)
int msg_onmsg(void *msgbuf, int msgsize, struct sockad *from);
void msg_clean();
void msg_init();
void msg_uninit();

extern int msgfd;

// event 
void setdio(int onoff);
void event_check();
void event_init();
void event_uninit();
void event_run();

// system setup
struct system_stru {
	char IOMod[80]  ;
	char ServerName[80] ;
	int cameranum ;
	int alarmnum ;
	int sensornum ;
	int breakMode ;
	int breakTime ;
	int breakSize ;
	int minDiskSpace ;
	int shutdowndelay ;
	char autodisk[32] ;
	char sensorname[16][32];
	int sensorinverted[16];
	int eventmarker_enable ;
	int eventmarker ;
	int eventmarker_prelock ;
	int eventmarker_postlock ;
	char ptz_en ;
	char ptz_port ;			// COM1: 0
	char ptz_baudrate ;		// times of 1200
	char ptz_protocol ;		// 0: PELCO "D"   1: PELCO "P"
	int  videoencrpt_en ;	// enable video encryption
	char videopassword[32] ;
	int  rebootonnoharddrive ; // seconds to reboot if no harddrive detected!
	char res[84] ;
};

struct gps {
  int		gps_valid ;
  double	gps_speed ;	// knots
  double	gps_direction ; // degree
  double  gps_latitude ;	// degree, + for north, - for south
  double  gps_longitud ;      // degree, + for east,  - for west
  double  gps_gpstime ;	// seconds from 1970-1-1 12:00:00 (utc)
};

// return true for ok
int dvr_getsystemsetup(struct system_stru * psys);
// return true for ok
int dvr_setsystemsetup(struct system_stru * psys);
// return true for ok
int dvr_getchannelsetup(int ch, struct DvrChannel_attr * pchannel, int attrsize);
// return true for ok
int dvr_setchannelsetup(int ch, struct DvrChannel_attr * pchannel, int attrsize);
	

// digital io functions
//extern "C" {
int dio_inputnum();
int dio_outputnum();
int dio_input( int no );
void dio_output( int no, int v);
int dio_isstandbymode();
// return 1 for poweroff switch turned off
int dio_poweroff();
//int dio_lockpower();
//int dio_unlockpower();
int dio_enablewatchdog();
int dio_disablewatchdog();
int dio_kickwatchdog();
//void dio_devicepower(int onoff);
//void dio_usb_led(int v);
//void dio_error_led(int v);
//void dio_videolost_led(int v);
int dio_syncrtc();
int dio_check() ;
int dio_setstate( int status ) ;
int dio_clearstate( int status ) ;
void dio_init(int prerun);
void dio_uninit();
int dio_hdpower(int on);
double gps_speed(int *gpsvalid_changed);
int gps_location( double * latitude, double * longitude );
void get_gps_data(struct gps *g);
extern double g_gpsspeed ;
extern int dio_ioprocess_mode ;
int dio_get_nodiskcheck();

//};

// sensor

class sensor_t {
	string m_name ;
	int m_inputpin ;
	int m_inverted ;
	int m_value ;
public:
	int m_eventmarker, m_rising, m_falling, m_immobile;
	sensor_t(int n);
	char * name() {
		return m_name.getstring();
	}
	int check();
	int value() {
		return m_value ;
	}
} ;

extern sensor_t ** sensors ;
extern int num_sensors ;

class alarm_t {
	string m_name ;
	int m_outputpin ;
	int m_value ;
public:
	alarm_t(int n);
	~alarm_t();
		
	char * name() {
		return m_name.getstring();
	}
	void clear();
	int setvalue(int v);
	void update();
} ;

extern alarm_t ** alarms ;
extern int num_alarms ;


void screen_init();
void screen_uninit();
int screen_io(int usdelay);

#endif							// __dvr_h__
