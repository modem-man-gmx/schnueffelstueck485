#include <string>
#include <cstring>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <iomanip>
#include <ctime>
#include <sys/timeb.h>
#include <limits>
#include <cmath>
//#include <unistd.h>
//#include <errno.h>
#include <getopt.h>

#include "st485.h"

using namespace std;

void run();
void sleep_ms( int milliseconds );
static const char* get_timestamp( char* buf );

static int verbose_flag = 0;
//static int modbus_aggressive_flag = 0;
//static int modbus_debug_flag = 0;
//static int modbus_status_flag = -1;

std::ofstream data_file;

int main( const int argc, char* const* argv )
{
  //int res = -1;
  // some defaults
  string device("/dev/ttyUSB1");
  unsigned int Baud = 9600;
  unsigned char Parity = 'N';

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

  run();

  cout << "Bye." << endl << endl;
  return 0;
}


void run()
{
  char stamp[100];

  sleep_ms(3000);
  cout << get_timestamp(stamp) << "Dummy=" << 1 << endl;
  sleep_ms(3000);
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
