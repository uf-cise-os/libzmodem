/* Timing routines for computing elapsed wall time */
/* Michael D. Black, Computer Science Innovations */
/* Melbourne, FL.  mblack@csihq.com */

#include <sys/time.h>
#include <unistd.h>
#include "timing.h"

int gettimeofday (struct timeval *tv, struct timezone *tz);


double 
timing (int reset)
{
  static double elaptime, starttime, stoptime;
  struct timeval tv;
  struct timezone tz;

  tz.tz_dsttime = DST_NONE;
  gettimeofday (&tv, &tz);
  if (reset) {
    starttime = tv.tv_sec + tv.tv_usec/1000000.0;
    return starttime;
  }
  else {
    stoptime = tv.tv_sec + tv.tv_usec/1000000.0;
    elaptime = stoptime - starttime;
    return elaptime;
  }
}

/*#define TEST*/
#ifdef TEST
main()
{
	int i;
	printf("timing %g\n",timing(1));
	printf("timing %g\n",timing(0));
	for(i=0;i<20;i++){
	sleep(1);
	printf("timing %g\n",timing(0));
	}
}
#endif
