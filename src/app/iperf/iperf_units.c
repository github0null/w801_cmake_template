/*---------------------------------------------------------------
 * Copyright (c) 1999,2000,2001,2002,2003
 * The Board of Trustees of the University of Illinois
 * All Rights Reserved.
 *---------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software (Iperf) and associated
 * documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 *
 * Redistributions of source code must retain the above
 * copyright notice, this list of conditions and
 * the following disclaimers.
 *
 *
 * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimers in the documentation and/or other materials
 * provided with the distribution.
 *
 *
 * Neither the names of the University of Illinois, NCSA,
 * nor the names of its contributors may be used to endorse
 * or promote products derived from this Software without
 * specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE CONTIBUTORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ________________________________________________________________
 * National Laboratory for Applied Network Research
 * National Center for Supercomputing Applications
 * University of Illinois at Urbana-Champaign
 * http://www.ncsa.uiuc.edu
 * ________________________________________________________________
 *
 * stdio.c
 * by Mark Gates <mgates@nlanr.net>
 * and Ajay Tirumalla <tirumala@ncsa.uiuc.edu>
 * -------------------------------------------------------------------
 * input and output numbers, converting with kilo, mega, giga
 * ------------------------------------------------------------------- */

#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include "iperf.h"

#ifdef __cplusplus
extern    "C"
{
#endif

    const long KILO_UNIT = 1024;
    const long MEGA_UNIT = 1024 * 1024;
    const long GIGA_UNIT = 1024 * 1024 * 1024;

    const long KILO_UNIT_SI = 1000;
    const long MEGA_UNIT_SI = 1000 * 1000;
    const long GIGA_UNIT_SI = 1000 * 1000 * 1000;

/* -------------------------------------------------------------------
 * unit_atof
 *
 * Given a string of form #x where # is a number and x is a format
 * character listed below, this returns the interpreted integer.
 * Gg, Mm, Kk are giga, mega, kilo respectively
 * ------------------------------------------------------------------- */

//    double    unit_atof(const char *s)
    u64 unit_atof(const char *s)
    {
	//double    n;
	//u64 n;
	int n;
	char      suffix = '\0';

	         // assert(s != NULL);

	/* scan the number and any suffices */
	//sscanf(s, "%lf%c", &n, &suffix);
	sscanf(s, "%d%c", &n, &suffix);

	/* convert according to [Gg Mm Kk] */
	switch    (suffix)
	{
	case 'G':
	    n *= GIGA_UNIT;
	    break;
	case 'M':
	    n *= MEGA_UNIT;
	    break;
	case 'K':
	    n *= KILO_UNIT;
	    break;
	case 'g':
	    n *= GIGA_UNIT_SI;
	    break;
	case 'm':
	    n *= MEGA_UNIT_SI;
	    break;
	case 'k':
	    n *= KILO_UNIT_SI;
	    break;
	default:
	    break;
	}
	          return n;
    }				/* end unit_atof */

/* -------------------------------------------------------------------
 * unit_atoi
 *
 * Given a string of form #x where # is a number and x is a format
 * character listed below, this returns the interpreted integer.
 * Gg, Mm, Kk are giga, mega, kilo respectively
 * ------------------------------------------------------------------- */

    iperf_size_t unit_atoi(const char *s)
    {
	//double    n;
	int    n;
	char      suffix = '\0';

	          //assert(s != NULL);

	/* scan the number and any suffices */
	//sscanf(s, "%lf%c", &n, &suffix);
	sscanf(s, "%d%c", &n, &suffix);

	/* convert according to [Gg Mm Kk] */
	switch    (suffix)
	{
	case 'G':
	    n *= GIGA_UNIT;
	    break;
	case 'M':
	    n *= MEGA_UNIT;
	    break;
	case 'K':
	    n *= KILO_UNIT;
	    break;
	case 'g':
	    n *= GIGA_UNIT_SI;
	    break;
	case 'm':
	    n *= MEGA_UNIT_SI;
	    break;
	case 'k':
	    n *= KILO_UNIT_SI;
	    break;
	default:
	    break;
	}
	          return (iperf_size_t) n;
    }				/* end unit_atof */

/* -------------------------------------------------------------------
 * constants for byte_printf
 * ------------------------------------------------------------------- */

/* used as indices into conversion_bytes[], label_byte[], and label_bit[] */
    enum
    {
	UNIT_CONV,
	KILO_CONV,
	MEGA_CONV,
	GIGA_CONV
    };

/* factor to multiply the number by */
    const double conversion_bytes_1[] =
    {
	1.0,			/* unit */
	1.0 / 1024,		/* kilo */
	1.0 / 1024 / 1024,	/* mega */
	1.0 / 1024 / 1024 / 1024/* giga */
    };

/* factor to multiply the number by for bits*/
    const double conversion_bits_1[] =
    {
	1.0,			/* unit */
	1.0 / 1000,		/* kilo */
	1.0 / 1000 / 1000,	/* mega */
	1.0 / 1000 / 1000 / 1000/* giga */
    };

/* factor to multiply the number by */
    const float conversion_bytes_2[] =
    {
	1,			/* unit */
	1 / 1024,		/* kilo */
	1 / 1024 / 1024,	/* mega */
	1 / 1024 / 1024 / 1024/* giga */
    };
/* factor to multiply the number by for bits*/
    const float conversion_bits_2[] =
    {
	1,			/* unit */
	1 / 1000,		/* kilo */
	1 / 1000 / 1000,	/* mega */
	1 / 1000 / 1000 / 1000/* giga */
    };

/* factor to multiply the number by */
    const float conversion_bytes[] =
    {
	1,			/* unit */
	1024,		/* kilo */
	1024 * 1024,	/* mega */
	1024 * 1024 * 1024/* giga */
    };

/* factor to multiply the number by for bits*/
    const float conversion_bits[] =
    {
	1,			/* unit */
	1000,		/* kilo */
	1000 * 1000,	/* mega */
	1000 * 1000 * 1000/* giga */
    };

/* labels for Byte formats [KMG] */
    const char *label_byte[] =
    {
	"Byte",
	"KByte",
	"MByte",
	"GByte"
    };

/* labels for bit formats [kmg] */
    const char *label_bit[] =
    {
	"bit",
	"Kbit",
	"Mbit",
	"Gbit"
    };

/* -------------------------------------------------------------------
 * unit_snprintf
 *
 * Given a number in bytes and a format, converts the number and
 * prints it out with a bits or bytes label.
 *   B, K, M, G, A for Byte, Kbyte, Mbyte, Gbyte, adaptive byte
 *   b, k, m, g, a for bit,  Kbit,  Mbit,  Gbit,  adaptive bit
 * adaptive picks the "best" one based on the number.
 * s should be at least 11 chars long
 * (4 digits + space + 5 chars max + null)
 * ------------------------------------------------------------------- */

    void      unit_snprintf(char *s, int inLen,
			              double inNum, char inFormat)
    {
	int       conv;
	const char *suffix;
	const char *format;
	//printf("in: %f, tmp1 = %f\n", inNum, tmp1 /(1024*1024));

	/* convert to bits for [bkmga] */
	if        (!isupper((int) inFormat))
	{
	    inNum *= 8;
			//printf("in 1: %d\n", inNum);
	}
	switch    (toupper(inFormat))
	{
	case 'B':
	    conv = UNIT_CONV;
	    break;
	case 'K':
	    conv = KILO_CONV;
	    break;
	case 'M':
	    conv = MEGA_CONV;
	    break;
	case 'G':
	    conv = GIGA_CONV;
	    break;

	default:
	case 'A':
	    {
		u64    tmpNum = inNum;
		conv = UNIT_CONV;

		if (isupper((int) inFormat))
		{

		    while (tmpNum >= 1024 && conv <= GIGA_CONV)
		    {
			tmpNum /= 1024;
			conv++;
		    }
			//printf("in 2: %d\n", tmpNum);
		} else
		{
		    while (tmpNum >= 1000 && conv <= GIGA_CONV)
		    {
			tmpNum /= 1000;
			conv++;
		    }
			//printf("in 3: %d\n", tmpNum);
		}
		break;
	    }
	}

	if (!isupper((int) inFormat))
	{
	    inNum /= conversion_bits[conv];
	    suffix = label_bit[conv];
		//printf("in 4: %d\n", inNum);
	} else
	{
	    inNum /= conversion_bytes[conv];
	    suffix = label_byte[conv];
		//printf("in 5: %d\n", inNum);
	}

	/* print such that we always fit in 4 places */
	if (inNum < 9.995)
	{			/* 9.995 would be rounded to 10.0 */
	    format = "%4.2f %s";/* #.## */
	} else if (inNum < 99.95)
	{			/* 99.95 would be rounded to 100 */
	    format = "%4.1f %s";/* ##.# */
	} else if (inNum < 999.5)
	{			/* 999.5 would be rounded to 1000 */
	    format = "%4.0f %s";/* ### */
	} else
	{			/* 1000-1024 fits in 4 places If not using
				 * Adaptive sizes then this code will not
				 * control spaces */
	    format = "%4.0f %s";/* #### */
	}

	//format = "%4.2f %s";
//	os_snprintf(s, inLen, format, inNum, suffix);
	snprintf(s, inLen, format, inNum, suffix);
	
	//printf("s: %s\n", s);
    }				/* end unit_snprintf */

/* -------------------------------------------------------------------
 * unit_snprintf
 *
 * Given a number in bytes and a format, converts the number and
 * prints it out with a bits or bytes label.
 *   B, K, M, G, A for Byte, Kbyte, Mbyte, Gbyte, adaptive byte
 *   b, k, m, g, a for bit,  Kbit,  Mbit,  Gbit,  adaptive bit
 * adaptive picks the "best" one based on the number.
 * s should be at least 11 chars long
 * (4 digits + space + 5 chars max + null)
 * ------------------------------------------------------------------- */

    void      unit_snprintf_1(char *s, int inLen,
			              double inNum, char inFormat)
    {
	int       conv;
	const char *suffix;
	const char *format;

	/* convert to bits for [bkmga] */
	if        (!isupper((int) inFormat))
	{
	    inNum *= 8;
	}
	switch    (toupper(inFormat))
	{
	case 'B':
	    conv = UNIT_CONV;
	    break;
	case 'K':
	    conv = KILO_CONV;
	    break;
	case 'M':
	    conv = MEGA_CONV;
	    break;
	case 'G':
	    conv = GIGA_CONV;
	    break;

	default:
	case 'A':
	    {
		double    tmpNum = inNum;
		conv = UNIT_CONV;

		if (isupper((int) inFormat))
		{
		    while (tmpNum >= 1024.0 && conv <= GIGA_CONV)
		    {
			tmpNum /= 1024.0;
			conv++;
		    }
		} else
		{
		    while (tmpNum >= 1000.0 && conv <= GIGA_CONV)
		    {
			tmpNum /= 1000.0;
			conv++;
		    }
		}
		break;
	    }
	}

	if (!isupper((int) inFormat))
	{
	    inNum *= conversion_bits[conv];
	    suffix = label_bit[conv];
	} else
	{
	    inNum *= conversion_bytes[conv];
	    suffix = label_byte[conv];
	}

	/* print such that we always fit in 4 places */
	if (inNum < 9.995)
	{			/* 9.995 would be rounded to 10.0 */
	    format = "%4.2f %s";/* #.## */
	} else if (inNum < 99.95)
	{			/* 99.95 would be rounded to 100 */
	    format = "%4.1f %s";/* ##.# */
	} else if (inNum < 999.5)
	{			/* 999.5 would be rounded to 1000 */
	    format = "%4.0f %s";/* ### */
	} else
	{			/* 1000-1024 fits in 4 places If not using
				 * Adaptive sizes then this code will not
				 * control spaces */
	    format = "%4.0f %s";/* #### */
	}
//	os_snprintf(s, inLen, format, inNum, suffix);
	snprintf(s, inLen, format, inNum, suffix);

    }				/* end unit_snprintf */

#ifdef __cplusplus
}				/* end extern "C" */

#endif
