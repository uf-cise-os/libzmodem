/* strerror.c -- strerror() replacement
 * Copyright (C) 1996 Uwe Ohse
 * 
 * This has been placed in the public domain: do with it what you want.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 */

char *
strerror(int num)
{
	extern char *sys_errlist[];
	extern int sys_nerr;

	if (num < 0 || num > sys_nerr)
		return "Unknown system error / illegal error number";

	return sys_errlist[num];
}
