
#include "../../cfg.h"

#include <stdio.h>

#include "../../dvrsvr/crypt.h"
#include "../../dvrsvr/genclass.h"
#include "../../dvrsvr/cfg.h"

char dvrconfigfile[]="/etc/dvr/dvr.conf" ;
char tzfile[] = "tz_option" ;
char * dvrworkdir = "/davinci/dvr" ;

int main()
{
    char * zoneinfobuf ;
    string s ;
    char * p ;
    int l;
    int i;
    config_enum enumkey ;
    config dvrconfig(dvrconfigfile);
    string tzi ;
    string value ;
    int ivalue ;
    char buf[100] ;
    FILE * fvalue ;
    FILE * f_id ;
    int sensor_number ;
    
    // sensor number
    sensor_number=dvrconfig.getvalueint("io", "inputnum");
    if( sensor_number<=0 || sensor_number>32 ) {
        sensor_number=6 ;                   // default
    }
   
    fvalue = fopen("sensor_number", "w");
    if( fvalue ) {
        fprintf(fvalue, "%d", sensor_number );
        fclose( fvalue );
    }

#ifdef PWII_APP    
    // write mf id
    value = dvrconfig.getvalue("system", "mfid" );
    if( value.length()>0 ) {
        fvalue = fopen("manufacture_id", "w");
        if( fvalue ) {
            fprintf(fvalue, "%s", value.getstring() );
            fclose( fvalue );
        }
    }
#endif

#ifdef 	TVS_APP
       // write TVS mf id
    value = dvrconfig.getvalue("system", "tvsmfid" );
    if( value.length()>0 ) {
        fvalue = fopen("tvs_manufacture_id", "w");
        if( fvalue ) {
            fprintf(fvalue, "%s", value.getstring() );
            fclose( fvalue );
        }
        fvalue = fopen("tvs_ivcs_prefix", "w");
        if( fvalue ) {
            fprintf(fvalue, "%s", value.getstring()+2 );
            fclose( fvalue );
        }
    } 
#endif

    // write system_value
    fvalue = fopen( "system_value", "w");
    if( fvalue ) {

        // JSON head
        fprintf(fvalue, "{" );

        // dvr_server_name 
//        value = dvrconfig.getvalue("system","hostname");
//        if( value.length()>0 ) {
//            fprintf(fvalue, "\"dvr_server_name\":\"%s\",", value.getstring() );
//        }
        
        // pwii
#ifdef PWII_APP
        value = dvrconfig.getvalue("system","id1");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"vehicleid\":\"%s\",", value.getstring() );
        }
            
        value = dvrconfig.getvalue("system","id2");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"district\":\"%s\",", value.getstring() );
        }

        value = dvrconfig.getvalue("system","serial");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"unit\":\"%s\",", value.getstring() );
        }
#endif

#ifdef TVS_APP
        if( value.length()>0 ) {
            fprintf(fvalue, "\"tvs_licenseplate\":\"%s\",", value.getstring() );
        }
            
        value = dvrconfig.getvalue("system","tvs_medallion");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"tvs_medallion\":\"%s\",", value.getstring() );
        }

        value = dvrconfig.getvalue("system","tvs_ivcs_serial");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"tvs_ivcs_serial\":\"%s\",", value.getstring() );
        }
#endif
        // dummy password
        fprintf(fvalue, "\"password\":\"********\",");
      
        // dvr_time_zone
        value = dvrconfig.getvalue("system", "timezone");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"dvr_time_zone\":\"%s\",", value.getstring() );
            FILE * ftimezone ;
            ftimezone = fopen("timezone", "w");
            if( ftimezone ) {
                fprintf(ftimezone, "%s", value.getstring() );
                fclose( ftimezone );
            }

            ftimezone = fopen("tz_env", "w");
            if( ftimezone ) {

                // time zone enviroment
                tzi=dvrconfig.getvalue( "timezones", value.getstring() );
                if( tzi.length()>0 ) {
                    p=strchr(tzi.getstring(), ' ' ) ;
                    if( p ) {
                        *p=0;
                    }
                    p=strchr(tzi.getstring(), '\t' ) ;
                    if( p ) {
                        *p=0;
                    }
                    fprintf(ftimezone, "%s", tzi.getstring() );
                }
                else {
                    fprintf(ftimezone, "%s", value.getstring() );
                }
                fclose( ftimezone );
            }
        }

        // shutdown_delay
        value = dvrconfig.getvalue("system", "shutdowndelay");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"shutdown_delay\":\"%s\",", value.getstring() );
        }

        // standbytime
        value = dvrconfig.getvalue("system", "standbytime");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"standbytime\":\"%s\",", value.getstring() );
        }

        
        // file_size
        value = dvrconfig.getvalue("system", "maxfilesize");
        if( value.length()>0 ) {
            l=value.length();
            p=value.getstring();
            if( p[l-1]=='M' ) {         // use Mega bytes only
                p[l-1]=0;
            }
            fprintf(fvalue, "\"file_size\":\"%s\",", p );
        }
        
        // file_time
        value = dvrconfig.getvalue("system", "maxfilelength");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"file_time\":\"%s\",", value.getstring() );
        }
        
        // minimun_disk_space
        value = dvrconfig.getvalue("system", "mindiskspace");
        if( value.length()>0 ) {
            l=value.length();
            p=value.getstring();
            if( p[l-1]=='M' ) {         // use Mega bytes only
                p[l-1]=0;
            }
            fprintf(fvalue, "\"minimun_disk_space\":\"%s\",", p );
        }

        // Max file length
        value = dvrconfig.getvalue("system", "maxfilelength");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"file_time\":\"%s\",", value.getstring() );
        }
        
        // pre_lock_time
        value = dvrconfig.getvalue("eventmarker", "prelock");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"pre_lock_time\":\"%s\",", value.getstring() );
        }
        
        // post_lock_time
        value = dvrconfig.getvalue("eventmarker", "postlock");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"post_lock_time\":\"%s\",", value.getstring() );
        }
        
        // no rec playback
        ivalue = dvrconfig.getvalueint("system", "norecplayback");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"norecplayback\":\"on\"," );
        }

        // no rec liveview
        ivalue = dvrconfig.getvalueint("system", "noreclive");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"noreclive\":\"on\"," );
        }

        
        // en_file_encryption
        int file_encrypt = dvrconfig.getvalueint("system", "fileencrypt");
        if( file_encrypt>0 ) {
            fprintf(fvalue, "\"en_file_encryption\":\"on\"," );
        }
        
        fprintf( fvalue, "\"file_password\":\"********\",");

        
        // gpslog enable
        ivalue = dvrconfig.getvalueint("glog", "gpsdisable");
        if( ivalue == 0 ) {
            fprintf(fvalue, "\"en_gpslog\":\"on\",");
        }
        
        // gps port
        value = dvrconfig.getvalue("glog", "serialport");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gpsport\":\"%s\",", value.getstring() );
        }
        
        // gps baud rate
        value = dvrconfig.getvalue("glog", "serialbaudrate");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gpsbaudrate\":\"%s\",", value.getstring() );
        }
        
        // g-force data
        // 

        value = dvrconfig.getvalue( "io", "gsensor_forward");	
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_forward\":\"%s\",", value.getstring() );
        }
        
        value = dvrconfig.getvalue( "io", "gsensor_upward");	
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_upward\":\"%s\",", value.getstring() );
        }

        value = dvrconfig.getvalue( "io", "gsensor_forward_trigger");	
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_forward_trigger\":\"%s\",", value.getstring() );
        }
        value = dvrconfig.getvalue( "io", "gsensor_backward_trigger");	
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_backward_trigger\":\"%s\",", value.getstring() );
        }
        value = dvrconfig.getvalue( "io", "gsensor_right_trigger");	
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_right_trigger\":\"%s\",", value.getstring() );
        }
        value = dvrconfig.getvalue( "io", "gsensor_left_trigger");	
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_left_trigger\":\"%s\",", value.getstring() );
        }
        value = dvrconfig.getvalue( "io", "gsensor_down_trigger");	
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_downward_trigger\":\"%s\",", value.getstring() );
        }
        value = dvrconfig.getvalue( "io", "gsensor_up_trigger");	
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_upward_trigger\":\"%s\",", value.getstring() );
        }
        
        ivalue=0 ;
        f_id = fopen( "/var/dvr/gsensor", "r" );
        if( f_id ) {
            fscanf( f_id, "%d", &ivalue);
            fclose(f_id);
        }
        if( ivalue ) {
            fprintf( fvalue, "\"gforce_available\":\"1\",");
        }
        else {
            fprintf( fvalue, "\"gforce_available\":\"0\",");
        }

#ifdef	PWII_APP
        // Video output selection
        ivalue = dvrconfig.getvalueint("VideoOut", "startchannel");
        fprintf( fvalue, "\"videoout\":\"%d\",", ivalue);
#endif

        fprintf( fvalue, "\"objname\":\"system_value\" }");
        
        fclose( fvalue );
    }

  
    // write camera_value
    int camera_number = dvrconfig.getvalueint("system", "totalcamera");
    if( camera_number<=0 ) {
        camera_number=2 ;
    }
    fvalue = fopen("camera_number", "w" );
    if( fvalue ) {
        fprintf(fvalue, "%d", camera_number );
        fclose( fvalue );
    }
    
    for( i=1; i<=camera_number; i++ ) {
        sprintf(buf, "camera_value_%d", i);
        fvalue=fopen(buf, "w");
        if(fvalue){
            char section[10] ;
            
            // JSON head
            fprintf(fvalue, "{" );
            
            fprintf(fvalue, "\"cameraid\":\"%d\",", i );
            fprintf(fvalue, "\"nextcameraid\":\"%d\",", i );
            
            sprintf(section, "camera%d", i);
            // enable_camera
            if( dvrconfig.getvalueint(section, "enable") ) {
                fprintf(fvalue, "\"enable_camera\":\"on\"," );
            }
            
            // camera_name
            value = dvrconfig.getvalue(section, "name");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"camera_name\":\"%s\",", value.getstring() );
            }
            
            // recording_mode
            value = dvrconfig.getvalue(section, "recordmode");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"recording_mode\":\"%s\",", value.getstring() );
            }            
            
            // Video Type for Special version 2. (less options)
            ivalue = dvrconfig.getvalueint(section, "videotype");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"videotype\":\"%d\",", ivalue );
                
                // Camera Type
                value = dvrconfig.getvalue(section, "cameratype");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"cameratype\":\"%s\",", value.getstring() );
                }            
                
            }            
            else {
                
                // # resolution, 0:CIF, 1:2CIF, 2:DCIF, 3:4CIF
                // resolution
                value = dvrconfig.getvalue(section, "resolution");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"resolution\":\"%s\",", value.getstring() );
                }            
                
                // frame_rate
                value = dvrconfig.getvalue(section, "framerate");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"frame_rate\":\"%s\",", value.getstring() );
                }            
                
                //          # Bitrate control
                //          bitrateen=1
                //          # Bitrate mode, 0:VBR, 1:CBR
                //          bitratemode=0
                //          bitrate=1200000
                // bit_rate_mode
                if( dvrconfig.getvalueint(section, "bitrateen")) {
                    fprintf(fvalue, "\"bit_rate_mode\":\"%d\",", dvrconfig.getvalueint(section,"bitratemode")+1 );
                }
                else {
                    fprintf(fvalue, "\"bit_rate_mode\":\"0\"," );
                }
                
                // bit_rate
                value = dvrconfig.getvalue(section, "bitrate");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"bit_rate\":\"%s\",", value.getstring() );
                }            
                
                //          # picture quality, 0:lowest, 10:highest
                // picture_quaity
                value = dvrconfig.getvalue(section, "quality");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"picture_quaity\":\"%s\",", value.getstring() );
                }         
            }
            
            // picture controls
            value = dvrconfig.getvalue(section, "brightness");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"brightness\":\"%s\",", value.getstring() );
            }         
            value = dvrconfig.getvalue(section, "contrast");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"contrast\":\"%s\",", value.getstring() );
            }         
            value = dvrconfig.getvalue(section, "saturation");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"saturation\":\"%s\",", value.getstring() );
            }         
            value = dvrconfig.getvalue(section, "hue");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"hue\":\"%s\",", value.getstring() );
            }         
                
            // trigger and osd
            for( ivalue=1; ivalue<=sensor_number; ivalue++ ) {
                char trigger[30], osd[30] ;
                int  itrig, iosd ;
                sprintf(trigger, "trigger%d", ivalue );
                sprintf(osd, "sensorosd%d", ivalue);
                itrig = dvrconfig.getvalueint( section, trigger );
                iosd  = dvrconfig.getvalueint( section, osd );
                if( iosd>0 ) {
                    fprintf(fvalue, "\"sensor%d_osd\":\"on\",", ivalue );
                }
                if( itrig>0 ) {
                    fprintf(fvalue, "\"sensor%d_trigger\":\"on\",", ivalue );
                    if( itrig & 1 ) {
                        fprintf(fvalue, "\"sensor%d_trigger_on\":\"on\",", ivalue );
                    }
                    if( itrig & 2 ) {
                        fprintf(fvalue, "\"sensor%d_trigger_off\":\"on\",", ivalue );
                    }
                    if( itrig & 4 ) {
                        fprintf(fvalue, "\"sensor%d_trigger_turnon\":\"on\",", ivalue );
                    }
                    if( itrig & 8 ) {
                        fprintf(fvalue, "\"sensor%d_trigger_turnoff\":\"on\",", ivalue );
                    }
                }

/*                
                sprintf(trigger, "trigger%d_prerec", ivalue );
                value = dvrconfig.getvalue(section, trigger );
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"trigger%d_prerec\":\"%s\",", ivalue, value.getstring() );
                }  

                sprintf(trigger, "trigger%d_postrec", ivalue );
                value = dvrconfig.getvalue(section, trigger );
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"trigger%d_postrec\":\"%s\",", ivalue, value.getstring() );
                }  
*/                
            }

            // pre_recording_time
            value = dvrconfig.getvalue(section, "prerecordtime");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"pre_recording_time\":\"%s\",", value.getstring() );
            }  

            // post_recording_time
            value = dvrconfig.getvalue(section, "postrecordtime");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"post_recording_time\":\"%s\",", value.getstring() );
            }  

            // show_gps
            ivalue = dvrconfig.getvalueint(section, "showgps");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"show_gps\":\"on\"," );
            }  
            
            // gpsunit
            // speed_display
            value = dvrconfig.getvalue(section, "gpsunit");
            fprintf(fvalue, "\"speed_display\":\"%s\",", value.getstring() );
    
            ivalue = dvrconfig.getvalueint(section, "showgpslocation");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"show_gps_coordinate\":\"on\"," );
            }  
  
            ivalue = dvrconfig.getvalueint(section, "show_medallion");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"show_medallion\":\"on\"," );
            }  
            ivalue = dvrconfig.getvalueint(section, "show_licenseplate");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"show_licenseplate\":\"on\"," );
            }  
            ivalue = dvrconfig.getvalueint(section, "show_ivcs");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"show_ivcs\":\"on\"," );
            }  
            ivalue = dvrconfig.getvalueint(section, "show_cameraserial");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"show_cameraserial\":\"on\"," );
            }  
            
             // record_alarm_mode
            value = dvrconfig.getvalue( section, "recordalarmpattern" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"record_alarm_mode\":\"%s\",", value.getstring() );
            }  
            value = dvrconfig.getvalue( section, "recordalarm" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"record_alarm_led\":\"%s\",", value.getstring() );
            }  
            
             // video_lost_alarm_mode
            value = dvrconfig.getvalue( section, "videolostalarmpattern" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"video_lost_alarm_mode\":\"%s\",", value.getstring() );
            }  
            value = dvrconfig.getvalue( section, "videolostalarm" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"video_lost_alarm_led\":\"%s\",", value.getstring() );
            }  
            
            // motion_alarm_mode
            value = dvrconfig.getvalue( section, "motionalarmpattern" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"motion_alarm_mode\":\"%s\",", value.getstring() );
            }  
            value = dvrconfig.getvalue( section, "motionalarm" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"motion_alarm_led\":\"%s\",", value.getstring() );
            }  

            // motion_sensitivity
            value = dvrconfig.getvalue( section, "motionsensitivity" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"motion_sensitivity\":\"%s\",", value.getstring() );
            }  
            
            // disableaudio
            ivalue = dvrconfig.getvalueint( section, "disableaudio" );
            if( ivalue ) {
                fprintf(fvalue, "\"disableaudio\":\"%s\",", "on" );
            }  

            // key interval
            value = dvrconfig.getvalue( section, "key_interval" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"key_interval\":\"%s\",", value.getstring() );
            }  
            
            // b_frames
            value = dvrconfig.getvalue( section, "b_frames" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"b_frames\":\"%s\",", value.getstring() );
            }  
            
            // p_frames
            value = dvrconfig.getvalue( section, "p_frames" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"p_frames\":\"%s\",", value.getstring() );
            }  

            fprintf(fvalue, "\"objname\":\"camera_value_%d\" }" ,i );
            
            fclose( fvalue );
        }
    }
    
    // make a soft link to camera_value
    remove("camera_value");
    symlink("camera_value_1", "camera_value");
    
    // write sensor_value
    fvalue = fopen("sensor_value", "w");
    if( fvalue ) {
        char section[20] ;
        
        // JSON head
        fprintf(fvalue, "{" );
        
        // sensor number
        fprintf(fvalue, "\"sensor_number\":\"%d\",", sensor_number );

        for( i=1; i<=sensor_number; i++ ) {
            sprintf( section, "sensor%d", i );
            value = dvrconfig.getvalue(section, "name") ;
            // write sensor value
            if( value.length()>0 ) {
                fprintf( fvalue, "\"%s_name\": \"%s\",", section, value.getstring() );
            }

            // inverted value
            if( dvrconfig.getvalueint(section, "inverted") ) {
                fprintf( fvalue, "\"%s_inverted\":\"on\",", section );
            }
            else {
                fprintf( fvalue, "\"%s_inverted\":\"off\",", section );
            }

            // event marker
            if( dvrconfig.getvalueint(section, "eventmarker") ) {
                fprintf( fvalue, "\"%s_eventmarker\":\"on\",", section );
            }
            else {
                fprintf( fvalue, "\"%s_eventmarker\":\"off\",", section );
            }

        }
        
        fprintf(fvalue, "\"objname\":\"sensor_value\" }" );
        fclose( fvalue );
    }
    
#ifndef TVS_APP    
    // write network_value
    fvalue = fopen("network_value", "w");
    if( fvalue ) {
        FILE * ipfile ;
        
        // JSON head
        fprintf(fvalue, "{" );

        // eth_ip
        sprintf(buf, "%s/eth_ip", dvrworkdir );
        ipfile = fopen( buf, "r" );
        if( ipfile ) {
            i=fread( buf, 1, sizeof(buf), ipfile );
            if( i>0 ) {
                while( i>0 ) {
                    if( buf[i-1]<' ' ) {
                        i--;
                    }
                    else {
                        break;
                    }
                }
                buf[i]=0;
                fprintf(fvalue, "\"eth_ip\":\"%s\",", buf );
            }
            fclose( ipfile );
        }

        // eth_mask
        sprintf(buf, "%s/eth_mask", dvrworkdir );
        ipfile = fopen( buf, "r" );
        if( ipfile ) {
            i=fread( buf, 1, sizeof(buf), ipfile );
            if( i>0 ) {
                while( i>0 ) {
                    if( buf[i-1]<' ' ) {
                        i--;
                    }
                    else {
                        break;
                    }
                }
                buf[i]=0;
                fprintf(fvalue, "\"eth_mask\":\"%s\",", buf );
            }
            fclose( ipfile );
        }

        // wifi_ip
        sprintf(buf, "%s/wifi_ip", dvrworkdir );
        ipfile = fopen( buf, "r" );
        if( ipfile ) {
            i=fread( buf, 1, sizeof(buf), ipfile );
            if( i>0 ) {
                while( i>0 ) {
                    if( buf[i-1]<' ' ) {
                        i--;
                    }
                    else {
                        break;
                    }
                }
                buf[i]=0;
                fprintf(fvalue, "\"wifi_ip\":\"%s\",", buf );
            }
            fclose( ipfile );
        }

        // wifi_mask
        sprintf(buf, "%s/wifi_mask", dvrworkdir );
        ipfile = fopen( buf, "r" );
        if( ipfile ) {
            i=fread( buf, 1, sizeof(buf), ipfile );
            if( i>0 ) {
                while( i>0 ) {
                    if( buf[i-1]<' ' ) {
                        i--;
                    }
                    else {
                        break;
                    }
                }
                buf[i]=0;
                fprintf(fvalue, "\"wifi_mask\":\"%s\",", buf );
            }
            fclose( ipfile );
        }

        // gateway
        sprintf(buf, "%s/gateway_1", dvrworkdir );
        ipfile = fopen( buf, "r" );
        if( ipfile ) {
            i=fread( buf, 1, sizeof(buf), ipfile );
            if( i>0 ) {
                while( i>0 ) {
                    if( buf[i-1]<' ' ) {
                        i--;
                    }
                    else {
                        break;
                    }
                }
                buf[i]=0;
                fprintf(fvalue, "\"gateway_1\":\"%s\",", buf );
            }
            fclose( ipfile );
        }

        // wifi id
        char essid[10], id[128], enc[10], key[128] ;
        sprintf(buf, "%s/wifi_id", dvrworkdir );
        ipfile = fopen( buf, "r" );
        essid[0]=0;
        id[0]=0;
        enc[0]=0;
        key[0]=0;
        if( ipfile ) {
            fscanf(ipfile, "%9s %127s %9s %127s", &essid, &id, &enc, &key ) ;
            if( strcmp( essid, "essid")==0 ) {
                fprintf( fvalue, "\"wifi_essid\":\"%s\",", id );
            }
            if( strcmp( enc, "enc")==0 ) {
                fprintf( fvalue, "\"wifi_key\":\"%s\",", key );
            }
            fclose( ipfile );
        }

        fprintf(fvalue, "\"objname\":\"network_value\" }" );
        fclose( fvalue );
    }
#endif	// ! TVS_APP    

#ifdef POWERCYCLETEST    
    // write cycletest_value
    
    fvalue = fopen("cycletest_value", "w");
    if( fvalue ) {

        // JSON head
        fprintf(fvalue, "{" );

        // cycletest
        ivalue = dvrconfig.getvalueint("debug", "cycletest");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"cycletest\":\"on\"," );
        }  
        
        // cyclefile
        value = dvrconfig.getvalue("debug", "cyclefile");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"cyclefile\":\"%s\",", value.getstring() );
        }
        
        // cycleserver
        value = dvrconfig.getvalue("debug", "cycleserver");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"cycleserver\":\"%s\",", value.getstring() );
        }

        // norecord
        ivalue = dvrconfig.getvalueint("system", "norecord");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"norecord\":\"on\"," );
        }  
        
        fprintf(fvalue, "\"objname\":\"cycletest_value\" }" );
        fclose( fvalue );

    }
#endif    

    
    // write tz_option
    fvalue = fopen( "tz_option", "w");
    if( fvalue ) {
        // initialize enumkey
        enumkey.line=0 ;
        while( (p=dvrconfig.enumkey("timezones", &enumkey))!=NULL ) {
            tzi=dvrconfig.getvalue("timezones", p );
            fprintf(fvalue, "<option value=\"%s\">", p );
            if( tzi.length()<=0 ) {
                fprintf(fvalue, "%s", p);
            }
            else {
                zoneinfobuf=tzi.getstring();
                while( *zoneinfobuf != ' ' && 
                      *zoneinfobuf != '\t' &&
                      *zoneinfobuf != 0 ) {
                          zoneinfobuf++ ;
                      }
                fprintf(fvalue, "%s -%s", p, zoneinfobuf );
            }
            fprintf(fvalue, "</option>\n");
        }
        fclose( fvalue );
    }
    
    // write led_number
    ivalue=dvrconfig.getvalueint("io", "outputnum");
    if( ivalue<=0 || ivalue>32 ) {
        ivalue=4 ;
    }
    fvalue = fopen("led_number","w");
    if( fvalue ) {
        fprintf(fvalue, "%d", ivalue );
        fclose(fvalue);
    }

    // write firmware version
    system("cp /davinci/dvr/firmwareid ./firmware_version");
    
    // write dvrsvr port number, used by eagle_setup
    value = dvrconfig.getvalue("network", "port") ;
    if( value.length()>0 ){
        fvalue = fopen("dvrsvr_port","w");
        if( fvalue ) {
            fprintf(fvalue, "%s", value.getstring() );
            fclose(fvalue);
        }
    }
    
    remove("savedstat");
    
    return 0;
}