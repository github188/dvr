
// screen process

#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>

#include "dvr.h"

#ifndef STANDARD_NTSC
#define STANDARD_NTSC (1)
#endif
#ifndef STANDARD_PAL
#define STANDARD_PAL (2)
#endif

int screen_sockfd ;

#ifdef NO_ONBOARD_EAGLE
// define all dummy screen functions
int screen_setliveview( int channel )
{
    return 0;
}

int screen_menu( int level )
{
    return 0;
}

int screen_key( int keycode, int keydown )
{
    return 0;
}

int screen_draw()
{
    return 0;
}

// called periodically by main process
int screen_io(int usdelay)
{
    return 0;
}

int screen_onframe( cap_frame * capframe )
{
    return 0;
}

// initialize screen
void screen_init(config &dvrconfig)
{
}

// screen finish, clean up
void screen_uninit()
{
}
#else	// ONBOAR EAGLE

#include "fbwindow.h"

static char mousedevname[] = "/dev/input/mice" ;
static int  mousedev = 0 ;
static int mousemaxx, mousemaxy ;
static int mousex, mousey ;
static int mousebuttons ;

static int ScreenNum = 2 ;
static int ScreenStandard = STANDARD_NTSC ;
static int ScreenMarginX = 5 ;
static int ScreenMarginY = 0 ;

#ifdef PWII_APP
int screen_rearaudio ;
#endif

static window * topwindow ;
char resource::resource_dir[128] ;

window * window::focuswindow ;
window * window::mouseonwindow ;
int window::gredraw ;
static cursor * Screen_cursor ;

// to support video out liveview on ip camera
int screen_liveview_channel ;
int screen_liveview_handle ;
static int screen_liveaudio ;
static int screen_play_jumptime ;
static int screen_play_cliptime ;
static int screen_play_maxjumptime ;
static int screen_play_doublejumptimer ;
static int screen_keepcapture ;


// status window on video screen
class video_icon : public window {

    resource m_icon ;

    public:
    video_icon( window * parent, int id, int x, int y, int w, int h ) :
        window( parent, id, x, y, w, h )
    {
    }

    void seticon( char * iconfile )
    {
        if( iconfile==NULL ) {
            if( m_icon.valid() ) {
                m_icon.close();
                redraw();
            }
            return ;
        }
        m_icon.open( iconfile );
        redraw();
    }
        // event handler
    protected:
        virtual void paint() {
            setcolor (COLOR(0,0,0,0)) ;	// full transparent
            setpixelmode (DRAW_PIXELMODE_COPY);
            fillrect ( 0, 0, m_pos.w, m_pos.h );
            if( m_icon.valid() ) {
                drawbitmap( m_icon, 0, 0, 0, 0, m_pos.w, m_pos.h );
            }
        }
};

// status window on video screen
class video_status : public window {
    string m_status ;
    public:
    video_status( window * parent, int id, int x, int y, int w, int h ) :
        window( parent, id, x, y, w, h )
    {
    }

    void setstatus( char * status )
    {
        if( strcmp( status, m_status)!=0 ) {
            m_status = status ;
            redraw();
        }
    }
        // event handler
    protected:
    virtual void paint()
    {
        setcolor (COLOR(0,0,0,0)) ;	// full transparent
        setpixelmode (DRAW_PIXELMODE_COPY);
        fillrect ( 0, 0, m_pos.w, m_pos.h );
        resource font("mono32b.font");
        setcolor(COLOR(240,240,80,200));
        drawtext( 0, 0, m_status, font );
    }
};

#define DECODE_MODE_QUIT    (0)
#define DECODE_MODE_PAUSE   (1)
#define DECODE_MODE_PLAY    (2)
#define DECODE_MODE_PLAY_FASTFORWARD    (3)
#define DECODE_MODE_PLAY_FASTBACKWARD    (4)
#define DECODE_MODE_BACKWARD (5)
#define DECODE_MODE_FORWARD  (6)
#define DECODE_MODE_PRIOR   (7)
#define DECODE_MODE_NEXT    (8)

// decode speed, 0=fastest, 3=fast, 4=normal, 5=slow, 6=slowest, >=7 error
#define DECODE_SPEED_FASTEST    (0)
#define DECODE_SPEED_FAST       (3)
#define DECODE_SPEED_NORMAL     (4)
#define DECODE_SPEED_SLOW       (5)
#define DECODE_SPEED_SLOWEST    (6)

#define VIDEO_MODE_NONE     (0)
#define VIDEO_MODE_LIVE     (1)
#define VIDEO_MODE_MENU     (2)
#define VIDEO_MODE_PLAYBACK (3)
#define VIDEO_MODE_LCDOFF   (4)
#define VIDEO_MODE_STANDBY  (5)

#define ID_VIDEO_SCREEN (1001)
#define ID_MENU_SCREEN (1101)

#define ID_TIMER_DECODER    (3001)


// command send to video screen
#define CMD_MENUOFF (2001)

#ifdef PWII_APP
void dio_pwii_lcd( int lcdon );
void dio_pwii_standby( int standby );
#endif

#define MAXPOLICIDLISTLINE (6)

#ifdef PWII_APP

static int screen_nomenu ;  		//  do not display police id menu
static int screen_nostartmenu ;  	//  do not show police id input menu on startup
static int screen_menustartup=0 ;

// status window on video screen
class pwii_menu : public window {
    int m_level ;           // 0: top level, 1: Enter Officer ID, 2: Enter classification number
    int m_select ;          // selection on current level
    int m_maxselect ;       // selection from 0 to maxselect
    int m_dispbegin ;
    array <string> m_officerIDlist ;

public:
    pwii_menu( window * parent, int id, int x, int y, int w, int h )
        :window( parent, id, x, y, w, h ) {
        level(1);
    }

    virtual ~pwii_menu() {
        m_id = 0 ;                  // so parent can't find me.
        m_parent->command( CMD_MENUOFF );
    }

    void refresh() {
        settimer( 60000, 1 );           // turn off timer
        redraw();
    }

    void level(int l) {
        m_level = l ;
        if( m_level == 1 ) {
            m_maxselect = 0 ;
            m_select = 0 ;
            m_officerIDlist.empty();
        }
        else if( m_level == 2 ) {
            m_officerIDlist.empty();
            // whats in the ID list?
            //    first item : "NO ID (bypass)",
            //    second item : current (previously entered) police ID if present (not bypassed)
            //    other item : avaiable ID except current ID

            int il ;
            m_officerIDlist[0]="NO ID (bypass)" ;
            il = 1 ;
            if( strlen(g_policeid)>0 ) {
                m_officerIDlist[il++]=g_policeid ;
                m_select = 1 ;
            }
            else {
                m_select = 0 ;
            }

            FILE * fid ;
            fid=fopen(g_policeidlistfile, "r");
            if( fid ) {
                char idline[100] ;
                while( fgets(idline, 100, fid) ) {
                    str_trimtail(idline);
                    if( strlen(idline)<=0 ) continue ;
                    if( strcmp(idline, g_policeid)==0 ) continue ;
                    m_officerIDlist[il++]=idline ;
                }
                fclose(fid);
            }
            m_maxselect = il-1 ;
            m_dispbegin = 0 ;
        }
        refresh();
    }

    void enter() {
        if( m_level == 1 && m_select == 0 ) {
            level(2);
        }
        else if( m_level == 2 ) {
            // update current police ID
            if( m_select == 0 ) {
                // select no ID (bypass)
                g_policeid[0]=0 ;
                dvr_log( "Police ID bypassed!");
            }
            else {
                strcpy(g_policeid, m_officerIDlist[m_select]);
                dvr_log( "Police ID selected : %s", g_policeid );
            }
            m_officerIDlist[0]="";
            m_officerIDlist.sort();
            // write police id list file
            FILE * fid ;
            fid=fopen(g_policeidlistfile, "w");
            if( fid ) {
                fprintf(fid, "%s\n", g_policeid );      // write current ID or empty line
                int i ;
                for( i=0; i<m_officerIDlist.size(); i++ ) {
                    if( m_officerIDlist[i].length()<=0 ) continue ;
                    if( strcmp(m_officerIDlist[i], g_policeid)==0 ) continue ;
                    fprintf( fid, "%s\n", (char *)m_officerIDlist[i]);
                }
                fclose(fid);
            }

            m_officerIDlist.empty();
            level(1);
        }
        refresh();
    }

    void cancel() {
        if( m_level == 1 ) {
            destroy();
        }
        else if( m_level == 2 ) {
            // back to level 1 menu with no change
            level(1);
        }
        refresh();
    }

    void nextpage() {
        if( m_level == 1 ) {
        }
        else if( m_level == 2 ) {
            if( m_select < m_maxselect ) {
                m_select++ ;
            }
            m_dispbegin = m_select-MAXPOLICIDLISTLINE+1 ;
            if( m_dispbegin<0 ) m_dispbegin=0 ;
        }
        refresh();
    }

    void prevpage() {
        if( m_level == 1 ) {
        }
        else if( m_level == 2 ) {
            if( m_select > 0 ){
                m_select-- ;
            }
            if( m_select<m_dispbegin ) {
                m_dispbegin=m_select ;
            }
        }
        refresh();
    }

    void next() {
        if( m_level == 1 ) {
        }
        else if( m_level == 2 ) {
            if( m_select < m_maxselect ) {
                m_select++ ;
            }
            m_dispbegin = m_select-MAXPOLICIDLISTLINE+1 ;
            if( m_dispbegin<0 ) m_dispbegin=0 ;
        }
        refresh();
    }

    void prev() {
        if( m_level == 1 ) {
        }
        else if( m_level == 2 ) {
            if( m_select > 0 ){
                m_select-- ;
            }
            if( m_select<m_dispbegin ) {
                m_dispbegin=m_select ;
            }
        }
        refresh();
    }

    // event handler
protected:
    virtual void paint() {						// paint window

        char sbuf[256] ;
        int y ;
        resource font("mono32b.font");
        setpixelmode (DRAW_PIXELMODE_COPY);
        resource ball ("ball.pic") ;

        if( m_level==1 ) {

            setcolor (COLOR(30, 71, 145, 190 )) ;
            fillrect ( 50, 80, m_pos.w-100, m_pos.h-160 );
            y = 120 ;
            setcolor(COLOR(240,240,80,255));
            sprintf( sbuf, "VRI: %s", g_vri[0]?g_vri:"(NA)" );
            drawtext( 55, y, sbuf, font );
            y+=40 ;
            // show disk avaialbe space
            int rectime, locktime, remtime ;
            if( disk_stat( &rectime, &locktime, &remtime ) ) {
                remtime = (rectime-locktime+remtime)/60 ;
                remtime /= cap_channels ;
                locktime /= 60 ;
                locktime /= cap_channels ;
                setcolor(COLOR(240,240,80,255));
                sprintf( sbuf, "Video length %d:%02d  Remain %d:%02d",
                         locktime/60,
                         locktime%60,
                         remtime/60,
                         remtime%60 );
                drawtext( 55, y, sbuf, font );
                y+=40 ;
            }
            if( strlen(g_policeid)>0 ) {
                setcolor(COLOR(240,240,80,255));
                sprintf( sbuf, "Officer ID: %s", g_policeid );
                drawtext( 55, y, sbuf, font );
                sprintf( sbuf, "Change Officer ID" );
                drawtext( 80, 280, sbuf, font );
                drawbitmap(ball, (80-20), (280+15));
            }
            else {
                setcolor(COLOR(240,240,80,255));
                sprintf( sbuf, "Officer ID: (NA)" );
                drawtext( 55, y, sbuf, font );
                sprintf( sbuf, "Enter Officer ID" );
                drawtext( 80, 280, sbuf, font );
                drawbitmap(ball, (80-20), (280+15));
            }
        }
        else if( m_level==2 ) {
            int i ;
            setcolor (COLOR(30, 71, 145, 190 )) ;
            fillrect ( 50, 80, m_pos.w-100, m_pos.h-160 );

            setcolor(COLOR(240,240,80,255));
            drawtextex( 80, 100, "SELECT OFFICER ID", font, 30, 50 );

            int x = 80 ;
            int y = 160 ;

            for( i=0; i<MAXPOLICIDLISTLINE; i++ ) {
                if( i+m_dispbegin > m_maxselect ) break;
                drawtext( x, y, m_officerIDlist[i+m_dispbegin], font ) ;
                if( (i+m_dispbegin)==m_select ) {
                    drawbitmap( ball, (x-20), (y+15) ) ;
                }
                y+=40 ;
            }
        }
    }

    virtual void ontimer( int id ) {
        if( id==1 ) {
            destroy();
        }
        return ;
    }
};

#endif

class video_screen : public window {

protected:

    int m_select ;       // 1: currently selected, 0: unselected. (only one selected video screen)

    int m_decode_speed ;            // decoder speed
    int m_decode_runmode ;          // decoder thread running,
    int m_decode_modesafe ;         // safe to change decoding mode
    playback * m_decode_ply ;

//    pthread_t  m_decodethreadid ;
    dvrtime  m_streamreftime ;      // stream start(restart) time
    dvrtime  m_streamtime ;         // remember streaming playback time
    dvrtime  m_playbacktime ;       // remember previous playback time

    int m_videomode ;           // 0: live, 1: playback, 2: lcdoff, 3: standby
    int m_playchannel ;     	// playing channel, 0: first channel, 1: second channel
    int m_eaglechannel ;		// related eagle channel

    video_status * m_statuswin ;        // status txt on top-left corner
    video_icon   * m_icon ;             // display a icon when button pressed

    int m_jumptime ;
    int m_keytime ;
    int m_timerstate ;
    int m_keypad_up ;

public:
    video_screen( window * parent, int id, int x, int y, int w, int h ) :
        window( parent, id, x, y, w, h ) {
        // initial channel number,
        m_playchannel = 0 ;
        m_eaglechannel = 0 ;

        m_select = 0 ;
        // create recording light

        m_videomode=VIDEO_MODE_NONE ;
        //            m_decode_handle = 0 ;
        time_dvrtime_init(&m_streamtime, 2000);
        time_dvrtime_init(&m_playbacktime, 2000);
        m_statuswin = new video_status(this, 1, 30, 60, 200, 50 );
        m_icon = new video_icon(this, 2, (m_pos.w-75)/2, (m_pos.h-75)/2, 75, 75 );
#ifdef PWII_APP
        if( screen_menustartup==0 && screen_nostartmenu==0 && id == ID_VIDEO_SCREEN ) {
            // this timer would open POLICE ID menu
            settimer( 2000, ID_VIDEO_SCREEN ) ;
            screen_menustartup=1 ;
        }
        m_decode_modesafe = 0 ;
#endif
        m_keypad_up = 0 ;
    }

    ~video_screen() {
        if( m_videomode== VIDEO_MODE_PLAYBACK ) {
            stopdecode();
        }
        stop();
        delete m_statuswin ;
        delete m_icon ;
    }

    void liveview( int channel ) {
        if( m_videomode <= VIDEO_MODE_PLAYBACK ) {
            startliveview(channel);
        }
    }

    void openmenu( int level ) {
        if( m_videomode <= VIDEO_MODE_PLAYBACK ) {
            startliveview(m_playchannel);
        }
#ifdef PWII_APP
        startmenu() ;
        pwii_menu * pmenu = (pwii_menu *)findwindow( ID_MENU_SCREEN ) ;
        if( pmenu ) {
            pmenu->level(level);
        }
#endif      // PWII_APP

        updatestatus();
    }

    // select audio
    void select(int s) {
        if( m_select != s ) {
            m_select = s ;
            if( m_videomode == VIDEO_MODE_LIVE ) {     // live view
                startliveview(m_playchannel);
            }
            else {
                ;   // support only one channel playback now.
            }
            redraw();       // redraw box
        }
    }

    // stop every thing?
    void stop() {
        stopdecode();
#ifdef PWII_APP
        stopmenu();
#endif
        stopliveview();
    }

    // event handler
protected:

    virtual void paint() {
        setcolor (COLOR(0,0,0,0)) ;	// full transparent
        setpixelmode (DRAW_PIXELMODE_COPY);
        fillrect ( 0, 0, m_pos.w, m_pos.h );
        /*
   if( m_select ) {
                setcolor ( COLOR( 0xe0, 0x10, 0x10, 0xff) ) ;	// selected border
            }
            else {
                setcolor ( COLOR( 0x30, 0x20, 0x20, 0xff) ) ;	    // border color
            }
            drawrect(0, 0, m_pos.w, m_pos.h);		// draw border
            drawrect(1, 1, m_pos.w-2, m_pos.h-2 );
*/
    }

    virtual void onmouse( int x, int y, int buttons ) {
        if( (buttons & 0x11)==0x11 ){       // click
            select(1);
        }
    }

    virtual void oncommand( int id, void * param ) {	// children send command to parent
        if( id == CMD_MENUOFF ) {
#ifdef PWII_APP
            stopmenu();
#endif  // PWII_APP
        }
    }

    void startliveview(int channel)    {
        if( channel<0 || channel>=cap_channels ) {
            channel = m_playchannel ;
        }

        screen_liveaudio=1 ;
#ifdef PWII_APP
        if( channel==pwii_rear_ch ) {
            screen_liveaudio=screen_rearaudio ;
        }
#endif
        if( m_videomode == VIDEO_MODE_LIVE &&
            m_playchannel == channel )
        {
            if( m_select ) {
                // turn on/off audio channel
                dvr_screen_audio(screen_sockfd, m_eaglechannel, screen_liveaudio) ;
            }
            else {
                dvr_screen_audio(screen_sockfd, m_eaglechannel, 0) ;
            }
            return ;
        }
        stop();

        // start new live view channel
        if( channel < cap_channels ) {
            m_playchannel = channel ;
            m_eaglechannel = cap_channel[m_playchannel]->geteaglechannel();
            dvr_screen_live(screen_sockfd, m_eaglechannel) ;
            if( m_select ) {
                dvr_screen_audio(screen_sockfd, m_eaglechannel, screen_liveaudio) ;
            }
            else {
                dvr_screen_audio(screen_sockfd, m_eaglechannel, 0) ;
            }
            m_videomode = VIDEO_MODE_LIVE ;
        }
        updatestatus();
    }

    void stopliveview() {
        if( m_videomode == VIDEO_MODE_LIVE )
        {
            dvr_screen_stop(screen_sockfd, m_eaglechannel) ;
            m_videomode = VIDEO_MODE_NONE ;
        }
    }

#ifdef PWII_APP
    void startmenu() {
        //            stop();

        if( screen_nomenu ) {		// don't start menu if disabled
            return ;
        }
        new pwii_menu(this, ID_MENU_SCREEN, 0, 0, m_pos.w, m_pos.h);
        m_videomode= VIDEO_MODE_MENU ;
        updatestatus();
    }

    void stopmenu() {
        if( m_videomode == VIDEO_MODE_MENU )
        {
            pwii_menu * pmenu ;                // menu
            pmenu = (pwii_menu *)findwindow( ID_MENU_SCREEN );
            if( pmenu ) {
                delete pmenu ;
            }
            m_videomode = VIDEO_MODE_LIVE ;
            updatestatus();
        }
    }
#endif  // PWII_APP

    void restartdecoder() {
        dvr_screen_stop(screen_sockfd, m_eaglechannel);
        if( m_decode_runmode == DECODE_MODE_PLAY ) {
            m_decode_speed = DECODE_SPEED_NORMAL ;
        }
        else {
            m_decode_speed = DECODE_SPEED_FASTEST ;  // make decoder run super faster, so I can take care the player speed
        }
        dvr_screen_play(screen_sockfd, m_eaglechannel, m_decode_speed) ;
        time_now( &m_playbacktime );
        m_decode_ply->getstreamtime(&m_streamreftime) ;
        m_streamtime = m_streamreftime ;
        updatestatus();
    }

    void updatestatus()  {
        if( m_videomode == VIDEO_MODE_LIVE ) {      // live mode
            m_statuswin->setstatus( "LIVE" );
        }
        else if( m_videomode == VIDEO_MODE_PLAYBACK ) { // playback mode,
            if( m_decode_runmode == DECODE_MODE_PAUSE ) {
                m_statuswin->setstatus( "PAUSED" );
            }
            else if( m_decode_runmode == DECODE_MODE_PLAY_FASTFORWARD ) {
                m_statuswin->setstatus( "FORWARD" );
            }
            else if( m_decode_runmode == DECODE_MODE_PLAY_FASTBACKWARD ) {
                m_statuswin->setstatus( "BACKWARD" );
            }
            else {                      // DECODE_MODE_PLAY
                m_statuswin->setstatus( "PLAY" );
            }
        }
        else if( m_videomode == VIDEO_MODE_MENU ) {     // menu mode
            m_statuswin->setstatus( "" );
        }
        else if( m_videomode == VIDEO_MODE_LCDOFF ) {      // live mode
            m_statuswin->setstatus( "LCDOFF" );
        }
        else if( m_videomode == VIDEO_MODE_STANDBY ) {      // live mode
            m_statuswin->setstatus( "OFF" );
        }
    }

    void decode_run()  {
        dvrtime tnow ;
        int diff_stream, diff_ref ;

        if( m_decode_runmode<=0 ) {
            return ;
        }

        // repeat this timer
        settimer( 50, ID_TIMER_DECODER );

        if( m_decode_runmode == DECODE_MODE_PLAY ) {
            m_decode_ply->getstreamtime(&m_streamtime);
            diff_stream = time_dvrtime_diffms( &m_streamtime , &m_streamreftime );
            time_now(&tnow);
            diff_ref = time_dvrtime_diffms( &tnow, &m_playbacktime );
            if( (diff_ref-diff_stream)>5000 || (diff_ref-diff_stream)<-5000 ) {
                restartdecoder();
                return ;
            }
            if( (diff_ref-diff_stream)>0 ) {
                void * buf=NULL ;
                int    bufsize=0 ;
                int    frametype ;
                m_decode_ply->getstreamdata(&buf, &bufsize, &frametype );
                if( bufsize>0 ) {
                    dvr_screen_playinput(screen_sockfd, m_eaglechannel, buf, bufsize);
                }
                settimer( 1, ID_TIMER_DECODER );        // set faster timer
            }
            m_decode_modesafe = 1 ;
        }
        else if( m_decode_runmode == DECODE_MODE_PAUSE ) {
            m_decode_modesafe = 1 ;
        }
        else if( m_decode_runmode == DECODE_MODE_BACKWARD ) {
            dvrtime cbegin, pbegin, end ;
            if( m_decode_ply->getcurrentcliptime( &cbegin, &end ) ) {
                if( time_dvrtime_diff( &m_streamtime , &cbegin ) > m_jumptime )
                {
                    time_dvrtime_del(&m_streamtime, m_jumptime);
                    m_decode_ply->seek(&m_streamtime);
                }
                else if( m_decode_ply->getprevcliptime( &pbegin, &end ) ) {
                    time_dvrtime_del(&end, m_jumptime*3);
                    m_decode_ply->seek(&end);
                }
                else {
                    m_decode_ply->seek(&cbegin);
                }
            }
            else if( m_decode_ply->getprevcliptime( &cbegin, &end ) ) {
                time_dvrtime_del(&end, m_jumptime);
                m_decode_ply->seek(&end);
            }
            m_decode_runmode = DECODE_MODE_PLAY ;
            restartdecoder();
        }
        else if( m_decode_runmode == DECODE_MODE_FORWARD ) {
            time_dvrtime_add(&m_streamtime, m_jumptime);
            m_decode_ply->seek(&m_streamtime);
            m_decode_runmode = DECODE_MODE_PLAY ;
            restartdecoder();
        }
        else if( m_decode_runmode == DECODE_MODE_PRIOR ) {
            dvrtime begin, end ;

            if( m_decode_ply->getprevcliptime( &begin, &end ) ) {
                m_decode_ply->seek(&begin);
            }
            else if( m_decode_ply->getcurrentcliptime( &begin, &end ) ) {
                m_decode_ply->seek(&begin);
            }

/*
            if( m_decode_ply->getcurrentcliptime( &cbegin, &end ) ) {
                if( time_dvrtime_diff( &m_streamtime , &cbegin ) > screen_play_cliptime )
                {
                    time_dvrtime_del(&m_streamtime, screen_play_cliptime);
                    m_decode_ply->seek(&m_streamtime);
                }
                else if ( time_dvrtime_diff( &m_streamtime , &cbegin ) > screen_play_jumptime )
                {
                    m_decode_ply->seek(&cbegin);
                }
                else if( m_decode_ply->getprevcliptime( &pbegin, &end ) ) {
                    time_dvrtime_del(&end, screen_play_cliptime );
                    if( pbegin < end ) {
                        m_decode_ply->seek(&end);
                    }
                    else {
                        m_decode_ply->seek(&pbegin);
                    }
                }
                else {
                    m_decode_ply->seek(&cbegin);
                }
            }
            else if( m_decode_ply->getprevcliptime( &pbegin, &end ) ) {
                time_dvrtime_del(&end, screen_play_cliptime );
                if( pbegin < end ) {
                    m_decode_ply->seek(&end);
                }
                else {
                    m_decode_ply->seek(&pbegin);
                }
            }
*/

            m_decode_runmode = DECODE_MODE_PLAY ;
            restartdecoder();
        }
        else if( m_decode_runmode == DECODE_MODE_NEXT ) {
            dvrtime begin, end ;
            if( m_decode_ply->getnextcliptime( &begin, &end ) ) {
                m_decode_ply->seek(&begin);
                restartdecoder();
            }

/*
            if( m_decode_ply->getcurrentcliptime( &begin, &end ) ) {
                if( time_dvrtime_diff( &end, &m_streamtime ) > screen_play_cliptime ) {
                    time_dvrtime_add(&m_streamtime, screen_play_cliptime);
                    m_decode_ply->seek(&m_streamtime);
                }
                else if( m_decode_ply->getnextcliptime( &begin, &end ) ) {
                    m_decode_ply->seek(&begin);
                }
                else {
                    m_decode_ply->seek(&end);
                }
            }
*/
            m_decode_runmode = DECODE_MODE_PLAY ;
        }


    }


    void startdecode(int channel)  {
        stop();

        if( channel < cap_channels ) {
            if( !screen_keepcapture ){
                cap_stop();       // stop live codec, so decoder has full DSP power
            }
//            disk_archive_stop();   // stop archiving
            m_playchannel = channel ;
            m_eaglechannel = cap_channel[channel]->geteaglechannel();

            //                res = SetDecodeScreen(MAIN_OUTPUT, 1, 1);
            //                res = SetDecodeAudio(MAIN_OUTPUT, 1, 1);
            m_videomode = VIDEO_MODE_PLAYBACK ;
            m_decode_runmode = DECODE_MODE_PLAY ;
//            pthread_create(&m_decodethreadid, NULL, s_playback_thread, (void *)(this));


            m_decode_ply = new playback( m_playchannel, 1 ) ;

            m_decode_speed = DECODE_SPEED_NORMAL ;

            dvrtime tnow ;
            time_now( &tnow );
            if( time_dvrtime_diff( &tnow, &m_playbacktime )> (60*30) ) {
                m_streamtime = tnow ;
                m_decode_ply->seek( &m_streamtime );
                m_decode_runmode = DECODE_MODE_PRIOR ;
            }
            else {
                m_decode_ply->seek(&m_streamtime);
                m_decode_runmode = DECODE_MODE_PLAY ;
                restartdecoder();
            }

            settimer(10, ID_TIMER_DECODER);
        }
    }

    void stopdecode()  {
        if( m_videomode != VIDEO_MODE_PLAYBACK )
            return ;

        m_decode_runmode = DECODE_MODE_QUIT ;

        dvr_screen_stop(screen_sockfd, m_eaglechannel);

        //            if( m_decode_handle > 0 ) {
        //                StopDecode(m_decode_handle) ;
        //                m_decode_handle = 0 ;
        //            }
        if(m_decode_ply!=NULL){
            delete m_decode_ply ;
            m_decode_ply = NULL ;
        }
        time_now( &m_playbacktime );
        m_decode_runmode = DECODE_MODE_QUIT ;

//        pthread_join(m_decodethreadid, NULL);
//        m_decodethreadid = 0 ;

        // stop decode screen
        //            res = SetDecodeScreen(MAIN_OUTPUT, 1, 0);
        //            res = SetDecodeAudio(MAIN_OUTPUT, 1, 0);
        m_videomode = VIDEO_MODE_NONE ;
        cap_start();       // start video capture.

    }


    virtual void ontimer( int id ) {
#ifdef PWII_APP
        if( id == ID_VIDEO_SCREEN ) {
            openmenu(2);
        }
        else if( id == VK_MEDIA_STOP ) {
            stopdecode();
            stopliveview();
            m_videomode=VIDEO_MODE_STANDBY ;
            dio_pwii_standby(1);
        }
        else if( id == VK_POWER ) {
            if( m_videomode == VIDEO_MODE_LCDOFF ) {
                m_videomode=VIDEO_MODE_STANDBY ;
                dio_pwii_standby(1);
            }
        }
        else if( id == ID_TIMER_DECODER ) {
            decode_run() ;
        }
        else {
            // key pad repeat
            if( m_videomode == VIDEO_MODE_PLAYBACK &&
                (m_decode_runmode == DECODE_MODE_PLAY || m_decode_runmode == DECODE_MODE_PAUSE) )
            {
                if( id == VK_MEDIA_PREV_TRACK ) {           // play backward
                    //                m_decode_runmode = DECODE_MODE_PLAY_FASTBACKWARD ;
                    m_decode_runmode = DECODE_MODE_BACKWARD ;
                    settimer(3000, id);
                }
                else if( id == VK_MEDIA_NEXT_TRACK ) {      // play forward
                    //                m_decode_runmode = DECODE_MODE_PLAY_FASTFORWARD ;
                    m_decode_runmode = DECODE_MODE_FORWARD ;
                    settimer(3000, id);
                }
            }
        }
#endif  // PWII_APP

        updatestatus();
        return ;
    }

    virtual int onkey( int keycode, int keydown ) {		// keyboard/keypad event
        if( keydown ) {
            if( keycode == VK_MEDIA_PREV_TRACK ) {     // (REW) jump backward
                if( m_videomode == VIDEO_MODE_LIVE ) { // live mode
                }
                else if( m_videomode == VIDEO_MODE_PLAYBACK ) {   // playback
                    if( m_decode_runmode == DECODE_MODE_PLAY || m_decode_runmode == DECODE_MODE_PAUSE ) {
                        if( keycode == m_keypad_up && // pressed same key, double the jump timer
                            (g_timetick-m_keytime)/1000 < screen_play_doublejumptimer ) {
                            if( m_jumptime < screen_play_maxjumptime ) {
                                m_jumptime *= 2 ;
                            }
                        }
                        else {
                            m_jumptime = screen_play_jumptime ;
                        }
                        m_decode_runmode = DECODE_MODE_BACKWARD ;  //  jump backward.
                    }
                    m_icon->seticon( "rwd.pic" );
                    settimer( 3000, keycode ) ;
                }
                else if( m_videomode == VIDEO_MODE_MENU ) {   // menu mode
#ifdef PWII_APP
                    pwii_menu * pmenu ;                // menu
                    pmenu = (pwii_menu *)findwindow( ID_MENU_SCREEN );
                    if( pmenu ) {
                        pmenu->prev();
                    }
#endif      // PWII_APP
                }
            }
            else if( keycode == VK_MEDIA_NEXT_TRACK ) { //  (FF) jump forward.
                if( m_videomode == VIDEO_MODE_LIVE ) { 	// live mode, switch audio on/off
                    screen_liveaudio = !screen_liveaudio ;
                    //	 SetPreviewAudio(MAIN_OUTPUT,eagle32_hikhandle(m_playchannel),screen_liveaudio);
                    dvr_screen_audio(screen_sockfd, m_eaglechannel, screen_liveaudio) ;
                }
                else if( m_videomode == VIDEO_MODE_PLAYBACK ) {   // playback
                    if( m_decode_runmode == DECODE_MODE_PLAY || m_decode_runmode == DECODE_MODE_PAUSE ) {
                        if( keycode == m_keypad_up && // pressed same key, double the jump timer
                            (g_timetick-m_keytime)/1000 < screen_play_doublejumptimer ) {
                            if( m_jumptime < screen_play_maxjumptime ) {
                                m_jumptime *= 2 ;
                            }
                        }
                        else {
                            m_jumptime = screen_play_jumptime ;
                        }
                        m_decode_runmode = DECODE_MODE_FORWARD ;  //  jump backward.
                        settimer( 3000, keycode ) ;
                    }
                    m_icon->seticon( "fwd.pic" );
                    settimer( 3000, keycode );
                }
                else if( m_videomode == VIDEO_MODE_MENU ) {   // menu mode
#ifdef PWII_APP
                    pwii_menu * pmenu ;                // menu
                    pmenu = (pwii_menu *)findwindow( ID_MENU_SCREEN );
                    if( pmenu ) {
                        pmenu->next();
                    }
#endif      // PWII_APP
                }
            }
            else if( keycode == VK_MEDIA_PLAY_PAUSE ) { //  in live mode, jump to playback mode, in playback mode, switch between pause and play
                if( m_videomode == VIDEO_MODE_LIVE ) { // live mode, set to playback mode.
                    startdecode(m_playchannel);
                    m_videomode = VIDEO_MODE_PLAYBACK ;
                }
                else if( m_videomode == VIDEO_MODE_PLAYBACK ) {   // playback
                    // switch between play -> slow -> pause
                    if( m_decode_runmode == DECODE_MODE_PLAY ) {
                        m_decode_runmode = DECODE_MODE_PAUSE ;
                        m_icon->seticon( "pause.pic" );
                    }
                    else {
                        m_decode_runmode = DECODE_MODE_PLAY ;
                        m_icon->seticon( "play.pic" );
                    }
                }
                else if( m_videomode == VIDEO_MODE_MENU ) {   // menu mode
#ifdef PWII_APP
                    pwii_menu * pmenu ;                // menu
                    pmenu = (pwii_menu *)findwindow( ID_MENU_SCREEN );
                    if( pmenu ) {
                        pmenu->cancel();
                    }
#endif
                }
            }
            else if( keycode == VK_MEDIA_STOP ) {      // stop playback if in playback mode, enter menu mode if in live view mode
                if( m_videomode ==  VIDEO_MODE_LIVE ) {
                    /*
#ifdef	PWII_APP
                        stopliveview();
                        dio_pwii_lcd(0);
                        m_videomode = VIDEO_MODE_LCDOFF ;
                        settimer( 5000, keycode );
#endif
*/
#ifdef PWII_APP
                    // bring up menu mode
                    startmenu();
#endif      /// PWII_APP
                }
                else if( m_videomode == VIDEO_MODE_MENU ) {   // menu mode
#ifdef PWII_APP
                    pwii_menu * pmenu ;                // menu
                    pmenu = (pwii_menu *)findwindow( ID_MENU_SCREEN );
                    if( pmenu ) {
                        pmenu->enter();
                    }
#endif      // PWII_APP
                }
                else if( m_videomode == VIDEO_MODE_PLAYBACK ) {   // playback
                    m_icon->seticon( "stop.pic" );
                    if( m_decode_runmode == DECODE_MODE_PLAY || m_decode_runmode == DECODE_MODE_PAUSE ) {
                        startliveview(m_playchannel);
                    }
                    // settimer( 5000, keycode );
                }
                else if( m_videomode == VIDEO_MODE_LCDOFF ) {   // lcd off
#ifdef	PWII_APP
                    dio_pwii_lcd( 1 ) ;           // turn lcd on
#endif
                    startliveview(m_playchannel);
                }
                else if( m_videomode == VIDEO_MODE_STANDBY ) {   // blackout mode
#ifdef	PWII_APP
                    dio_pwii_standby( 0 );        // jump out of standby
                    dio_pwii_lcd( 1 ) ;           // turn lcd on
#endif
                    startliveview(m_playchannel);
                }
            }
            else if( keycode == VK_PRIOR ) { //  PR
                if( m_videomode==VIDEO_MODE_LIVE ) {   // live
                    // switch live view channel
                    int retry = cap_channels ;
                    int ch = m_playchannel ;
                    while( retry-->0 ) {
                        ch -= ScreenNum ;
                        while( ch<0 ) {
                            ch+=cap_channels ;
                        }
                        if( cap_channel[ch]->enabled() ) {
                            startliveview(ch);
                            break;
                        }
                    }
                }
                else if( m_videomode == VIDEO_MODE_PLAYBACK ) {   // playback
                    if( m_decode_runmode == DECODE_MODE_PLAY || m_decode_runmode == DECODE_MODE_PAUSE ) {
                        m_decode_runmode = DECODE_MODE_PRIOR ;
                        m_icon->seticon( "rwd.pic" );
                    }
                }
                else if( m_videomode == VIDEO_MODE_MENU ) {   // menu mode
#ifdef PWII_APP
                    pwii_menu * pmenu ;                // menu
                    pmenu = (pwii_menu *)findwindow( ID_MENU_SCREEN );
                    if( pmenu ) {
                        pmenu->prevpage();
                    }
#endif      // PWII_APP
                }
            }
            else if( keycode == VK_NEXT ) { //  NX
                if( m_videomode==VIDEO_MODE_LIVE ) {   // live, black LCD
                    // switch live view channel
                    int retry = cap_channels ;
                    int ch = m_playchannel ;
                    while( retry-->0 ) {
                        ch += ScreenNum ;
                        while( ch>=cap_channels ) {
                            ch-=cap_channels ;
                        }
                        if( cap_channel[ch]->enabled() ) {
                            startliveview(ch);
                            break;
                        }
                    }
                }
                else if( m_videomode == VIDEO_MODE_PLAYBACK ) {   // playback
                    if( m_decode_runmode == DECODE_MODE_PLAY || m_decode_runmode == DECODE_MODE_PAUSE ) {
                        m_decode_runmode = DECODE_MODE_NEXT ;
                        m_icon->seticon( "fwd.pic" );
                    }
                }
                else if( m_videomode == VIDEO_MODE_MENU ) {   // menu mode
#ifdef PWII_APP
                    pwii_menu * pmenu ;                // menu
                    pmenu = (pwii_menu *)findwindow( ID_MENU_SCREEN );
                    if( pmenu ) {
                        pmenu->nextpage();
                    }
#endif      // PWII_APP
                }
            }
            //                else if( keycode == VK_EM ) { //  event marker key
            //                    m_icon->seticon( "tm.pic" );
            //                }
            else if( keycode == VK_POWER ) {                //  LCD power on/off and blackout
                if( m_videomode <= VIDEO_MODE_PLAYBACK ) {  // playback
                    stop();
#ifdef	PWII_APP
                    dio_pwii_lcd(0);
                    m_videomode = VIDEO_MODE_LCDOFF ;
#endif
                    settimer( 5000, keycode );
                }
                else if( m_videomode == VIDEO_MODE_LCDOFF ) {   // lcd off
#ifdef	PWII_APP
                    dio_pwii_lcd( 1 ) ;           // turn lcd on
#endif
                    startliveview(m_playchannel);
                }
                else if( m_videomode == VIDEO_MODE_STANDBY ) {   // blackout mode
#ifdef	PWII_APP
                    dio_pwii_standby( 0 );        // jump out of standby
                    dio_pwii_lcd( 1 ) ;           // turn lcd on
#endif
                    startliveview(m_playchannel);
                }
            }
            else if( keycode == VK_SILENCE ) { //  switch audio on <--> off
                if( m_videomode == VIDEO_MODE_LIVE ) { // live mode
                    screen_liveaudio = !screen_liveaudio ;
                    //						SetPreviewAudio(MAIN_OUTPUT,eagle32_hikhandle(m_playchannel),screen_liveaudio);
                    dvr_screen_audio(screen_sockfd, m_eaglechannel, screen_liveaudio) ;
                }
                // else {
                //   SetDecodeAudio(MAIN_OUTPUT, m_playchannel%ScreenNum+1, 1);
                // }
            }
            // speaker mute status
            else if( keycode == VK_MUTE ) {     // Mute key
                m_icon->seticon( "mute.pic" );
            }
            else if( keycode == VK_SPKON ) {     // Speaker on key
                m_icon->seticon( "spkon.pic" );
            }
        }
        else {                  // key up
            m_keytime = g_timetick ;
            killtimer( keycode );           // delete timer
            m_keypad_up = keycode ;
            m_icon->seticon( NULL );
        }
        updatestatus();
        return 1 ;      // key been procesed.
    }

    virtual int onfocus( int focus ) {
        int i;
        video_screen * vs ;
        for( i=0; i<ScreenNum; i++ ) {
            vs = (video_screen *) m_parent->findwindow( i ) ;
            if( vs==NULL ) break;
            if( vs==this ) {
                select(1);
            }
            else {
                vs->select(0);
            }
        }
        redraw();
        return 0 ;
    }

};


class controlpanel : public window {
protected:
    UINT32 m_backgroundcolor;
    window * firstchild ;

public:
    controlpanel( window * parent, int x, int y, int w, int h )
        : window( parent, 0, x, y, w, h ) {
        // create all controls
        hide();
    }

    ~controlpanel() {
    }

    // event handler
protected:
    virtual void paint() {						// paint window
        setpixelmode (DRAW_PIXELMODE_COPY);
        setcolor ( COLOR( 0xe0, 0x10, 0x10, 0xff) ) ;	// selected border
        resource pic ;
        int x=0 ;
        pic.open( "rwd.pic" );
        drawbitmap( pic, x, 0, 0, 0, 0, 0 );
        pic.open( "play.pic" );
        drawbitmap( pic, x+=80, 0, 0, 0, 0, 0 );
        pic.open( "fwd.pic" );
        drawbitmap( pic, x+=80, 0, 0, 0, 0, 0 );
        pic.open( "pause.pic" );
        drawbitmap( pic, x+=80, 0, 0, 0, 0, 0 );
        pic.open( "stop.pic" );
        drawbitmap( pic, x+=80, 0, 0, 0, 0, 0 );
    }

    virtual void oncommand( int id, void * param ) {	// children send command to parent
    }

    // buttons : low 4 bits=button status, high 4 bits=press/release
    virtual void onmouse( int x, int y, int buttons ) {		// mouse event
        if( (buttons & 0x11) == 0x11 ) {                     // click
            if( y<70 ) {
                if( x<80 ) {                // backward
                }
                else if( x<160 ) {          // play
                }
                else if( x<160 ) {          // fwd
                }
                else if( x<160 ) {          // pause
                }
                else if( x<160 ) {          // stop
                }
            }
        }
    }

    virtual void onmouseleave() {							// mouse pointer leave window
        settimer( 40000, 1 );
    }

    virtual void onmouseenter() {							// mouse pointer enter window
    }

    // keyboard or button input, return 1: processed (key event don't go to children), 0: no processed
    virtual int onkey( int keycode, int keydown ) {		    // keyboard/keypad event
        return 0 ;
    }

    virtual void ontimer( int id ) {
        hide();
        return ;
    }


};

class iomsg : public window {
public:
    iomsg( window * parent, int id, int x, int y, int w, int h ) :
        window( parent, id, x, y, w, h )
    {
        settimer( 2000, 1 );
    }

    // event handler
protected:
    virtual void paint() {
        char * iomsg = dio_getiomsg() ;
        if( iomsg && *iomsg ) {
            int l=strlen(iomsg);
            if( l>0 ) {
                setpixelmode (DRAW_PIXELMODE_COPY);
                resource font("mono32b.font");
                int h = font.fontheight();
                int w = font.fontwidth();
                setcolor (COLOR(0,50,240,128)) ;
                fillrect ( 0, 0, w*l, h );
                setcolor(COLOR(240,240,80,200));
                drawtext( 0, 0, iomsg, font );
            }
        }
    }

    virtual void ontimer( int id ) {
        char * iomsg = dio_getiomsg() ;
        if( iomsg!=NULL && *iomsg != 0 ) {
            redraw();
        }
        settimer( 2000, 1 );
        return ;
    }
};

class mainwin : public window {
protected:
    UINT32 m_backgroundcolor;
    window * firstchild ;
    cursor * mycursor ;
    controlpanel * panel ;
    iomsg * iomessage ;

public:
    mainwin(config &dvrconfig) :
        window( NULL, 0, 0, 0, 720, 480 )
    {
        int startchannel ;
        video_screen * vs ;
        m_pos.x=0 ;
        m_pos.y=0 ;
        m_pos.w = draw_screenwidth ();
        m_pos.h = draw_screenheight ();

        if( ScreenNum==1 ) {
            startchannel = dvrconfig.getvalueint("VideoOut", "startchannel" );
            vs = new video_screen( this, ID_VIDEO_SCREEN, ScreenMarginX, ScreenMarginY, m_pos.w-ScreenMarginX*2, m_pos.h-ScreenMarginY*2 );
            vs->liveview( startchannel ) ;
            vs->select(1);
        }
        else if( ScreenNum==2 ) {
            vs = new video_screen( this, ID_VIDEO_SCREEN, ScreenMarginX, m_pos.h/4, m_pos.w/2-ScreenMarginX, m_pos.h/2-ScreenMarginY );
            vs->liveview( 0 ) ;
            vs->select(1);
            vs = new video_screen( this, ID_VIDEO_SCREEN+1,     m_pos.w/2, m_pos.h/4, m_pos.w/2-ScreenMarginX, m_pos.h/2-ScreenMarginY );
            vs->liveview( 1 ) ;
        }
        else {         // ScreenNum==4
            ScreenNum = 4 ;
            vs = new video_screen( this, ID_VIDEO_SCREEN, ScreenMarginX, ScreenMarginY, m_pos.w/2-ScreenMarginX, m_pos.h/2-ScreenMarginY );
            vs->liveview( 0 ) ;
            vs->select(1);
            vs = new video_screen( this, ID_VIDEO_SCREEN+1,     m_pos.w/2, ScreenMarginY, m_pos.w/2-ScreenMarginX, m_pos.h/2-ScreenMarginY );
            vs->liveview( 1 ) ;
            vs = new video_screen( this, ID_VIDEO_SCREEN+2, ScreenMarginX,     m_pos.h/2, m_pos.w/2-ScreenMarginX, m_pos.h/2-ScreenMarginY );
            vs->liveview( 2 ) ;
            vs = new video_screen( this, ID_VIDEO_SCREEN+3,     m_pos.w/2,     m_pos.h/2, m_pos.w/2-ScreenMarginX, m_pos.h/2-ScreenMarginY );
            vs->liveview( 3 ) ;
        }

        string control ;
        control = dvrconfig.getvalue("VideoOut", "control" );
        int ctrl_x, ctrl_y, ctrl_w, ctrl_h ;
        ctrl_x=0 ;
        ctrl_y=0 ;
        ctrl_w=0 ;
        ctrl_h=0 ;
        sscanf(control, "%d,%d,%d,%d", &ctrl_x, &ctrl_y, &ctrl_w, &ctrl_h );
        if( ctrl_x>0 && ctrl_y>0 && ctrl_w>0 && ctrl_h>0 ) {
            panel = new controlpanel( this, ctrl_x, ctrl_y, ctrl_w, ctrl_h );
        }
        else {
            panel = NULL ;
        }

        // text message on top
        iomessage = new iomsg( this, 1, 50, m_pos.h/2-20, m_pos.w-100, 40 );
    }

    ~mainwin(){

    }

    // event handler
protected:
    virtual void paint() {
        //            setcolor (COLOR( 0, 0, 0x10, 0xff) );
        //            setpixelmode (DRAW_PIXELMODE_COPY);
        //            fillrect ( 0, 0, m_pos.w, m_pos.h );
    }

    // buttons : low 4 bits=button status, high 4 bits=press/release
    virtual void onmouse( int x, int y, int buttons ) {		// mouse event
    }

};

int screen_setliveview( int channel )
{
    video_screen * vs ;
    if( topwindow!=NULL ) {
        vs = (video_screen *) topwindow->findwindow( ID_VIDEO_SCREEN ) ;
        if( vs ) {
            vs->liveview( channel );
            return 1 ;
        }
    }
    return 0;
}

int screen_menu( int level )
{
    video_screen * vs ;

#ifdef PWII_APP
    if( screen_nomenu ) {  //  do not show police id input menu on startup
        return 0 ;
    }
#endif
    if( topwindow!=NULL ) {
        vs = (video_screen *) topwindow->findwindow( ID_VIDEO_SCREEN ) ;
        if( vs ) {
            vs->openmenu( level );
            return 1 ;
        }
    }
    return 0;
}

int screen_mouseevent(int x, int y, int buttons)
{
    if( topwindow!=NULL ) {
        topwindow->mouseevent( x, y, buttons );
    }
    return 1;
}

int screen_key( int keycode, int keydown )
{
#ifdef PWII_APP
    void rec_pwii_toggle_rec(int ch) ;
    void rec_pwii_recon();
    void rec_pwii_recoff();
#endif  // PWII_APP

    NET_DPRINT("screen_key  code : %04x  down: %d\n", keycode, keydown );

    if( keycode == (int) VK_TM ) {
        if( keydown ) {
            screen_setliveview(-1);         // switch to live view
            dvr_log("TraceMark pressed!");
            event_tm = 1 ;
            rec_update();
        }
        else {
            dvr_log("TraceMark released!");
            event_tm = 0 ;
         }
    }
#ifdef PWII_APP
    else if( keycode==(int)VK_RECON ) {     // record on, (turn on all record)
        if( keydown ) {
            screen_setliveview(-1);
            rec_pwii_recon();
            dvr_log("RECON pressed!");
        }
    }
    else if( keycode==(int)VK_RECOFF ) {     // record on, (turn on all record)
        if( keydown ) {
            rec_pwii_recoff();
            dvr_log("RECOFF pressed!");
        }
    }
    else if( keycode>=(int)VK_C1 && keycode<=(int)VK_C8 ) {     // record C1-C8
        if( keydown ) {
            screen_setliveview(keycode-(int)VK_C1);             // start live view this camera
            rec_pwii_toggle_rec( keycode-(int)VK_C1 ) ;
            dvr_log("C%d pressed!", keycode-(int)VK_C1+1);
        }
    }
    else if( keycode==(int)VK_LP ) {                             // LP key
        if( keydown ) {
            screen_setliveview(pwii_front_ch);                   // start front camera
            dio_pwii_lpzoomin( 1 );
            dvr_log("LP pressed!");
        }
        else {
            dio_pwii_lpzoomin( 0 );
            dvr_log("LP released!");
        }
    }
    /*
    else if( keycode==(int)VK_MUTE ) {                           // Mute key
        if( keydown ) {
            dvr_log("MUTE pressed!");
        }
        else {
            dvr_log("MUTE released!");
        }
        screen_update();    // just do the screen update for now. (Hardware do mute already)
    }
    */
#endif
    else if( topwindow ) {
//        dvr_log("Key code 0x%02x %s.", keycode, keydown?"pressed":"released" );
        topwindow->key( keycode, keydown );
    }
    return 0;
}

int screen_draw()
{
    int cursorshow ;
    if( window::gredraw ) {
        window::gredraw=0 ;
        cursorshow = (Screen_cursor!=NULL)&&(Screen_cursor->isshow());
        if( cursorshow ) {
            Screen_cursor->hide();		// hide mouse cursor
        }

        topwindow->draw();			// do all the painting

        draw_refresh();				// let eaglesvr refresh screen. (eagle34 only)

        if( cursorshow ) {
            Screen_cursor->show();		// show mouse cursor
        }
    }
    return 0;
}

void screen_update()
{
    if( topwindow ) {
        topwindow->redraw();
        screen_draw();
    }
}

struct mouse_event {
    int x, y ;
    int buttons ;
    int time ;
} ;

static int readok(int fd, int usdelay)
{
    struct timeval timeout = { 0, usdelay };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    if (select(fd + 1, &fds, NULL, NULL, &timeout) > 0) {
        return FD_ISSET(fd, &fds);
    } else {
        return 0;
    }
}

static int getmouseevent( struct mouse_event * mev, int usdelay )
{
    if( mousedev>0 && Screen_cursor ) {
        static int mouseeventtime ;
        int offsetx=0, offsety=0;
        int x=mousex, y=mousey ;
        int buttons=mousebuttons ;
        UINT8 mousebuf[8] ;

        if( readok(mousedev, usdelay) ) {
            if( read(mousedev, mousebuf, 8) >=3 &&
               ( (mousebuf[0] & 0xc8 )==0x8 ) )
            {
                if( mousebuf[0] & 0x10 ) {
                    offsetx = ((-1)&(~0x0ff)) | mousebuf[1] ;
                }
                else {
                    offsetx = mousebuf[1] ;
                }
                if( mousebuf[0] & 0x20 ) {
                    offsety = ((-1)&(~0x0ff)) | mousebuf[2] ;
                }
                else {
                    offsety = mousebuf[2] ;
                }
                buttons = mousebuf[0] & 0x7 ;
                x+=offsetx ;
                if( x<0 )x=0 ;
                if( x>mousemaxx ) x=mousemaxx ;
                y-=offsety ;
                if( y<0 )y=0;
                if( y>mousemaxy ) y=mousemaxy ;
            }
        }
        if( x!=mousex || y!=mousey || buttons!=mousebuttons ) {
            mev->x = x ;
            mev->y = y ;
            // buttons value, low 4 bits=button status, high 4 bits=press/release
            mev->buttons = buttons | ( (buttons^mousebuttons)<<4 ) ;
            if( mousex!=x || mousey!=y ) {
                mev->buttons |= 0x100 ;
            }
            mousex=x ;
            mousey=y ;
            mousebuttons=buttons ;
            mouseeventtime = g_timetick ;
            mev->time = mouseeventtime ;

            if( !Screen_cursor->isshow() ) {
                Screen_cursor->show() ;
            }
            Screen_cursor->move( x, y ) ;

            return 1 ;
        }
        if( (g_timetick-mouseeventtime) > 60000 ) {     // mouse idling for 1 min
            Screen_cursor->hide();
        }
    }
    return 0 ;
}

struct key_event {
    int keycode ;
    int keydown ;
} ;

static int getkeyevent( struct key_event * kev )
{
#ifdef    PWII_APP
    return dio_getpwiikeycode(&(kev->keycode), &(kev->keydown));
#else
    return 0 ;
#endif
}

// called periodically by main process
int screen_io(int usdelay)
{
    struct mouse_event mev ;
    struct key_event kev ;

    // key pad
    if( getkeyevent( &kev ) ) {
        screen_key ( kev.keycode, kev.keydown );
        return 1 ;
    }

    // mouse
    if( getmouseevent( &mev, usdelay ) ) {
        screen_mouseevent(mev.x, mev.y, mev.buttons );
        return 1 ;
    }

    // window timer
    topwindow->checktimer();

    screen_draw();

    return 0;
}

int screen_onframe( cap_frame * capframe )
{
    if( screen_liveview_handle>0 &&
       capframe->channel==screen_liveview_channel)
    {
//        InputAvData( screen_liveview_handle, capframe->framedata, capframe->framesize );
    }
    return 0;
}

// initialize screen
void screen_init(config &dvrconfig)
{
    string v ;
    int i;

    ScreenNum = dvrconfig.getvalueint("VideoOut", "screennum" );
    if( ScreenNum!=4 && ScreenNum!=2 ) {		// support 4, 2, 1 screen mode
        ScreenNum=1 ;			// set to one screen mode by default
    }

    // NTSC : 1  , PAL : 2
    ScreenStandard = dvrconfig.getvalueint("VideoOut", "standard" );
    if( ScreenStandard != STANDARD_PAL ) {
        ScreenStandard = STANDARD_NTSC ;
    }

    screen_liveaudio = 1;		// enable audio by default

    screen_play_jumptime = dvrconfig.getvalueint("VideoOut", "jumptime" );
    if( screen_play_jumptime < 5 ) screen_play_jumptime=5 ;

    screen_play_maxjumptime = dvrconfig.getvalueint("VideoOut", "maxjumptime" );
    if( screen_play_jumptime > 3600 ) screen_play_maxjumptime=3600 ;

    screen_play_doublejumptimer = dvrconfig.getvalueint("VideoOut", "doublejumptimer" );
    if( screen_play_doublejumptimer <= 0 || screen_play_doublejumptimer>30 ) {
        screen_play_doublejumptimer = 5 ;
    }

    screen_play_cliptime = dvrconfig.getvalueint("VideoOut", "cliptime" );
    if( screen_play_cliptime < 60 ) screen_play_cliptime = 60 ;

    v = dvrconfig.getvalue("VideoOut", "resource" );
    if( v.length()>0 ) {
        strncpy( resource::resource_dir, v, 128 );
    }

#ifdef PWII_APP
    screen_rearaudio = dvrconfig.getvalueint("VideoOut", "rearaudio" );
    screen_keepcapture = dvrconfig.getvalueint("VideoOut", "keepcapture" );
    screen_nostartmenu = dvrconfig.getvalueint("VideoOut", "nostartmenu" );
    screen_nomenu = dvrconfig.getvalueint("VideoOut", "nomenu" );
#endif

    // initial eaglesvr server
    v = dvrconfig.getvalue("VideoOut", "server" );
    if( v.length()==0 ) {
        v="127.0.0.1" ;			// default to connect localhost
    }
    int videoport = dvrconfig.getvalueint("VideoOut", "port" ) ;
    if( videoport==0 ) {
        videoport=EAGLESVRPORT ;
    }

    for( i=0; i<5; i++ ) {		// retry 5 times
        screen_sockfd = net_connect(v, videoport );
        if( screen_sockfd > 0 ) {
            if( dvr_screen_setmode( screen_sockfd, ScreenStandard, ScreenNum ) ) {
                dvr_log( "Screen connected, set screen number %d.");
                break;
            }
            closesocket(screen_sockfd);
            sleep(1);
        }
    }

    int ch ;
    for(ch=0; ch<4; ch++) {
        dvr_screen_stop( screen_sockfd, ch);
    }

    // select Video output format to NTSC
    draw_init(ScreenStandard);

    topwindow = new mainwin(dvrconfig);
    topwindow->redraw();
    window::focuswindow = NULL ;

    // initialize mouse and pointer
    mousemaxx = draw_screenwidth ()-1 ;
    mousemaxy = draw_screenheight ()-1 ;
    mousex = mousemaxx/2 ;
    mousey = mousemaxy/2 ;
    mousebuttons = 0 ;
    window::mouseonwindow = NULL ;
    Screen_cursor = new cursor( "arrow.cursor", 0, 0 );

    mousedev = open( mousedevname, O_RDONLY );

    screen_liveview_channel = -1 ;
    screen_liveview_handle = 0 ;

}

// screen finish, clean up
void screen_uninit()
{
    // disable live view on ip camera
    screen_liveview_channel = -1 ;
    screen_liveview_handle = 0 ;

    // remove cursor
    delete Screen_cursor ;
    Screen_cursor = NULL ;

    // close mouse device
    if( mousedev>0 ) {
        close( mousedev );
        mousedev = -1 ;
    }
    mousebuttons = 0 ;

    // delete all windows
    if( topwindow ) {
        delete topwindow ;
        topwindow = NULL ;
        window::focuswindow = NULL ;
        window::mouseonwindow = NULL ;
    }

    draw_finish();
    closesocket(screen_sockfd);
}

#endif	// NO_ONBOARD_EAGLE

