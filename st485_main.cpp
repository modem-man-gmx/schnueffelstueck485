#include <string>
#include <iomanip>
// C library headers
#include <fstream>
#include <cstdlib>
//#include <cstdio>
#include <cstring>
#include <iostream>
#include <ctime>
#include <sys/timeb.h>
#include <limits>
//#include <cmath>
#include <errno.h>
#include <unistd.h> // write(), read(), close()
// Linux headers
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
//#include <termbits.h> // termios2, since 2018 on *buntu & co? but not on MSYS2/MinGW ...
#include <getopt.h>


#include "st485.h"

using namespace std;

bool run();
void sleep_ms( int milliseconds );
static const char* get_timestamp( char* buf );
int BaudCodeFromBaudRate(unsigned int Baud);
int CharBitsFromDataBits(unsigned int Data);

const unsigned uart_slice = 100; // c_cc[VTIME] 1 slice size

// some defaults
static int verbose_flag = 0;
//static int modbus_aggressive_flag = 0;
//static int modbus_debug_flag = 0;
//static int modbus_status_flag = -1;
static string device("/dev/ttyUSB1");
static unsigned int Baud = 9600;
static unsigned int Data = 8;
static unsigned char Parity = 'N';
static unsigned int Stop = 1;
static unsigned wait_slice = 100;
static long max_wait = 10*1000L;



std::ofstream data_file;

int main( const int argc, char* const* argv )
{
  //int res = -1;

  cout << "+++ We scan RS485/USB for modbus or the like +++" << endl;

  int c;

  while (1)
  {
    static struct option long_options[] =
    {
      /* These options set a flag. */
      {"verbose",     no_argument,       &verbose_flag, 1 },
      {"brief",       no_argument,       &verbose_flag, 0 },
//    {"debug",       no_argument,       &modbus_debug_flag, 1 },
//    {"status",      no_argument,       &modbus_status_flag, 1 },
//    {"no-status",   no_argument,       &modbus_status_flag, 0 },
      /* These options don’t set a flag. We distinguish them by their indices. */
      {"device",      required_argument, 0, 'd'},
      {"baud",        required_argument, 0, 'b'},
      {"parity",      required_argument, 0, 'p'},
//    {"host-id",     required_argument, 0, 'i'},
      {0, 0, 0, 0}
    };
    /* getopt_long stores the option index here. */
    int option_index = 0;

    c = getopt_long( argc, argv, "d:b:p:i:", long_options, &option_index );

    /* Detect the end of the options. */
    if (c == -1) break;

    // note: option_index is only valid, if the switch was a --long-option alike switch
    switch (c)
    {
      case 0:
        /* If this option set a flag, do nothing else now. */
        if( long_options[option_index].flag != 0 )
          { break; }
        if( verbose_flag ) cout << "option " << long_options[option_index].name;
        if( optarg && verbose_flag )
          cout << " with arg " << optarg;
        if( verbose_flag ) cout << endl;
        break;

      case 'd':
        if( verbose_flag ) cout << "option -d with value \"" << ((optarg)?optarg:"NULL") << "\"" << endl;
        if( optarg )
          device = optarg;
        break;

      case 'b':
        if( verbose_flag ) cout << "option -b with value \"" << ((optarg)?optarg:"NULL") << "\"" << endl;
        if( ! optarg || ! isdigit( optarg[0] ) )
        {
          cerr << "value is not numerical" << endl;
          abort();
        }
        Baud = static_cast<unsigned int>( atoi( optarg ) );
        break;

      case 'p':
        {
          if( verbose_flag ) cout << "option -p with value \"" << ((optarg)?optarg:"NULL") << "\"" << endl;
          char letter = optarg ? optarg[0] : '?';
          if( ! strchr( "EeOoNn", letter ) )
          {
            cerr << "value is not one of E/O/N (or lower case)" << endl;
            abort();
          }
          Parity = static_cast<unsigned char>( letter );
        }; break;
/*
      case 'i':
        if( verbose_flag ) cout << "option -i with value \"" << ((optarg)?optarg:"NULL") << "\"" << endl;
        if( ! optarg || ! isdigit( optarg[0] ) )
        {
          cerr << "value is not numerical" << endl;
          abort();
        }
        SlaveAddress = static_cast<uint8_t>( atoi( optarg ) );
        break;
*/
      case ':': // an optional argument was not given for any of the optional_argument fields (okay, then use default value)
        switch( optopt )
        {
          default :
            if( verbose_flag ) cerr << "option " << optopt << " is missing a required argument!" << endl;
            abort();
        }
        break;

      case '?': /* getopt_long already printed an error message. ??? really? not in my version! */
        break;

      default:
        abort ();
    }
  } // end-of-while(1)

  /* Instead of reporting ‘--verbose’
     and ‘--brief’ as they are encountered,
     we report the final status resulting from them. */
  if( verbose_flag ) cout << "verbose flag is set" << endl;
//if( -1 == modbus_status_flag ) modbus_status_flag = verbose_flag; // in verbose we want also to see status, if not specified.

  /* Print any remaining command line arguments (not options), typically filenames. */
  if( optind < argc && verbose_flag )
  {
    cout << "non-option ARGV-elements: ";
    while( optind < argc )
      cout << argv[optind++] << " ";
    cout << endl;
  }


  std::ios oldState(nullptr);
  oldState.copyfmt(std::cout);

  if( argc <= 1 )
  {
    cout << endl << "--- No arguments given. Known args would be with samples: ---" << endl;
    cout << "  --verbose    (no_argument), make talkative"  << endl;
    cout << "  --brief      (no_argument), the opposite"    << endl;
//  cout << "  --debug      (no_argument), dump out low level I/O from libmodbus" << endl;
//  cout << "  --status     (no_argument), do ask Host ID/status by Cmd 11 (status)" << endl;
//  cout << "  --no-status  (no_argument), do not ask Host about status" << endl;
    cout << "  --device /dev/ttxUSB0" /* or -d/dev/ttyUSB0"*/ << endl;
    cout << "  --baud 9600 or --baud=9600" /*or -b9600"*/     << endl;
    cout << "  --parity E|O|N" /* or -pN"*/                   << endl;
//  cout << "  --host-id 44 (decimal!)" /* or -i44"*/         << endl;
  }

  cout << endl << "--- We go with ---" << endl;
  cout << "  Verbose output   : " << (verbose_flag            ?"true":"false") << endl;
//cout << "  Modbus aggressive: " << (modbus_aggressive_flag  ?"true":"false") << endl;
//cout << "  Modbus debug out : " << (modbus_debug_flag       ?"true":"false") << endl;
//cout << "  Modbus status inq: " << (modbus_status_flag      ?"true":"false") << endl;
  cout << "  Device name      : " << device << endl;
  cout << "  Baud Rate (RS485): " << Baud << endl;
  cout << "  Parity bit       : " << Parity << endl;
//std::cout.copyfmt(oldState);
//cout << "  Modbus Host ID   : " << static_cast<int>(SlaveAddress)
//       << " (0x" << std::setfill('0') << std::setw(2) << hex << static_cast<int>(SlaveAddress) << ")"
//       << endl;
//std::cout.copyfmt(oldState);
  cout << endl;

  run();

  cout << "Bye." << endl << endl;
  return 0;
}


bool run()
{
  char stamp[100];
  
  int openmode = O_RDWR; // O_RDONLY
 
  cout << get_timestamp(stamp) << "open( \"" << device << "\", " << openmode << " ) ..." << endl;
  int serial_port = open( device.c_str(), openmode );

  // Check for errors
  if( 0 > serial_port )
  {
    cout << get_timestamp(stamp) << "open( \"" << device << "\" ) failed, Error:" << errno << " from open: " << strerror(errno) << endl;
    switch(errno)
    {
      case ENOENT:
        cout << get_timestamp(stamp) << "probably wrong device name." << endl;
#       ifdef __CYGWIN__
        cout << get_timestamp(stamp) << "try one of \"ls /dev/tty*\"" << endl;
#       endif
        break;
      case EPERM : // fallthrough
      case EACCES:
        cout << get_timestamp(stamp) << "probably missed to set \"sudo adduser $USER dialout\"" << endl;
        break;
    }
    return false;
  }
  
  // Create new termios struct, we call it 'tty' for convention
  // No need for "= {0}" at the end as we'll immediately write the existing
  // config to this struct
  struct termios tty;

  // Read in existing settings, and handle any error
  // NOTE: This is important! POSIX states that the struct passed to tcsetattr()
  // must have been initialized with a call to tcgetattr() overwise behaviour
  // is undefined
  if( 0 != tcgetattr( serial_port, &tty ) )
  {
    cout << get_timestamp(stamp) << "tcgetattr() failed, Error:" << errno << " from open: " << strerror(errno) << endl;
    return false;
  }

  /* ---------- SEE ALSO: termbits.h or sys/termios.h ------------
  static unsigned int Baud = 9600;
  static unsigned int Data = 8;
  static unsigned char Parity = 'N';
  static unsigned int Stop = 1;
  ------------*/
  
  // ========= PARITY ====================
  tty.c_cflag &= ~(PARENB | PARODD | HUPCL);      // shut off parity
  switch( Parity )
  {
    case 'E' : tty.c_cflag |= PARENB; break; // Set parity bit, enabling parity
    case 'O' : tty.c_cflag |=  (PARENB | PARODD); break; // Set parity bit, enabling parity and ODD
    case 'N' : // fallthrough;
    default  : tty.c_cflag &= ~(PARENB | PARODD); break; // Clear parity bit, disabling parity (most common)
  }

  // ========= STOP BITS ====================
  if( 1==Stop )
  { tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
  }
  else
  { tty.c_cflag |= CSTOPB; // Set stop field, two stop bits used in communication
  }

  // ========= DATA BITS PER BYTE ====================
  tty.c_cflag &= ~(CSIZE); // Clear all the size bits, then use the statements below
  tty.c_cflag |= CharBitsFromDataBits( Data ); //CS8;
  
  // ========= Hardware Flow Control (CRTSCTS) ====================
  tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
  // not for modbus: tty.c_cflag |= CRTSCTS;  // Enable RTS/CTS hardware flow control
  
  // ========= Software Flow Control (IXOFF, IXON, IXANY) ====================
  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

  // ========= Modem Lines ====================
  tty.c_cflag |= (CLOCAL | CREAD); // ignore modem controls and SIGHUP on DCD drop, + enable reading

  // ========= CANONICAL MODE? ====================
  // UNIX systems provide two basic modes of input, canonical and non-canonical mode. 
  // In canonical mode, input is processed when a new line character is received. 
  // This means: the receiving application receives that data line-by-line. 
  // (usually undesirable when dealing with a serial port, and so we normally want to disable canonical mode.)
  tty.c_lflag &= ~ICANON;
  
  // ========= ECHO OFF ====================
  // If this bit is set, sent characters will be echoed back. Because we disabled canonical mode, 
  // I don’t think these bits actually do anything, but it doesn’t harm to disable them just in case!
  tty.c_lflag &= ~ECHO; // Disable echo
  tty.c_lflag &= ~ECHOE; // Disable erasure
  tty.c_lflag &= ~ECHONL; // Disable new-line echo
  
  // ========= SPECIAL SIGNAL CHARS? ====================
  // When the ISIG bit is set, INTR, QUIT and SUSP characters are interpreted. 
  // We don’t want this with a serial port, so clear this bit:
  tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
  
  // ========= Output Modes (c_oflag) ====================
  // The c_oflag member of the termios struct contains low-level settings for output processing. 
  // When configuring a serial port, we want to disable any special handling of output chars/bytes, so do the following:
  tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
  tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

  // ========= VMIN and VTIME (c_cc) ====================
  // An important point to note is that VTIME means slightly different things depending on what VMIN is.
  // When VMIN is 0, VTIME specifies a time-out from the start of the read() call.
  // But when VMIN is > 0, VTIME specifies the time-out from the start of the first received character.
  // Let’s explore the different combinations:
  // VMIN = 0, VTIME = 0: No blocking, return immediately with what is available
  // VMIN > 0, VTIME = 0: This will make read() always wait for bytes (exactly how many is determined by VMIN), so read() could block indefinitely.
  // VMIN = 0, VTIME > 0: This is a blocking read of any number of chars with a maximum timeout (given by VTIME). 
  //               read() will block until either any amount of data is available, or the timeout occurs.
  //               This could be a good mode for most cases
  // VMIN > 0, VTIME > 0: Block until either VMIN characters have been received, or VTIME after first character has elapsed. 
  // Note that the timeout for VTIME does not begin until the first character is received.
  // VMIN and VTIME are both defined as the type cc_t, an alias for unsigned char (1 byte).
  // This puts an upper limit on the number of VMIN characters to be 255 and the maximum timeout of 25.5 seconds (255 deciseconds).
  // NOTE:
  // "Returning as soon as any data is received" does not mean you will only get 1 byte at a time. 
  // Depending on the OS latency, serial port speed, hardware buffers and many other things you have no direct control over,
  // you may receive any number of bytes.
  // 
  // For example, if we wanted to wait for up to 1s, returning as soon as any data was received, we could use:
  if( wait_slice < uart_slice )
  {
    cout << get_timestamp(stamp) << "WARNING: time slice " << wait_slice << " < " << uart_slice << " (default uart slicing)." << endl;
    wait_slice = uart_slice;
  }
  else if( wait_slice % uart_slice )
  {
    cout << get_timestamp(stamp) << "WARNING: time slice " << wait_slice << " is not n * " << uart_slice << " (default uart slicing), will get reduced." << endl;
  }

  tty.c_cc[VTIME] = wait_slice / uart_slice;    // Wait for up to 0,1 * n Sec (1 deciseconds == 100ms), returning as soon as any data is received.
  tty.c_cc[VMIN] = 0; // returning as soon as any data is received.

  // ========= Baud Rate ====================
  int BaudCode = BaudCodeFromBaudRate( Baud );
  cfsetospeed( &tty, BaudCode );
  cfsetispeed( &tty, BaudCode );
  // tty.c_lflag = 0;

  if( 0 != tcsetattr( serial_port, TCSANOW, &tty ) )
  {
    cout << get_timestamp(stamp) << "tcsetattr() failed, Error:" << errno << " from open: " << strerror(errno) << endl;
    return false;
  }

  unsigned char msg[] = "HELLO\n";
  int wr = write( serial_port, msg, sizeof(msg) );

  long silence=0;
  struct timeb start, stop;
  bool dot=false;

  cout << get_timestamp(stamp) << "waiting up to " << (max_wait/1000) << "," << (max_wait%1000) << " sec for data ..." << endl;
  do
  {
    char readbuff[128];

    ftime(&start); // start.millitm
    int rd = read( serial_port, readbuff, sizeof(readbuff) );  // read or return after tty.c_cc[VTIME] * 100 ms
    if(rd==0)
    {
      if(0==silence%1000) {dot=true; cout << "." << std::flush;}
      unsigned reminder = wait_slice % uart_slice;
      sleep_ms( reminder );
      silence += wait_slice;
      continue;
    }
    else if(rd<0)
    {
      if(dot) cout << endl;
      cout << get_timestamp(stamp) << "read() failed, Error:" << errno << " from open: " << strerror(errno) << endl;
      break;
    }
    ftime(&stop); // stop.millitm
    silence = stop.millitm - start.millitm;
    if(dot) {dot=false;cout << endl;}
    cout << get_timestamp(stamp) << "got " << rd << " Bytes." << endl;
  } while( silence < max_wait );

  if(dot) cout << endl;
  cout << get_timestamp(stamp) << "close( \"" << device << "\" )." << endl;
  close( serial_port );
  return true;
}





void sleep_ms(int milliseconds)
{
  struct timespec ts;
  ts.tv_sec = milliseconds / 1000;
  ts.tv_nsec = (milliseconds % 1000) * 1000000;
  nanosleep(&ts, NULL);
}


// VERY DIRTY shit!! need rewriting!!!
static const char* get_timestamp( char* buf )
{
  if (buf)
  {
    struct timeb start;
    char append[100];
    ftime(&start);
    strftime(buf, 100, "%H:%M:%S", localtime(&start.time));

    /* append milliseconds */
    sprintf( append, ":%03u ", start.millitm );
    strcat( buf, append );
    return buf;
  }
  return "";
}


int BaudCodeFromBaudRate(unsigned int Baud)
{
  switch( Baud )
  {
    case 0     : return B0;
    case 50    : return B50;
    case 75    : return B75;
    case 110   : return B110;
    case 134   : return B134;
    case 150   : return B150;
    case 200   : return B200;
    case 300   : return B300;
    case 600   : return B600;
    case 1200  : return B1200;
    case 1800  : return B1800;
    case 2400  : return B2400;
    case 4800  : return B4800;
    case 9600  : return B9600;
    case 19200 : return B19200;
    case 38400 : return B38400;
    case 57600 : return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    case 460800: return B460800;
    default    : return B0;
  }
}

int CharBitsFromDataBits(unsigned int Data)
{
  switch( Data )
  {
    case 5: return CS5;
    case 6: return CS6;
    case 7: return CS7;
    case 8: return CS8;
    default:return CS8;
  }
}