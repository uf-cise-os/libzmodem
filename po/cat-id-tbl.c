/* Automatically generated by po2tbl.sed from lrzsz.pot.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "libgettext.h"

const struct _msg_ent _msg_tbl[] = {
  {"", 1},
  {"io_mode(,2) in rbsb.c not implemented\n", 2},
  {"caught signal %d; exiting", 3},
  {"command tries", 4},
  {"packetlength", 5},
  {"packetlength out of range 24..%ld", 6},
  {"framelength", 7},
  {"framelength out of range 32..%ld", 8},
  {"min_bps", 9},
  {"min_bps must be >= 0", 10},
  {"min_bps_time", 11},
  {"min_bps_time must be > 1", 12},
  {"hour to large (0..23)", 13},
  {"unparsable stop time\n", 14},
  {"minute to large (0..59)", 15},
  {"stop time to small", 16},
  {"stop-at", 17},
  {"timeout", 18},
  {"timeout out of range 10..1000", 19},
  {"security violation: can't do that under restricted shell\n", 20},
  {"window size", 21},
  {"cannot turnoff syslog", 22},
  {"startup delay", 23},
  {"out of memory", 24},
  {"this program was never intended to be used setuid\n", 25},
  {"need at least one file to send", 26},
  {"Can't send command in restricted mode\n", 27},
  {"hostname too long\n", 28},
  {"illegal server address\n", 29},
  {"can read only one file from stdin", 30},
  {"Transfer incomplete\n", 31},
  {"Transfer complete\n", 32},
  {"send_pseudo %s: cannot open tmpfile %s: %s", 33},
  {"send_pseudo %s: cannot lstat tmpfile %s: %s", 34},
  {"send_pseudo %s: avoiding symlink trap", 35},
  {"send_pseudo %s: cannot write to tmpfile %s: %s", 36},
  {"send_pseudo %s: failed", 37},
  {"send_pseudo %s: ok", 38},
  {"Answering TIMESYNC at %s", 39},
  {"timezone", 40},
  {"timezone unknown", 41},
  {"Can't open any requested files.", 42},
  {"security violation: not allowed to upload from %s", 43},
  {"cannot open %s", 44},
  {"is not a file: %s", 45},
  {"%s/%s: error occured", 46},
  {"skipped: %s", 47},
  {"%s/%s: skipped", 48},
  {"Bytes Sent:%7ld   BPS:%-8ld                        \n", 49},
  {"Sending %s, %ld blocks: ", 50},
  {"Give your local XMODEM receive command now.", 51},
  {"Sending: %s\n", 52},
  {"Timeout on pathname", 53},
  {"Receiver Cancelled", 54},
  {"No ACK on EOT", 55},
  {"Xmodem sectors/kbytes sent: %3d/%2dk", 56},
  {"Ymodem sectors/kbytes sent: %3d/%2dk", 57},
  {"Cancelled", 58},
  {"Timeout on sector ACK", 59},
  {"NAK on sector", 60},
  {"Got burst for sector ACK", 61},
  {"Got %02x for sector ACK", 62},
  {"Retry Count Exceeded", 63},
  {"Try `%s --help' for more information.\n", 64},
  {"%s version %s\n", 65},
  {"Usage: %s [options] file ...\n", 66},
  {"   or: %s [options] -{c|i} COMMAND\n", 67},
  {"Send file(s) with ZMODEM/YMODEM/XMODEM protocol\n", 68},
  {"\
    (X) = option applies to XMODEM only\n\
    (Y) = option applies to YMODEM only\n\
    (Z) = option applies to ZMODEM only\n", 69},
  {"\
  -+, --append                append to existing destination file (Z)\n\
  -2, --twostop               use 2 stop bits\n\
  -4, --try-4k                go up to 4K blocksize\n\
      --start-4k              start with 4K blocksize (doesn't try 8)\n\
  -8, --try-8k                go up to 8K blocksize\n\
      --start-8k              start with 8K blocksize\n\
  -a, --ascii                 ASCII transfer (change CR/LF to LF)\n\
  -b, --binary                binary transfer\n\
  -B, --bufsize N             buffer N bytes (N==auto: buffer whole file)\n\
  -c, --command COMMAND       execute remote command COMMAND (Z)\n\
  -C, --command-tries N       try N times to execute a command (Z)\n\
  -d, --dot-to-slash          change '.' to '/' in pathnames (Y/Z)\n\
      --delay-startup N       sleep N seconds before doing anything\n\
  -e, --escape                escape all control characters (Z)\n\
  -E, --rename                force receiver to rename files it already has\n\
  -f, --full-path             send full pathname (Y/Z)\n\
  -i, --immediate-command CMD send remote CMD, return immediately (Z)\n\
  -h, --help                  print this usage message\n\
  -k, --1k                    send 1024 byte packets (X)\n\
  -L, --packetlen N           limit subpacket length to N bytes (Z)\n\
  -l, --framelen N            limit frame length to N bytes (l>=L) (Z)\n\
  -m, --min-bps N             stop transmission if BPS below N\n\
  -M, --min-bps-time N          for at least N seconds (default: 120)\n", 70},
  {"\
  -n, --newer                 send file if source newer (Z)\n\
  -N, --newer-or-longer       send file if source newer or longer (Z)\n\
  -o, --16-bit-crc            use 16 bit CRC instead of 32 bit CRC (Z)\n\
  -O, --disable-timeouts      disable timeout code, wait forever\n\
  -p, --protect               protect existing destination file (Z)\n\
  -r, --resume                resume interrupted file transfer (Z)\n\
  -R, --restricted            restricted, more secure mode\n\
  -q, --quiet                 quiet (no progress reports)\n\
  -s, --stop-at {HH:MM|+N}    stop transmission at HH:MM or in N seconds\n\
      --tcp-server            open socket, wait for connection (Z)\n\
      --tcp-client ADDR:PORT  open socket, connect to ... (Z)\n\
  -u, --unlink                unlink file after transmission\n\
  -U, --unrestrict            turn off restricted mode (if allowed to)\n\
  -v, --verbose               be verbose, provide debugging information\n\
  -w, --windowsize N          Window is N bytes (Z)\n\
  -X, --xmodem                use XMODEM protocol\n\
  -y, --overwrite             overwrite existing files\n\
  -Y, --overwrite-or-skip     overwrite existing files, else skip\n\
      --ymodem                use YMODEM protocol\n\
  -Z, --zmodem                use ZMODEM protocol\n\
\n\
short options use the same arguments as the long ones\n", 71},
  {"got ZRQINIT", 72},
  {"got ZCAN", 73},
  {"blklen now %d\n", 74},
  {"zsendfdata: bps rate %ld below min %ld", 75},
  {"zsendfdata: reached stop time", 76},
  {"Bytes Sent:%7ld/%7ld   BPS:%-8ld ETA %02d:%02d  ", 77},
  {"calc_blklen: reduced to %d due to error\n", 78},
  {"calc_blklen: returned old value %d due to low bpe diff\n", 79},
  {"calc_blklen: old %ld, new %ld, d %ld\n", 80},
  {"calc_blklen: calc total_bytes=%ld, bpe=%ld, ec=%ld\n", 81},
  {"calc_blklen: blklen %d, ok %ld, failed %ld -> %lu\n", 82},
  {"calc_blklen: returned %d as best\n", 83},
  {"\
\n\
countem: Total %d %ld\n", 84},
  {"Bad escape sequence %x", 85},
  {"Sender Canceled", 86},
  {"TIMEOUT", 87},
  {"Bad data subpacket", 88},
  {"Data subpacket too long", 89},
  {"Garbage count exceeded", 90},
  {"Got %s", 91},
  {"Retry %d: ", 92},
  {"don't have settimeofday, will not set time\n", 93},
  {"not running as root (this is good!), can not set time\n", 94},
  {"bytes_per_error", 95},
  {"bytes-per-error should be >100", 96},
  {"O_SYNC not supported by the kernel", 97},
  {"garbage on commandline", 98},
  {"Usage: %s [options] [filename.if.xmodem]\n", 99},
  {"Receive files with ZMODEM/YMODEM/XMODEM protocol\n", 100},
  {"\
  -+, --append                append to existing files\n\
  -a, --ascii                 ASCII transfer (change CR/LF to LF)\n\
  -b, --binary                binary transfer\n\
  -B, --bufsize N             buffer N bytes (N==auto: buffer whole file)\n\
  -c, --with-crc              Use 16 bit CRC (X)\n\
  -C, --allow-remote-commands allow execution of remote commands (Z)\n\
  -D, --null                  write all received data to /dev/null\n\
      --delay-startup N       sleep N seconds before doing anything\n\
  -e, --escape                Escape control characters (Z)\n\
  -E, --rename                rename any files already existing\n\
      --errors N              generate CRC error every N bytes (debugging)\n\
  -h, --help                  Help, print this usage message\n\
  -m, --min-bps N             stop transmission if BPS below N\n\
  -M, --min-bps-time N          for at least N seconds (default: 120)\n\
  -O, --disable-timeouts      disable timeout code, wait forever for data\n\
      --o-sync                open output file(s) in synchronous write mode\n\
  -p, --protect               protect existing files\n\
  -q, --quiet                 quiet, no progress reports\n\
  -r, --resume                try to resume interrupted file transfer (Z)\n\
  -R, --restricted            restricted, more secure mode\n\
  -s, --stop-at {HH:MM|+N}    stop transmission at HH:MM or in N seconds\n\
  -S, --timesync              request remote time (twice: set local time)\n\
      --syslog[=off]          turn syslog on or off, if possible\n\
  -t, --timeout N             set timeout to N tenths of a second\n\
      --tcp-server            open socket, wait for connection (Z)\n\
      --tcp-client ADDR:PORT  open socket, connect to ... (Z)\n\
  -u, --keep-uppercase        keep upper case filenames\n\
  -U, --unrestrict            disable restricted mode (if allowed to)\n\
  -v, --verbose               be verbose, provide debugging information\n\
  -w, --windowsize N          Window is N bytes (Z)\n\
  -X  --xmodem                use XMODEM protocol\n\
  -y, --overwrite             Yes, clobber existing file if any\n\
      --ymodem                use YMODEM protocol\n\
  -Z, --zmodem                use ZMODEM protocol\n\
\n\
short options use the same arguments as the long ones\n", 101},
  {"%s waiting to receive.", 102},
  {"\rBytes received: %7ld/%7ld   BPS:%-6ld                \r\n", 103},
  {"%s: ready to receive %s", 104},
  {"\rBytes received: %7ld   BPS:%-6ld                \r\n", 105},
  {"\
\r\n\
%s: %s removed.\r\n", 106},
  {"Pathname fetch returned EOT", 107},
  {"Received dup Sector", 108},
  {"Sync Error", 109},
  {"CRC", 110},
  {"Checksum", 111},
  {"Sector number garbled", 112},
  {"Sender Cancelled", 113},
  {"Got 0%o sector header", 114},
  {"file name ends with a /, skipped: %s\n", 115},
  {"zmanag=%d, Lzmanag=%d\n", 116},
  {"zconv=%d\n", 117},
  {"file exists, skipped: %s\n", 118},
  {"TIMESYNC: here %ld, remote %ld, diff %ld seconds\n", 119},
  {"TIMESYNC: cannot set time: %s\n", 120},
  {"Topipe", 121},
  {"Receiving: %s\n", 122},
  {"Blocks received: %d", 123},
  {"%s: %s exists\n", 124},
  {"%s:\tSecurity Violation", 125},
  {"remote command execution requested", 126},
  {"not executed", 127},
  {"got ZRINIT", 128},
  {"Skipped", 129},
  {"rzfile: bps rate %ld below min %ld", 130},
  {"rzfile: reached stop time", 131},
  {"\rBytes received: %7ld/%7ld   BPS:%-6ld ETA %02d:%02d  ", 132},
  {"file close error", 133},
};

int _msg_tbl_length = 133;
