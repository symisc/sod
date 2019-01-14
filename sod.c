/*
* SOD - An Embedded Computer Vision & Machine Learning Library.
* Copyright (C) 2018 - 2019 PixLab| Symisc Systems. https://sod.pixlab.io
* Version 1.1.8
*
* Symisc Systems employs a dual licensing model that offers customers
* a choice of either our open source license (GPLv3) or a commercial
* license.
*
* For information on licensing, redistribution of the SOD library, and for a DISCLAIMER OF ALL WARRANTIES
* please visit:
*     https://pixlab.io/sod
* or contact:
*     licensing@symisc.net
*     support@pixlab.io
*/
/*
* This file is part of Symisc SOD - Open Source Release (GPLv3)
*
* SOD is free software : you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* SOD is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with SOD. If not, see <http://www.gnu.org/licenses/>.
*/
/* $SymiscID: sod.c v1.1.8 Win10 2018-02-02 05:34 stable <devel@symisc.net> $ */
#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
/*
* Ignore Microsoft compilers warnings on fopen() which is used only
* by the CNN layer for reading SOD models or saving Realnets models to disk.
*/
#define _CRT_SECURE_NO_WARNINGS
#endif /*_CRT_SECURE_NO_WARNINGS*/
/* Disable the double to float warning */
#pragma warning(disable:4244)
#pragma warning(disable:4305)
#endif /* _MSC_VER */
/* Standard C library includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#ifdef SOD_MEM_DEBUG
/* Memory leak detection which is done under Visual Studio only */
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif /* SOD_MEM_DEBUG */
#include <float.h>
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif /* _USE_MATH_DEFINES */
#include <math.h>
#include <string.h>
#include <limits.h>
/* Local includes */
#include "sod.h"
/* Forward declaration */
typedef struct SySet SySet;
typedef struct SyBlob SyBlob;
typedef struct SyString SyString;
typedef struct sod_vfs sod_vfs;
/*
* A generic dynamic set.
*/
struct SySet
{
	void *pBase;               /* Base pointer */
	size_t nUsed;              /* Total number of used slots  */
	size_t nSize;              /* Total number of available slots */
	size_t eSize;              /* Size of a single slot */
	void *pUserData;           /* User private data associated with this container */
};
#define SySetBasePtr(S)           ((S)->pBase)
#define SySetBasePtrJump(S, OFFT)  (&((char *)(S)->pBase)[OFFT*(S)->eSize])
#define SySetUsed(S)              ((S)->nUsed)
#define SySetSize(S)              ((S)->nSize)
#define SySetElemSize(S)          ((S)->eSize)
#define SySetSetUserData(S, DATA)  ((S)->pUserData = DATA)
#define SySetGetUserData(S)       ((S)->pUserData)
/*
* A variable length containers for generic data (Mostly dynamic string).
*/
struct SyBlob
{
	void   *pBlob;	          /* Base pointer */
	size_t  nByte;	          /* Total number of used bytes */
	size_t  mByte;	          /* Total number of available bytes */
	int  nFlags;	          /* Blob internal flags, see below */
};
/*
* Container for non null terminated strings.
*/
struct SyString
{
	const char *zString;  /* Raw string (May not be null terminated) */
	size_t     nByte;     /* Raw string length */
};
#define SXBLOB_LOCKED	0x01	/* Blob is locked [i.e: Cannot auto grow] */
#define SXBLOB_STATIC	0x02	/* Not allocated from heap   */
#define SXBLOB_RDONLY   0x04    /* Read-Only data */

#define SyBlobFreeSpace(BLOB)	 ((BLOB)->mByte - (BLOB)->nByte)
#define SyBlobLength(BLOB)	     ((BLOB)->nByte)
#define SyBlobData(BLOB)	     ((BLOB)->pBlob)
#define SyBlobCurData(BLOB)	     ((void*)(&((char*)(BLOB)->pBlob)[(BLOB)->nByte]))
#define SyBlobDataAt(BLOB, OFFT)	 ((void *)(&((char *)(BLOB)->pBlob)[OFFT]))
#define SyBlobGetAllocator(BLOB) ((BLOB)->pAllocator)
#define SyStringData(RAW)	((RAW)->zString)
#define SyStringLength(RAW)	((RAW)->nByte)
#define SyStringInitFromBuf(RAW, ZBUF, NLEN){\
	(RAW)->zString 	= (const char *)ZBUF;\
	(RAW)->nByte	= (size_t)(NLEN);\
}
#define SyStringDupPtr(RAW1, RAW2)\
	(RAW1)->zString = (RAW2)->zString;\
	(RAW1)->nByte = (RAW2)->nByte;

#define SyStringTrimLeadingChar(RAW, CHAR)\
	while((RAW)->nByte > 0 && (RAW)->zString[0] == CHAR ){\
			(RAW)->zString++;\
			(RAW)->nByte--;\
	}
#define SyStringTrimTrailingChar(RAW, CHAR)\
	while((RAW)->nByte > 0 && (RAW)->zString[(RAW)->nByte - 1] == CHAR){\
		(RAW)->nByte--;\
	}
#define SyStringCmp(RAW1, RAW2, xCMP)\
	(((RAW1)->nByte == (RAW2)->nByte) ? xCMP((RAW1)->zString, (RAW2)->zString, (RAW2)->nByte) : (int)((RAW1)->nByte - (RAW2)->nByte))
/*
* value pathinfo(string $path [,int $options = PATHINFO_DIRNAME | PATHINFO_BASENAME | PATHINFO_EXTENSION | PATHINFO_FILENAME ])
* Returns information about a file path.
* Taken from the Symisc PH7 source tree, http://ph7.symisc.net
*/
typedef struct sod_path_info sod_path_info;
struct sod_path_info
{
	SyString sDir; /* Directory [i.e: /var/www] */
	SyString sBasename; /* Basename [i.e httpd.conf] */
	SyString sExtension; /* File extension [i.e xml,pdf..] */
	SyString sFilename;  /* Filename */
};
#ifndef MAX
#define MAX(a, b) ((a)>(b)?(a):(b))
#endif /* MAX */
#ifndef MIN
#define MIN(a, b) ((a)<(b)?(a):(b))
#endif /* MIN */
/*
* CAPIREF: OS Interface Object
*
* An instance of the sod_vfs object defines the interface between
* the sod core and the underlying operating system.  The "vfs"
* in the name of the object stands for "Virtual File System".
*
* Only a single vfs can be registered within the sod core.
* Vfs registration is done using the [sod_lib_config()] interface
* with a configuration verb set to LIBCOX_LIB_CONFIG_VFS.
* Note that Windows and UNIX (Linux, FreeBSD, Solaris, Mac OS X, etc.) users
* does not have to worry about registering and installing a vfs since sod
* come with a built-in vfs for these platforms that implements most the methods
* defined below.
*
* Clients running on exotic systems (ie: Other than Windows and UNIX systems)
* must register their own vfs in order to be able to use the sod library.
*
* The value of the iVersion field is initially 1 but may be larger in
* future versions of sod.
*
* The szOsFile field is the size of the subclassed [sod_file] structure
* used by this VFS. mxPathname is the maximum length of a pathname in this VFS.
*
* At least szOsFile bytes of memory are allocated by sod to hold the [sod_file]
* structure passed as the third argument to xOpen. The xOpen method does not have to
* allocate the structure; it should just fill it in. Note that the xOpen method must
* set the sod_file.pMethods to either a valid [sod_io_methods] object or to NULL.
* xOpen must do this even if the open fails. sod expects that the sod_file.pMethods
* element will be valid after xOpen returns regardless of the success or failure of the
* xOpen call.
*/
#define LIBCOX_VFS_VERSION 2900 /* 2.9 */
struct sod_vfs {
	const char *zName;       /* Name of this virtual file system [i.e: Windows, UNIX, etc.] */
	int iVersion;            /* Structure version number (currently 2.6) */
	int szOsFile;           
	int mxPathname;          /* Maximum file pathname length */
							 /* Directory functions */
	int(*xChdir)(const char *);                     /* Change directory */
	int(*xGetcwd)(SyBlob *);                /* Get the current working directory */
	int(*xMkdir)(const char *, int, int);           /* Make directory */
	int(*xRmdir)(const char *);                     /* Remove directory */
	int(*xIsdir)(const char *);                     /* Tells whether the filename is a directory */
	int(*xRename)(const char *, const char *);       /* Renames a file or directory */
	int(*xRealpath)(const char *, SyBlob *);    /* Return canonicalized absolute pathname*/
												/* Dir handle */
	int(*xOpenDir)(const char *, void **);    /* Open directory handle */
	void(*xCloseDir)(void *pHandle);                           /* Close directory handle */
	int(*xDirRead)(void *pHandle, SyBlob *);     /* Read the next entry from the directory handle */
	void(*xDirRewind)(void *pHandle);                   /* Rewind the cursor */
														/* Systems functions */
	int(*xUnlink)(const char *);                    /* Deletes a file */
	int(*xFileExists)(const char *);                /* Checks whether a file or directory exists */
	int64_t(*xFreeSpace)(const char *);        /* Available space on filesystem or disk partition */
	int64_t(*xTotalSpace)(const char *);       /* Total space on filesystem or disk partition */
	int64_t(*xFileSize)(const char *);         /* Gets file size */
	int(*xIsfile)(const char *);                    /* Tells whether the filename is a regular file */
	int(*xReadable)(const char *);                  /* Tells whether a file exists and is readable */
	int(*xWritable)(const char *);                  /* Tells whether the filename is writable */
	int(*xExecutable)(const char *);                /* Tells whether the filename is executable */
	int(*xGetenv)(const char *, SyBlob *);      /* Gets the value of an environment variable */
	int(*xSetenv)(const char *, const char *);       /* Sets the value of an environment variable */
	int(*xMmap)(const char *, void **, size_t *); /* Read-only memory map of the whole file */
	void(*xUnmap)(void *, size_t);                /* Unmap a memory view */
	void(*xTempDir)(SyBlob *);                 /* Get path of the temporary directory */
	float(*xTicks)();                          /* High precision timer */
};
/* @Implementation */
static int SyBlobInit(SyBlob *pBlob)
{
	pBlob->pBlob = 0;
	pBlob->mByte = pBlob->nByte = 0;
	pBlob->nFlags = 0;
	return SOD_OK;
}
#ifndef SXBLOB_MIN_GROWTH
#define SXBLOB_MIN_GROWTH 16
#endif
static int BlobPrepareGrow(SyBlob *pBlob, size_t *pByte)
{
	size_t nByte;
	void *pNew;
	nByte = *pByte;
	if (pBlob->nFlags & (SXBLOB_LOCKED | SXBLOB_STATIC)) {
		if (SyBlobFreeSpace(pBlob) < nByte) {
			*pByte = SyBlobFreeSpace(pBlob);
			if ((*pByte) == 0) {
				return -1;
			}
		}
		return SOD_OK;
	}
	if (pBlob->nFlags & SXBLOB_RDONLY) {
		/* Make a copy of the read-only item */
		if (pBlob->nByte > 0) {
			pNew = realloc(pBlob->pBlob, pBlob->nByte);
			if (pNew == 0) {
				return SOD_OUTOFMEM;
			}
			pBlob->pBlob = pNew;
			pBlob->mByte = pBlob->nByte;
		}
		else {
			pBlob->pBlob = 0;
			pBlob->mByte = 0;
		}
		/* Remove the read-only flag */
		pBlob->nFlags &= ~SXBLOB_RDONLY;
	}
	if (SyBlobFreeSpace(pBlob) >= nByte) {
		return SOD_OK;
	}
	if (pBlob->mByte > 0) {
		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;
	}
	else if (nByte < SXBLOB_MIN_GROWTH) {
		nByte = SXBLOB_MIN_GROWTH;
	}
	pNew = realloc(pBlob->pBlob, nByte);
	if (pNew == 0) {
		return SOD_OUTOFMEM;
	}
	pBlob->pBlob = pNew;
	pBlob->mByte = nByte;
	return SOD_OK;
}
static int SyBlobAppend(SyBlob *pBlob, const void *pData, size_t nSize)
{
	uint8_t *zBlob;
	int rc;
	if (nSize < 1) {
		return SOD_OK;
	}
	rc = BlobPrepareGrow(&(*pBlob), &nSize);
	if (SOD_OK != rc) {
		return rc;
	}
	if (pData) {
		zBlob = (uint8_t *)pBlob->pBlob;
		zBlob = &zBlob[pBlob->nByte];
		pBlob->nByte += nSize;
		memcpy(zBlob, pData, nSize);
	}
	return SOD_OK;
}
#define SyBlobStrAppend(pBlob,STR) SyBlobAppend(&(*pBlob), (const void *)STR,strlen(STR))
#define SyBlobNullAppend(pBlob)   if( SOD_OK == SyBlobAppend(&(*pBlob), (const void *)"\0", sizeof(char)) ){(pBlob)->nByte--;}
static inline int SyBlobReset(SyBlob *pBlob)
{
	pBlob->nByte = 0;
	if (pBlob->nFlags & SXBLOB_RDONLY) {
		/* Read-only (Not malloced chunk) */
		pBlob->pBlob = 0;
		pBlob->mByte = 0;
		pBlob->nFlags &= ~SXBLOB_RDONLY;
	}
	return SOD_OK;
}
static int SyBlobRelease(SyBlob *pBlob)
{
	if ((pBlob->nFlags & (SXBLOB_STATIC | SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0) {
		free(pBlob->pBlob);
	}
	pBlob->pBlob = 0;
	pBlob->nByte = pBlob->mByte = 0;
	pBlob->nFlags = 0;
	return SOD_OK;
}
#ifdef SOD_ENABLE_NET_TRAIN
/* SyRunTimeApi: sxfmt.c */
#define SXFMT_BUFSIZ 1024 /* Conversion buffer size */
/* Signature of the consumer routine */
typedef int(*ProcConsumer)(const void *, unsigned int, void *);
/*
** Conversion types fall into various categories as defined by the
** following enumeration.
*/
#define SXFMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */
#define SXFMT_FLOAT       2 /* Floating point.%f */
#define SXFMT_EXP         3 /* Exponentional notation.%e and %E */
#define SXFMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */
#define SXFMT_SIZE        5 /* Total number of characters processed so far.%n */
#define SXFMT_STRING      6 /* Strings.%s */
#define SXFMT_PERCENT     7 /* Percent symbol.%% */
#define SXFMT_CHARX       8 /* Characters.%c */
#define SXFMT_ERROR       9 /* Used to indicate no such conversion type */
/* Extension by Symisc Systems */
#define SXFMT_RAWSTR     13 /* %z Pointer to raw string (SyString *) */
#define SXFMT_UNUSED     15 
/*
** Allowed values for SyFmtInfo.flags
*/
#define SXFLAG_SIGNED	0x01
#define SXFLAG_UNSIGNED 0x02
/* Allowed values for SyFmtConsumer.nType */
#define SXFMT_CONS_PROC		1	/* Consumer is a procedure */
#define SXFMT_CONS_STR		2	/* Consumer is a managed string */
#define SXFMT_CONS_FILE		5	/* Consumer is an open File */
#define SXFMT_CONS_BLOB		6	/* Consumer is a BLOB */
/*
** Each built-in conversion character (ex: the 'd' in "%d") is described
** by an instance of the following structure
*/
typedef struct SyFmtInfo SyFmtInfo;
struct SyFmtInfo
{
	char fmttype;  /* The format field code letter [i.e: 'd', 's', 'x'] */
	uint8_t base;     /* The base for radix conversion */
	int flags;    /* One or more of SXFLAG_ constants below */
	uint8_t type;     /* Conversion paradigm */
	char *charset; /* The character set for conversion */
	char *prefix;  /* Prefix on non-zero values in alt format */
};
typedef struct SyFmtConsumer SyFmtConsumer;
struct SyFmtConsumer
{
	size_t nLen; /* Total output length */
	int nType; /* Type of the consumer see below */
	int rc;	/* Consumer return value;Abort processing if rc != SOD_OK */
	union {
		struct {
			ProcConsumer xUserConsumer;
			void *pUserData;
		}sFunc;
		SyBlob *pBlob;
	}uConsumer;
};
static int getdigit(long double *val, int *cnt)
{
	long double d;
	int digit;

	if ((*cnt)++ >= 16) {
		return '0';
	}
	digit = (int)*val;
	d = digit;
	*val = (*val - d)*10.0;
	return digit + '0';
}
/*
* The following routine was taken from the SQLITE2 source tree and was
* extended by Symisc Systems to fit its need.
* Status: Public Domain
*/
static int InternFormat(ProcConsumer xConsumer, void *pUserData, const char *zFormat, va_list ap)
{
	/*
	* The following table is searched linearly, so it is good to put the most frequently
	* used conversion types first.
	*/
	static const SyFmtInfo aFmt[] = {
		{ 'd', 10, SXFLAG_SIGNED, SXFMT_RADIX, "0123456789", 0 },
	{ 's',  0, 0, SXFMT_STRING,     0,                  0 },
	{ 'c',  0, 0, SXFMT_CHARX,      0,                  0 },
	{ 'x', 16, 0, SXFMT_RADIX,      "0123456789abcdef", "x0" },
	{ 'X', 16, 0, SXFMT_RADIX,      "0123456789ABCDEF", "X0" },
	/* -- Extensions by Symisc Systems -- */
	{ 'z',  0, 0, SXFMT_RAWSTR,     0,                   0 }, /* Pointer to a raw string (SyString *) */
	{ 'B',  2, 0, SXFMT_RADIX,      "01",                "b0" },
	/* -- End of Extensions -- */
	{ 'o',  8, 0, SXFMT_RADIX,      "01234567",         "0" },
	{ 'u', 10, 0, SXFMT_RADIX,      "0123456789",       0 },
	{ 'f',  0, SXFLAG_SIGNED, SXFMT_FLOAT,       0,     0 },
	{ 'e',  0, SXFLAG_SIGNED, SXFMT_EXP,        "e",    0 },
	{ 'E',  0, SXFLAG_SIGNED, SXFMT_EXP,        "E",    0 },
	{ 'g',  0, SXFLAG_SIGNED, SXFMT_GENERIC,    "e",    0 },
	{ 'G',  0, SXFLAG_SIGNED, SXFMT_GENERIC,    "E",    0 },
	{ 'i', 10, SXFLAG_SIGNED, SXFMT_RADIX, "0123456789", 0 },
	{ 'n',  0, 0, SXFMT_SIZE,       0,                  0 },
	{ '%',  0, 0, SXFMT_PERCENT,    0,                  0 },
	{ 'p', 10, 0, SXFMT_RADIX,      "0123456789",       0 }
	};
	int c;                     /* Next character in the format string */
	char *bufpt;               /* Pointer to the conversion buffer */
	int precision;             /* Precision of the current field */
	int length;                /* Length of the field */
	int idx;                   /* A general purpose loop counter */
	int width;                 /* Width of the current field */
	uint8_t flag_leftjustify;   /* True if "-" flag is present */
	uint8_t flag_plussign;      /* True if "+" flag is present */
	uint8_t flag_blanksign;     /* True if " " flag is present */
	uint8_t flag_alternateform; /* True if "#" flag is present */
	uint8_t flag_zeropad;       /* True if field width constant starts with zero */
	uint8_t flag_long;          /* True if "l" flag is present */
	int64_t longvalue;         /* Value for integer types */
	const SyFmtInfo *infop;  /* Pointer to the appropriate info structure */
	char buf[SXFMT_BUFSIZ];  /* Conversion buffer */
	char prefix;             /* Prefix character."+" or "-" or " " or '\0'.*/
	uint8_t errorflag = 0;      /* True if an error is encountered */
	uint8_t xtype;              /* Conversion paradigm */
	static char spaces[] = "                                                  ";
#define etSPACESIZE ((int)sizeof(spaces)-1)
	long double realvalue;    /* Value for real types */
	int  exp;                /* exponent of real numbers */
	double rounder;          /* Used for rounding floating point values */
	uint8_t flag_dp;            /* True if decimal point should be shown */
	uint8_t flag_rtz;           /* True if trailing zeros should be removed */
	uint8_t flag_exp;           /* True to force display of the exponent */
	int nsd;                 /* Number of significant digits returned */
	int rc;

	length = 0;
	bufpt = 0;
	for (; (c = (*zFormat)) != 0; ++zFormat) {
		if (c != '%') {
			unsigned int amt;
			bufpt = (char *)zFormat;
			amt = 1;
			while ((c = (*++zFormat)) != '%' && c != 0) amt++;
			rc = xConsumer((const void *)bufpt, amt, pUserData);
			if (rc != SOD_OK) {
				return SOD_ABORT; /* Consumer routine request an operation abort */
			}
			if (c == 0) {
				return errorflag > 0 ? -1 : SOD_OK;
			}
		}
		if ((c = (*++zFormat)) == 0) {
			errorflag = 1;
			rc = xConsumer("%", sizeof("%") - 1, pUserData);
			if (rc != SOD_OK) {
				return SOD_ABORT; /* Consumer routine request an operation abort */
			}
			return errorflag > 0 ? -1 : SOD_OK;
		}
		/* Find out what flags are present */
		flag_leftjustify = flag_plussign = flag_blanksign =
			flag_alternateform = flag_zeropad = 0;
		do {
			switch (c) {
			case '-':   flag_leftjustify = 1;     c = 0;   break;
			case '+':   flag_plussign = 1;        c = 0;   break;
			case ' ':   flag_blanksign = 1;       c = 0;   break;
			case '#':   flag_alternateform = 1;   c = 0;   break;
			case '0':   flag_zeropad = 1;         c = 0;   break;
			default:                                       break;
			}
		} while (c == 0 && (c = (*++zFormat)) != 0);
		/* Get the field width */
		width = 0;
		if (c == '*') {
			width = va_arg(ap, int);
			if (width<0) {
				flag_leftjustify = 1;
				width = -width;
			}
			c = *++zFormat;
		}
		else {
			while (c >= '0' && c <= '9') {
				width = width * 10 + c - '0';
				c = *++zFormat;
			}
		}
		if (width > SXFMT_BUFSIZ - 10) {
			width = SXFMT_BUFSIZ - 10;
		}
		/* Get the precision */
		precision = -1;
		if (c == '.') {
			precision = 0;
			c = *++zFormat;
			if (c == '*') {
				precision = va_arg(ap, int);
				if (precision<0) precision = -precision;
				c = *++zFormat;
			}
			else {
				while (c >= '0' && c <= '9') {
					precision = precision * 10 + c - '0';
					c = *++zFormat;
				}
			}
		}
		/* Get the conversion type modifier */
		flag_long = 0;
		if (c == 'l' || c == 'q' /* BSD quad (expect a 64-bit integer) */) {
			flag_long = (c == 'q') ? 2 : 1;
			c = *++zFormat;
			if (c == 'l') {
				/* Standard printf emulation 'lld' (expect a 64bit integer) */
				flag_long = 2;
			}
		}
		/* Fetch the info entry for the field */
		infop = 0;
		xtype = SXFMT_ERROR;
		for (idx = 0; idx< (int)sizeof(aFmt) / sizeof(aFmt[0]); idx++) {
			if (c == aFmt[idx].fmttype) {
				infop = &aFmt[idx];
				xtype = infop->type;
				break;
			}
		}
		/*
		** At this point, variables are initialized as follows:
		**
		**   flag_alternateform          TRUE if a '#' is present.
		**   flag_plussign               TRUE if a '+' is present.
		**   flag_leftjustify            TRUE if a '-' is present or if the
		**                               field width was negative.
		**   flag_zeropad                TRUE if the width began with 0.
		**   flag_long                   TRUE if the letter 'l' (ell) or 'q'(BSD quad) prefixed
		**                               the conversion character.
		**   flag_blanksign              TRUE if a ' ' is present.
		**   width                       The specified field width.This is
		**                               always non-negative.Zero is the default.
		**   precision                   The specified precision.The default
		**                               is -1.
		**   xtype                       The object of the conversion.
		**   infop                       Pointer to the appropriate info struct.
		*/
		switch (xtype) {
		case SXFMT_RADIX:
			if (flag_long > 0) {
				if (flag_long > 1) {
					/* BSD quad: expect a 64-bit integer */
					longvalue = va_arg(ap, int64_t);
				}
				else {
					longvalue = va_arg(ap, long double);
				}
			}
			else {
				if (infop->flags & SXFLAG_SIGNED) {
					longvalue = va_arg(ap, int);
				}
				else {
					longvalue = va_arg(ap, unsigned int);
				}
			}
			/* Limit the precision to prevent overflowing buf[] during conversion */
			if (precision>SXFMT_BUFSIZ - 40) precision = SXFMT_BUFSIZ - 40;
#if 1
			/* For the format %#x, the value zero is printed "0" not "0x0".
			** I think this is stupid.*/
			if (longvalue == 0) flag_alternateform = 0;
#else
			/* More sensible: turn off the prefix for octal (to prevent "00"),
			** but leave the prefix for hex.*/
			if (longvalue == 0 && infop->base == 8) flag_alternateform = 0;
#endif
			if (infop->flags & SXFLAG_SIGNED) {
				if (longvalue<0) {
					longvalue = -longvalue;
					/* Ticket 1433-003 */
					if (longvalue < 0) {
						/* Overflow */
						longvalue = 0x7FFFFFFFFFFFFFFF;
					}
					prefix = '-';
				}
				else if (flag_plussign)  prefix = '+';
				else if (flag_blanksign)  prefix = ' ';
				else                       prefix = 0;
			}
			else {
				if (longvalue<0) {
					longvalue = -longvalue;
					/* Ticket 1433-003 */
					if (longvalue < 0) {
						/* Overflow */
						longvalue = 0x7FFFFFFFFFFFFFFF;
					}
				}
				prefix = 0;
			}
			if (flag_zeropad && precision<width - (prefix != 0)) {
				precision = width - (prefix != 0);
			}
			bufpt = &buf[SXFMT_BUFSIZ - 1];
			{
				register char *cset;      /* Use registers for speed */
				register int base;
				cset = infop->charset;
				base = infop->base;
				do {                                           /* Convert to ascii */
					*(--bufpt) = cset[longvalue%base];
					longvalue = longvalue / base;
				} while (longvalue>0);
			}
			length = &buf[SXFMT_BUFSIZ - 1] - bufpt;
			for (idx = precision - length; idx>0; idx--) {
				*(--bufpt) = '0';                             /* Zero pad */
			}
			if (prefix) *(--bufpt) = prefix;               /* Add sign */
			if (flag_alternateform && infop->prefix) {      /* Add "0" or "0x" */
				char *pre, x;
				pre = infop->prefix;
				if (*bufpt != pre[0]) {
					for (pre = infop->prefix; (x = (*pre)) != 0; pre++) *(--bufpt) = x;
				}
			}
			length = &buf[SXFMT_BUFSIZ - 1] - bufpt;
			break;
		case SXFMT_FLOAT:
		case SXFMT_EXP:
		case SXFMT_GENERIC:
			realvalue = va_arg(ap, double);
			if (precision<0) precision = 6;         /* Set default precision */
			if (precision>SXFMT_BUFSIZ - 40) precision = SXFMT_BUFSIZ - 40;
			if (realvalue<0.0) {
				realvalue = -realvalue;
				prefix = '-';
			}
			else {
				if (flag_plussign)          prefix = '+';
				else if (flag_blanksign)    prefix = ' ';
				else                         prefix = 0;
			}
			if (infop->type == SXFMT_GENERIC && precision>0) precision--;
			rounder = 0.0;
#if 0
			/* Rounding works like BSD when the constant 0.4999 is used.Wierd! */
			for (idx = precision, rounder = 0.4999; idx>0; idx--, rounder *= 0.1);
#else
			/* It makes more sense to use 0.5 */
			for (idx = precision, rounder = 0.5; idx>0; idx--, rounder *= 0.1);
#endif
			if (infop->type == SXFMT_FLOAT) realvalue += rounder;
			/* Normalize realvalue to within 10.0 > realvalue >= 1.0 */
			exp = 0;
			if (realvalue>0.0) {
				while (realvalue >= 1e8 && exp <= 350) { realvalue *= 1e-8; exp += 8; }
				while (realvalue >= 10.0 && exp <= 350) { realvalue *= 0.1; exp++; }
				while (realvalue<1e-8 && exp >= -350) { realvalue *= 1e8; exp -= 8; }
				while (realvalue<1.0 && exp >= -350) { realvalue *= 10.0; exp--; }
				if (exp>350 || exp<-350) {
					bufpt = "NaN";
					length = 3;
					break;
				}
			}
			bufpt = buf;
			/*
			** If the field type is etGENERIC, then convert to either etEXP
			** or etFLOAT, as appropriate.
			*/
			flag_exp = xtype == SXFMT_EXP;
			if (xtype != SXFMT_FLOAT) {
				realvalue += rounder;
				if (realvalue >= 10.0) { realvalue *= 0.1; exp++; }
			}
			if (xtype == SXFMT_GENERIC) {
				flag_rtz = !flag_alternateform;
				if (exp<-4 || exp>precision) {
					xtype = SXFMT_EXP;
				}
				else {
					precision = precision - exp;
					xtype = SXFMT_FLOAT;
				}
			}
			else {
				flag_rtz = 0;
			}
			/*
			** The "exp+precision" test causes output to be of type etEXP if
			** the precision is too large to fit in buf[].
			*/
			nsd = 0;
			if (xtype == SXFMT_FLOAT && exp + precision<SXFMT_BUFSIZ - 30) {
				flag_dp = (precision>0 || flag_alternateform);
				if (prefix) *(bufpt++) = prefix;         /* Sign */
				if (exp<0)  *(bufpt++) = '0';            /* Digits before "." */
				else for (; exp >= 0; exp--) *(bufpt++) = (char)getdigit(&realvalue, &nsd);
				if (flag_dp) *(bufpt++) = '.';           /* The decimal point */
				for (exp++; exp<0 && precision>0; precision--, exp++) {
					*(bufpt++) = '0';
				}
				while ((precision--)>0) *(bufpt++) = (char)getdigit(&realvalue, &nsd);
				*(bufpt--) = 0;                           /* Null terminate */
				if (flag_rtz && flag_dp) {     /* Remove trailing zeros and "." */
					while (bufpt >= buf && *bufpt == '0') *(bufpt--) = 0;
					if (bufpt >= buf && *bufpt == '.') *(bufpt--) = 0;
				}
				bufpt++;                            /* point to next free slot */
			}
			else {    /* etEXP or etGENERIC */
				flag_dp = (precision>0 || flag_alternateform);
				if (prefix) *(bufpt++) = prefix;   /* Sign */
				*(bufpt++) = (char)getdigit(&realvalue, &nsd);  /* First digit */
				if (flag_dp) *(bufpt++) = '.';     /* Decimal point */
				while ((precision--)>0) *(bufpt++) = (char)getdigit(&realvalue, &nsd);
				bufpt--;                            /* point to last digit */
				if (flag_rtz && flag_dp) {          /* Remove tail zeros */
					while (bufpt >= buf && *bufpt == '0') *(bufpt--) = 0;
					if (bufpt >= buf && *bufpt == '.') *(bufpt--) = 0;
				}
				bufpt++;                            /* point to next free slot */
				if (exp || flag_exp) {
					*(bufpt++) = infop->charset[0];
					if (exp<0) { *(bufpt++) = '-'; exp = -exp; } /* sign of exp */
					else { *(bufpt++) = '+'; }
					if (exp >= 100) {
						*(bufpt++) = (char)((exp / 100) + '0');                /* 100's digit */
						exp %= 100;
					}
					*(bufpt++) = (char)(exp / 10 + '0');                     /* 10's digit */
					*(bufpt++) = (char)(exp % 10 + '0');                     /* 1's digit */
				}
			}
			/* The converted number is in buf[] and zero terminated.Output it.
			** Note that the number is in the usual order, not reversed as with
			** integer conversions.*/
			length = bufpt - buf;
			bufpt = buf;

			/* Special case:  Add leading zeros if the flag_zeropad flag is
			** set and we are not left justified */
			if (flag_zeropad && !flag_leftjustify && length < width) {
				int i;
				int nPad = width - length;
				for (i = width; i >= nPad; i--) {
					bufpt[i] = bufpt[i - nPad];
				}
				i = prefix != 0;
				while (nPad--) bufpt[i++] = '0';
				length = width;
			}
			break;
		case SXFMT_SIZE: {
			size_t *pSize = va_arg(ap, size_t *);
			*pSize = ((SyFmtConsumer *)pUserData)->nLen;
			length = width = 0;
		}
						 break;
		case SXFMT_PERCENT:
			buf[0] = '%';
			bufpt = buf;
			length = 1;
			break;
		case SXFMT_CHARX:
			c = va_arg(ap, int);
			buf[0] = (char)c;
			/* Limit the precision to prevent overflowing buf[] during conversion */
			if (precision>SXFMT_BUFSIZ - 40) precision = SXFMT_BUFSIZ - 40;
			if (precision >= 0) {
				for (idx = 1; idx<precision; idx++) buf[idx] = (char)c;
				length = precision;
			}
			else {
				length = 1;
			}
			bufpt = buf;
			break;
		case SXFMT_STRING:
			bufpt = va_arg(ap, char*);
			if (bufpt == 0) {
				bufpt = " ";
				length = (int)sizeof(" ") - 1;
				break;
			}
			length = precision;
			if (precision < 0) {
				/* Symisc extension */
				length = (int)strlen(bufpt);
			}
			if (precision >= 0 && precision<length) length = precision;
			break;
		case SXFMT_RAWSTR: {
			/* Symisc extension */
			SyString *pStr = va_arg(ap, SyString *);
			if (pStr == 0 || pStr->zString == 0) {
				bufpt = " ";
				length = (int)sizeof(char);
				break;
			}
			bufpt = (char *)pStr->zString;
			length = (int)pStr->nByte;
			break;
		}
		case SXFMT_ERROR:
			buf[0] = '?';
			bufpt = buf;
			length = (int)sizeof(char);
			if (c == 0) zFormat--;
			break;
		}/* End switch over the format type */
		 /*
		 ** The text of the conversion is pointed to by "bufpt" and is
		 ** "length" characters long.The field width is "width".Do
		 ** the output.
		 */
		if (!flag_leftjustify) {
			register int nspace;
			nspace = width - length;
			if (nspace>0) {
				while (nspace >= etSPACESIZE) {
					rc = xConsumer(spaces, etSPACESIZE, pUserData);
					if (rc != SOD_OK) {
						return SOD_ABORT; /* Consumer routine request an operation abort */
					}
					nspace -= etSPACESIZE;
				}
				if (nspace>0) {
					rc = xConsumer(spaces, (unsigned int)nspace, pUserData);
					if (rc != SOD_OK) {
						return SOD_ABORT; /* Consumer routine request an operation abort */
					}
				}
			}
		}
		if (length>0) {
			rc = xConsumer(bufpt, (unsigned int)length, pUserData);
			if (rc != SOD_OK) {
				return SOD_ABORT; /* Consumer routine request an operation abort */
			}
		}
		if (flag_leftjustify) {
			register int nspace;
			nspace = width - length;
			if (nspace>0) {
				while (nspace >= etSPACESIZE) {
					rc = xConsumer(spaces, etSPACESIZE, pUserData);
					if (rc != SOD_OK) {
						return SOD_ABORT; /* Consumer routine request an operation abort */
					}
					nspace -= etSPACESIZE;
				}
				if (nspace>0) {
					rc = xConsumer(spaces, (unsigned int)nspace, pUserData);
					if (rc != SOD_OK) {
						return SOD_ABORT; /* Consumer routine request an operation abort */
					}
				}
			}
		}
	}/* End for loop over the format string */
	return errorflag ? -1 : SOD_OK;
}
static int FormatConsumer(const void *pSrc, unsigned int nLen, void *pData)
{
	SyFmtConsumer *pConsumer = (SyFmtConsumer *)pData;
	int rc = SOD_ABORT;
	switch (pConsumer->nType) {
	case SXFMT_CONS_PROC:
		/* User callback */
		rc = pConsumer->uConsumer.sFunc.xUserConsumer(pSrc, nLen, pConsumer->uConsumer.sFunc.pUserData);
		break;
	case SXFMT_CONS_BLOB:
		/* Blob consumer */
		rc = SyBlobAppend(pConsumer->uConsumer.pBlob, pSrc, (unsigned int)nLen);
		break;
	default:
		/* Unknown consumer */
		break;
	}
	/* Update total number of bytes consumed so far */
	pConsumer->nLen += nLen;
	pConsumer->rc = rc;
	return rc;
}
static int32_t FormatMount(int32_t nType, void *pConsumer, ProcConsumer xUserCons, void *pUserData, size_t *pOutLen, const char *zFormat, va_list ap)
{
	SyFmtConsumer sCons;
	sCons.nType = nType;
	sCons.rc = SOD_OK;
	sCons.nLen = 0;
	if (pOutLen) {
		*pOutLen = 0;
	}
	switch (nType) {
	case SXFMT_CONS_PROC:
		sCons.uConsumer.sFunc.xUserConsumer = xUserCons;
		sCons.uConsumer.sFunc.pUserData = pUserData;
		break;
	case SXFMT_CONS_BLOB:
		sCons.uConsumer.pBlob = (SyBlob *)pConsumer;
		break;
	default:
		return -1; /* Unknown */
	}
	InternFormat(FormatConsumer, &sCons, zFormat, ap);
	if (pOutLen) {
		*pOutLen = sCons.nLen;
	}
	return sCons.rc;
}
static size_t SyBlobFormatAp(SyBlob *pBlob, const char *zFormat, va_list ap)
{
	size_t n = 0; /* cc warning */
	FormatMount(SXFMT_CONS_BLOB, &(*pBlob), 0, 0, &n, zFormat, ap);
	SyBlobNullAppend(pBlob);
	return n;
}
#endif /* SOD_ENABLE_NET_TRAIN */
/*
* Dynamic Set Implementation.
*/
static int SySetInit(SySet *pSet, size_t ElemSize)
{
	pSet->nSize = 0;
	pSet->nUsed = 0;
	pSet->eSize = ElemSize;
	pSet->pBase = 0;
	pSet->pUserData = 0;
	return 0;
}
static int SySetPut(SySet *pSet, const void *pItem)
{
	unsigned char *zbase;
	if (pSet->nUsed >= pSet->nSize) {
		void *pNew;
		if (pSet->nSize < 1) {
			pSet->nSize = 8;
		}
		pNew = realloc(pSet->pBase, pSet->eSize * pSet->nSize * 2);
		if (pNew == 0) {
			return SOD_OUTOFMEM;
		}
		pSet->pBase = pNew;
		pSet->nSize <<= 1;
	}
	if (pItem) {
		zbase = (unsigned char *)pSet->pBase;
		memcpy((void *)&zbase[pSet->nUsed * pSet->eSize], pItem, pSet->eSize);
		pSet->nUsed++;
	}
	return SOD_OK;
}
static inline void SySetReset(SySet *pSet)
{
	pSet->nUsed = 0;
}
#ifdef SOD_ENABLE_NET_TRAIN
static void * SySetPeek(SySet *pSet)
{
	const char *zBase;
	if (pSet->nUsed <= 0) {
		return 0;
	}
	zBase = (const char *)pSet->pBase;
	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];
}
#endif /* SOD_ENABLE_NET_TRAIN */
static void * SySetFetch(SySet *pSet, size_t idx)
{
	const char *zBase;
	if (idx >= pSet->nUsed) {
		return 0;
	}
	zBase = (const char *)pSet->pBase;
	return (void *)&zBase[idx * pSet->eSize];
}
static void SySetRelease(SySet *pSet)
{
	if (pSet->pBase) {
		free(pSet->pBase);
	}
	pSet->pBase = 0;
	pSet->nUsed = 0;
}
static int SySetAlloc(SySet *pSet, int nItem)
{
	if (pSet->nSize > 0) {
		return SOD_LIMIT;
	}
	if (nItem < 8) {
		nItem = 8;
	}
	pSet->pBase = realloc(pSet->pBase, pSet->eSize * nItem);
	if (pSet->pBase == 0) {
		return SOD_OUTOFMEM;
	}
	pSet->nSize = nItem;
	return SOD_OK;
}
#if defined (_WIN32) || defined (WIN32) ||  defined (_WIN64) || defined (WIN64) || defined(__MINGW32__) || defined (_MSC_VER)
/* Windows Systems */
#if !defined(__WINNT__)
#define __WINNT__
#endif 
#else
/*
* By default we will assume that we are compiling on a UNIX like (iOS and Android included) system.
* Otherwise the OS_OTHER directive must be defined.
*/
#if !defined(OS_OTHER)
#if !defined(__UNIXES__)
#define __UNIXES__
#endif /* __UNIXES__ */
#else
#endif /* OS_OTHER */
#endif /* __WINNT__/__UNIXES__ */
/*
* SOD Built-in VFS which is Based on Libcox another open source library developed by Symisc Systems.
*/
/*
* Symisc libcox: Cross Platform Utilities & System Calls.
* Copyright (C) 2014, 2015 Symisc Systems http://libcox.symisc.net/
* Version 1.7
* For information on licensing, redistribution of this file, and for a DISCLAIMER OF ALL WARRANTIES
* please contact Symisc Systems via:
*       legal@symisc.net
*       licensing@symisc.net
*       contact@symisc.net
* or visit:
*      http://libcox.symisc.net/
*/
/* $SymiscID: vfs.c v1.3 FreeBSD 2017-05-22 01:19 stable <chm@symisc.net> $ */
/*
* Virtual File System (VFS) for libcox (SOD modified).
*/
/*
* Wrapper function used to mimic some Libcox code that is no longer needed here.
*/
static inline int libcox_result_string(SyBlob *pBlob, const char *zBuf, int nLen)
{
	int rc;
	if (nLen < 0) {
		nLen = (int)strlen(zBuf);
	}
	rc = SyBlobAppend(&(*pBlob), zBuf, (size_t)nLen);
	SyBlobNullAppend(&(*pBlob));
	return rc;
}
#ifdef __WINNT__
/*
* Windows VFS implementation for the LIBCOX engine.
* Authors:
*    Symisc Systems, devel@symisc.net.
*    Copyright (C) Symisc Systems, http://libcox.symisc.net
* Status:
*    Stable.
*/
/* What follows here is code that is specific to windows systems. */
#include <Windows.h>
/*
** Convert a UTF-8 string to microsoft unicode (UTF-16?).
**
** Space to hold the returned string is obtained from HeapAlloc().
** Taken from the sqlite3 source tree
** status: Public Domain
*/
static WCHAR *utf8ToUnicode(const char *zFilename) {
	int nChar;
	WCHAR *zWideFilename;

	nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, 0, 0);
	zWideFilename = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, nChar * sizeof(zWideFilename[0]));
	if (zWideFilename == 0) {
		return 0;
	}
	nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, zWideFilename, nChar);
	if (nChar == 0) {
		HeapFree(GetProcessHeap(), 0, zWideFilename);
		return 0;
	}
	return zWideFilename;
}
/*
** Convert a UTF-8 filename into whatever form the underlying
** operating system wants filenames in.Space to hold the result
** is obtained from HeapAlloc() and must be freed by the calling
** function.
** Taken from the sqlite3 source tree
** status: Public Domain
*/
static void *convertUtf8Filename(const char *zFilename) {
	void *zConverted;
	zConverted = utf8ToUnicode(zFilename);
	return zConverted;
}
/*
** Convert microsoft unicode to UTF-8.  Space to hold the returned string is
** obtained from HeapAlloc().
** Taken from the sqlite3 source tree
** status: Public Domain
*/
static char *unicodeToUtf8(const WCHAR *zWideFilename) {
	char *zFilename;
	int nByte;

	nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, 0, 0, 0, 0);
	zFilename = (char *)HeapAlloc(GetProcessHeap(), 0, nByte);
	if (zFilename == 0) {
		return 0;
	}
	nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, zFilename, nByte, 0, 0);
	if (nByte == 0) {
		HeapFree(GetProcessHeap(), 0, zFilename);
		return 0;
	}
	return zFilename;
}
/* int (*xchdir)(const char *) */
static int WinVfs_chdir(const char *zPath)
{
	void * pConverted;
	BOOL rc;
	pConverted = convertUtf8Filename(zPath);
	if (pConverted == 0) {
		return -1;
	}
	rc = SetCurrentDirectoryW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(), 0, pConverted);
	return rc ? SOD_OK : -1;
}
/* int (*xGetcwd)(SyBlob *) */
static int WinVfs_getcwd(SyBlob *pCtx)
{
	WCHAR zDir[2048];
	char *zConverted;
	DWORD rc;
	/* Get the current directory */
	rc = GetCurrentDirectoryW(sizeof(zDir), zDir);
	if (rc < 1) {
		return -1;
	}
	zConverted = unicodeToUtf8(zDir);
	if (zConverted == 0) {
		return -1;
	}
	libcox_result_string(pCtx, zConverted, -1/*Compute length automatically*/); /* Will make it's own copy */
	HeapFree(GetProcessHeap(), 0, zConverted);
	return SOD_OK;
}
/* int (*xMkdir)(const char *, int, int) */
static int WinVfs_mkdir(const char *zPath, int mode, int recursive)
{
	void * pConverted;
	BOOL rc;
	pConverted = convertUtf8Filename(zPath);
	if (pConverted == 0) {
		return -1;
	}
	mode = 0; /* MSVC warning */
	recursive = 0;
	rc = CreateDirectoryW((LPCWSTR)pConverted, 0);
	HeapFree(GetProcessHeap(), 0, pConverted);
	return rc ? SOD_OK : -1;
}
/* int (*xRmdir)(const char *) */
static int WinVfs_rmdir(const char *zPath)
{
	void * pConverted;
	BOOL rc;
	pConverted = convertUtf8Filename(zPath);
	if (pConverted == 0) {
		return -1;
	}
	rc = RemoveDirectoryW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(), 0, pConverted);
	return rc ? SOD_OK : -1;
}
/* int (*xIsdir)(const char *) */
static int WinVfs_isdir(const char *zPath)
{
	void * pConverted;
	DWORD dwAttr;
	pConverted = convertUtf8Filename(zPath);
	if (pConverted == 0) {
		return -1;
	}
	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(), 0, pConverted);
	if (dwAttr == INVALID_FILE_ATTRIBUTES) {
		return -1;
	}
	return (dwAttr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
}
/* int (*xRename)(const char *, const char *) */
static int WinVfs_Rename(const char *zOld, const char *zNew)
{
	void *pOld, *pNew;
	BOOL rc = 0;
	pOld = convertUtf8Filename(zOld);
	if (pOld == 0) {
		return -1;
	}
	pNew = convertUtf8Filename(zNew);
	if (pNew) {
		rc = MoveFileW((LPCWSTR)pOld, (LPCWSTR)pNew);
	}
	HeapFree(GetProcessHeap(), 0, pOld);
	if (pNew) {
		HeapFree(GetProcessHeap(), 0, pNew);
	}
	return rc ? SOD_OK : -1;
}
/* int (*xRealpath)(const char *, SyBlob *) */
static int WinVfs_Realpath(const char *zPath, SyBlob *pCtx)
{
	WCHAR zTemp[2048];
	void *pPath;
	char *zReal;
	DWORD n;
	pPath = convertUtf8Filename(zPath);
	if (pPath == 0) {
		return -1;
	}
	n = GetFullPathNameW((LPCWSTR)pPath, 0, 0, 0);
	if (n > 0) {
		if (n >= sizeof(zTemp)) {
			n = sizeof(zTemp) - 1;
		}
		GetFullPathNameW((LPCWSTR)pPath, n, zTemp, 0);
	}
	HeapFree(GetProcessHeap(), 0, pPath);
	if (!n) {
		return -1;
	}
	zReal = unicodeToUtf8(zTemp);
	if (zReal == 0) {
		return -1;
	}
	libcox_result_string(pCtx, zReal, -1); /* Will make it's own copy */
	HeapFree(GetProcessHeap(), 0, zReal);
	return SOD_OK;
}
/* int (*xUnlink)(const char *) */
static int WinVfs_unlink(const char *zPath)
{
	void * pConverted;
	BOOL rc;
	pConverted = convertUtf8Filename(zPath);
	if (pConverted == 0) {
		return -1;
	}
	rc = DeleteFileW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(), 0, pConverted);
	return rc ? SOD_OK : -1;
}
/* int64_t (*xFreeSpace)(const char *) */
static int64_t WinVfs_DiskFreeSpace(const char *zPath)
{
	DWORD dwSectPerClust, dwBytesPerSect, dwFreeClusters, dwTotalClusters;
	void * pConverted;
	WCHAR *p;
	BOOL rc;
	pConverted = convertUtf8Filename(zPath);
	if (pConverted == 0) {
		return 0;
	}
	p = (WCHAR *)pConverted;
	for (; *p; p++) {
		if (*p == '\\' || *p == '/') {
			*p = '\0';
			break;
		}
	}
	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted, &dwSectPerClust, &dwBytesPerSect, &dwFreeClusters, &dwTotalClusters);
	if (!rc) {
		return 0;
	}
	return (int64_t)dwFreeClusters * dwSectPerClust * dwBytesPerSect;
}
/* int64_t (*xTotalSpace)(const char *) */
static int64_t WinVfs_DiskTotalSpace(const char *zPath)
{
	DWORD dwSectPerClust, dwBytesPerSect, dwFreeClusters, dwTotalClusters;
	void * pConverted;
	WCHAR *p;
	BOOL rc;
	pConverted = convertUtf8Filename(zPath);
	if (pConverted == 0) {
		return 0;
	}
	p = (WCHAR *)pConverted;
	for (; *p; p++) {
		if (*p == '\\' || *p == '/') {
			*p = '\0';
			break;
		}
	}
	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted, &dwSectPerClust, &dwBytesPerSect, &dwFreeClusters, &dwTotalClusters);
	if (!rc) {
		return 0;
	}
	return (int64_t)dwTotalClusters * dwSectPerClust * dwBytesPerSect;
}
/* int (*xFileExists)(const char *) */
static int WinVfs_FileExists(const char *zPath)
{
	void * pConverted;
	DWORD dwAttr;
	pConverted = convertUtf8Filename(zPath);
	if (pConverted == 0) {
		return -1;
	}
	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(), 0, pConverted);
	if (dwAttr == INVALID_FILE_ATTRIBUTES) {
		return -1;
	}
	return SOD_OK;
}
/* Open a file in a read-only mode */
static HANDLE OpenReadOnly(LPCWSTR pPath)
{
	DWORD dwType = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS;
	DWORD dwShare = FILE_SHARE_READ | FILE_SHARE_WRITE;
	DWORD dwAccess = GENERIC_READ;
	DWORD dwCreate = OPEN_EXISTING;
	HANDLE pHandle;
	pHandle = CreateFileW(pPath, dwAccess, dwShare, 0, dwCreate, dwType, 0);
	if (pHandle == INVALID_HANDLE_VALUE) {
		return 0;
	}
	return pHandle;
}
/* int64_t (*xFileSize)(const char *) */
static int64_t WinVfs_FileSize(const char *zPath)
{
	DWORD dwLow, dwHigh;
	void * pConverted;
	int64_t nSize;
	HANDLE pHandle;

	pConverted = convertUtf8Filename(zPath);
	if (pConverted == 0) {
		return -1;
	}
	/* Open the file in read-only mode */
	pHandle = OpenReadOnly((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(), 0, pConverted);
	if (pHandle) {
		dwLow = GetFileSize(pHandle, &dwHigh);
		nSize = dwHigh;
		nSize <<= 32;
		nSize += dwLow;
		CloseHandle(pHandle);
	}
	else {
		nSize = -1;
	}
	return nSize;
}
/* int (*xIsfile)(const char *) */
static int WinVfs_isfile(const char *zPath)
{
	void * pConverted;
	DWORD dwAttr;
	pConverted = convertUtf8Filename(zPath);
	if (pConverted == 0) {
		return -1;
	}
	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(), 0, pConverted);
	if (dwAttr == INVALID_FILE_ATTRIBUTES) {
		return -1;
	}
	return (dwAttr & (FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE)) ? SOD_OK : -1;
}
/* int (*xWritable)(const char *) */
static int WinVfs_iswritable(const char *zPath)
{
	void * pConverted;
	DWORD dwAttr;
	pConverted = convertUtf8Filename(zPath);
	if (pConverted == 0) {
		return -1;
	}
	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(), 0, pConverted);
	if (dwAttr == INVALID_FILE_ATTRIBUTES) {
		return -1;
	}
	if ((dwAttr & (FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_NORMAL)) == 0) {
		/* Not a regular file */
		return -1;
	}
	if (dwAttr & FILE_ATTRIBUTE_READONLY) {
		/* Read-only file */
		return -1;
	}
	/* File is writable */
	return SOD_OK;
}
/* int (*xExecutable)(const char *) */
static int WinVfs_isexecutable(const char *zPath)
{
	void * pConverted;
	DWORD dwAttr;
	pConverted = convertUtf8Filename(zPath);
	if (pConverted == 0) {
		return -1;
	}
	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(), 0, pConverted);
	if (dwAttr == INVALID_FILE_ATTRIBUTES) {
		return -1;
	}
	if ((dwAttr & FILE_ATTRIBUTE_NORMAL) == 0) {
		/* Not a regular file */
		return -1;
	}
	/* FIXEME: GetBinaryType or another call to make sure this thing is executable  */

	/* File is executable */
	return SOD_OK;
}
/* int (*xGetenv)(const char *, SyBlob *) */
static int WinVfs_Getenv(const char *zVar, SyBlob *pCtx)
{
	char zValue[1024];
	DWORD n;
	/*
	* According to MSDN
	* If lpBuffer is not large enough to hold the data, the return
	* value is the buffer size, in characters, required to hold the
	* string and its terminating null character and the contents
	* of lpBuffer are undefined.
	*/
	n = sizeof(zValue);
	/* Extract the environment value */
	n = GetEnvironmentVariableA(zVar, zValue, sizeof(zValue));
	if (!n) {
		/* No such variable*/
		return -1;
	}
	libcox_result_string(pCtx, zValue, (int)n);
	return SOD_OK;
}
/* int (*xSetenv)(const char *, const char *) */
static int WinVfs_Setenv(const char *zName, const char *zValue)
{
	BOOL rc;
	rc = SetEnvironmentVariableA(zName, zValue);
	return rc ? SOD_OK : -1;
}
/* int (*xMmap)(const char *, void **, size_t *) */
static int WinVfs_Mmap(const char *zPath, void **ppMap, size_t *pSize)
{
	DWORD dwSizeLow, dwSizeHigh;
	HANDLE pHandle, pMapHandle;
	void *pConverted, *pView;

	pConverted = convertUtf8Filename(zPath);
	if (pConverted == 0) {
		return -1;
	}
	pHandle = OpenReadOnly((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(), 0, pConverted);
	if (pHandle == 0) {
		return -1;
	}
	/* Get the file size */
	dwSizeLow = GetFileSize(pHandle, &dwSizeHigh);
	/* Create the mapping */
	pMapHandle = CreateFileMappingW(pHandle, 0, PAGE_READONLY, dwSizeHigh, dwSizeLow, 0);
	if (pMapHandle == 0) {
		CloseHandle(pHandle);
		return -1;
	}
	*pSize = ((int64_t)dwSizeHigh << 32) | dwSizeLow;
	/* Obtain the view */
	pView = MapViewOfFile(pMapHandle, FILE_MAP_READ, 0, 0, (SIZE_T)(*pSize));
	if (pView) {
		/* Let the upper layer point to the view */
		*ppMap = pView;
	}
	/* Close the handle
	* According to MSDN it's OK the close the HANDLES.
	*/
	CloseHandle(pMapHandle);
	CloseHandle(pHandle);
	return pView ? SOD_OK : -1;
}
/* void (*xUnmap)(void *, size_t)  */
static void WinVfs_Unmap(void *pView, size_t nSize)
{
	nSize = 0; /* Compiler warning */
	UnmapViewOfFile(pView);
}
/* void (*xTempDir)(SyBlob *) */
static void WinVfs_TempDir(SyBlob *pCtx)
{
	CHAR zTemp[1024];
	DWORD n;
	n = GetTempPathA(sizeof(zTemp), zTemp);
	if (n < 1) {
		/* Assume the default windows temp directory */
		libcox_result_string(pCtx, "C:\\Windows\\Temp", -1/*Compute length automatically*/);
	}
	else {
		libcox_result_string(pCtx, zTemp, (int)n);
	}
}
/* void (*GetTicks)() */
static float WinVfs_GetTicks()
{
	static double freq = -1.0;
	LARGE_INTEGER lint;

	if (freq < 0.0) {
		if (!QueryPerformanceFrequency(&lint))
			return -1.0f;
		freq = lint.QuadPart;
	}
	if (!QueryPerformanceCounter(&lint))
		return -1.0f;
	return (float)(lint.QuadPart / freq);
}
/* An instance of the following structure is used to record state information
* while iterating throw directory entries.
*/
typedef struct WinDir_Info WinDir_Info;
struct WinDir_Info
{
	HANDLE pDirHandle;
	void *pPath;
	WIN32_FIND_DATAW sInfo;
	int rc;
};
/* int (*xOpenDir)(const char *, void **) */
static int WinDir_Open(const char *zPath, void **ppHandle)
{
	WinDir_Info *pDirInfo;
	void *pConverted;
	char *zPrep;
	size_t n;
	/* Prepare the path */
	n = strlen(zPath);
	zPrep = (char *)HeapAlloc(GetProcessHeap(), 0, n + sizeof("\\*") + 4);
	if (zPrep == 0) {
		return -1;
	}
	memcpy(zPrep, (const void *)zPath, n);
	zPrep[n] = '\\';
	zPrep[n + 1] = '*';
	zPrep[n + 2] = 0;
	pConverted = convertUtf8Filename(zPrep);
	HeapFree(GetProcessHeap(), 0, zPrep);
	if (pConverted == 0) {
		return -1;
	}
	/* Allocate a new instance */
	pDirInfo = (WinDir_Info *)HeapAlloc(GetProcessHeap(), 0, sizeof(WinDir_Info));
	if (pDirInfo == 0) {
		return -1;
	}
	pDirInfo->rc = SOD_OK;
	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pConverted, &pDirInfo->sInfo);
	if (pDirInfo->pDirHandle == INVALID_HANDLE_VALUE) {
		/* Cannot open directory */
		HeapFree(GetProcessHeap(), 0, pConverted);
		HeapFree(GetProcessHeap(), 0, pDirInfo);
		return -1;
	}
	/* Save the path */
	pDirInfo->pPath = pConverted;
	/* Save our structure */
	*ppHandle = pDirInfo;
	return SOD_OK;
}
/* void (*xCloseDir)(void *) */
static void WinDir_Close(void *pUserData)
{
	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;
	if (pDirInfo->pDirHandle != INVALID_HANDLE_VALUE) {
		FindClose(pDirInfo->pDirHandle);
	}
	HeapFree(GetProcessHeap(), 0, pDirInfo->pPath);
	HeapFree(GetProcessHeap(), 0, pDirInfo);
}
/* int (*xDirRead)(void *, SyBlob *) */
static int WinDir_Read(void *pUserData, SyBlob *pVal)
{
	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;
	LPWIN32_FIND_DATAW pData;
	char *zName;
	BOOL rc;
	size_t n;
	if (pDirInfo->rc != SOD_OK) {
		/* No more entry to process */
		return -1;
	}
	pData = &pDirInfo->sInfo;
	for (;;) {
		zName = unicodeToUtf8(pData->cFileName);
		if (zName == 0) {
			/* Out of memory */
			return -1;
		}
		n = strlen(zName);
		/* Ignore '.' && '..' */
		if (n > sizeof("..") - 1 || zName[0] != '.' || (n == sizeof("..") - 1 && zName[1] != '.')) {
			break;
		}
		HeapFree(GetProcessHeap(), 0, zName);
		rc = FindNextFileW(pDirInfo->pDirHandle, &pDirInfo->sInfo);
		if (!rc) {
			return -1;
		}
	}
	/* Return the current file name */
	libcox_result_string(pVal, zName, -1);
	HeapFree(GetProcessHeap(), 0, zName);
	/* Point to the next entry */
	rc = FindNextFileW(pDirInfo->pDirHandle, &pDirInfo->sInfo);
	if (!rc) {
		pDirInfo->rc = -1;
	}
	return SOD_OK;
}
/* void (*xRewindDir)(void *) */
static void WinDir_Rewind(void *pUserData)
{
	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;
	FindClose(pDirInfo->pDirHandle);
	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pDirInfo->pPath, &pDirInfo->sInfo);
	if (pDirInfo->pDirHandle == INVALID_HANDLE_VALUE) {
		pDirInfo->rc = -1;
	}
	else {
		pDirInfo->rc = SOD_OK;
	}
}
#ifndef MAX_PATH
#define MAX_PATH 260
#endif /* MAX_PATH */
/* Export the windows vfs */
static const sod_vfs sWinVfs = {
	"Windows_vfs",
	LIBCOX_VFS_VERSION,
	0,
	MAX_PATH,
	WinVfs_chdir,    /* int (*xChdir)(const char *) */
	WinVfs_getcwd,   /* int (*xGetcwd)(SyBlob *) */
	WinVfs_mkdir,    /* int (*xMkdir)(const char *, int, int) */
	WinVfs_rmdir,    /* int (*xRmdir)(const char *) */
	WinVfs_isdir,    /* int (*xIsdir)(const char *) */
	WinVfs_Rename,   /* int (*xRename)(const char *, const char *) */
	WinVfs_Realpath, /*int (*xRealpath)(const char *, SyBlob *)*/
   /* Directory */
	WinDir_Open,
	WinDir_Close,
	WinDir_Read,
	WinDir_Rewind,
	/* Systems function */
	WinVfs_unlink, /* int (*xUnlink)(const char *) */
	WinVfs_FileExists, /* int (*xFileExists)(const char *) */
	WinVfs_DiskFreeSpace, /* int64_t (*xFreeSpace)(const char *) */
	WinVfs_DiskTotalSpace, /* int64_t (*xTotalSpace)(const char *) */
	WinVfs_FileSize, /* int64_t (*xFileSize)(const char *) */
	WinVfs_isfile,     /* int (*xIsfile)(const char *) */
	WinVfs_isfile,     /* int (*xReadable)(const char *) */
	WinVfs_iswritable, /* int (*xWritable)(const char *) */
	WinVfs_isexecutable, /* int (*xExecutable)(const char *) */
	WinVfs_Getenv,     /* int (*xGetenv)(const char *, SyBlob *) */
	WinVfs_Setenv,     /* int (*xSetenv)(const char *, const char *) */
	WinVfs_Mmap,       /* int (*xMmap)(const char *, void **, size_t *) */
	WinVfs_Unmap,      /* void (*xUnmap)(void *, size_t);  */
	WinVfs_TempDir,    /* void (*xTempDir)(SyBlob *) */
	WinVfs_GetTicks  /* float (*xTicks)(void) */
};

#elif defined(__UNIXES__)
/*
* UNIX VFS implementation for the LIBCOX engine.
* Authors:
*    Symisc Systems, devel@symisc.net.
*    Copyright (C) Symisc Systems, http://libcox.symisc.net
* Status:
*    Stable.
*/
#include <sys/types.h>
#include <limits.h>
#ifdef SOD_ENABLE_NET_TRAIN
#include <sys/time.h>
#else
#include <time.h>
#endif /* SOD_ENABLE_NET_TRAIN */
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <dirent.h>
#include <utime.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif /* PATH_MAX */

/* int (*xchdir)(const char *) */
static int UnixVfs_chdir(const char *zPath)
{
	int rc;
	rc = chdir(zPath);
	return rc == 0 ? SOD_OK : -1;
}
/* int (*xGetcwd)(SyBlob *) */
static int UnixVfs_getcwd(SyBlob *pCtx)
{
	char zBuf[4096];
	char *zDir;
	/* Get the current directory */
	zDir = getcwd(zBuf, sizeof(zBuf));
	if (zDir == 0) {
		return -1;
	}
	libcox_result_string(pCtx, zDir, -1/*Compute length automatically*/);
	return SOD_OK;
}
/* int (*xMkdir)(const char *, int, int) */
static int UnixVfs_mkdir(const char *zPath, int mode, int recursive)
{
	int rc;
	rc = mkdir(zPath, mode);
	recursive = 0; /* cc warning */
	return rc == 0 ? SOD_OK : -1;
}
/* int (*xIsdir)(const char *) */
static int UnixVfs_isdir(const char *zPath)
{
	struct stat st;
	int rc;
	rc = stat(zPath, &st);
	if (rc != 0) {
		return -1;
	}
	rc = S_ISDIR(st.st_mode);
	return rc ? 1 : 0;
}
/* int (*xUnlink)(const char *) */
static int UnixVfs_unlink(const char *zPath)
{
	int rc;
	rc = unlink(zPath);
	return rc == 0 ? SOD_OK : -1;
}
/* int (*xFileExists)(const char *) */
static int UnixVfs_FileExists(const char *zPath)
{
	int rc;
	rc = access(zPath, F_OK);
	return rc == 0 ? SOD_OK : -1;
}
/* int64_t (*xFileSize)(const char *) */
static int64_t UnixVfs_FileSize(const char *zPath)
{
	struct stat st;
	int rc;
	rc = stat(zPath, &st);
	if (rc != 0) {
		return -1;
	}
	return (int64_t)st.st_size;
}
/* int (*xIsfile)(const char *) */
static int UnixVfs_isfile(const char *zPath)
{
	struct stat st;
	int rc;
	rc = stat(zPath, &st);
	if (rc != 0) {
		return -1;
	}
	rc = S_ISREG(st.st_mode);
	return rc ? SOD_OK : -1;
}
/* int (*xReadable)(const char *) */
static int UnixVfs_isreadable(const char *zPath)
{
	int rc;
	rc = access(zPath, R_OK);
	return rc == 0 ? SOD_OK : -1;
}
/* int (*xWritable)(const char *) */
static int UnixVfs_iswritable(const char *zPath)
{
	int rc;
	rc = access(zPath, W_OK);
	return rc == 0 ? SOD_OK : -1;
}
/* int (*xExecutable)(const char *) */
static int UnixVfs_isexecutable(const char *zPath)
{
	int rc;
	rc = access(zPath, X_OK);
	return rc == 0 ? SOD_OK : -1;
}
/* int (*xMmap)(const char *, void **, size_t *) */
static int UnixVfs_Mmap(const char *zPath, void **ppMap, size_t *pSize)
{
	struct stat st;
	void *pMap;
	int fd;
	int rc;
	/* Open the file in a read-only mode */
	fd = open(zPath, O_RDONLY);
	if (fd < 0) {
		return -1;
	}
	/* stat the handle */
	fstat(fd, &st);
	/* Obtain a memory view of the whole file */
	pMap = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	rc = SOD_OK;
	if (pMap == MAP_FAILED) {
		rc = -1;
	}
	else {
		/* Point to the memory view */
		*ppMap = pMap;
		*pSize = st.st_size;
	}
	close(fd);
	return rc;
}
/* void (*xUnmap)(void *, size_t)  */
static void UnixVfs_Unmap(void *pView, size_t nSize)
{
	munmap(pView, nSize);
}
/* int (*xOpenDir)(const char *, void **) */
static int UnixDir_Open(const char *zPath, void **ppHandle)
{
	DIR *pDir;
	/* Open the target directory */
	pDir = opendir(zPath);
	if (pDir == 0) {
		return -1;
	}
	/* Save our structure */
	*ppHandle = pDir;
	return SOD_OK;
}
/* void (*xCloseDir)(void *) */
static void UnixDir_Close(void *pUserData)
{
	closedir((DIR *)pUserData);
}
/* (*xReadDir)(void *, SyBlob *) */
static int UnixDir_Read(void *pUserData, SyBlob *pVal)
{
	DIR *pDir = (DIR *)pUserData;
	struct dirent *pEntry;
	char *zName = 0; /* cc warning */
	uint32_t n = 0;
	for (;;) {
		pEntry = readdir(pDir);
		if (pEntry == 0) {
			/* No more entries to process */
			return -1;
		}
		zName = pEntry->d_name;
		n = strlen(zName);
		/* Ignore '.' && '..' */
		if (n > sizeof("..") - 1 || zName[0] != '.' || (n == sizeof("..") - 1 && zName[1] != '.')) {
			break;
		}
		/* Next entry */
	}
	/* Return the current file name */
	libcox_result_string(pVal, zName, (int)n);
	return SOD_OK;
}
/* void (*xRewindDir)(void *) */
static void UnixDir_Rewind(void *pUserData)
{
	rewinddir((DIR *)pUserData);
}
/* float xTicks() */
static float UnixVfs_get_ticks()
{
	/* This is merely used for reporting training time between
	* epochs so we really don't care about precision here.
	*/
#ifdef SOD_ENABLE_NET_TRAIN
	struct timeval time;
	gettimeofday(&time, 0);
	return (float)time.tv_sec + 1000*time.tv_usec;
#else
	return (float)time(0);
#endif
}
/* Export the UNIX vfs */
static const sod_vfs sUnixVfs = {
	"Unix_vfs",
	LIBCOX_VFS_VERSION,
	0,
	PATH_MAX,
	UnixVfs_chdir,    /* int (*xChdir)(const char *) */
	UnixVfs_getcwd,   /* int (*xGetcwd)(SyBlob *) */
	UnixVfs_mkdir,    /* int (*xMkdir)(const char *, int, int) */
	0,    /* int (*xRmdir)(const char *) */
	UnixVfs_isdir,    /* int (*xIsdir)(const char *) */
	0,   /* int (*xRename)(const char *, const char *) */
	0, /*int (*xRealpath)(const char *, SyBlob *)*/
	/* Directory */
	UnixDir_Open,
	UnixDir_Close,
	UnixDir_Read,
	UnixDir_Rewind,
   /* Systems */
    UnixVfs_unlink,   /* int (*xUnlink)(const char *) */
    UnixVfs_FileExists, /* int (*xFileExists)(const char *) */
	0,             /* int64_t (*xFreeSpace)(const char *) */
	0,             /* int64_t (*xTotalSpace)(const char *) */
	UnixVfs_FileSize, /* int64_t (*xFileSize)(const char *) */
	UnixVfs_isfile,     /* int (*xIsfile)(const char *) */
	UnixVfs_isreadable, /* int (*xReadable)(const char *) */
	UnixVfs_iswritable, /* int (*xWritable)(const char *) */
	UnixVfs_isexecutable, /* int (*xExecutable)(const char *) */
	0,     /* int (*xGetenv)(const char *, SyBlob *) */
	0,     /* int (*xSetenv)(const char *, const char *) */
	UnixVfs_Mmap,       /* int (*xMmap)(const char *, void **, int64_t *) */
	UnixVfs_Unmap,      /* void (*xUnmap)(void *, int64_t);  */
	0,    /* void (*xTempDir)(SyBlob *) */
	UnixVfs_get_ticks   /* float (*xTicks)() */
};
#endif /* __WINNT__/__UNIXES__ */
/*
* Export the builtin vfs.
* Return a pointer to the builtin vfs if available.
* Otherwise return the null_vfs [i.e: a no-op vfs] instead.
* Note:
*  The built-in vfs is always available for Windows/UNIX systems.
* Note:
*  If the engine is compiled with the LIBCOX_DISABLE_DISK_IO/LIBCOX_DISABLE_BUILTIN_FUNC
*  directives defined then this function return the null_vfs instead.
*/
static const sod_vfs * sodExportBuiltinVfs(void)
{
#ifdef __WINNT__
	return &sWinVfs;
#else
	/* Assume UNIX-Like */
	return &sUnixVfs;
#endif /* __WINNT__/__UNIXES__ */
}
/* @VFS */
#ifdef TWO_PI
#undef TWO_PI
#endif /* Important */
/* From http://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform */
#define TWO_PI 6.2831853071795864769252866
static float rand_normal()
{
	static int haveSpare = 0;
	static double rand1, rand2;

	if (haveSpare)
	{
		haveSpare = 0;
		return sqrt(rand1) * sin(rand2);
	}

	haveSpare = 1;

	rand1 = rand() / ((double)RAND_MAX);
	if (rand1 < 1e-100) rand1 = 1e-100;
	rand1 = -2 * log(rand1);
	rand2 = (rand() / ((double)RAND_MAX)) * TWO_PI;

	return sqrt(rand1) * cos(rand2);
}
static float rand_uniform(float min, float max)
{
	if (max < min) {
		float swap = min;
		min = max;
		max = swap;
	}
	return ((float)rand() / RAND_MAX * (max - min)) + min;
}
static int constrain_int(int a, int min, int max)
{
	if (a < min) return min;
	if (a > max) return max;
	return a;
}
static int rand_int(int min, int max)
{
	if (max < min) {
		int s = min;
		min = max;
		max = s;
	}
	int r = (rand() % (max - min + 1)) + min;
	return r;
}
static float rand_scale(float s)
{
	float scale = rand_uniform(1, s);
	if (rand() % 2) return scale;
	return 1. / scale;
}
static inline void sod_md_alloc_dyn_img(sod_img *pFrame, int w, int h, int c)
{
	if (pFrame->data == 0 || pFrame->w < w || pFrame->h < h || pFrame->c != c) {
		pFrame->w = w;
		pFrame->h = h;
		pFrame->c = c;
		pFrame->data = realloc(pFrame->data, w * h * c * sizeof(float));
	}
}
#ifndef SOD_DISABLE_CNN
/*
 * List of implemented layers (excluding LSTM).
 */
typedef enum {
	CONVOLUTIONAL,
	DECONVOLUTIONAL,
	CONNECTED,
	MAXPOOL,
	SOFTMAX,
	DETECTION,
	DROPOUT,
	CROP,
	ROUTE,
	COST,
	NORMALIZATION,
	AVGPOOL,
	LOCAL,
	SHORTCUT,
	ACTIVE,
	RNN,
	GRU,
	CRNN,
	BATCHNORM,
	NETWORK,
	XNOR,
	REGION,
	REORG,
	LSTM,
	BLANK
} SOD_CNN_LAYER_TYPE;
/* Forward declaration */
typedef struct tree tree;
typedef struct network network;
typedef struct network_state network_state;
typedef struct layer layer;
typedef enum {
	CONSTANT, STEP, EXP, POLY, STEPS, SIG, RANDOM
} learning_rate_policy;
typedef enum {
	LOGISTIC, RELU, RELIE, LINEAR, RAMP, TANH, PLSE, LEAKY, ELU, LOGGY, STAIR, HARDTAN, LHTAN
}ACTIVATION;
typedef enum {
	SSE, MASKED, SMOOTH
} COST_TYPE;
struct tree {
	int *leaf;
	int n;
	int *parent;
	int *child;
	int *group;
	char **name;

	int groups;
	int *group_size;
	int *group_offset;
};
struct network {
	sod_cnn *pNet;
	float *workspace;
	int n;
	int batch;
	uint64_t *seen;
	float epoch;
	int subdivisions;
	float momentum;
	float decay;
	layer *layers;
	int outputs;
	float *output;
	learning_rate_policy policy;
	size_t workspace_size;
	float learning_rate;
	float gamma;
	float scale;
	float power;
	int time_steps;
	int step;
	int max_batches;
	float *scales;
	int   *steps;
	int num_steps;
	int burn_in;

	int adam;
	float B1;
	float B2;
	float eps;

	int inputs;
	int h, w, c;
	int max_crop;
	int min_crop;
	float angle;
	float aspect;
	float exposure;
	float saturation;
	float hue;

	int gpu_index;
	tree *hierarchy;

#if 0 /* SOD_GPU */
	float **input_gpu;
	float **truth_gpu;
#endif
};
struct network_state {
	float *truth;
	float *input;
	float *delta;
	float *workspace;
	int train;
	int index;
	network *net;
};
struct layer {
	SOD_CNN_LAYER_TYPE type;
	ACTIVATION activation;
	COST_TYPE cost_type;
	void(*forward)   (struct layer, struct network_state);
	void(*backward)  (struct layer, struct network_state);
	void(*update)    (struct layer, int, float, float, float);
	void(*forward_gpu)   (struct layer, struct network_state);
	void(*backward_gpu)  (struct layer, struct network_state);
	void(*update_gpu)    (struct layer, int, float, float, float);
	int batch_normalize;
	int shortcut;
	int batch;
	int forced;
	int flipped;
	int inputs;
	int outputs;
	int truths;
	int h, w, c;
	int out_h, out_w, out_c;
	int n;
	int max_boxes;
	int groups;
	int size;
	int side;
	int stride;
	int reverse;
	int pad;
	int sqrt;
	int flip;
	int index;
	int binary;
	int xnor;
	int steps;
	int hidden;
	float dot;
	float angle;
	float jitter;
	float saturation;
	float exposure;
	float shift;
	float ratio;
	int softmax;
	int classes;
	int coords;
	int background;
	int rescore;
	int objectness;
	int does_cost;
	int joint;
	int noadjust;
	int reorg;
	int log;

	int adam;
	float B1;
	float B2;
	float eps;
	int t;

	float alpha;
	float beta;
	float kappa;

	float coord_scale;
	float object_scale;
	float noobject_scale;
	float mask_scale;
	float class_scale;
	int bias_match;
	int random;
	float thresh;
	int classfix;
	int absolute;

	int dontload;
	int dontloadscales;

	float temperature;
	float probability;
	float scale;

	char  * cweights;
	int   * indexes;
	int   * input_layers;
	int   * input_sizes;
	int   * map;
	float * rand;
	float * cost;
	float * state;
	float * prev_state;
	float * forgot_state;
	float * forgot_delta;
	float * state_delta;

	float * concat;
	float * concat_delta;

	float * binary_weights;

	float * biases;
	float * bias_updates;

	float * scales;
	float * scale_updates;

	float * weights;
	float * weight_updates;

	float * col_image;
	float * delta;
	float * output;
	float * squared;
	float * norms;

	float * spatial_mean;
	float * mean;
	float * variance;

	float * mean_delta;
	float * variance_delta;

	float * rolling_mean;
	float * rolling_variance;

	float * x;
	float * x_norm;

	float * m;
	float * v;

	float * z_cpu;
	float * r_cpu;
	float * h_cpu;

	float * binary_input;

	struct layer *input_layer;
	struct layer *self_layer;
	struct layer *output_layer;

	struct layer *input_gate_layer;
	struct layer *state_gate_layer;
	struct layer *input_save_layer;
	struct layer *state_save_layer;
	struct layer *input_state_layer;
	struct layer *state_state_layer;

	struct layer *input_z_layer;
	struct layer *state_z_layer;

	struct layer *input_r_layer;
	struct layer *state_r_layer;

	struct layer *input_h_layer;
	struct layer *state_h_layer;

	tree *softmax_tree;

	size_t workspace_size;

#if 0 /* SOD_GPU */
	int *indexes_gpu;

	float *z_gpu;
	float *r_gpu;
	float *h_gpu;

	float *m_gpu;
	float *v_gpu;

	float * prev_state_gpu;
	float * forgot_state_gpu;
	float * forgot_delta_gpu;
	float * state_gpu;
	float * state_delta_gpu;
	float * gate_gpu;
	float * gate_delta_gpu;
	float * save_gpu;
	float * save_delta_gpu;
	float * concat_gpu;
	float * concat_delta_gpu;

	float *binary_input_gpu;
	float *binary_weights_gpu;

	float * mean_gpu;
	float * variance_gpu;

	float * rolling_mean_gpu;
	float * rolling_variance_gpu;

	float * variance_delta_gpu;
	float * mean_delta_gpu;

	float * col_image_gpu;

	float * x_gpu;
	float * x_norm_gpu;
	float * weights_gpu;
	float * weight_updates_gpu;

	float * biases_gpu;
	float * bias_updates_gpu;

	float * scales_gpu;
	float * scale_updates_gpu;

	float * output_gpu;
	float * delta_gpu;
	float * rand_gpu;
	float * squared_gpu;
	float * norms_gpu;
#ifdef CUDNN
	cudnnTensorDescriptor_t srcTensorDesc, dstTensorDesc;
	cudnnTensorDescriptor_t dsrcTensorDesc, ddstTensorDesc;
	cudnnFilterDescriptor_t weightDesc;
	cudnnFilterDescriptor_t dweightDesc;
	cudnnConvolutionDescriptor_t convDesc;
	cudnnConvolutionFwdAlgo_t fw_algo;
	cudnnConvolutionBwdDataAlgo_t bd_algo;
	cudnnConvolutionBwdFilterAlgo_t bf_algo;
#endif
#endif
};
typedef struct {
	float x, y, w, h;
} box;
#define SOD_NET_ALLOCATED 1 /* Memory allocated for th network */
#define SOD_NET_STATE_READY 7 /* Network ready for prediction */
/* Processing Flags */
#define SOD_LAYER_RNN    0x001  /* RNN Layer detected */
#define SOD_RNN_MAP_FILE 0x002  /* Mapped RNN training file */
struct sod_cnn {
	const sod_vfs *pVfs;
	SySet aBoxes;
	sod_img sRz;
	sod_img sPart;
	sod_img sTmpim;
	float nms;
	float temp;
	float hier_thresh;
	float thresh;
	int nInput;
	float *aInput;
	int flags;
	const char *zRnnSeed;
	int rnn_gen_len;
	int c_rnn;
	int ow;
	int oh;
	network net; /* The network  */
	layer det;  /* Detection layer */
	box *boxes;
	float **probs;
	float *pOut;      /* Prediction output */
	int nOuput;       /* pOut[] length */
	const char *zErr; /* Error message if any */
	uint32_t nErr;    /* Parse error count */
	int state;        /* Network state */
	const char **azNames; /**/
	SyBlob sRnnConsumer;
	SyBlob sLogConsumer;
	ProcRnnCallback xRnn; /* RNN callback */
	void *pRnnData;
	ProcLogCallback xLog; /* Log callback */
	void *pLogData;
};
/*
* CNN Built-in Configurations.
*/
static const char zYolo[] = {
	"[net]\n"
	"batch=1\n"
	"subdivisions=1\n"
	"width=416\n"
	"height=416\n"
	"channels=3\n"
	"momentum=0.9\n"
	"decay=0.0005\n"
	"angle=0\n"
	"saturation = 1.5\n"
	"exposure = 1.5\n"
	"hue=.1\n"
	"\n"
	"learning_rate=0.001\n"
	"max_batches = 120000\n"
	"policy=steps\n"
	"steps=-1,100,80000,100000\n"
	"scales=.1,10,.1,.1\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=32\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=64\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=128\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=64\n"
	"size=1\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=128\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=256\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=128\n"
	"size=1\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=256\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=512\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=256\n"
	"size=1\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=512\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=256\n"
	"size=1\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=512\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=1024\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=512\n"
	"size=1\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=1024\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=512\n"
	"size=1\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=1024\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"\n"
	"#######\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"filters=1024\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"filters=1024\n"
	"activation=leaky\n"
	"\n"
	"[route]\n"
	"layers=-9\n"
	"\n"
	"[reorg]\n"
	"stride=2\n"
	"\n"
	"[route]\n"
	"layers=-1,-3\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"filters=1024\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"size=1\n"
	"stride=1\n"
	"pad=1\n"
	"filters=425\n"
	"activation=linear\n"
	"\n"
	"[region]\n"
	"anchors = 0.738768,0.874946,  2.42204,2.65704,  4.30971,7.04493,  10.246,4.59428,  12.6868,11.8741\n"
	"bias_match=1\n"
	"classes=80\n"
	"coords=4\n"
	"num=5\n"
	"softmax=1\n"
	"jitter=.2\n"
	"rescore=1\n"
	"\n"
	"object_scale=5\n"
	"noobject_scale=1\n"
	"class_scale=1\n"
	"coord_scale=1\n"
	"\n"
	"absolute=1\n"
	"thresh = .6\n"
	"random=0\n"
};
static const char zTiny[] = {
	"[net]\n"
	"batch=64\n"
	"subdivisions=8\n"
	"width=416\n"
	"height=416\n"
	"channels=3\n"
	"momentum=0.9\n"
	"decay=0.0005\n"
	"angle=0\n"
	"saturation = 1.5\n"
	"exposure = 1.5\n"
	"hue=.1\n"
	"\n"
	"learning_rate=0.001\n"
	"max_batches = 120000\n"
	"policy=steps\n"
	"steps=-1,100,80000,100000\n"
	"scales=.1,10,.1,.1\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=16\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=32\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=64\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=128\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=256\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=512\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=1\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=1024\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"###########\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"filters=1024\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"size=1\n"
	"stride=1\n"
	"pad=1\n"
	"filters=425\n"
	"activation=linear\n"
	"\n"
	"[region]\n"
	"anchors = 0.738768,0.874946,  2.42204,2.65704,  4.30971,7.04493,  10.246,4.59428,  12.6868,11.8741\n"
	"bias_match=1\n"
	"classes=80\n"
	"coords=4\n"
	"num=5\n"
	"softmax=1\n"
	"jitter=.2\n"
	"rescore=1\n"
	"\n"
	"object_scale=5\n"
	"noobject_scale=1\n"
	"class_scale=1\n"
	"coord_scale=1\n"
	"\n"
	"absolute=1\n"
	"thresh = .6\n"
	"random=1\n"
};
static const char zfaceCnn[] = {
	"[net]\n"
	"batch=128\n"
	"subdivisions=8\n"
	"width=416\n"
	"height=416\n"
	"channels=3\n"
	"momentum=0.9\n"
	"decay=0.0005\n"
	"angle=0\n"
	"saturation = 1.5\n"
	"exposure = 1.5\n"
	"hue=.1\n"
	"#adam=1\n"
	"\n"
	"learning_rate=0.001\n"
	"max_batches = 50000\n"
	"policy=steps\n"
	"steps=-1,500,5000,10000\n"
	"scales=.1,10,.1,.1\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=8\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=16\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=32\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=64\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=32\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=64\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"#[maxpool]\n"
	"#size=2\n"
	"#stride=1\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=32\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"###########\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"filters=64\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"size=1\n"
	"stride=1\n"
	"pad=1\n"
	"filters=30\n"
	"activation=linear\n"
	"\n"
	"[region]\n"
	"anchors = 0.7,0.86,2.1,2.1,4.0,4.16,8.1,8.1,12.0,12.16\n"
	"bias_match=1\n"
	"classes=1\n"
	"coords=4\n"
	"num=5\n"
	"softmax=1\n"
	"jitter=.2\n"
	"rescore=1\n"
	"\n"
	"object_scale=5\n"
	"noobject_scale=1\n"
	"class_scale=1\n"
	"coord_scale=1\n"
	"\n"
	"absolute=1\n"
	"thresh = .6\n"
	"random=1\n"
	""
};
static const char zRnn[] = {
	"[net]\n"
	"subdivisions=1\n"
	"inputs=256\n"
	"batch = 1\n"
	"momentum=0.9\n"
	"decay=0.001\n"
	"max_batches = 2000\n"
	"time_steps=1\n"
	"learning_rate=0.1\n"
	"policy=steps\n"
	"steps=1000,1500\n"
	"scales=.1,.1\n"
	"\n"
	"[rnn]\n"
	"batch_normalize=1\n"
	"output = 1024\n"
	"hidden=1024\n"
	"activation=leaky\n"
	"\n"
	"[rnn]\n"
	"batch_normalize=1\n"
	"output = 1024\n"
	"hidden=1024\n"
	"activation=leaky\n"
	"\n"
	"[rnn]\n"
	"batch_normalize=1\n"
	"output = 1024\n"
	"hidden=1024\n"
	"activation=leaky\n"
	"\n"
	"[connected]\n"
	"output=256\n"
	"activation=leaky\n"
	"\n"
	"[softmax]\n"
	"\n"
	"[cost]\n"
	"type=sse\n"
	"\n"
};
static const char zTinyVoc[] = {
	"[net]\n"
	"batch=64\n"
	"subdivisions=8\n"
	"width=416\n"
	"height=416\n"
	"channels=3\n"
	"momentum=0.9\n"
	"decay=0.0005\n"
	"angle=0\n"
	"saturation = 1.5\n"
	"exposure = 1.5\n"
	"hue=.1\n"
	"\n"
	"learning_rate=0.001\n"
	"max_batches = 40200\n"
	"policy=steps\n"
	"steps=-1,100,20000,30000\n"
	"scales=.1,10,.1,.1\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=16\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=32\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=64\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=128\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=256\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=2\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=512\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"[maxpool]\n"
	"size=2\n"
	"stride=1\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"filters=1024\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"activation=leaky\n"
	"\n"
	"###########\n"
	"\n"
	"[convolutional]\n"
	"batch_normalize=1\n"
	"size=3\n"
	"stride=1\n"
	"pad=1\n"
	"filters=1024\n"
	"activation=leaky\n"
	"\n"
	"[convolutional]\n"
	"size=1\n"
	"stride=1\n"
	"pad=1\n"
	"filters=125\n"
	"activation=linear\n"
	"\n"
	"[region]\n"
	"anchors = 1.08,1.19,  3.42,4.41,  6.63,11.38,  9.42,5.11,  16.62,10.52\n"
	"bias_match=1\n"
	"classes=20\n"
	"coords=4\n"
	"num=5\n"
	"softmax=1\n"
	"jitter=.2\n"
	"rescore=1\n"
	"\n"
	"object_scale=5\n"
	"noobject_scale=1\n"
	"class_scale=1\n"
	"coord_scale=1\n"
	"\n"
	"absolute=1\n"
	"thresh = .6\n"
	"random=1\n"
};
static const char *zCoco[] = {
	"person",
	"bicycle",
	"car",
	"motorbike",
	"aeroplane",
	"bus",
	"train",
	"truck",
	"boat",
	"traffic light",
	"fire hydrant",
	"stop sign",
	"parking meter",
	"bench",
	"bird",
	"cat",
	"dog",
	"horse",
	"sheep",
	"cow",
	"elephant",
	"bear",
	"zebra",
	"giraffe",
	"backpack",
	"umbrella",
	"handbag",
	"tie",
	"suitcase",
	"frisbee",
	"skis",
	"snowboard",
	"sports ball",
	"kite",
	"baseball bat",
	"baseball glove",
	"skateboard",
	"surfboard",
	"tennis racket",
	"bottle",
	"wine glass",
	"cup",
	"fork",
	"knife",
	"spoon",
	"bowl",
	"banana",
	"apple",
	"sandwich",
	"orange",
	"broccoli",
	"carrot",
	"hot dog",
	"pizza",
	"donut",
	"cake",
	"chair",
	"sofa",
	"pottedplant",
	"bed",
	"diningtable",
	"toilet",
	"tvmonitor",
	"laptop",
	"mouse",
	"remote",
	"keyboard",
	"cell phone",
	"microwave",
	"oven",
	"toaster",
	"sink",
	"refrigerator",
	"book",
	"clock",
	"vase",
	"scissors",
	"teddy bear",
	"hair drier",
	"toothbrush"
};
static const char *zVoc[] = {
	"aeroplane",
	"bicycle",
	"bird",
	"boat",
	"bottle",
	"bus",
	"car",
	"cat",
	"chair",
	"cow",
	"diningtable",
	"dog",
	"horse",
	"motorbike",
	"person",
	"pottedplant",
	"sheep",
	"sofa",
	"train",
	"tvmonitor"
};
static const char *zFace[] = {
	"face"
};

/*
* Cross platform srtncmp
*/
static int sy_strcmp(const char *zA, const char *zB)
{
	for (;;) {
		int c = tolower(zA[0]);
		int d = tolower(zB[0]);
		int e = c - d;
		if (e != 0) return e;
		if (c == 0) break;
		zA++;
		zB++;
	}
	return 0; /* Equal string */
}
static inline float stair_activate(float x)
{
	int n = floor(x);
	if (n % 2 == 0) return floor(x / 2.);
	else return (x - n) + floor(x / 2.);
}
static inline float hardtan_activate(float x)
{
	if (x < -1) return -1;
	if (x > 1) return 1;
	return x;
}
static inline float linear_activate(float x) { return x; }
static inline float logistic_activate(float x) { return 1. / (1. + exp(-x)); }
static inline float loggy_activate(float x) { return 2. / (1. + exp(-x)) - 1; }
static inline float relu_activate(float x) { return x * (x>0); }
static inline float elu_activate(float x) { return (x >= 0)*x + (x < 0)*(exp(x) - 1); }
static inline float relie_activate(float x) { return (x>0) ? x : .01*x; }
static inline float ramp_activate(float x) { return x * (x>0) + .1*x; }
static inline float leaky_activate(float x) { return (x>0) ? x : .1*x; }
static inline float tanh_activate(float x) { return (exp(2 * x) - 1) / (exp(2 * x) + 1); }
static inline float plse_activate(float x)
{
	if (x < -4) return .01 * (x + 4);
	if (x > 4)  return .01 * (x - 4) + 1;
	return .125*x + .5;
}
static inline float lhtan_activate(float x)
{
	if (x < 0) return .001*x;
	if (x > 1) return .001*(x - 1) + 1;
	return x;
}
static inline float lhtan_gradient(float x)
{
	if (x > 0 && x < 1) return 1;
	return .001;
}
static inline float hardtan_gradient(float x)
{
	if (x > -1 && x < 1) return 1;
	return 0;
}
static inline float linear_gradient() { return 1; }
static inline float logistic_gradient(float x) { return (1 - x)*x; }
static inline float loggy_gradient(float x)
{
	float y = (x + 1.) / 2.;
	return 2 * (1 - y)*y;
}
static inline float stair_gradient(float x)
{
	if (floor(x) == x) return 0;
	return 1;
}
static inline float relu_gradient(float x) { return (x>0); }
static inline float elu_gradient(float x) { return (x >= 0) + (x < 0)*(x + 1); }
static inline float relie_gradient(float x) { return (x>0) ? 1 : .01; }
static inline float ramp_gradient(float x) { return (x>0) + .1; }
static inline float leaky_gradient(float x) { return (x>0) ? 1 : .1; }
static inline float tanh_gradient(float x) { return 1 - x * x; }
static inline float plse_gradient(float x) { return (x < 0 || x > 1) ? .01 : .125; }
static void reorg_cpu(float *x, int w, int h, int c, int batch, int stride, int forward, float *out)
{
	int b, i, j, k;
	int out_c = c / (stride*stride);

	for (b = 0; b < batch; ++b) {
		for (k = 0; k < c; ++k) {
			for (j = 0; j < h; ++j) {
				for (i = 0; i < w; ++i) {
					int in_index = i + w * (j + h * (k + c * b));
					int c2 = k % out_c;
					int offset = k / out_c;
					int w2 = i * stride + offset % stride;
					int h2 = j * stride + offset / stride;
					int out_index = w2 + w * stride*(h2 + h * stride*(c2 + out_c * b));
					if (forward) out[out_index] = x[in_index];
					else out[in_index] = x[out_index];
				}
			}
		}
	}
}
static void flatten(float *x, int size, int layers, int batch, int forward)
{
	float *swap = calloc(size*layers*batch, sizeof(float));
	int i, c, b;
	for (b = 0; b < batch; ++b) {
		for (c = 0; c < layers; ++c) {
			for (i = 0; i < size; ++i) {
				int i1 = b * layers*size + c * size + i;
				int i2 = b * layers*size + i * layers + c;
				if (forward) swap[i2] = x[i1];
				else swap[i1] = x[i2];
			}
		}
	}
	memcpy(x, swap, size*layers*batch * sizeof(float));
	free(swap);
}
static void weighted_sum_cpu(float *a, float *b, float *s, int n, float *c)
{
	int i;
	for (i = 0; i < n; ++i) {
		c[i] = s[i] * a[i] + (1 - s[i])*(b ? b[i] : 0);
	}
}
static void col2im_add_pixel(float *im, int height, int width,
	int row, int col, int channel, int pad, float val)
{
	row -= pad;
	col -= pad;

	if (row < 0 || col < 0 ||
		row >= height || col >= width) return;
	im[col + width * (row + height * channel)] += val;
}
/* This one might be too, can't remember. */
static inline void col2im_cpu(float* data_col,
	int channels, int height, int width,
	int ksize, int stride, int pad, float* data_im)
{
	int c, h, w;
	int height_col = (height + 2 * pad - ksize) / stride + 1;
	int width_col = (width + 2 * pad - ksize) / stride + 1;

	int channels_col = channels * ksize * ksize;
	for (c = 0; c < channels_col; ++c) {
		int w_offset = c % ksize;
		int h_offset = (c / ksize) % ksize;
		int c_im = c / ksize / ksize;
		for (h = 0; h < height_col; ++h) {
			for (w = 0; w < width_col; ++w) {
				int im_row = h_offset + h * stride;
				int im_col = w_offset + w * stride;
				int col_index = (c * height_col + h) * width_col + w;
				double val = data_col[col_index];
				col2im_add_pixel(data_im, height, width,
					im_row, im_col, c_im, pad, val);
			}
		}
	}
}
static inline void shortcut_cpu(int batch, int w1, int h1, int c1, float *add, int w2, int h2, int c2, float *out)
{
	int stride = w1 / w2;
	int sample = w2 / w1;

	if (stride < 1) stride = 1;
	if (sample < 1) sample = 1;
	int minw = (w1 < w2) ? w1 : w2;
	int minh = (h1 < h2) ? h1 : h2;
	int minc = (c1 < c2) ? c1 : c2;

	int i, j, k, b;
	for (b = 0; b < batch; ++b) {
		for (k = 0; k < minc; ++k) {
			for (j = 0; j < minh; ++j) {
				for (i = 0; i < minw; ++i) {
					int out_index = i * sample + w2 * (j*sample + h2 * (k + c2 * b));
					int add_index = i * stride + w1 * (j*stride + h1 * (k + c1 * b));
					out[out_index] += add[add_index];
				}
			}
		}
	}
}
static inline void mean_cpu(float *x, int batch, int filters, int spatial, float *mean)
{
	float scale = 1. / (batch * spatial);
	int i, j, k;
	for (i = 0; i < filters; ++i) {
		mean[i] = 0;
		for (j = 0; j < batch; ++j) {
			for (k = 0; k < spatial; ++k) {
				int index = j * filters*spatial + i * spatial + k;
				mean[i] += x[index];
			}
		}
		mean[i] *= scale;
	}
}
static inline void variance_cpu(float *x, float *mean, int batch, int filters, int spatial, float *variance)
{
	float scale = 1. / (batch * spatial - 1);
	int i, j, k;
	for (i = 0; i < filters; ++i) {
		variance[i] = 0;
		for (j = 0; j < batch; ++j) {
			for (k = 0; k < spatial; ++k) {
				int index = j * filters*spatial + i * spatial + k;
				variance[i] += pow((x[index] - mean[i]), 2);
			}
		}
		variance[i] *= scale;
	}
}
static inline void normalize_cpu(float *x, float *mean, float *variance, int batch, int filters, int spatial)
{
	int b, f, i;
	for (b = 0; b < batch; ++b) {
		for (f = 0; f < filters; ++f) {
			for (i = 0; i < spatial; ++i) {
				int index = b * filters*spatial + f * spatial + i;
				x[index] = (x[index] - mean[f]) / (sqrt(variance[f]) + .000001f);
			}
		}
	}
}
static inline void const_cpu(int N, float ALPHA, float *X, int INCX)
{
	int i = 0;
	for (;;) { if (i >= N)break; X[i*INCX] = ALPHA; i++; }
}
static inline void mul_cpu(int N, float *X, int INCX, float *Y, int INCY)
{
	int i = 0;
	for (;;) { if (i >= N)break; Y[i*INCY] *= X[i*INCX]; i++; }
}
static inline void pow_cpu(int N, float ALPHA, float *X, int INCX, float *Y, int INCY)
{
	int i = 0;
	for (;;) { if (i >= N)break; Y[i*INCY] = pow(X[i*INCX], ALPHA); i++; }
}
static inline void axpy_cpu(int N, float ALPHA, float *X, int INCX, float *Y, int INCY)
{
	int i = 0;
	for (;;) { if (i >= N)break; Y[i*INCY] += ALPHA * X[i*INCX]; i++; }
}
static inline void scal_cpu(int N, float ALPHA, float *X, int INCX)
{
	int i = 0;
	for (;;) { if (i >= N)break; X[i*INCX] *= ALPHA; i++; }
}
static inline void fill_cpu(int N, float ALPHA, float *X, int INCX)
{
	int i = 0;
	for (;;) { if (i >= N)break; X[i*INCX] = ALPHA; i++; }
}
static inline void copy_cpu(int N, float *X, int INCX, float *Y, int INCY)
{
	int i = 0;
	for (;;) { if (i >= N)break; Y[i*INCY] = X[i*INCX]; i++; }
}
static void smooth_l1_cpu(int n, float *pred, float *truth, float *delta, float *error)
{
	int i;
	for (i = 0; i < n; ++i) {
		float diff = truth[i] - pred[i];
		float abs_val = fabs(diff);
		if (abs_val < 1) {
			error[i] = diff * diff;
			delta[i] = diff;
		}
		else {
			error[i] = 2 * abs_val - 1;
			delta[i] = (diff < 0) ? -1 : 1;
		}
	}
}
static inline void l2_cpu(int n, float *pred, float *truth, float *delta, float *error)
{
	int i = 0;
	for (;;) {
		if (i >= n)break;
		float diff = truth[i] - pred[i];
		error[i] = diff * diff;
		delta[i] = diff;
		i++;
	}
}
static void softmax(float *input, int n, float temp, float *output)
{
	int i = 0;
	float sum = 0;
	float largest = -FLT_MAX;
	for (;;) {
		if (i >= n)break;
		if (input[i] > largest) largest = input[i];
		i++;
	}
	i = 0;
	for (;;) {
		if (i >= n)break;
		float e = exp(input[i] / temp - largest / temp);
		sum += e;
		output[i] = e;
		i++;
	}
	i = 0;
	for (;;) {
		if (i >= n)break;
		output[i] /= sum;
		i++;
	}
}
static inline float mag_array(float *a, int n)
{
	int i;
	float sum = 0;
	for (i = 0; i < n; ++i) {
		sum += a[i] * a[i];
	}
	return sqrt(sum);
}
static inline float sum_array(float *a, int n)
{
	int i;
	float sum = 0;
	for (i = 0; i < n; ++i) sum += a[i];
	return sum;
}
static inline void scale_array(float *a, int n, float s)
{
	int i;
	for (i = 0; i < n; ++i) {
		a[i] *= s;
	}
}
static int sample_array(float *a, int n)
{
	float sum = sum_array(a, n);
	scale_array(a, n, 1. / sum);
	float r = rand_uniform(0, 1);
	int i;
	for (i = 0; i < n; ++i) {
		r = r - a[i];
		if (r <= 0) return i;
	}
	return n - 1;
}
static void make_network(int n, network *out)
{
	network net;
	memset(&net, 0, sizeof(network));
	net.n = n;
	net.layers = calloc(net.n, sizeof(layer));
	net.seen = calloc(1, sizeof(uint64_t));
#if 0 /* SOD_GPU */
	net.input_gpu = calloc(1, sizeof(float *));
	net.truth_gpu = calloc(1, sizeof(float *));
#endif
	*out = net;
}
static void free_layer(layer *l, layer *p)
{
	if (l->type == DROPOUT) {
		if (l->rand)           free(l->rand);
#if 0 /* SOD_GPU */
		if (l->rand_gpu)             cuda_free(l->rand_gpu);
#endif
		return;
	}
	if (l->cweights) {
		free(l->cweights);
	}
	if (l->indexes) {
		free(l->indexes);
	}
	if (l->input_layers) {
		free(l->input_layers);

	}
	if (l->input_sizes) {
		free(l->input_sizes);

	}
	if (l->map) {
		free(l->map);

	}
	if (l->rand) {
		free(l->rand);

	}
	if (l->cost) {
		free(l->cost);

	}
	if (l->state) {
		free(l->state);

	}
	if (l->prev_state) {
		free(l->prev_state);

	}
	if (l->forgot_state) {
		free(l->forgot_state);

	}
	if (l->forgot_delta) {
		free(l->forgot_delta);

	}
	if (l->state_delta) {
		free(l->state_delta);

	}
	if (l->concat) {
		free(l->concat);

	}
	if (l->concat_delta) {
		free(l->concat_delta);

	}
	if (l->binary_weights) {
		free(l->binary_weights);

	}
	if (l->biases) {
		free(l->biases);

	}
	if (l->bias_updates) {
		free(l->bias_updates);

	}
	if (l->scales) {
		free(l->scales);

	}
	if (l->scale_updates) {
		free(l->scale_updates);

	}
	if (l->weights) {
		free(l->weights);

	}
	if (l->weight_updates) {
		free(l->weight_updates);

	}
	if (l->delta && (!p || p->delta != l->delta)) {
		free(l->delta);

	}
	if (l->output && (!p || p->output != l->output)) {
		free(l->output);

	}
	if (l->squared) {
		free(l->squared);

	}
	if (l->norms) {
		free(l->norms);

	}
	if (l->spatial_mean) {
		free(l->spatial_mean);

	}
	if (l->mean) {
		free(l->mean);

	}
	if (l->variance) {
		free(l->variance);

	}
	if (l->mean_delta) {
		free(l->mean_delta);

	}
	if (l->variance_delta) {
		free(l->variance_delta);

	}
	if (l->rolling_mean) {
		free(l->rolling_mean);

	}
	if (l->rolling_variance) {
		free(l->rolling_variance);

	}
	if (l->x) {
		free(l->x);

	}
	if (l->x_norm) {
		free(l->x_norm);

	}
	if (l->m) {
		free(l->m);

	}
	if (l->v) {
		free(l->v);

	}
	if (l->z_cpu) {
		free(l->z_cpu);

	}
	if (l->r_cpu) {
		free(l->r_cpu);

	}
	if (l->h_cpu) {
		free(l->h_cpu);

	}
	if (l->binary_input) {
		free(l->binary_input);

	}
#if 0 /* SOD_GPU */
	if (l->indexes_gpu)           cuda_free((float *)l->indexes_gpu);

	if (l->z_gpu)                   cuda_free(l->z_gpu);
	if (l->r_gpu)                   cuda_free(l->r_gpu);
	if (l->h_gpu)                   cuda_free(l->h_gpu);
	if (l->m_gpu)                   cuda_free(l->m_gpu);
	if (l->v_gpu)                   cuda_free(l->v_gpu);
	if (l->prev_state_gpu)          cuda_free(l->prev_state_gpu);
	if (l->forgot_state_gpu)        cuda_free(l->forgot_state_gpu);
	if (l->forgot_delta_gpu)        cuda_free(l->forgot_delta_gpu);
	if (l->state_gpu)               cuda_free(l->state_gpu);
	if (l->state_delta_gpu)         cuda_free(l->state_delta_gpu);
	if (l->gate_gpu)                cuda_free(l->gate_gpu);
	if (l->gate_delta_gpu)          cuda_free(l->gate_delta_gpu);
	if (l->save_gpu)                cuda_free(l->save_gpu);
	if (l->save_delta_gpu)          cuda_free(l->save_delta_gpu);
	if (l->concat_gpu)              cuda_free(l->concat_gpu);
	if (l->concat_delta_gpu)        cuda_free(l->concat_delta_gpu);
	if (l->binary_input_gpu)        cuda_free(l->binary_input_gpu);
	if (l->binary_weights_gpu)      cuda_free(l->binary_weights_gpu);
	if (l->mean_gpu)                cuda_free(l->mean_gpu);
	if (l->variance_gpu)            cuda_free(l->variance_gpu);
	if (l->rolling_mean_gpu)        cuda_free(l->rolling_mean_gpu);
	if (l->rolling_variance_gpu)    cuda_free(l->rolling_variance_gpu);
	if (l->variance_delta_gpu)      cuda_free(l->variance_delta_gpu);
	if (l->mean_delta_gpu)          cuda_free(l->mean_delta_gpu);
	if (l->x_gpu)                   cuda_free(l->x_gpu);
	if (l->x_norm_gpu)              cuda_free(l->x_norm_gpu);
	if (l->weights_gpu)             cuda_free(l->weights_gpu);
	if (l->weight_updates_gpu)      cuda_free(l->weight_updates_gpu);
	if (l->biases_gpu)              cuda_free(l->biases_gpu);
	if (l->bias_updates_gpu)        cuda_free(l->bias_updates_gpu);
	if (l->scales_gpu)              cuda_free(l->scales_gpu);
	if (l->scale_updates_gpu)       cuda_free(l->scale_updates_gpu);
	if (l->output_gpu)              cuda_free(l->output_gpu);
	if (l->delta_gpu)               cuda_free(l->delta_gpu);
	if (l->rand_gpu)                cuda_free(l->rand_gpu);
	if (l->squared_gpu)             cuda_free(l->squared_gpu);
	if (l->norms_gpu)               cuda_free(l->norms_gpu);
#endif
}
static void free_network(network *net)
{
	int i;
	for (i = 0; i < net->n; ++i) {
		layer *l = &net->layers[i];
		if (l->input_layer) {
			free_layer(l->input_layer, l);
			free(l->input_layer);
		}
		if (l->self_layer) {
			free_layer(l->self_layer, l);
			free(l->self_layer);
		}
		if (l->output_layer) {
			free_layer(l->output_layer, l);
			free(l->output_layer);
		}
		if (l->input_z_layer) {
			free_layer(l->input_z_layer, l);
			free(l->input_z_layer);
		}
		if (l->state_z_layer) {
			free_layer(l->state_z_layer, l);
			free(l->state_z_layer);
		}
		if (l->input_r_layer) {
			free_layer(l->input_r_layer, l);
			free(l->input_r_layer);
		}
		if (l->state_r_layer) {
			free_layer(l->state_r_layer, l);
			free(l->state_r_layer);
		}
		if (l->input_h_layer) {
			free_layer(l->input_h_layer, l);
			free(l->input_h_layer);
		}
		if (l->state_h_layer) {
			free_layer(l->state_h_layer, l);
			free(l->state_h_layer);
		}
		free_layer(l, 0);
	}
	if (net->layers) {
		free(net->layers);
	}
	if (net->seen) {
		free(net->seen);
	}
	if (net->scales) {
		free(net->scales);
	}
	if (net->steps) {
		free(net->steps);
	}
	if (net->workspace_size) {
#if 0 /* SOD_GPU */
		if (net.gpu_index >= 0) {
			cuda_free(net->workspace);
		}
		else {
			free(net->workspace);
		}
#else
		free(net->workspace);
#endif
	}
}
static void forward_network(network *net, network_state state)
{
	int i = 0;
	layer l;
	state.workspace = net->workspace;
	for (;;) {
		if (i >= net->n) break;
		l = net->layers[i];
		state.index = i;
		if (l.delta) {
			fill_cpu(l.outputs * l.batch, 0, l.delta, 1);
		}
		l.forward(l, state);
		state.input = l.output;
		i++;
	}
}
static float *get_network_output(network *net)
{
#if 0 /* SOD_GPU */
	if (gpu_index >= 0) return get_network_output_gpu(net);
#endif
	int i;
	for (i = net->n - 1; i > 0; --i) if (net->layers[i].type != COST) break;
	net->pNet->nOuput = net->layers[i].inputs;
	return net->layers[i].output;
}
static int get_network_output_size(network *net)
{
	int i;
	for (i = net->n - 1; i > 0; --i) if (net->layers[i].type != COST) break;
	return net->layers[i].outputs;
}
static int get_network_input_size(network *net)
{
	return net->layers[0].inputs;
}
#if 0
static void backward_network(network *net, network_state state)
{
	int i;
	float *original_input = state.input;
	float *original_delta = state.delta;
	state.workspace = net->workspace;
	for (i = net->n - 1; i >= 0; --i) {
		state.index = i;
		if (i == 0) {
			state.input = original_input;
			state.delta = original_delta;
		}
		else {
			layer prev = net->layers[i - 1];
			state.input = prev.output;
			state.delta = prev.delta;
		}
		layer l = net->layers[i];
		l.backward(l, state);
	}
}
#endif
static float *network_predict(network *net, float *input)
{
#if 0 /* SOD_GPU */
	if (gpu_index >= 0)  return network_predict_gpu(net, input);
#else
	network_state state;
	float *out;
	state.net = net;
	state.index = 0;
	state.input = input;
	state.truth = 0;
	state.train = 0;
	state.delta = 0;
	forward_network(net, state);
	out = get_network_output(net);
	return out;
#endif /* SOD_GPU */
}
static void set_batch_network(network *net, int b)
{
	net->batch = b;
	int i;
	for (i = 0; i < net->n; ++i) {
		net->layers[i].batch = b;
#ifdef CUDNN
		if (net->layers[i].type == CONVOLUTIONAL) {
			cudnn_convolutional_setup(net->layers + i);
		}
#endif
	}
}
static void transpose_matrix(float *a, int rows, int cols)
{
	float *transpose = calloc(rows*cols, sizeof(float));
	int x, y;
	for (x = 0; x < rows; ++x) {
		for (y = 0; y < cols; ++y) {
			transpose[y*rows + x] = a[x*cols + y];
		}
	}
	memcpy(a, transpose, rows*cols * sizeof(float));
	free(transpose);
}
static void load_convolutional_weights(layer l, FILE *fp)
{
	int num = l.n*l.c*l.size*l.size;
	fread(l.biases, sizeof(float), l.n, fp);
	if (l.batch_normalize && (!l.dontloadscales)) {
		fread(l.scales, sizeof(float), l.n, fp);
		fread(l.rolling_mean, sizeof(float), l.n, fp);
		fread(l.rolling_variance, sizeof(float), l.n, fp);
	}
	fread(l.weights, sizeof(float), num, fp);
	if (l.adam) {
		fread(l.m, sizeof(float), num, fp);
		fread(l.v, sizeof(float), num, fp);
	}
	if (l.flipped) {
		transpose_matrix(l.weights, l.c*l.size*l.size, l.n);
	}
#if 0 /* SOD_GPU */
	if (gpu_index >= 0) {
		push_convolutional_layer(l);
	}
#endif
}
static void load_connected_weights(layer l, FILE *fp, int transpose)
{
	fread(l.biases, sizeof(float), l.outputs, fp);
	fread(l.weights, sizeof(float), l.outputs*l.inputs, fp);
	if (transpose) {
		transpose_matrix(l.weights, l.inputs, l.outputs);
	}
	if (l.batch_normalize && (!l.dontloadscales)) {
		fread(l.scales, sizeof(float), l.outputs, fp);
		fread(l.rolling_mean, sizeof(float), l.outputs, fp);
		fread(l.rolling_variance, sizeof(float), l.outputs, fp);
	}
#if 0 /* SOD_GPU */
	if (gpu_index >= 0) {
		push_connected_layer(l);
	}
#endif
}
static void load_batchnorm_weights(layer l, FILE *fp)
{
	fread(l.scales, sizeof(float), l.c, fp);
	fread(l.rolling_mean, sizeof(float), l.c, fp);
	fread(l.rolling_variance, sizeof(float), l.c, fp);
#if 0 /* SOD_GPU */
	if (gpu_index >= 0) {
		push_batchnorm_layer(l);
	}
#endif
}
static int load_weights_upto(network *net, const char *filename, int cutoff)
{
	int i, major;
	int minor;
	int revision;
	int transpose;
	FILE *fp;
#if 0 /* SOD_GPU */
	if (net->gpu_index >= 0) {
		cuda_set_device(net->gpu_index);
	}
#endif
	fp = fopen(filename, "rb");
	if (!fp) {
		net->pNet->nErr++;
		net->pNet->zErr = "Cannot open SOD model";
		return SOD_IOERR;
	}

	fread(&major, sizeof(int), 1, fp);
	fread(&minor, sizeof(int), 1, fp);
	fread(&revision, sizeof(int), 1, fp);
	if ((major * 10 + minor) >= 2 && major < 1000 && minor < 1000) {
		fread(net->seen, sizeof(uint64_t), 1, fp);
	}
	else {
		int iseen = 0;
		fread(&iseen, sizeof(int), 1, fp);
		*net->seen = iseen;
	}
	transpose = (major > 1000) || (minor > 1000);
	for (i = 0; i < net->n && i < cutoff; ++i) {
		layer l = net->layers[i];
		if (l.dontload) continue;
		if (l.type == CONVOLUTIONAL /*|| l.type == DECONVOLUTIONAL*/) {
			load_convolutional_weights(l, fp);
		}
		if (l.type == CONNECTED) {
			load_connected_weights(l, fp, transpose);
		}
		if (l.type == BATCHNORM) {
			load_batchnorm_weights(l, fp);
		}
		if (l.type == CRNN) {
			load_convolutional_weights(*(l.input_layer), fp);
			load_convolutional_weights(*(l.self_layer), fp);
			load_convolutional_weights(*(l.output_layer), fp);
		}
		if (l.type == RNN) {
			load_connected_weights(*(l.input_layer), fp, transpose);
			load_connected_weights(*(l.self_layer), fp, transpose);
			load_connected_weights(*(l.output_layer), fp, transpose);
		}
		if (l.type == GRU) {
			load_connected_weights(*(l.input_z_layer), fp, transpose);
			load_connected_weights(*(l.input_r_layer), fp, transpose);
			load_connected_weights(*(l.input_h_layer), fp, transpose);
			load_connected_weights(*(l.state_z_layer), fp, transpose);
			load_connected_weights(*(l.state_r_layer), fp, transpose);
			load_connected_weights(*(l.state_h_layer), fp, transpose);
		}
		if (l.type == LOCAL) {
			int locations = l.out_w*l.out_h;
			int size = l.size*l.size*l.c*l.n*locations;
			fread(l.biases, sizeof(float), l.outputs, fp);
			fread(l.weights, sizeof(float), size, fp);
#if 0 /* SOD_GPU */
			if (gpu_index >= 0) {
				push_local_layer(l);
			}
#endif
		}
	}
	fclose(fp);
	return SOD_OK;
}
static int load_weights(network *net, const char *filename)
{
	return load_weights_upto(net, filename, net->n);
}
/* =============== CFG Parser ====================== */
typedef struct node {
	void *val;
	struct node *next;
	struct node *prev;
} node;

typedef struct list {
	int size;
	node *front;
	node *back;
} list;
typedef struct size_params {
	int batch;
	int inputs;
	int h;
	int w;
	int c;
	int index;
	int time_steps;
	network net;
} size_params;
typedef struct {
	char *type;
	list *options;
}section;
static int is_network(section *s)
{
	return (strcmp(s->type, "[net]") == 0
		|| strcmp(s->type, "[network]") == 0);
}
static char *fgetl(FILE *fp)
{
	if (feof(fp)) return 0;
	size_t size = 512;
	char *line = malloc(size * sizeof(char));
	if (!fgets(line, (int)size, fp)) {
		free(line);
		return 0;
	}

	size_t curr = strlen(line);

	while ((line[curr - 1] != '\n') && !feof(fp)) {
		if (curr == size - 1) {
			size *= 2;
			line = realloc(line, size * sizeof(char));
			if (!line) {
				return 0;
			}
		}
		size_t readsize = size - curr;
		if (readsize > INT_MAX) readsize = INT_MAX - 1;
		fgets(&line[curr], (int)readsize, fp);
		curr = strlen(line);
	}
	if (line[curr - 1] == '\n') line[curr - 1] = '\0';

	return line;
}
static list *make_list()
{
	list *l = malloc(sizeof(list));
	l->size = 0;
	l->front = 0;
	l->back = 0;
	return l;
}
static void list_insert(list *l, void *val)
{
	node *lnew = malloc(sizeof(node));
	lnew->val = val;
	lnew->next = 0;

	if (!l->back) {
		l->front = lnew;
		lnew->prev = 0;
	}
	else {
		l->back->next = lnew;
		lnew->prev = l->back;
	}
	l->back = lnew;
	++l->size;
}
static void free_node(node *n)
{
	node *next;
	while (n) {
		next = n->next;
		free(n);
		n = next;
	}
}
static void free_list(list *l)
{
	free_node(l->front);
	free(l);
}
static void strip(char *s)
{
	size_t i;
	size_t len = strlen(s);
	size_t offset = 0;
	for (i = 0; i < len; ++i) {
		char c = s[i];
		if (c == ' ' || c == '\t' || c == '\n') ++offset;
		else s[i - offset] = c;
	}
	s[len - offset] = '\0';
}
typedef struct {
	char *key;
	char *val;
	int used;
} kvp;
static void option_insert(list *l, char *key, char *val)
{
	kvp *p = malloc(sizeof(kvp));
	p->key = key;
	p->val = val;
	p->used = 0;
	list_insert(l, p);
}
static int read_option(char *s, list *options)
{
	size_t i;
	size_t len = strlen(s);
	char *val = 0;
	for (i = 0; i < len; ++i) {
		if (s[i] == '=') {
			s[i] = '\0';
			val = s + i + 1;
			break;
		}
	}
	if (i == len - 1) return 0;
	char *key = s;
	option_insert(options, key, val);
	return 1;
}
static void free_section(section *s)
{
	free(s->type);
	node *n = s->options->front;
	while (n) {
		kvp *pair = (kvp *)n->val;
		free(pair->key);
		free(pair);
		node *next = n->next;
		free(n);
		n = next;
	}
	free(s->options);
	free(s);
}
static int next_line(const char **pzPtr, char **pzBuf)
{
	const char *zPtr = *pzPtr; /* Must be null terminated */
	const char *zCur = zPtr;
	if (zPtr[0] == 0) {
		return -1;
	}
	for (;;) {
		if (zPtr[0] == '\n' || zPtr[0] == 0) break;
		zPtr++;
	}
	size_t n = (size_t)(zPtr - zCur);
	if (zPtr[0] == '\n') zPtr++;
	char *zBuf = malloc(n + 1);
	if (!zBuf) return -1;
	memcpy(zBuf, zCur, n);
	zBuf[n] = 0;
	*pzBuf = zBuf;
	*pzPtr = zPtr;
	return SOD_OK;
}
static list *read_cfg(const char *zConf)
{
	char *line;
	int nu = 0;
	list *sections;
	section *current = 0;

	sections = make_list();

	while (next_line(&zConf, &line) == SOD_OK) {
		++nu;
		strip(line);
		switch (line[0]) {
		case '[':
			current = malloc(sizeof(section));
			list_insert(sections, current);
			current->options = make_list();
			current->type = line;
			break;
		case '\0':
		case '#':
		case ';':
			free(line);
			break;
		default:
			if (!read_option(line, current->options)) {
				free(line);
			}
			break;
		}
	}
	return sections;
}
static char *option_find(list *l, char *key)
{
	node *n = l->front;
	while (n) {
		kvp *p = (kvp *)n->val;
		if (strcmp(p->key, key) == 0) {
			p->used = 1;
			return p->val;
		}
		n = n->next;
	}
	return 0;
}
static int option_find_int(list *l, char *key, int def)
{
	char *v = option_find(l, key);
	if (v) return atoi(v);
	return def;
}
static float option_find_float(list *l, char *key, float def)
{
	char *v = option_find(l, key);
	if (v) return atof(v);
	return def;
}
static char *option_find_str(list *l, char *key, char *def)
{
	char *v = option_find(l, key);
	if (v) return v;
	return def;
}
static learning_rate_policy get_policy(char *s)
{
	if (strcmp(s, "random") == 0) return RANDOM;
	if (strcmp(s, "poly") == 0) return POLY;
	if (strcmp(s, "constant") == 0) return CONSTANT;
	if (strcmp(s, "step") == 0) return STEP;
	if (strcmp(s, "exp") == 0) return EXP;
	if (strcmp(s, "sigmoid") == 0) return SIG;
	if (strcmp(s, "steps") == 0) return STEPS;

	return CONSTANT;
}
static SOD_CNN_LAYER_TYPE string_to_layer_type(char * type)
{
	if (strcmp(type, "[shortcut]") == 0) return SHORTCUT;
	if (strcmp(type, "[crop]") == 0) return CROP;
	if (strcmp(type, "[cost]") == 0) return COST;
	if (strcmp(type, "[detection]") == 0) return DETECTION;
	if (strcmp(type, "[region]") == 0) return REGION;
	if (strcmp(type, "[local]") == 0) return LOCAL;
	if (strcmp(type, "[conv]") == 0
		|| strcmp(type, "[convolutional]") == 0) return CONVOLUTIONAL;
	if (strcmp(type, "[activation]") == 0) return ACTIVE;
	if (strcmp(type, "[net]") == 0
		|| strcmp(type, "[network]") == 0) return NETWORK;
	if (strcmp(type, "[crnn]") == 0) return CRNN;
	if (strcmp(type, "[gru]") == 0) return GRU;
	if (strcmp(type, "[rnn]") == 0) return RNN;
	if (strcmp(type, "[conn]") == 0
		|| strcmp(type, "[connected]") == 0) return CONNECTED;
	if (strcmp(type, "[max]") == 0
		|| strcmp(type, "[maxpool]") == 0) return MAXPOOL;
	if (strcmp(type, "[reorg]") == 0) return REORG;
	if (strcmp(type, "[avg]") == 0
		|| strcmp(type, "[avgpool]") == 0) return AVGPOOL;
	if (strcmp(type, "[dropout]") == 0) return DROPOUT;
	if (strcmp(type, "[lrn]") == 0
		|| strcmp(type, "[normalization]") == 0) return NORMALIZATION;
	if (strcmp(type, "[batchnorm]") == 0) return BATCHNORM;
	if (strcmp(type, "[soft]") == 0
		|| strcmp(type, "[softmax]") == 0) return SOFTMAX;
	if (strcmp(type, "[route]") == 0) return ROUTE;
	return BLANK;
}
static int parse_net_options(list *options, network *net)
{
	int subdivs;
	net->batch = option_find_int(options, "batch", 1);
	net->learning_rate = option_find_float(options, "learning_rate", .001);
	net->momentum = option_find_float(options, "momentum", .9);
	net->decay = option_find_float(options, "decay", .0001);
	subdivs = option_find_int(options, "subdivisions", 1);
	net->time_steps = option_find_int(options, "time_steps", 1);
	net->batch /= subdivs;
	net->batch *= net->time_steps;
	net->subdivisions = subdivs;

	net->adam = option_find_int(options, "adam", 0);
	if (net->adam) {
		net->B1 = option_find_float(options, "B1", .9);
		net->B2 = option_find_float(options, "B2", .999);
		net->eps = option_find_float(options, "eps", .000001);
	}

	net->h = option_find_int(options, "height", 0);
	net->w = option_find_int(options, "width", 0);
	net->c = option_find_int(options, "channels", 0);
	net->inputs = option_find_int(options, "inputs", net->h * net->w * net->c);
	net->max_crop = option_find_int(options, "max_crop", net->w * 2);
	net->min_crop = option_find_int(options, "min_crop", net->w);

	net->angle = option_find_float(options, "angle", 0);
	net->aspect = option_find_float(options, "aspect", 1);
	net->saturation = option_find_float(options, "saturation", 1);
	net->exposure = option_find_float(options, "exposure", 1);
	net->hue = option_find_float(options, "hue", 0);

	if (!net->inputs && !(net->h && net->w && net->c)) {
		net->pNet->nErr++;
		net->pNet->zErr = "No input parameters supplied";
		return -1;
	}
	char *policy_s = option_find_str(options, "policy", "constant");
	net->policy = get_policy(policy_s);
	net->burn_in = option_find_int(options, "burn_in", 0);
	if (net->policy == STEP) {
		net->step = option_find_int(options, "step", 1);
		net->scale = option_find_float(options, "scale", 1);
	}
	else if (net->policy == STEPS) {
		char *l = option_find(options, "steps");
		char *p = option_find(options, "scales");
		if (!l || !p) {
			net->pNet->nErr++;
			net->pNet->zErr = "STEPS policy must have steps and scales in cfg file";
			return -1;
		}

		int len = (int)strlen(l);
		int n = 1;
		int i;
		for (i = 0; i < len; ++i) {
			if (l[i] == ',') ++n;
		}
		int *steps = calloc(n, sizeof(int));
		float *scales = calloc(n, sizeof(float));
		for (i = 0; i < n; ++i) {
			int step = atoi(l);
			float scale = atof(p);
			l = strchr(l, ',') + 1;
			p = strchr(p, ',') + 1;
			steps[i] = step;
			scales[i] = scale;
		}
		net->scales = scales;
		net->steps = steps;
		net->num_steps = n;
	}
	else if (net->policy == EXP) {
		net->gamma = option_find_float(options, "gamma", 1);
	}
	else if (net->policy == SIG) {
		net->gamma = option_find_float(options, "gamma", 1);
		net->step = option_find_int(options, "step", 1);
	}
	else if (net->policy == POLY || net->policy == RANDOM) {
		net->power = option_find_float(options, "power", 1);
	}
	net->max_batches = option_find_int(options, "max_batches", 0);
	return 0;
}
typedef layer convolutional_layer;
static ACTIVATION get_activation(char *s)
{
	if (strcmp(s, "logistic") == 0) return LOGISTIC;
	if (strcmp(s, "loggy") == 0) return LOGGY;
	if (strcmp(s, "relu") == 0) return RELU;
	if (strcmp(s, "elu") == 0) return ELU;
	if (strcmp(s, "relie") == 0) return RELIE;
	if (strcmp(s, "plse") == 0) return PLSE;
	if (strcmp(s, "hardtan") == 0) return HARDTAN;
	if (strcmp(s, "lhtan") == 0) return LHTAN;
	if (strcmp(s, "linear") == 0) return LINEAR;
	if (strcmp(s, "ramp") == 0) return RAMP;
	if (strcmp(s, "leaky") == 0) return LEAKY;
	if (strcmp(s, "tanh") == 0) return TANH;
	if (strcmp(s, "stair") == 0) return STAIR;

	return RELU;
}
/* =============================================================== Convolution =============================================================== */
static inline void swap_binary(convolutional_layer *l)
{
	float *swap = l->weights;
	l->weights = l->binary_weights;
	l->binary_weights = swap;

#if 0 /* SOD_GPU */
	swap = l->weights_gpu;
	l->weights_gpu = l->binary_weights_gpu;
	l->binary_weights_gpu = swap;
#endif
}
static void binarize_weights(float *weights, int n, int size, float *binary)
{
	int i, f;
	for (f = 0; f < n; ++f) {
		float mean = 0;
		for (i = 0; i < size; ++i) {
			mean += fabs(weights[f*size + i]);
		}
		mean = mean / size;
		for (i = 0; i < size; ++i) {
			binary[f*size + i] = (weights[f*size + i] > 0) ? mean : -mean;
		}
	}
}
static inline void binarize_cpu(float *input, int n, float *binary)
{
	int i = 0;
	for (;;) {
		if (i >= n)break;
		binary[i] = (input[i] > 0) ? 1 : -1;
		i++;
	}
}
#define convolutional_out_height(l) ((l.h + 2 * l.pad - l.size) / l.stride + 1)
#define convolutional_out_width(l) ((l.w + 2 * l.pad - l.size) / l.stride + 1)
static inline float im2col_get_pixel(float *im, int height, int width,
	int row, int col, int channel, int pad)
{
	row -= pad;
	col -= pad;

	if (row < 0 || col < 0 ||
		row >= height || col >= width) return 0;
	return im[col + width * (row + height * channel)];
}
/*
* From Berkeley Vision's Caffe!
* https://github.com/BVLC/caffe/blob/master/LICENSE
*/
static inline void im2col_cpu(float* data_im,
	int channels, int height, int width,
	int ksize, int stride, int pad, float* data_col)
{
	int c, h, w;
	int height_col = (height + 2 * pad - ksize) / stride + 1;
	int width_col = (width + 2 * pad - ksize) / stride + 1;
	int channels_col = channels * ksize * ksize;
	int w_offset, h_offset, c_im, im_row, im_col, col_index;
	c = 0;
	for (;;) {
		if (c >= channels_col) break;
		w_offset = c % ksize;
		h_offset = (c / ksize) % ksize;
		c_im = c / ksize / ksize;
		for (h = 0; h < height_col; ++h) {
			for (w = 0; w < width_col; ++w) {
				im_row = h_offset + h * stride;
				im_col = w_offset + w * stride;
				col_index = (c * height_col + h) * width_col + w;
				data_col[col_index] = im2col_get_pixel(data_im, height, width,
					im_row, im_col, c_im, pad);
			}
		}
		c++;
	}
}
static inline void gemm_nn(int M, int N, int K, float ALPHA,
	float *A, int lda,
	float *B, int ldb,
	float *C, int ldc)
{
	register int i, j, k;
	i = 0;
	for (;;) {
		if (i >= M)break;
		k = 0;
		for (;;) {
			register float A_PART;
			if (k >= K)break;
			A_PART = ALPHA * A[i*lda + k];
			for (j = 0; j < N; ++j) {
				C[i*ldc + j] += A_PART * B[k*ldb + j];
			}
			k++;
		}
		i++;
	}
}
static inline void gemm_nt(int M, int N, int K, float ALPHA,
	float *A, int lda,
	float *B, int ldb,
	float *C, int ldc)
{
	int i = 0, j, k;

	for (;;) {
		if (i >= M)break;
		for (j = 0; j < N; ++j) {
			register float sum = 0;
			for (k = 0; k < K; ++k) {
				sum += ALPHA * A[i*lda + k] * B[j*ldb + k];
			}
			C[i*ldc + j] += sum;
		}
		i++;
	}
}
static inline void gemm_tn(int M, int N, int K, float ALPHA,
	float *A, int lda,
	float *B, int ldb,
	float *C, int ldc)
{
	int i = 0, j, k;

	for (;;) {
		if (i >= M)break;
		for (k = 0; k < K; ++k) {
			register float A_PART = ALPHA * A[k*lda + i];
			for (j = 0; j < N; ++j) {
				C[i*ldc + j] += A_PART * B[k*ldb + j];
			}
		}
		i++;
	}
}
static inline void gemm_tt(int M, int N, int K, float ALPHA,
	float *A, int lda,
	float *B, int ldb,
	float *C, int ldc)
{
	int i = 0, j, k;

	for (;;) {
		if (i >= M)break;
		for (j = 0; j < N; ++j) {
			register float sum = 0;
			for (k = 0; k < K; ++k) {
				sum += ALPHA * A[i + k * lda] * B[k + j * ldb];
			}
			C[i*ldc + j] += sum;
		}
		i++;
	}
}
static inline void gemm_cpu(int TA, int TB, int M, int N, int K, float ALPHA,
	float *A, int lda,
	float *B, int ldb,
	float BETA,
	float *C, int ldc)
{
	int i = 0, j;

	for (;;) {
		if (i >= M)break;
		j = 0;
		for (;;) {
			if (j >= N) {
				break;
			}
			C[i*ldc + j] *= BETA; j++;
		}
		i++;
	}
	if (!TA && !TB) {
		gemm_nn(M, N, K, ALPHA, A, lda, B, ldb, C, ldc);
	}
	else if (TA && !TB) {
		gemm_tn(M, N, K, ALPHA, A, lda, B, ldb, C, ldc);
	}
	else if (!TA && TB) {
		gemm_nt(M, N, K, ALPHA, A, lda, B, ldb, C, ldc);
	}
	else {
		gemm_tt(M, N, K, ALPHA, A, lda, B, ldb, C, ldc);
	}
}
static inline void gemm(int TA, int TB, int M, int N, int K, float ALPHA,
	float *A, int lda,
	float *B, int ldb,
	float BETA,
	float *C, int ldc)
{
	gemm_cpu(TA, TB, M, N, K, ALPHA, A, lda, B, ldb, BETA, C, ldc);
}
static void scale_bias(float *output, float *scales, int batch, int n, int size)
{
	int i, j, b;
	for (b = 0; b < batch; ++b) {
		for (i = 0; i < n; ++i) {
			for (j = 0; j < size; ++j) {
				output[(b*n + i)*size + j] *= scales[i];
			}
		}
	}
}
static void forward_batchnorm_layer(layer l, network_state state)
{
	if (l.type == BATCHNORM) copy_cpu(l.outputs*l.batch, state.input, 1, l.output, 1);
	if (l.type == CONNECTED) {
		l.out_c = l.outputs;
		l.out_h = l.out_w = 1;
	}
	if (state.train) {
		mean_cpu(l.output, l.batch, l.out_c, l.out_h*l.out_w, l.mean);
		variance_cpu(l.output, l.mean, l.batch, l.out_c, l.out_h*l.out_w, l.variance);

		scal_cpu(l.out_c, .9, l.rolling_mean, 1);
		axpy_cpu(l.out_c, .1, l.mean, 1, l.rolling_mean, 1);
		scal_cpu(l.out_c, .9, l.rolling_variance, 1);
		axpy_cpu(l.out_c, .1, l.variance, 1, l.rolling_variance, 1);

		copy_cpu(l.outputs*l.batch, l.output, 1, l.x, 1);
		normalize_cpu(l.output, l.mean, l.variance, l.batch, l.out_c, l.out_h*l.out_w);
		copy_cpu(l.outputs*l.batch, l.output, 1, l.x_norm, 1);
	}
	else {
		normalize_cpu(l.output, l.rolling_mean, l.rolling_variance, l.batch, l.out_c, l.out_h*l.out_w);
	}
	scale_bias(l.output, l.scales, l.batch, l.out_c, l.out_h*l.out_w);
}
static inline float activate(float x, ACTIVATION a)
{
	switch (a) {
	case LINEAR:
		return linear_activate(x);
	case LOGISTIC:
		return logistic_activate(x);
	case LOGGY:
		return loggy_activate(x);
	case RELU:
		return relu_activate(x);
	case ELU:
		return elu_activate(x);
	case RELIE:
		return relie_activate(x);
	case RAMP:
		return ramp_activate(x);
	case LEAKY:
		return leaky_activate(x);
	case TANH:
		return tanh_activate(x);
	case PLSE:
		return plse_activate(x);
	case STAIR:
		return stair_activate(x);
	case HARDTAN:
		return hardtan_activate(x);
	case LHTAN:
		return lhtan_activate(x);
	}
	return 0;
}
static inline void activate_array(float *x, const int n, const ACTIVATION a)
{
	int i;
	for (i = 0; i < n; ++i) {
		x[i] = activate(x[i], a);
	}
}
static float gradient(float x, ACTIVATION a)
{
	switch (a) {
	case LINEAR:
		return linear_gradient();
	case LOGISTIC:
		return logistic_gradient(x);
	case LOGGY:
		return loggy_gradient(x);
	case RELU:
		return relu_gradient(x);
	case ELU:
		return elu_gradient(x);
	case RELIE:
		return relie_gradient(x);
	case RAMP:
		return ramp_gradient(x);
	case LEAKY:
		return leaky_gradient(x);
	case TANH:
		return tanh_gradient(x);
	case PLSE:
		return plse_gradient(x);
	case STAIR:
		return stair_gradient(x);
	case HARDTAN:
		return hardtan_gradient(x);
	case LHTAN:
		return lhtan_gradient(x);
	}
	return 0;
}
static inline void gradient_array(const float *x, const int n, const ACTIVATION a, float *delta)
{
	int i = 0;
	for (;;) {
		if (i >= n)break;
		delta[i] *= gradient(x[i], a);
		i++;
	}
}
static void forward_convolutional_layer(convolutional_layer l, network_state state)
{
	int out_h = convolutional_out_height(l);
	int out_w = convolutional_out_width(l);
	int i, bx, j;
	int m, k, n;
	float *a, *b, *c;

	fill_cpu(l.outputs*l.batch, 0, l.output, 1);

	if (l.xnor) {
		binarize_weights(l.weights, l.n, l.c*l.size*l.size, l.binary_weights);
		swap_binary(&l);
		binarize_cpu(state.input, l.c*l.h*l.w*l.batch, l.binary_input);
		state.input = l.binary_input;
	}

	m = l.n;
	k = l.size*l.size*l.c;
	n = out_h * out_w;


	a = l.weights;
	b = state.workspace;
	c = l.output;

	i = 0;

	for (;;) {
		if (i >= l.batch)break;

		im2col_cpu(state.input, l.c, l.h, l.w,
			l.size, l.stride, l.pad, b);
		gemm(0, 0, m, n, k, 1, a, k, b, n, 1, c, n);
		c += n * m;
		state.input += l.c*l.h*l.w;

		i++;
	}

	if (l.batch_normalize) {
		forward_batchnorm_layer(l, state);
	}

	bx = 0;

	for (;;) {
		if (bx >= l.batch) break;
		for (i = 0; i < l.n; ++i) {
			for (j = 0; j < n; ++j) {
				l.output[(bx*n + i)*n + j] += l.biases[i];
			}
		}
		bx++;
	}
	i = 0;
	/* 	activate_array(l.output, m*n*l.batch, l.activation); */

	for (;;) {
		if (i >= m * n*l.batch)break;
		l.output[i] = activate(l.output[i], l.activation);
		i++;
	}
	if (l.binary || l.xnor) swap_binary(&l);
}
static void backward_bias(float *bias_updates, float *delta, int batch, int n, int size)
{
	int i, b;
	for (b = 0; b < batch; ++b) {
		for (i = 0; i < n; ++i) {
			bias_updates[i] += sum_array(delta + size * (i + b * n), size);
		}
	}
}
static void backward_scale_cpu(float *x_norm, float *delta, int batch, int n, int size, float *scale_updates)
{
	int i, b, f;
	for (f = 0; f < n; ++f) {
		float sum = 0;
		for (b = 0; b < batch; ++b) {
			for (i = 0; i < size; ++i) {
				int index = i + size * (f + n * b);
				sum += delta[index] * x_norm[index];
			}
		}
		scale_updates[f] += sum;
	}
}
static void mean_delta_cpu(float *delta, float *variance, int batch, int filters, int spatial, float *mean_delta)
{

	int i, j, k;
	for (i = 0; i < filters; ++i) {
		mean_delta[i] = 0;
		for (j = 0; j < batch; ++j) {
			for (k = 0; k < spatial; ++k) {
				int index = j * filters*spatial + i * spatial + k;
				mean_delta[i] += delta[index];
			}
		}
		mean_delta[i] *= (-1. / sqrt(variance[i] + .00001f));
	}
}
static void  variance_delta_cpu(float *x, float *delta, float *mean, float *variance, int batch, int filters, int spatial, float *variance_delta)
{

	int i, j, k;
	for (i = 0; i < filters; ++i) {
		variance_delta[i] = 0;
		for (j = 0; j < batch; ++j) {
			for (k = 0; k < spatial; ++k) {
				int index = j * filters*spatial + i * spatial + k;
				variance_delta[i] += delta[index] * (x[index] - mean[i]);
			}
		}
		variance_delta[i] *= -.5 * pow(variance[i] + .00001f, (float)(-3. / 2.));
	}
}
static void normalize_delta_cpu(float *x, float *mean, float *variance, float *mean_delta, float *variance_delta, int batch, int filters, int spatial, float *delta)
{
	int f, j, k;
	for (j = 0; j < batch; ++j) {
		for (f = 0; f < filters; ++f) {
			for (k = 0; k < spatial; ++k) {
				int index = j * filters*spatial + f * spatial + k;
				delta[index] = delta[index] * 1. / (sqrt(variance[f]) + .00001f) + variance_delta[f] * 2. * (x[index] - mean[f]) / (spatial * batch) + mean_delta[f] / (spatial*batch);
			}
		}
	}
}
static void backward_batchnorm_layer(const layer l, network_state state)
{
	backward_scale_cpu(l.x_norm, l.delta, l.batch, l.out_c, l.out_w*l.out_h, l.scale_updates);

	scale_bias(l.delta, l.scales, l.batch, l.out_c, l.out_h*l.out_w);

	mean_delta_cpu(l.delta, l.variance, l.batch, l.out_c, l.out_w*l.out_h, l.mean_delta);
	variance_delta_cpu(l.x, l.delta, l.mean, l.variance, l.batch, l.out_c, l.out_w*l.out_h, l.variance_delta);
	normalize_delta_cpu(l.x, l.mean, l.variance, l.mean_delta, l.variance_delta, l.batch, l.out_c, l.out_w*l.out_h, l.delta);
	if (l.type == BATCHNORM) copy_cpu(l.outputs*l.batch, l.delta, 1, state.delta, 1);
}
static void backward_convolutional_layer(convolutional_layer l, network_state state)
{
	int i;
	int m = l.n;
	int n = l.size*l.size*l.c;
	int k = convolutional_out_height(l)*
		convolutional_out_width(l);

	gradient_array(l.output, m*k*l.batch, l.activation, l.delta);
	backward_bias(l.bias_updates, l.delta, l.batch, l.n, k);

	if (l.batch_normalize) {
		backward_batchnorm_layer(l, state);
	}

	for (i = 0; i < l.batch; ++i) {
		float *a = l.delta + i * m*k;
		float *b = state.workspace;
		float *c = l.weight_updates;

		float *im = state.input + i * l.c*l.h*l.w;

		im2col_cpu(im, l.c, l.h, l.w,
			l.size, l.stride, l.pad, b);
		gemm(0, 1, m, n, k, 1, a, k, b, k, 1, c, n);

		if (state.delta) {
			a = l.weights;
			b = l.delta + i * m*k;
			c = state.workspace;

			gemm(1, 0, n, k, m, 1, a, n, b, k, 0, c, k);

			col2im_cpu(state.workspace, l.c, l.h, l.w, l.size, l.stride, l.pad, state.delta + i * l.c*l.h*l.w);
		}
	}
}
static void update_convolutional_layer(convolutional_layer l, int batch, float learning_rate, float momentum, float decay)
{
	int size = l.size*l.size*l.c*l.n;
	axpy_cpu(l.n, learning_rate / batch, l.bias_updates, 1, l.biases, 1);
	scal_cpu(l.n, momentum, l.bias_updates, 1);

	if (l.scales) {
		axpy_cpu(l.n, learning_rate / batch, l.scale_updates, 1, l.scales, 1);
		scal_cpu(l.n, momentum, l.scale_updates, 1);
	}

	axpy_cpu(size, -decay * batch, l.weights, 1, l.weight_updates, 1);
	axpy_cpu(size, learning_rate / batch, l.weight_updates, 1, l.weights, 1);
	scal_cpu(size, momentum, l.weight_updates, 1);
}
static size_t get_workspace_size(layer l) {
	return (size_t)l.out_h*l.out_w*l.size*l.size*l.c * sizeof(float);
}
static convolutional_layer make_convolutional_layer(int batch, int h, int w, int c, int n, int size, int stride, int padding, ACTIVATION activation, int batch_normalize, int binary, int xnor, int adam)
{
	int i;
	convolutional_layer l = { 0 };
	l.type = CONVOLUTIONAL;

	l.h = h;
	l.w = w;
	l.c = c;
	l.n = n;
	l.binary = binary;
	l.xnor = xnor;
	l.batch = batch;
	l.stride = stride;
	l.size = size;
	l.pad = padding;
	l.batch_normalize = batch_normalize;

	l.weights = calloc(c*n*size*size, sizeof(float));
	l.weight_updates = calloc(c*n*size*size, sizeof(float));

	l.biases = calloc(n, sizeof(float));
	l.bias_updates = calloc(n, sizeof(float));

	/* float scale = 1./sqrt(size*size*c); */
	float scale = sqrt(2. / (size*size*c));
	for (i = 0; i < c*n*size*size; ++i) l.weights[i] = scale * rand_uniform(-1, 1);
	int out_h = convolutional_out_height(l);
	int out_w = convolutional_out_width(l);
	l.out_h = out_h;
	l.out_w = out_w;
	l.out_c = n;
	l.outputs = l.out_h * l.out_w * l.out_c;
	l.inputs = l.w * l.h * l.c;

	l.output = calloc(l.batch*l.outputs, sizeof(float));
	l.delta = calloc(l.batch*l.outputs, sizeof(float));

	l.forward = forward_convolutional_layer;
	l.backward = backward_convolutional_layer;
	l.update = update_convolutional_layer;
	if (binary) {
		l.binary_weights = calloc(c*n*size*size, sizeof(float));
		l.cweights = calloc(c*n*size*size, sizeof(char));
		l.scales = calloc(n, sizeof(float));
	}
	if (xnor) {
		l.binary_weights = calloc(c*n*size*size, sizeof(float));
		l.binary_input = calloc(l.inputs*l.batch, sizeof(float));
	}

	if (batch_normalize) {
		l.scales = calloc(n, sizeof(float));
		l.scale_updates = calloc(n, sizeof(float));
		for (i = 0; i < n; ++i) {
			l.scales[i] = 1;
		}

		l.mean = calloc(n, sizeof(float));
		l.variance = calloc(n, sizeof(float));

		l.mean_delta = calloc(n, sizeof(float));
		l.variance_delta = calloc(n, sizeof(float));

		l.rolling_mean = calloc(n, sizeof(float));
		l.rolling_variance = calloc(n, sizeof(float));
		l.x = calloc(l.batch*l.outputs, sizeof(float));
		l.x_norm = calloc(l.batch*l.outputs, sizeof(float));
	}
	if (adam) {
		l.adam = 1;
		l.m = calloc(c*n*size*size, sizeof(float));
		l.v = calloc(c*n*size*size, sizeof(float));
	}

#if 0 /* SOD_GPU */
	l.forward_gpu = forward_convolutional_layer_gpu;
	l.backward_gpu = backward_convolutional_layer_gpu;
	l.update_gpu = update_convolutional_layer_gpu;

	if (gpu_index >= 0) {
		if (adam) {
			l.m_gpu = cuda_make_array(l.m, c*n*size*size);
			l.v_gpu = cuda_make_array(l.v, c*n*size*size);
		}

		l.weights_gpu = cuda_make_array(l.weights, c*n*size*size);
		l.weight_updates_gpu = cuda_make_array(l.weight_updates, c*n*size*size);

		l.biases_gpu = cuda_make_array(l.biases, n);
		l.bias_updates_gpu = cuda_make_array(l.bias_updates, n);

		l.delta_gpu = cuda_make_array(l.delta, l.batch*out_h*out_w*n);
		l.output_gpu = cuda_make_array(l.output, l.batch*out_h*out_w*n);

		if (binary) {
			l.binary_weights_gpu = cuda_make_array(l.weights, c*n*size*size);
		}
		if (xnor) {
			l.binary_weights_gpu = cuda_make_array(l.weights, c*n*size*size);
			l.binary_input_gpu = cuda_make_array(0, l.inputs*l.batch);
		}

		if (batch_normalize) {
			l.mean_gpu = cuda_make_array(l.mean, n);
			l.variance_gpu = cuda_make_array(l.variance, n);

			l.rolling_mean_gpu = cuda_make_array(l.mean, n);
			l.rolling_variance_gpu = cuda_make_array(l.variance, n);

			l.mean_delta_gpu = cuda_make_array(l.mean, n);
			l.variance_delta_gpu = cuda_make_array(l.variance, n);

			l.scales_gpu = cuda_make_array(l.scales, n);
			l.scale_updates_gpu = cuda_make_array(l.scale_updates, n);

			l.x_gpu = cuda_make_array(l.output, l.batch*out_h*out_w*n);
			l.x_norm_gpu = cuda_make_array(l.output, l.batch*out_h*out_w*n);
		}
#ifdef CUDNN
		cudnnCreateTensorDescriptor(&l.srcTensorDesc);
		cudnnCreateTensorDescriptor(&l.dstTensorDesc);
		cudnnCreateFilterDescriptor(&l.weightDesc);
		cudnnCreateTensorDescriptor(&l.dsrcTensorDesc);
		cudnnCreateTensorDescriptor(&l.ddstTensorDesc);
		cudnnCreateFilterDescriptor(&l.dweightDesc);
		cudnnCreateConvolutionDescriptor(&l.convDesc);
		cudnn_convolutional_setup(&l);
#endif
	}
#endif
	l.workspace_size = get_workspace_size(l);
	l.activation = activation;
	return l;
}
static int parse_convolutional(convolutional_layer *l, list *options, size_params params, sod_cnn *pNet)
{
	int n = option_find_int(options, "filters", 1);
	int size = option_find_int(options, "size", 1);
	int stride = option_find_int(options, "stride", 1);
	int pad = option_find_int(options, "pad", 0);
	int padding = option_find_int(options, "padding", 0);
	if (pad) padding = size / 2;

	char *activation_s = option_find_str(options, "activation", "logistic");
	ACTIVATION activation = get_activation(activation_s);

	int batch, h, w, c;
	h = params.h;
	w = params.w;
	c = params.c;
	batch = params.batch;
	if (!(h && w && c)) {
		pNet->zErr = "before convolutional, layer must output an image.";
		pNet->nErr++;
		return -1;
	}
	int batch_normalize = option_find_int(options, "batch_normalize", 0);
	int binary = option_find_int(options, "binary", 0);
	int xnor = option_find_int(options, "xnor", 0);

	*l = make_convolutional_layer(batch, h, w, c, n, size, stride, padding, activation, batch_normalize, binary, xnor, params.net.adam);
	l->flipped = option_find_int(options, "flipped", 0);
	l->dot = option_find_float(options, "dot", 0);
	if (params.net.adam) {
		l->B1 = params.net.B1;
		l->B2 = params.net.B2;
		l->eps = params.net.eps;
	}

	return SOD_OK;
}
/* =============================================================== LOCAL =============================================================== */
typedef layer local_layer;

static int local_out_height(local_layer l)
{
	int h = l.h;
	if (!l.pad) h -= l.size;
	else h -= 1;
	return h / l.stride + 1;
}
static int local_out_width(local_layer l)
{
	int w = l.w;
	if (!l.pad) w -= l.size;
	else w -= 1;
	return w / l.stride + 1;
}
static void forward_local_layer(const local_layer l, network_state state)
{
	int out_h = local_out_height(l);
	int out_w = local_out_width(l);
	int i, j;
	int locations = out_h * out_w;

	for (i = 0; i < l.batch; ++i) {
		copy_cpu(l.outputs, l.biases, 1, l.output + i * l.outputs, 1);
	}

	for (i = 0; i < l.batch; ++i) {
		float *input = state.input + i * l.w*l.h*l.c;
		im2col_cpu(input, l.c, l.h, l.w,
			l.size, l.stride, l.pad, l.col_image);
		float *output = l.output + i * l.outputs;
		for (j = 0; j < locations; ++j) {
			float *a = l.weights + j * l.size*l.size*l.c*l.n;
			float *b = l.col_image + j;
			float *c = output + j;

			int m = l.n;
			int n = 1;
			int k = l.size*l.size*l.c;

			gemm(0, 0, m, n, k, 1, a, k, b, locations, 1, c, locations);
		}
	}
	activate_array(l.output, l.outputs*l.batch, l.activation);
}
static void backward_local_layer(local_layer l, network_state state)
{
	int i, j;
	int locations = l.out_w*l.out_h;

	gradient_array(l.output, l.outputs*l.batch, l.activation, l.delta);

	for (i = 0; i < l.batch; ++i) {
		axpy_cpu(l.outputs, 1, l.delta + i * l.outputs, 1, l.bias_updates, 1);
	}

	for (i = 0; i < l.batch; ++i) {
		float *input = state.input + i * l.w*l.h*l.c;
		im2col_cpu(input, l.c, l.h, l.w,
			l.size, l.stride, l.pad, l.col_image);

		for (j = 0; j < locations; ++j) {
			float *a = l.delta + i * l.outputs + j;
			float *b = l.col_image + j;
			float *c = l.weight_updates + j * l.size*l.size*l.c*l.n;
			int m = l.n;
			int n = l.size*l.size*l.c;
			int k = 1;

			gemm(0, 1, m, n, k, 1, a, locations, b, locations, 1, c, n);
		}

		if (state.delta) {
			for (j = 0; j < locations; ++j) {
				float *a = l.weights + j * l.size*l.size*l.c*l.n;
				float *b = l.delta + i * l.outputs + j;
				float *c = l.col_image + j;

				int m = l.size*l.size*l.c;
				int n = 1;
				int k = l.n;

				gemm(1, 0, m, n, k, 1, a, m, b, locations, 0, c, locations);
			}

			col2im_cpu(l.col_image, l.c, l.h, l.w, l.size, l.stride, l.pad, state.delta + i * l.c*l.h*l.w);
		}
	}
}
static void update_local_layer(local_layer l, int batch, float learning_rate, float momentum, float decay)
{
	int locations = l.out_w*l.out_h;
	int size = l.size*l.size*l.c*l.n*locations;
	axpy_cpu(l.outputs, learning_rate / batch, l.bias_updates, 1, l.biases, 1);
	scal_cpu(l.outputs, momentum, l.bias_updates, 1);

	axpy_cpu(size, -decay * batch, l.weights, 1, l.weight_updates, 1);
	axpy_cpu(size, learning_rate / batch, l.weight_updates, 1, l.weights, 1);
	scal_cpu(size, momentum, l.weight_updates, 1);
}
#if 0 /* SOD_GPU */
static void forward_local_layer_gpu(const local_layer l, network_state state)
{
	int out_h = local_out_height(l);
	int out_w = local_out_width(l);
	int i, j;
	int locations = out_h * out_w;

	for (i = 0; i < l.batch; ++i) {
		copy_ongpu(l.outputs, l.biases_gpu, 1, l.output_gpu + i * l.outputs, 1);
	}

	for (i = 0; i < l.batch; ++i) {
		float *input = state.input + i * l.w*l.h*l.c;
		im2col_ongpu(input, l.c, l.h, l.w,
			l.size, l.stride, l.pad, l.col_image_gpu);
		float *output = l.output_gpu + i * l.outputs;
		for (j = 0; j < locations; ++j) {
			float *a = l.weights_gpu + j * l.size*l.size*l.c*l.n;
			float *b = l.col_image_gpu + j;
			float *c = output + j;

			int m = l.n;
			int n = 1;
			int k = l.size*l.size*l.c;

			gemm_ongpu(0, 0, m, n, k, 1, a, k, b, locations, 1, c, locations);
		}
	}
	activate_array_ongpu(l.output_gpu, l.outputs*l.batch, l.activation);
}
static void backward_local_layer_gpu(local_layer l, network_state state)
{
	int i, j;
	int locations = l.out_w*l.out_h;

	gradient_array_ongpu(l.output_gpu, l.outputs*l.batch, l.activation, l.delta_gpu);
	for (i = 0; i < l.batch; ++i) {
		axpy_ongpu(l.outputs, 1, l.delta_gpu + i * l.outputs, 1, l.bias_updates_gpu, 1);
	}

	for (i = 0; i < l.batch; ++i) {
		float *input = state.input + i * l.w*l.h*l.c;
		im2col_ongpu(input, l.c, l.h, l.w,
			l.size, l.stride, l.pad, l.col_image_gpu);

		for (j = 0; j < locations; ++j) {
			float *a = l.delta_gpu + i * l.outputs + j;
			float *b = l.col_image_gpu + j;
			float *c = l.weight_updates_gpu + j * l.size*l.size*l.c*l.n;
			int m = l.n;
			int n = l.size*l.size*l.c;
			int k = 1;

			gemm_ongpu(0, 1, m, n, k, 1, a, locations, b, locations, 1, c, n);
		}

		if (state.delta) {
			for (j = 0; j < locations; ++j) {
				float *a = l.weights_gpu + j * l.size*l.size*l.c*l.n;
				float *b = l.delta_gpu + i * l.outputs + j;
				float *c = l.col_image_gpu + j;

				int m = l.size*l.size*l.c;
				int n = 1;
				int k = l.n;

				gemm_ongpu(1, 0, m, n, k, 1, a, m, b, locations, 0, c, locations);
			}

			col2im_ongpu(l.col_image_gpu, l.c, l.h, l.w, l.size, l.stride, l.pad, state.delta + i * l.c*l.h*l.w);
		}
	}
}
static void update_local_layer_gpu(local_layer l, int batch, float learning_rate, float momentum, float decay)
{
	int locations = l.out_w*l.out_h;
	int size = l.size*l.size*l.c*l.n*locations;
	axpy_ongpu(l.outputs, learning_rate / batch, l.bias_updates_gpu, 1, l.biases_gpu, 1);
	scal_ongpu(l.outputs, momentum, l.bias_updates_gpu, 1);

	axpy_ongpu(size, -decay * batch, l.weights_gpu, 1, l.weight_updates_gpu, 1);
	axpy_ongpu(size, learning_rate / batch, l.weight_updates_gpu, 1, l.weights_gpu, 1);
	scal_ongpu(size, momentum, l.weight_updates_gpu, 1);
}
static void pull_local_layer(local_layer l)
{
	int locations = l.out_w*l.out_h;
	int size = l.size*l.size*l.c*l.n*locations;
	cuda_pull_array(l.weights_gpu, l.weights, size);
	cuda_pull_array(l.biases_gpu, l.biases, l.outputs);
}
static void push_local_layer(local_layer l)
{
	int locations = l.out_w*l.out_h;
	int size = l.size*l.size*l.c*l.n*locations;
	cuda_push_array(l.weights_gpu, l.weights, size);
	cuda_push_array(l.biases_gpu, l.biases, l.outputs);
}
#endif
static local_layer make_local_layer(int batch, int h, int w, int c, int n, int size, int stride, int pad, ACTIVATION activation)
{
	int i;
	local_layer l = { 0 };
	l.type = LOCAL;

	l.h = h;
	l.w = w;
	l.c = c;
	l.n = n;
	l.batch = batch;
	l.stride = stride;
	l.size = size;
	l.pad = pad;

	int out_h = local_out_height(l);
	int out_w = local_out_width(l);
	int locations = out_h * out_w;
	l.out_h = out_h;
	l.out_w = out_w;
	l.out_c = n;
	l.outputs = l.out_h * l.out_w * l.out_c;
	l.inputs = l.w * l.h * l.c;

	l.weights = calloc(c*n*size*size*locations, sizeof(float));
	l.weight_updates = calloc(c*n*size*size*locations, sizeof(float));

	l.biases = calloc(l.outputs, sizeof(float));
	l.bias_updates = calloc(l.outputs, sizeof(float));

	/* float scale = 1./sqrt(size*size*c); */
	float scale = sqrt(2. / (size*size*c));
	for (i = 0; i < c*n*size*size; ++i) l.weights[i] = scale * rand_uniform(-1, 1);

	l.col_image = calloc(out_h*out_w*size*size*c, sizeof(float));
	l.output = calloc(l.batch*out_h * out_w * n, sizeof(float));
	l.delta = calloc(l.batch*out_h * out_w * n, sizeof(float));

	l.forward = forward_local_layer;
	l.backward = backward_local_layer;
	l.update = update_local_layer;

#if 0 /* SOD_GPU */
	l.forward_gpu = forward_local_layer_gpu;
	l.backward_gpu = backward_local_layer_gpu;
	l.update_gpu = update_local_layer_gpu;

	l.weights_gpu = cuda_make_array(l.weights, c*n*size*size*locations);
	l.weight_updates_gpu = cuda_make_array(l.weight_updates, c*n*size*size*locations);

	l.biases_gpu = cuda_make_array(l.biases, l.outputs);
	l.bias_updates_gpu = cuda_make_array(l.bias_updates, l.outputs);

	l.col_image_gpu = cuda_make_array(l.col_image, out_h*out_w*size*size*c);
	l.delta_gpu = cuda_make_array(l.delta, l.batch*out_h*out_w*n);
	l.output_gpu = cuda_make_array(l.output, l.batch*out_h*out_w*n);

#endif
	l.activation = activation;
	return l;
}
static int parse_local(local_layer *l, list *options, size_params params, sod_cnn *pNet)
{
	int n = option_find_int(options, "filters", 1);
	int size = option_find_int(options, "size", 1);
	int stride = option_find_int(options, "stride", 1);
	int pad = option_find_int(options, "pad", 0);
	char *activation_s = option_find_str(options, "activation", "logistic");
	ACTIVATION activation = get_activation(activation_s);

	int batch, h, w, c;
	h = params.h;
	w = params.w;
	c = params.c;
	batch = params.batch;
	if (!(h && w && c)) {
		pNet->zErr = "Layer before local layer must output image.";
		pNet->nErr++;
		return -1;
	}

	*l = make_local_layer(batch, h, w, c, n, size, stride, pad, activation);
	return SOD_OK;
}
/* =============================================================== Activation =============================================================== */
static void forward_activation_layer(layer l, network_state state)
{
	copy_cpu(l.outputs*l.batch, state.input, 1, l.output, 1);
	activate_array(l.output, l.outputs*l.batch, l.activation);
}
static void backward_activation_layer(layer l, network_state state)
{
	gradient_array(l.output, l.outputs*l.batch, l.activation, l.delta);
	copy_cpu(l.outputs*l.batch, l.delta, 1, state.delta, 1);
}

#if 0 /* SOD_GPU */

static void forward_activation_layer_gpu(layer l, network_state state)
{
	copy_ongpu(l.outputs*l.batch, state.input, 1, l.output_gpu, 1);
	activate_array_ongpu(l.output_gpu, l.outputs*l.batch, l.activation);
}
static void backward_activation_layer_gpu(layer l, network_state state)
{
	gradient_array_ongpu(l.output_gpu, l.outputs*l.batch, l.activation, l.delta_gpu);
	copy_ongpu(l.outputs*l.batch, l.delta_gpu, 1, state.delta, 1);
}
#endif
static layer make_activation_layer(int batch, int inputs, ACTIVATION activation)
{
	layer l = { 0 };
	l.type = ACTIVE;

	l.inputs = inputs;
	l.outputs = inputs;
	l.batch = batch;

	l.output = calloc(batch*inputs, sizeof(float));
	l.delta = calloc(batch*inputs, sizeof(float));

	l.forward = forward_activation_layer;
	l.backward = backward_activation_layer;
#if 0 /* SOD_GPU */
	l.forward_gpu = forward_activation_layer_gpu;
	l.backward_gpu = backward_activation_layer_gpu;

	l.output_gpu = cuda_make_array(l.output, inputs*batch);
	l.delta_gpu = cuda_make_array(l.delta, inputs*batch);
#endif
	l.activation = activation;
	return l;
}
static int parse_activation(layer *la, list *options, size_params params)
{
	char *activation_s = option_find_str(options, "activation", "linear");
	ACTIVATION activation = get_activation(activation_s);

	*la = make_activation_layer(params.batch, params.inputs, activation);

	la->out_h = params.h;
	la->out_w = params.w;
	la->out_c = params.c;
	la->h = params.h;
	la->w = params.w;
	la->c = params.c;
	return SOD_OK;
}
/* ============================== Connected Layer ==============================*/
typedef layer connected_layer;
static void update_connected_layer(connected_layer l, int batch, float learning_rate, float momentum, float decay)
{
	axpy_cpu(l.outputs, learning_rate / batch, l.bias_updates, 1, l.biases, 1);
	scal_cpu(l.outputs, momentum, l.bias_updates, 1);

	if (l.batch_normalize) {
		axpy_cpu(l.outputs, learning_rate / batch, l.scale_updates, 1, l.scales, 1);
		scal_cpu(l.outputs, momentum, l.scale_updates, 1);
	}

	axpy_cpu(l.inputs*l.outputs, -decay * batch, l.weights, 1, l.weight_updates, 1);
	axpy_cpu(l.inputs*l.outputs, learning_rate / batch, l.weight_updates, 1, l.weights, 1);
	scal_cpu(l.inputs*l.outputs, momentum, l.weight_updates, 1);
}
static void forward_connected_layer(connected_layer l, network_state state)
{
	int i;
	fill_cpu(l.outputs*l.batch, 0, l.output, 1);
	int m = l.batch;
	int k = l.inputs;
	int n = l.outputs;
	float *a = state.input;
	float *b = l.weights;
	float *c = l.output;
	gemm(0, 1, m, n, k, 1, a, k, b, k, 1, c, n);
	if (l.batch_normalize) {
		if (state.train) {
			mean_cpu(l.output, l.batch, l.outputs, 1, l.mean);
			variance_cpu(l.output, l.mean, l.batch, l.outputs, 1, l.variance);

			scal_cpu(l.outputs, .95, l.rolling_mean, 1);
			axpy_cpu(l.outputs, .05, l.mean, 1, l.rolling_mean, 1);
			scal_cpu(l.outputs, .95, l.rolling_variance, 1);
			axpy_cpu(l.outputs, .05, l.variance, 1, l.rolling_variance, 1);

			copy_cpu(l.outputs*l.batch, l.output, 1, l.x, 1);
			normalize_cpu(l.output, l.mean, l.variance, l.batch, l.outputs, 1);
			copy_cpu(l.outputs*l.batch, l.output, 1, l.x_norm, 1);
		}
		else {
			normalize_cpu(l.output, l.rolling_mean, l.rolling_variance, l.batch, l.outputs, 1);
		}
		scale_bias(l.output, l.scales, l.batch, l.outputs, 1);
	}
	for (i = 0; i < l.batch; ++i) {
		axpy_cpu(l.outputs, 1, l.biases, 1, l.output + i * l.outputs, 1);
	}
	activate_array(l.output, l.outputs*l.batch, l.activation);
}
static void backward_connected_layer(connected_layer l, network_state state)
{
	int i;
	gradient_array(l.output, l.outputs*l.batch, l.activation, l.delta);
	for (i = 0; i < l.batch; ++i) {
		axpy_cpu(l.outputs, 1, l.delta + i * l.outputs, 1, l.bias_updates, 1);
	}
	if (l.batch_normalize) {
		backward_scale_cpu(l.x_norm, l.delta, l.batch, l.outputs, 1, l.scale_updates);

		scale_bias(l.delta, l.scales, l.batch, l.outputs, 1);

		mean_delta_cpu(l.delta, l.variance, l.batch, l.outputs, 1, l.mean_delta);
		variance_delta_cpu(l.x, l.delta, l.mean, l.variance, l.batch, l.outputs, 1, l.variance_delta);
		normalize_delta_cpu(l.x, l.mean, l.variance, l.mean_delta, l.variance_delta, l.batch, l.outputs, 1, l.delta);
	}

	int m = l.outputs;
	int k = l.batch;
	int n = l.inputs;
	float *a = l.delta;
	float *b = state.input;
	float *c = l.weight_updates;
	gemm(1, 0, m, n, k, 1, a, m, b, n, 1, c, n);

	m = l.batch;
	k = l.outputs;
	n = l.inputs;

	a = l.delta;
	b = l.weights;
	c = state.delta;

	if (c) gemm(0, 0, m, n, k, 1, a, k, b, n, 1, c, n);
}
static connected_layer make_connected_layer(int batch, int inputs, int outputs, ACTIVATION activation, int batch_normalize)
{
	int i;
	connected_layer l = { 0 };
	l.type = CONNECTED;

	l.inputs = inputs;
	l.outputs = outputs;
	l.batch = batch;
	l.batch_normalize = batch_normalize;
	l.h = 1;
	l.w = 1;
	l.c = inputs;
	l.out_h = 1;
	l.out_w = 1;
	l.out_c = outputs;

	l.output = calloc(batch*outputs, sizeof(float));
	l.delta = calloc(batch*outputs, sizeof(float));

	l.weight_updates = calloc(inputs*outputs, sizeof(float));
	l.bias_updates = calloc(outputs, sizeof(float));

	l.weights = calloc(outputs*inputs, sizeof(float));
	l.biases = calloc(outputs, sizeof(float));

	l.forward = forward_connected_layer;
	l.backward = backward_connected_layer;
	l.update = update_connected_layer;

	float scale = sqrt(2. / inputs);
	for (i = 0; i < outputs*inputs; ++i) {
		l.weights[i] = scale * rand_uniform(-1, 1);
	}

	for (i = 0; i < outputs; ++i) {
		l.biases[i] = 0;
	}

	if (batch_normalize) {
		l.scales = calloc(outputs, sizeof(float));
		l.scale_updates = calloc(outputs, sizeof(float));
		for (i = 0; i < outputs; ++i) {
			l.scales[i] = 1;
		}

		l.mean = calloc(outputs, sizeof(float));
		l.mean_delta = calloc(outputs, sizeof(float));
		l.variance = calloc(outputs, sizeof(float));
		l.variance_delta = calloc(outputs, sizeof(float));

		l.rolling_mean = calloc(outputs, sizeof(float));
		l.rolling_variance = calloc(outputs, sizeof(float));

		l.x = calloc(batch*outputs, sizeof(float));
		l.x_norm = calloc(batch*outputs, sizeof(float));
	}

#if 0 /* SOD_GPU */
	l.forward_gpu = forward_connected_layer_gpu;
	l.backward_gpu = backward_connected_layer_gpu;
	l.update_gpu = update_connected_layer_gpu;

	l.weights_gpu = cuda_make_array(l.weights, outputs*inputs);
	l.biases_gpu = cuda_make_array(l.biases, outputs);

	l.weight_updates_gpu = cuda_make_array(l.weight_updates, outputs*inputs);
	l.bias_updates_gpu = cuda_make_array(l.bias_updates, outputs);

	l.output_gpu = cuda_make_array(l.output, outputs*batch);
	l.delta_gpu = cuda_make_array(l.delta, outputs*batch);
	if (batch_normalize) {
		l.scales_gpu = cuda_make_array(l.scales, outputs);
		l.scale_updates_gpu = cuda_make_array(l.scale_updates, outputs);

		l.mean_gpu = cuda_make_array(l.mean, outputs);
		l.variance_gpu = cuda_make_array(l.variance, outputs);

		l.rolling_mean_gpu = cuda_make_array(l.mean, outputs);
		l.rolling_variance_gpu = cuda_make_array(l.variance, outputs);

		l.mean_delta_gpu = cuda_make_array(l.mean, outputs);
		l.variance_delta_gpu = cuda_make_array(l.variance, outputs);

		l.x_gpu = cuda_make_array(l.output, l.batch*outputs);
		l.x_norm_gpu = cuda_make_array(l.output, l.batch*outputs);
	}
#endif
	l.activation = activation;
	return l;
}
/* =============================================================== RNN =============================================================== */
static void increment_rnn_layer(layer *l, int steps)
{
	int num = l->outputs*l->batch*steps;
	l->output += num;
	l->delta += num;
	l->x += num;
	l->x_norm += num;

#if 0 /* SOD_GPU */
	l->output_gpu += num;
	l->delta_gpu += num;
	l->x_gpu += num;
	l->x_norm_gpu += num;
#endif
}
static void update_rnn_layer(layer l, int batch, float learning_rate, float momentum, float decay)
{
	update_connected_layer(*(l.input_layer), batch, learning_rate, momentum, decay);
	update_connected_layer(*(l.self_layer), batch, learning_rate, momentum, decay);
	update_connected_layer(*(l.output_layer), batch, learning_rate, momentum, decay);
}
static void forward_rnn_layer(layer l, network_state state)
{
	network_state s = { 0 };
	s.train = state.train;
	int i;
	layer input_layer = *(l.input_layer);
	layer self_layer = *(l.self_layer);
	layer output_layer = *(l.output_layer);

	fill_cpu(l.outputs * l.batch * l.steps, 0, output_layer.delta, 1);
	fill_cpu(l.hidden * l.batch * l.steps, 0, self_layer.delta, 1);
	fill_cpu(l.hidden * l.batch * l.steps, 0, input_layer.delta, 1);
	if (state.train) fill_cpu(l.hidden * l.batch, 0, l.state, 1);

	for (i = 0; i < l.steps; ++i) {
		s.input = state.input;
		forward_connected_layer(input_layer, s);

		s.input = l.state;
		forward_connected_layer(self_layer, s);

		float *old_state = l.state;
		if (state.train) l.state += l.hidden*l.batch;
		if (l.shortcut) {
			copy_cpu(l.hidden * l.batch, old_state, 1, l.state, 1);
		}
		else {
			fill_cpu(l.hidden * l.batch, 0, l.state, 1);
		}
		axpy_cpu(l.hidden * l.batch, 1, input_layer.output, 1, l.state, 1);
		axpy_cpu(l.hidden * l.batch, 1, self_layer.output, 1, l.state, 1);

		s.input = l.state;
		forward_connected_layer(output_layer, s);

		state.input += l.inputs*l.batch;
		increment_rnn_layer(&input_layer, 1);
		increment_rnn_layer(&self_layer, 1);
		increment_rnn_layer(&output_layer, 1);
	}
}
static void backward_rnn_layer(layer l, network_state state)
{
	network_state s = { 0 };
	s.train = state.train;
	int i;
	layer input_layer = *(l.input_layer);
	layer self_layer = *(l.self_layer);
	layer output_layer = *(l.output_layer);

	increment_rnn_layer(&input_layer, l.steps - 1);
	increment_rnn_layer(&self_layer, l.steps - 1);
	increment_rnn_layer(&output_layer, l.steps - 1);

	l.state += l.hidden*l.batch*l.steps;
	for (i = l.steps - 1; i >= 0; --i) {
		copy_cpu(l.hidden * l.batch, input_layer.output, 1, l.state, 1);
		axpy_cpu(l.hidden * l.batch, 1, self_layer.output, 1, l.state, 1);

		s.input = l.state;
		s.delta = self_layer.delta;
		backward_connected_layer(output_layer, s);

		l.state -= l.hidden*l.batch;
		/*
		if(i > 0){
		copy_cpu(l.hidden * l.batch, input_layer.output - l.hidden*l.batch, 1, l.state, 1);
		axpy_cpu(l.hidden * l.batch, 1, self_layer.output - l.hidden*l.batch, 1, l.state, 1);
		}else{
		fill_cpu(l.hidden * l.batch, 0, l.state, 1);
		}
		*/

		s.input = l.state;
		s.delta = self_layer.delta - l.hidden*l.batch;
		if (i == 0) s.delta = 0;
		backward_connected_layer(self_layer, s);

		copy_cpu(l.hidden*l.batch, self_layer.delta, 1, input_layer.delta, 1);
		if (i > 0 && l.shortcut) axpy_cpu(l.hidden*l.batch, 1, self_layer.delta, 1, self_layer.delta - l.hidden*l.batch, 1);
		s.input = state.input + i * l.inputs*l.batch;
		if (state.delta) s.delta = state.delta + i * l.inputs*l.batch;
		else s.delta = 0;
		backward_connected_layer(input_layer, s);

		increment_rnn_layer(&input_layer, -1);
		increment_rnn_layer(&self_layer, -1);
		increment_rnn_layer(&output_layer, -1);
	}
}
#if 0 /* SOD_GPU */

static void pull_rnn_layer(layer l)
{
	pull_connected_layer(*(l.input_layer));
	pull_connected_layer(*(l.self_layer));
	pull_connected_layer(*(l.output_layer));
}
static void push_rnn_layer(layer l)
{
	push_connected_layer(*(l.input_layer));
	push_connected_layer(*(l.self_layer));
	push_connected_layer(*(l.output_layer));
}
static void update_rnn_layer_gpu(layer l, int batch, float learning_rate, float momentum, float decay)
{
	update_connected_layer_gpu(*(l.input_layer), batch, learning_rate, momentum, decay);
	update_connected_layer_gpu(*(l.self_layer), batch, learning_rate, momentum, decay);
	update_connected_layer_gpu(*(l.output_layer), batch, learning_rate, momentum, decay);
}
static void forward_rnn_layer_gpu(layer l, network_state state)
{
	network_state s = { 0 };
	s.train = state.train;
	int i;
	layer input_layer = *(l.input_layer);
	layer self_layer = *(l.self_layer);
	layer output_layer = *(l.output_layer);

	fill_ongpu(l.outputs * l.batch * l.steps, 0, output_layer.delta_gpu, 1);
	fill_ongpu(l.hidden * l.batch * l.steps, 0, self_layer.delta_gpu, 1);
	fill_ongpu(l.hidden * l.batch * l.steps, 0, input_layer.delta_gpu, 1);
	if (state.train) fill_ongpu(l.hidden * l.batch, 0, l.state_gpu, 1);

	for (i = 0; i < l.steps; ++i) {
		s.input = state.input;
		forward_connected_layer_gpu(input_layer, s);

		s.input = l.state_gpu;
		forward_connected_layer_gpu(self_layer, s);

		float *old_state = l.state_gpu;
		if (state.train) l.state_gpu += l.hidden*l.batch;
		if (l.shortcut) {
			copy_ongpu(l.hidden * l.batch, old_state, 1, l.state_gpu, 1);
		}
		else {
			fill_ongpu(l.hidden * l.batch, 0, l.state_gpu, 1);
		}
		axpy_ongpu(l.hidden * l.batch, 1, input_layer.output_gpu, 1, l.state_gpu, 1);
		axpy_ongpu(l.hidden * l.batch, 1, self_layer.output_gpu, 1, l.state_gpu, 1);

		s.input = l.state_gpu;
		forward_connected_layer_gpu(output_layer, s);

		state.input += l.inputs*l.batch;
		increment_layer(&input_layer, 1);
		increment_layer(&self_layer, 1);
		increment_layer(&output_layer, 1);
	}
}
static void backward_rnn_layer_gpu(layer l, network_state state)
{
	network_state s = { 0 };
	s.train = state.train;
	int i;
	layer input_layer = *(l.input_layer);
	layer self_layer = *(l.self_layer);
	layer output_layer = *(l.output_layer);
	increment_layer(&input_layer, l.steps - 1);
	increment_layer(&self_layer, l.steps - 1);
	increment_layer(&output_layer, l.steps - 1);
	l.state_gpu += l.hidden*l.batch*l.steps;
	for (i = l.steps - 1; i >= 0; --i) {

		s.input = l.state_gpu;
		s.delta = self_layer.delta_gpu;
		backward_connected_layer_gpu(output_layer, s);

		l.state_gpu -= l.hidden*l.batch;

		copy_ongpu(l.hidden*l.batch, self_layer.delta_gpu, 1, input_layer.delta_gpu, 1);

		s.input = l.state_gpu;
		s.delta = self_layer.delta_gpu - l.hidden*l.batch;
		if (i == 0) s.delta = 0;
		backward_connected_layer_gpu(self_layer, s);

		//copy_ongpu(l.hidden*l.batch, self_layer.delta_gpu, 1, input_layer.delta_gpu, 1);
		if (i > 0 && l.shortcut) axpy_ongpu(l.hidden*l.batch, 1, self_layer.delta_gpu, 1, self_layer.delta_gpu - l.hidden*l.batch, 1);
		s.input = state.input + i * l.inputs*l.batch;
		if (state.delta) s.delta = state.delta + i * l.inputs*l.batch;
		else s.delta = 0;
		backward_connected_layer_gpu(input_layer, s);

		increment_layer(&input_layer, -1);
		increment_layer(&self_layer, -1);
		increment_layer(&output_layer, -1);
	}
}
#endif
static layer make_rnn_layer(int batch, int inputs, int hidden, int outputs, int steps, ACTIVATION activation, int batch_normalize, int log)
{
	batch = batch / steps;
	layer l = { 0 };
	l.batch = batch;
	l.type = RNN;
	l.steps = steps;
	l.hidden = hidden;
	l.inputs = inputs;

	l.state = calloc(batch*hidden*(steps + 1), sizeof(float));

	l.input_layer = malloc(sizeof(layer));

	*(l.input_layer) = make_connected_layer(batch*steps, inputs, hidden, activation, batch_normalize);
	l.input_layer->batch = batch;

	l.self_layer = malloc(sizeof(layer));

	*(l.self_layer) = make_connected_layer(batch*steps, hidden, hidden, (log == 2) ? LOGGY : (log == 1 ? LOGISTIC : activation), batch_normalize);
	l.self_layer->batch = batch;

	l.output_layer = malloc(sizeof(layer));

	*(l.output_layer) = make_connected_layer(batch*steps, hidden, outputs, activation, batch_normalize);
	l.output_layer->batch = batch;

	l.outputs = outputs;
	l.output = l.output_layer->output;
	l.delta = l.output_layer->delta;

	l.forward = forward_rnn_layer;
	l.backward = backward_rnn_layer;
	l.update = update_rnn_layer;
#if 0 /* SOD_GPU */
	l.forward_gpu = forward_rnn_layer_gpu;
	l.backward_gpu = backward_rnn_layer_gpu;
	l.update_gpu = update_rnn_layer_gpu;
	l.state_gpu = cuda_make_array(l.state, batch*hidden*(steps + 1));
	l.output_gpu = l.output_layer->output_gpu;
	l.delta_gpu = l.output_layer->delta_gpu;
#endif

	return l;
}
static int parse_rnn(layer *rl, list *options, size_params params)
{
	int output = option_find_int(options, "output", 1);
	int hidden = option_find_int(options, "hidden", 1);
	char *activation_s = option_find_str(options, "activation", "logistic");
	ACTIVATION activation = get_activation(activation_s);
	int batch_normalize = option_find_int(options, "batch_normalize", 0);
	int logistic = option_find_int(options, "logistic", 0);

	*rl = make_rnn_layer(params.batch, params.inputs, hidden, output, params.time_steps, activation, batch_normalize, logistic);

	rl->shortcut = option_find_int(options, "shortcut", 0);

	return SOD_OK;
}
/* =============================================================== GRU =============================================================== */
static void increment_layer_gru(layer *l, int steps)
{
	int num = l->outputs*l->batch*steps;
	l->output += num;
	l->delta += num;
	l->x += num;
	l->x_norm += num;

#if 0 /* SOD_GPU */
	l->output_gpu += num;
	l->delta_gpu += num;
	l->x_gpu += num;
	l->x_norm_gpu += num;
#endif
}
static void update_gru_layer(layer l, int batch, float learning_rate, float momentum, float decay)
{
	update_connected_layer(*(l.input_layer), batch, learning_rate, momentum, decay);
	update_connected_layer(*(l.self_layer), batch, learning_rate, momentum, decay);
	update_connected_layer(*(l.output_layer), batch, learning_rate, momentum, decay);
}
static void forward_gru_layer(layer l, network_state state)
{
	network_state s = { 0 };
	s.train = state.train;
	int i;
	layer input_z_layer = *(l.input_z_layer);
	layer input_r_layer = *(l.input_r_layer);
	layer input_h_layer = *(l.input_h_layer);

	layer state_z_layer = *(l.state_z_layer);
	layer state_r_layer = *(l.state_r_layer);
	layer state_h_layer = *(l.state_h_layer);

	fill_cpu(l.outputs * l.batch * l.steps, 0, input_z_layer.delta, 1);
	fill_cpu(l.outputs * l.batch * l.steps, 0, input_r_layer.delta, 1);
	fill_cpu(l.outputs * l.batch * l.steps, 0, input_h_layer.delta, 1);

	fill_cpu(l.outputs * l.batch * l.steps, 0, state_z_layer.delta, 1);
	fill_cpu(l.outputs * l.batch * l.steps, 0, state_r_layer.delta, 1);
	fill_cpu(l.outputs * l.batch * l.steps, 0, state_h_layer.delta, 1);
	if (state.train) {
		fill_cpu(l.outputs * l.batch * l.steps, 0, l.delta, 1);
		copy_cpu(l.outputs*l.batch, l.state, 1, l.prev_state, 1);
	}

	for (i = 0; i < l.steps; ++i) {
		s.input = l.state;
		forward_connected_layer(state_z_layer, s);
		forward_connected_layer(state_r_layer, s);

		s.input = state.input;
		forward_connected_layer(input_z_layer, s);
		forward_connected_layer(input_r_layer, s);
		forward_connected_layer(input_h_layer, s);


		copy_cpu(l.outputs*l.batch, input_z_layer.output, 1, l.z_cpu, 1);
		axpy_cpu(l.outputs*l.batch, 1, state_z_layer.output, 1, l.z_cpu, 1);

		copy_cpu(l.outputs*l.batch, input_r_layer.output, 1, l.r_cpu, 1);
		axpy_cpu(l.outputs*l.batch, 1, state_r_layer.output, 1, l.r_cpu, 1);

		activate_array(l.z_cpu, l.outputs*l.batch, LOGISTIC);
		activate_array(l.r_cpu, l.outputs*l.batch, LOGISTIC);

		copy_cpu(l.outputs*l.batch, l.state, 1, l.forgot_state, 1);
		mul_cpu(l.outputs*l.batch, l.r_cpu, 1, l.forgot_state, 1);

		s.input = l.forgot_state;
		forward_connected_layer(state_h_layer, s);

		copy_cpu(l.outputs*l.batch, input_h_layer.output, 1, l.h_cpu, 1);
		axpy_cpu(l.outputs*l.batch, 1, state_h_layer.output, 1, l.h_cpu, 1);

#ifdef SOD_USET
		activate_array(l.h_cpu, l.outputs*l.batch, TANH);
#else
		activate_array(l.h_cpu, l.outputs*l.batch, LOGISTIC);
#endif

		weighted_sum_cpu(l.state, l.h_cpu, l.z_cpu, l.outputs*l.batch, l.output);

		copy_cpu(l.outputs*l.batch, l.output, 1, l.state, 1);

		state.input += l.inputs*l.batch;
		l.output += l.outputs*l.batch;
		increment_layer_gru(&input_z_layer, 1);
		increment_layer_gru(&input_r_layer, 1);
		increment_layer_gru(&input_h_layer, 1);

		increment_layer_gru(&state_z_layer, 1);
		increment_layer_gru(&state_r_layer, 1);
		increment_layer_gru(&state_h_layer, 1);
	}
}
static void backward_gru_layer(layer l, network_state state) { (void)l; (void)state; /* shut up the compiler */ }

#if 0 /* SOD_GPU */

static void pull_gru_layer(layer l) {}
static void push_gru_layer(layer l) {}
static void update_gru_layer_gpu(layer l, int batch, float learning_rate, float momentum, float decay)
{
	update_connected_layer_gpu(*(l.input_r_layer), batch, learning_rate, momentum, decay);
	update_connected_layer_gpu(*(l.input_z_layer), batch, learning_rate, momentum, decay);
	update_connected_layer_gpu(*(l.input_h_layer), batch, learning_rate, momentum, decay);
	update_connected_layer_gpu(*(l.state_r_layer), batch, learning_rate, momentum, decay);
	update_connected_layer_gpu(*(l.state_z_layer), batch, learning_rate, momentum, decay);
	update_connected_layer_gpu(*(l.state_h_layer), batch, learning_rate, momentum, decay);
}
static void forward_gru_layer_gpu(layer l, network_state state)
{
	network_state s = { 0 };
	s.train = state.train;
	int i;
	layer input_z_layer = *(l.input_z_layer);
	layer input_r_layer = *(l.input_r_layer);
	layer input_h_layer = *(l.input_h_layer);

	layer state_z_layer = *(l.state_z_layer);
	layer state_r_layer = *(l.state_r_layer);
	layer state_h_layer = *(l.state_h_layer);

	fill_ongpu(l.outputs * l.batch * l.steps, 0, input_z_layer.delta_gpu, 1);
	fill_ongpu(l.outputs * l.batch * l.steps, 0, input_r_layer.delta_gpu, 1);
	fill_ongpu(l.outputs * l.batch * l.steps, 0, input_h_layer.delta_gpu, 1);

	fill_ongpu(l.outputs * l.batch * l.steps, 0, state_z_layer.delta_gpu, 1);
	fill_ongpu(l.outputs * l.batch * l.steps, 0, state_r_layer.delta_gpu, 1);
	fill_ongpu(l.outputs * l.batch * l.steps, 0, state_h_layer.delta_gpu, 1);
	if (state.train) {
		fill_ongpu(l.outputs * l.batch * l.steps, 0, l.delta_gpu, 1);
		copy_ongpu(l.outputs*l.batch, l.state_gpu, 1, l.prev_state_gpu, 1);
	}

	for (i = 0; i < l.steps; ++i) {
		s.input = l.state_gpu;
		forward_connected_layer_gpu(state_z_layer, s);
		forward_connected_layer_gpu(state_r_layer, s);

		s.input = state.input;
		forward_connected_layer_gpu(input_z_layer, s);
		forward_connected_layer_gpu(input_r_layer, s);
		forward_connected_layer_gpu(input_h_layer, s);


		copy_ongpu(l.outputs*l.batch, input_z_layer.output_gpu, 1, l.z_gpu, 1);
		axpy_ongpu(l.outputs*l.batch, 1, state_z_layer.output_gpu, 1, l.z_gpu, 1);

		copy_ongpu(l.outputs*l.batch, input_r_layer.output_gpu, 1, l.r_gpu, 1);
		axpy_ongpu(l.outputs*l.batch, 1, state_r_layer.output_gpu, 1, l.r_gpu, 1);

		activate_array_ongpu(l.z_gpu, l.outputs*l.batch, LOGISTIC);
		activate_array_ongpu(l.r_gpu, l.outputs*l.batch, LOGISTIC);

		copy_ongpu(l.outputs*l.batch, l.state_gpu, 1, l.forgot_state_gpu, 1);
		mul_ongpu(l.outputs*l.batch, l.r_gpu, 1, l.forgot_state_gpu, 1);

		s.input = l.forgot_state_gpu;
		forward_connected_layer_gpu(state_h_layer, s);

		copy_ongpu(l.outputs*l.batch, input_h_layer.output_gpu, 1, l.h_gpu, 1);
		axpy_ongpu(l.outputs*l.batch, 1, state_h_layer.output_gpu, 1, l.h_gpu, 1);

#ifdef SOD_USET
		activate_array_ongpu(l.h_gpu, l.outputs*l.batch, TANH);
#else
		activate_array_ongpu(l.h_gpu, l.outputs*l.batch, LOGISTIC);
#endif

		weighted_sum_gpu(l.state_gpu, l.h_gpu, l.z_gpu, l.outputs*l.batch, l.output_gpu);

		copy_ongpu(l.outputs*l.batch, l.output_gpu, 1, l.state_gpu, 1);

		state.input += l.inputs*l.batch;
		l.output_gpu += l.outputs*l.batch;
		increment_layer_gru(&input_z_layer, 1);
		increment_layer_gru(&input_r_layer, 1);
		increment_layer_gru(&input_h_layer, 1);

		increment_layer_gru(&state_z_layer, 1);
		increment_layer_gru(&state_r_layer, 1);
		increment_layer_gru(&state_h_layer, 1);
	}
}
static void backward_gru_layer_gpu(layer l, network_state state)
{
	network_state s = { 0 };
	s.train = state.train;
	int i;
	layer input_z_layer = *(l.input_z_layer);
	layer input_r_layer = *(l.input_r_layer);
	layer input_h_layer = *(l.input_h_layer);

	layer state_z_layer = *(l.state_z_layer);
	layer state_r_layer = *(l.state_r_layer);
	layer state_h_layer = *(l.state_h_layer);

	increment_layer_gru(&input_z_layer, l.steps - 1);
	increment_layer_gru(&input_r_layer, l.steps - 1);
	increment_layer_gru(&input_h_layer, l.steps - 1);

	increment_layer_gru(&state_z_layer, l.steps - 1);
	increment_layer_gru(&state_r_layer, l.steps - 1);
	increment_layer_gru(&state_h_layer, l.steps - 1);

	state.input += l.inputs*l.batch*(l.steps - 1);
	if (state.delta) state.delta += l.inputs*l.batch*(l.steps - 1);
	l.output_gpu += l.outputs*l.batch*(l.steps - 1);
	l.delta_gpu += l.outputs*l.batch*(l.steps - 1);
	for (i = l.steps - 1; i >= 0; --i) {
		if (i != 0) copy_ongpu(l.outputs*l.batch, l.output_gpu - l.outputs*l.batch, 1, l.prev_state_gpu, 1);
		float *prev_delta_gpu = (i == 0) ? 0 : l.delta_gpu - l.outputs*l.batch;

		copy_ongpu(l.outputs*l.batch, input_z_layer.output_gpu, 1, l.z_gpu, 1);
		axpy_ongpu(l.outputs*l.batch, 1, state_z_layer.output_gpu, 1, l.z_gpu, 1);

		copy_ongpu(l.outputs*l.batch, input_r_layer.output_gpu, 1, l.r_gpu, 1);
		axpy_ongpu(l.outputs*l.batch, 1, state_r_layer.output_gpu, 1, l.r_gpu, 1);

		activate_array_ongpu(l.z_gpu, l.outputs*l.batch, LOGISTIC);
		activate_array_ongpu(l.r_gpu, l.outputs*l.batch, LOGISTIC);

		copy_ongpu(l.outputs*l.batch, input_h_layer.output_gpu, 1, l.h_gpu, 1);
		axpy_ongpu(l.outputs*l.batch, 1, state_h_layer.output_gpu, 1, l.h_gpu, 1);

#ifdef SOD_USET
		activate_array_ongpu(l.h_gpu, l.outputs*l.batch, TANH);
#else
		activate_array_ongpu(l.h_gpu, l.outputs*l.batch, LOGISTIC);
#endif

		weighted_delta_gpu(l.prev_state_gpu, l.h_gpu, l.z_gpu, prev_delta_gpu, input_h_layer.delta_gpu, input_z_layer.delta_gpu, l.outputs*l.batch, l.delta_gpu);

#ifdef SOD_USET
		gradient_array_ongpu(l.h_gpu, l.outputs*l.batch, TANH, input_h_layer.delta_gpu);
#else
		gradient_array_ongpu(l.h_gpu, l.outputs*l.batch, LOGISTIC, input_h_layer.delta_gpu);
#endif

		copy_ongpu(l.outputs*l.batch, input_h_layer.delta_gpu, 1, state_h_layer.delta_gpu, 1);

		copy_ongpu(l.outputs*l.batch, l.prev_state_gpu, 1, l.forgot_state_gpu, 1);
		mul_ongpu(l.outputs*l.batch, l.r_gpu, 1, l.forgot_state_gpu, 1);
		fill_ongpu(l.outputs*l.batch, 0, l.forgot_delta_gpu, 1);

		s.input = l.forgot_state_gpu;
		s.delta = l.forgot_delta_gpu;

		backward_connected_layer_gpu(state_h_layer, s);
		if (prev_delta_gpu) mult_add_into_gpu(l.outputs*l.batch, l.forgot_delta_gpu, l.r_gpu, prev_delta_gpu);
		mult_add_into_gpu(l.outputs*l.batch, l.forgot_delta_gpu, l.prev_state_gpu, input_r_layer.delta_gpu);

		gradient_array_ongpu(l.r_gpu, l.outputs*l.batch, LOGISTIC, input_r_layer.delta_gpu);
		copy_ongpu(l.outputs*l.batch, input_r_layer.delta_gpu, 1, state_r_layer.delta_gpu, 1);

		gradient_array_ongpu(l.z_gpu, l.outputs*l.batch, LOGISTIC, input_z_layer.delta_gpu);
		copy_ongpu(l.outputs*l.batch, input_z_layer.delta_gpu, 1, state_z_layer.delta_gpu, 1);

		s.input = l.prev_state_gpu;
		s.delta = prev_delta_gpu;

		backward_connected_layer_gpu(state_r_layer, s);
		backward_connected_layer_gpu(state_z_layer, s);

		s.input = state.input;
		s.delta = state.delta;

		backward_connected_layer_gpu(input_h_layer, s);
		backward_connected_layer_gpu(input_r_layer, s);
		backward_connected_layer_gpu(input_z_layer, s);


		state.input -= l.inputs*l.batch;
		if (state.delta) state.delta -= l.inputs*l.batch;
		l.output_gpu -= l.outputs*l.batch;
		l.delta_gpu -= l.outputs*l.batch;
		increment_layer_gru(&input_z_layer, -1);
		increment_layer_gru(&input_r_layer, -1);
		increment_layer_gru(&input_h_layer, -1);

		increment_layer_gru(&state_z_layer, -1);
		increment_layer_gru(&state_r_layer, -1);
		increment_layer_gru(&state_h_layer, -1);
	}
}
#endif
static layer make_gru_layer(int batch, int inputs, int outputs, int steps, int batch_normalize)
{
	batch = batch / steps;
	layer l = { 0 };
	l.batch = batch;
	l.type = GRU;
	l.steps = steps;
	l.inputs = inputs;

	l.input_z_layer = malloc(sizeof(layer));

	*(l.input_z_layer) = make_connected_layer(batch*steps, inputs, outputs, LINEAR, batch_normalize);
	l.input_z_layer->batch = batch;

	l.state_z_layer = malloc(sizeof(layer));

	*(l.state_z_layer) = make_connected_layer(batch*steps, outputs, outputs, LINEAR, batch_normalize);
	l.state_z_layer->batch = batch;

	l.input_r_layer = malloc(sizeof(layer));

	*(l.input_r_layer) = make_connected_layer(batch*steps, inputs, outputs, LINEAR, batch_normalize);
	l.input_r_layer->batch = batch;

	l.state_r_layer = malloc(sizeof(layer));
	*(l.state_r_layer) = make_connected_layer(batch*steps, outputs, outputs, LINEAR, batch_normalize);
	l.state_r_layer->batch = batch;

	l.input_h_layer = malloc(sizeof(layer));

	*(l.input_h_layer) = make_connected_layer(batch*steps, inputs, outputs, LINEAR, batch_normalize);
	l.input_h_layer->batch = batch;

	l.state_h_layer = malloc(sizeof(layer));

	*(l.state_h_layer) = make_connected_layer(batch*steps, outputs, outputs, LINEAR, batch_normalize);
	l.state_h_layer->batch = batch;

	l.batch_normalize = batch_normalize;

	l.outputs = outputs;
	l.output = calloc(outputs*batch*steps, sizeof(float));
	l.delta = calloc(outputs*batch*steps, sizeof(float));
	l.state = calloc(outputs*batch, sizeof(float));
	l.prev_state = calloc(outputs*batch, sizeof(float));
	l.forgot_state = calloc(outputs*batch, sizeof(float));
	l.forgot_delta = calloc(outputs*batch, sizeof(float));

	l.r_cpu = calloc(outputs*batch, sizeof(float));
	l.z_cpu = calloc(outputs*batch, sizeof(float));
	l.h_cpu = calloc(outputs*batch, sizeof(float));

	l.forward = forward_gru_layer;
	l.backward = backward_gru_layer;
	l.update = update_gru_layer;

#if 0 /* SOD_GPU */
	l.forward_gpu = forward_gru_layer_gpu;
	l.backward_gpu = backward_gru_layer_gpu;
	l.update_gpu = update_gru_layer_gpu;

	l.forgot_state_gpu = cuda_make_array(l.output, batch*outputs);
	l.forgot_delta_gpu = cuda_make_array(l.output, batch*outputs);
	l.prev_state_gpu = cuda_make_array(l.output, batch*outputs);
	l.state_gpu = cuda_make_array(l.output, batch*outputs);
	l.output_gpu = cuda_make_array(l.output, batch*outputs*steps);
	l.delta_gpu = cuda_make_array(l.delta, batch*outputs*steps);
	l.r_gpu = cuda_make_array(l.output_gpu, batch*outputs);
	l.z_gpu = cuda_make_array(l.output_gpu, batch*outputs);
	l.h_gpu = cuda_make_array(l.output_gpu, batch*outputs);
#endif

	return l;
}
static int parse_gru(layer *l, list *options, size_params params)
{
	int output = option_find_int(options, "output", 1);
	int batch_normalize = option_find_int(options, "batch_normalize", 0);
	*l = make_gru_layer(params.batch, params.inputs, output, params.time_steps, batch_normalize);
	return SOD_OK;
}
/* =============================================================== CRNN =============================================================== */
static void increment_layer_crnn(layer *l, int steps)
{
	int num = l->outputs*l->batch*steps;
	l->output += num;
	l->delta += num;
	l->x += num;
	l->x_norm += num;

#if 0 /* SOD_GPU */
	l->output_gpu += num;
	l->delta_gpu += num;
	l->x_gpu += num;
	l->x_norm_gpu += num;
#endif
}
static void update_crnn_layer(layer l, int batch, float learning_rate, float momentum, float decay)
{
	update_convolutional_layer(*(l.input_layer), batch, learning_rate, momentum, decay);
	update_convolutional_layer(*(l.self_layer), batch, learning_rate, momentum, decay);
	update_convolutional_layer(*(l.output_layer), batch, learning_rate, momentum, decay);
}
static void forward_crnn_layer(layer l, network_state state)
{
	int i;
	network_state s = { 0 };
	layer input_layer = *(l.input_layer);
	layer self_layer = *(l.self_layer);
	layer output_layer = *(l.output_layer);
	s.train = state.train;
	fill_cpu(l.outputs * l.batch * l.steps, 0, output_layer.delta, 1);
	fill_cpu(l.hidden * l.batch * l.steps, 0, self_layer.delta, 1);
	fill_cpu(l.hidden * l.batch * l.steps, 0, input_layer.delta, 1);
	if (state.train) fill_cpu(l.hidden * l.batch, 0, l.state, 1);

	for (i = 0; i < l.steps; ++i) {
		float *old_state;
		s.input = state.input;
		forward_convolutional_layer(input_layer, s);

		s.input = l.state;
		forward_convolutional_layer(self_layer, s);

		old_state = l.state;
		if (state.train) l.state += l.hidden*l.batch;
		if (l.shortcut) {
			copy_cpu(l.hidden * l.batch, old_state, 1, l.state, 1);
		}
		else {
			fill_cpu(l.hidden * l.batch, 0, l.state, 1);
		}
		axpy_cpu(l.hidden * l.batch, 1, input_layer.output, 1, l.state, 1);
		axpy_cpu(l.hidden * l.batch, 1, self_layer.output, 1, l.state, 1);

		s.input = l.state;
		forward_convolutional_layer(output_layer, s);

		state.input += l.inputs*l.batch;
		increment_layer_crnn(&input_layer, 1);
		increment_layer_crnn(&self_layer, 1);
		increment_layer_crnn(&output_layer, 1);
	}
}
static void backward_crnn_layer(layer l, network_state state)
{
	network_state s = { 0 };

	int i;
	layer input_layer = *(l.input_layer);
	layer self_layer = *(l.self_layer);
	layer output_layer = *(l.output_layer);
	s.train = state.train;
	increment_layer_crnn(&input_layer, l.steps - 1);
	increment_layer_crnn(&self_layer, l.steps - 1);
	increment_layer_crnn(&output_layer, l.steps - 1);

	l.state += l.hidden*l.batch*l.steps;
	for (i = l.steps - 1; i >= 0; --i) {
		copy_cpu(l.hidden * l.batch, input_layer.output, 1, l.state, 1);
		axpy_cpu(l.hidden * l.batch, 1, self_layer.output, 1, l.state, 1);

		s.input = l.state;
		s.delta = self_layer.delta;
		backward_convolutional_layer(output_layer, s);

		l.state -= l.hidden*l.batch;
		/*
		if(i > 0){
		copy_cpu(l.hidden * l.batch, input_layer.output - l.hidden*l.batch, 1, l.state, 1);
		axpy_cpu(l.hidden * l.batch, 1, self_layer.output - l.hidden*l.batch, 1, l.state, 1);
		}else{
		fill_cpu(l.hidden * l.batch, 0, l.state, 1);
		}
		*/

		s.input = l.state;
		s.delta = self_layer.delta - l.hidden*l.batch;
		if (i == 0) s.delta = 0;
		backward_convolutional_layer(self_layer, s);

		copy_cpu(l.hidden*l.batch, self_layer.delta, 1, input_layer.delta, 1);
		if (i > 0 && l.shortcut) axpy_cpu(l.hidden*l.batch, 1, self_layer.delta, 1, self_layer.delta - l.hidden*l.batch, 1);
		s.input = state.input + i * l.inputs*l.batch;
		if (state.delta) s.delta = state.delta + i * l.inputs*l.batch;
		else s.delta = 0;
		backward_convolutional_layer(input_layer, s);

		increment_layer_crnn(&input_layer, -1);
		increment_layer_crnn(&self_layer, -1);
		increment_layer_crnn(&output_layer, -1);
	}
}

#if 0 /* SOD_GPU */

static void pull_crnn_layer(layer l)
{
	pull_convolutional_layer(*(l.input_layer));
	pull_convolutional_layer(*(l.self_layer));
	pull_convolutional_layer(*(l.output_layer));
}
static void push_crnn_layer(layer l)
{
	push_convolutional_layer(*(l.input_layer));
	push_convolutional_layer(*(l.self_layer));
	push_convolutional_layer(*(l.output_layer));
}
static void update_crnn_layer_gpu(layer l, int batch, float learning_rate, float momentum, float decay)
{
	update_convolutional_layer_gpu(*(l.input_layer), batch, learning_rate, momentum, decay);
	update_convolutional_layer_gpu(*(l.self_layer), batch, learning_rate, momentum, decay);
	update_convolutional_layer_gpu(*(l.output_layer), batch, learning_rate, momentum, decay);
}
static void forward_crnn_layer_gpu(layer l, network_state state)
{
	network_state s = { 0 };
	s.train = state.train;
	int i;
	layer input_layer = *(l.input_layer);
	layer self_layer = *(l.self_layer);
	layer output_layer = *(l.output_layer);

	fill_ongpu(l.outputs * l.batch * l.steps, 0, output_layer.delta_gpu, 1);
	fill_ongpu(l.hidden * l.batch * l.steps, 0, self_layer.delta_gpu, 1);
	fill_ongpu(l.hidden * l.batch * l.steps, 0, input_layer.delta_gpu, 1);
	if (state.train) fill_ongpu(l.hidden * l.batch, 0, l.state_gpu, 1);

	for (i = 0; i < l.steps; ++i) {
		s.input = state.input;
		forward_convolutional_layer_gpu(input_layer, s);

		s.input = l.state_gpu;
		forward_convolutional_layer_gpu(self_layer, s);

		float *old_state = l.state_gpu;
		if (state.train) l.state_gpu += l.hidden*l.batch;
		if (l.shortcut) {
			copy_ongpu(l.hidden * l.batch, old_state, 1, l.state_gpu, 1);
		}
		else {
			fill_ongpu(l.hidden * l.batch, 0, l.state_gpu, 1);
		}
		axpy_ongpu(l.hidden * l.batch, 1, input_layer.output_gpu, 1, l.state_gpu, 1);
		axpy_ongpu(l.hidden * l.batch, 1, self_layer.output_gpu, 1, l.state_gpu, 1);

		s.input = l.state_gpu;
		forward_convolutional_layer_gpu(output_layer, s);

		state.input += l.inputs*l.batch;
		increment_layer_crnn(&input_layer, 1);
		increment_layer_crnn(&self_layer, 1);
		increment_layer_crnn(&output_layer, 1);
	}
}
static void backward_crnn_layer_gpu(layer l, network_state state)
{
	network_state s = { 0 };
	s.train = state.train;
	int i;
	layer input_layer = *(l.input_layer);
	layer self_layer = *(l.self_layer);
	layer output_layer = *(l.output_layer);
	increment_layer(&input_layer, l.steps - 1);
	increment_layer(&self_layer, l.steps - 1);
	increment_layer(&output_layer, l.steps - 1);
	l.state_gpu += l.hidden*l.batch*l.steps;
	for (i = l.steps - 1; i >= 0; --i) {
		copy_ongpu(l.hidden * l.batch, input_layer.output_gpu, 1, l.state_gpu, 1);
		axpy_ongpu(l.hidden * l.batch, 1, self_layer.output_gpu, 1, l.state_gpu, 1);

		s.input = l.state_gpu;
		s.delta = self_layer.delta_gpu;
		backward_convolutional_layer_gpu(output_layer, s);

		l.state_gpu -= l.hidden*l.batch;

		s.input = l.state_gpu;
		s.delta = self_layer.delta_gpu - l.hidden*l.batch;
		if (i == 0) s.delta = 0;
		backward_convolutional_layer_gpu(self_layer, s);

		copy_ongpu(l.hidden*l.batch, self_layer.delta_gpu, 1, input_layer.delta_gpu, 1);
		if (i > 0 && l.shortcut) axpy_ongpu(l.hidden*l.batch, 1, self_layer.delta_gpu, 1, self_layer.delta_gpu - l.hidden*l.batch, 1);
		s.input = state.input + i * l.inputs*l.batch;
		if (state.delta) s.delta = state.delta + i * l.inputs*l.batch;
		else s.delta = 0;
		backward_convolutional_layer_gpu(input_layer, s);

		increment_layer_crnn(&input_layer, -1);
		increment_layer_crnn(&self_layer, -1);
		increment_layer_crnn(&output_layer, -1);
	}
}
#endif
static layer make_crnn_layer(int batch, int h, int w, int c, int hidden_filters, int output_filters, int steps, ACTIVATION activation, int batch_normalize)
{
	layer l = { 0 };
	batch = batch / steps;
	l.batch = batch;
	l.type = CRNN;
	l.steps = steps;
	l.h = h;
	l.w = w;
	l.c = c;
	l.out_h = h;
	l.out_w = w;
	l.out_c = output_filters;
	l.inputs = h * w*c;
	l.hidden = h * w * hidden_filters;
	l.outputs = l.out_h * l.out_w * l.out_c;

	l.state = calloc(l.hidden*batch*(steps + 1), sizeof(float));

	l.input_layer = malloc(sizeof(layer));

	*(l.input_layer) = make_convolutional_layer(batch*steps, h, w, c, hidden_filters, 3, 1, 1, activation, batch_normalize, 0, 0, 0);
	l.input_layer->batch = batch;

	l.self_layer = malloc(sizeof(layer));
	*(l.self_layer) = make_convolutional_layer(batch*steps, h, w, hidden_filters, hidden_filters, 3, 1, 1, activation, batch_normalize, 0, 0, 0);
	l.self_layer->batch = batch;

	l.output_layer = malloc(sizeof(layer));
	*(l.output_layer) = make_convolutional_layer(batch*steps, h, w, hidden_filters, output_filters, 3, 1, 1, activation, batch_normalize, 0, 0, 0);
	l.output_layer->batch = batch;

	l.output = l.output_layer->output;
	l.delta = l.output_layer->delta;

	l.forward = forward_crnn_layer;
	l.backward = backward_crnn_layer;
	l.update = update_crnn_layer;

#if 0 /* SOD_GPU */
	l.forward_gpu = forward_crnn_layer_gpu;
	l.backward_gpu = backward_crnn_layer_gpu;
	l.update_gpu = update_crnn_layer_gpu;

	l.state_gpu = cuda_make_array(l.state, l.hidden*batch*(steps + 1));
	l.output_gpu = l.output_layer->output_gpu;
	l.delta_gpu = l.output_layer->delta_gpu;
#endif

	return l;
}
static int parse_crnn(layer *l, list *options, size_params params)
{
	int output_filters = option_find_int(options, "output_filters", 1);
	int hidden_filters = option_find_int(options, "hidden_filters", 1);
	char *activation_s = option_find_str(options, "activation", "logistic");
	ACTIVATION activation = get_activation(activation_s);
	int batch_normalize = option_find_int(options, "batch_normalize", 0);

	*l = make_crnn_layer(params.batch, params.w, params.h, params.c, hidden_filters, output_filters, params.time_steps, activation, batch_normalize);
	l->shortcut = option_find_int(options, "shortcut", 0);
	return SOD_OK;
}
/* =============================================================== CONNECTED =============================================================== */
static int parse_connected(connected_layer *l, list *options, size_params params)
{
	int output = option_find_int(options, "output", 1);
	char *activation_s = option_find_str(options, "activation", "logistic");
	ACTIVATION activation = get_activation(activation_s);
	int batch_normalize = option_find_int(options, "batch_normalize", 0);
	*l = make_connected_layer(params.batch, params.inputs, output, activation, batch_normalize);
	return SOD_OK;
}
/* =============================================================== CROP =============================================================== */
typedef layer crop_layer;
static void forward_crop_layer(const crop_layer l, network_state state)
{
	int i, j, c, b, row, col;
	int index;
	int count = 0;
	int flip = (l.flip && rand() % 2);
	int dh = rand() % (l.h - l.out_h + 1);
	int dw = rand() % (l.w - l.out_w + 1);
	float scale = 2;
	float trans = -1;
	if (l.noadjust) {
		scale = 1;
		trans = 0;
	}
	if (!state.train) {
		flip = 0;
		dh = (l.h - l.out_h) / 2;
		dw = (l.w - l.out_w) / 2;
	}
	for (b = 0; b < l.batch; ++b) {
		for (c = 0; c < l.c; ++c) {
			for (i = 0; i < l.out_h; ++i) {
				for (j = 0; j < l.out_w; ++j) {
					if (flip) {
						col = l.w - dw - j - 1;
					}
					else {
						col = j + dw;
					}
					row = i + dh;
					index = col + l.w*(row + l.h*(c + l.c*b));
					l.output[count++] = state.input[index] * scale + trans;
				}
			}
		}
	}
}
static void backward_crop_layer(const crop_layer l, network_state state) { (void)l; (void)state; /* shut up the compiler */ }
static crop_layer make_crop_layer(int batch, int h, int w, int c, int crop_height, int crop_width, int flip, float angle, float saturation, float exposure)
{
	crop_layer l = { 0 };
	l.type = CROP;
	l.batch = batch;
	l.h = h;
	l.w = w;
	l.c = c;
	l.scale = (float)crop_height / h;
	l.flip = flip;
	l.angle = angle;
	l.saturation = saturation;
	l.exposure = exposure;
	l.out_w = crop_width;
	l.out_h = crop_height;
	l.out_c = c;
	l.inputs = l.w * l.h * l.c;
	l.outputs = l.out_w * l.out_h * l.out_c;
	l.output = calloc(l.outputs*batch, sizeof(float));
	l.forward = forward_crop_layer;
	l.backward = backward_crop_layer;

#if 0 /* SOD_GPU */
	l.forward_gpu = forward_crop_layer_gpu;
	l.backward_gpu = backward_crop_layer_gpu;
	l.output_gpu = cuda_make_array(l.output, l.outputs*batch);
	l.rand_gpu = cuda_make_array(0, l.batch * 8);
#endif
	return l;
}
static int parse_crop(crop_layer *l, list *options, size_params params, sod_cnn *pNet)
{
	int crop_height = option_find_int(options, "crop_height", 1);
	int crop_width = option_find_int(options, "crop_width", 1);
	int flip = option_find_int(options, "flip", 0);
	float angle = option_find_float(options, "angle", 0);
	float saturation = option_find_float(options, "saturation", 1);
	float exposure = option_find_float(options, "exposure", 1);

	int batch, h, w, c;
	h = params.h;
	w = params.w;
	c = params.c;
	batch = params.batch;
	if (!(h && w && c)) {
		pNet->zErr = "Layer before crop layer must output image.";
		pNet->nErr++;
		return -1;
	}
	*l = make_crop_layer(batch, h, w, c, crop_height, crop_width, flip, angle, saturation, exposure);
	l->shift = option_find_float(options, "shift", 0);
	l->noadjust = option_find_int(options, "noadjust", 0);
	return SOD_OK;
}
/* =============================================================== COST =============================================================== */
typedef layer cost_layer;
#define SECRET_NUM -1234
static COST_TYPE get_cost_type(char *s)
{
	if (strcmp(s, "sse") == 0) return SSE;
	if (strcmp(s, "masked") == 0) return MASKED;
	if (strcmp(s, "smooth") == 0) return SMOOTH;

	return SSE;
}
static void forward_cost_layer(cost_layer l, network_state state)
{
	if (!state.truth) return;
	if (l.cost_type == MASKED) {
		int i;
		for (i = 0; i < l.batch*l.inputs; ++i) {
			if (state.truth[i] == SECRET_NUM) state.input[i] = SECRET_NUM;
		}
	}
	if (l.cost_type == SMOOTH) {
		smooth_l1_cpu(l.batch*l.inputs, state.input, state.truth, l.delta, l.output);
	}
	else {
		l2_cpu(l.batch*l.inputs, state.input, state.truth, l.delta, l.output);
	}
	l.cost[0] = sum_array(l.output, l.batch*l.inputs);
}
static void backward_cost_layer(const cost_layer l, network_state state)
{
	axpy_cpu(l.batch*l.inputs, l.scale, l.delta, 1, state.delta, 1);
}

#if 0 /* SOD_GPU */

static void pull_cost_layer(cost_layer l)
{
	cuda_pull_array(l.delta_gpu, l.delta, l.batch*l.inputs);
}
static void push_cost_layer(cost_layer l)
{
	cuda_push_array(l.delta_gpu, l.delta, l.batch*l.inputs);
}
static int float_abs_compare(const void * a, const void * b)
{
	float fa = *(const float*)a;
	if (fa < 0) fa = -fa;
	float fb = *(const float*)b;
	if (fb < 0) fb = -fb;
	return (fa > fb) - (fa < fb);
}
static void forward_cost_layer_gpu(cost_layer l, network_state state)
{
	if (!state.truth) return;
	if (l.cost_type == MASKED) {
		mask_ongpu(l.batch*l.inputs, state.input, SECRET_NUM, state.truth);
	}

	if (l.cost_type == SMOOTH) {
		smooth_l1_gpu(l.batch*l.inputs, state.input, state.truth, l.delta_gpu, l.output_gpu);
	}
	else {
		l2_gpu(l.batch*l.inputs, state.input, state.truth, l.delta_gpu, l.output_gpu);
	}

	if (l.ratio) {
		cuda_pull_array(l.delta_gpu, l.delta, l.batch*l.inputs);
		qsort(l.delta, l.batch*l.inputs, sizeof(float), float_abs_compare);
		int n = (1 - l.ratio) * l.batch*l.inputs;
		float thresh = l.delta[n];
		thresh = 0;
		supp_ongpu(l.batch*l.inputs, thresh, l.delta_gpu, 1);
	}

	cuda_pull_array(l.output_gpu, l.output, l.batch*l.inputs);
	l.cost[0] = sum_array(l.output, l.batch*l.inputs);
}
static void backward_cost_layer_gpu(const cost_layer l, network_state state)
{
	axpy_ongpu(l.batch*l.inputs, l.scale, l.delta_gpu, 1, state.delta, 1);
}
#endif
static cost_layer make_cost_layer(int batch, int inputs, COST_TYPE cost_type, float scale)
{
	cost_layer l = { 0 };
	l.type = COST;

	l.scale = scale;
	l.batch = batch;
	l.inputs = inputs;
	l.outputs = inputs;
	l.cost_type = cost_type;
	l.delta = calloc(inputs*batch, sizeof(float));
	l.output = calloc(inputs*batch, sizeof(float));
	l.cost = calloc(1, sizeof(float));

	l.forward = forward_cost_layer;
	l.backward = backward_cost_layer;
#if 0 /* SOD_GPU */
	l.forward_gpu = forward_cost_layer_gpu;
	l.backward_gpu = backward_cost_layer_gpu;

	l.delta_gpu = cuda_make_array(l.output, inputs*batch);
	l.output_gpu = cuda_make_array(l.delta, inputs*batch);
#endif
	return l;
}
static int parse_cost(cost_layer *l, list *options, size_params params)
{
	char *type_s = option_find_str(options, "type", "sse");
	COST_TYPE type = get_cost_type(type_s);
	float scale = option_find_float(options, "scale", 1);
	*l = make_cost_layer(params.batch, params.inputs, type, scale);
	l->ratio = option_find_float(options, "ratio", 0);
	return SOD_OK;
}
/* ================================================ Softmax =============================== */
typedef layer softmax_layer;
static void softmax_tree(float *input, int batch, int inputs, float temp, tree *hierarchy, float *output)
{
	int b;
	for (b = 0; b < batch; ++b) {
		int i;
		int count = 0;
		for (i = 0; i < hierarchy->groups; ++i) {
			int group_size = hierarchy->group_size[i];
			softmax(input + b * inputs + count, group_size, temp, output + b * inputs + count);
			count += group_size;
		}
	}
}
static void forward_softmax_layer(const softmax_layer l, network_state state)
{
	int b;
	int inputs = l.inputs / l.groups;
	int batch = l.batch * l.groups;
	if (l.softmax_tree) {
		softmax_tree(state.input, batch, inputs, l.temperature, l.softmax_tree, l.output);
	}
	else {
		for (b = 0; b < batch; ++b) {
			softmax(state.input + b * inputs, inputs, l.temperature, l.output + b * inputs);
		}
	}
}
static void backward_softmax_layer(const softmax_layer l, network_state state)
{
	int i;
	for (i = 0; i < l.inputs*l.batch; ++i) {
		state.delta[i] += l.delta[i];
	}
}

#if 0 /* SOD_GPU */

static void pull_softmax_layer_output(const softmax_layer layer)
{
	cuda_pull_array(layer.output_gpu, layer.output, layer.inputs*layer.batch);
}
static void forward_softmax_layer_gpu(const softmax_layer l, network_state state)
{
	int inputs = l.inputs / l.groups;
	int batch = l.batch * l.groups;
	if (l.softmax_tree) {
		int i;
		int count = 0;
		for (i = 0; i < l.softmax_tree->groups; ++i) {
			int group_size = l.softmax_tree->group_size[i];
			softmax_gpu(state.input + count, group_size, inputs, batch, l.temperature, l.output_gpu + count);
			count += group_size;
		}
	}
	else {
		softmax_gpu(state.input, inputs, inputs, batch, l.temperature, l.output_gpu);
	}
}
static void backward_softmax_layer_gpu(const softmax_layer layer, network_state state)
{
	axpy_ongpu(layer.batch*layer.inputs, 1, layer.delta_gpu, 1, state.delta, 1);
}

#endif
static softmax_layer make_softmax_layer(int batch, int inputs, int groups)
{
	softmax_layer l = { 0 };
	l.type = SOFTMAX;
	l.batch = batch;
	l.groups = groups;
	l.inputs = inputs;
	l.outputs = inputs;
	l.output = calloc(inputs*batch, sizeof(float));
	l.delta = calloc(inputs*batch, sizeof(float));

	l.forward = forward_softmax_layer;
	l.backward = backward_softmax_layer;
#if 0 /* SOD_GPU */
	l.forward_gpu = forward_softmax_layer_gpu;
	l.backward_gpu = backward_softmax_layer_gpu;

	l.output_gpu = cuda_make_array(l.output, inputs*batch);
	l.delta_gpu = cuda_make_array(l.delta, inputs*batch);
#endif
	return l;
}
/* ============================== BOX ================================= */
static box float_to_box(float *f)
{
	box b;
	b.x = f[0];
	b.y = f[1];
	b.w = f[2];
	b.h = f[3];
	return b;
}
static float overlap(float x1, float w1, float x2, float w2)
{
	float l1 = x1 - w1 / 2;
	float l2 = x2 - w2 / 2;
	float left = l1 > l2 ? l1 : l2;
	float r1 = x1 + w1 / 2;
	float r2 = x2 + w2 / 2;
	float right = r1 < r2 ? r1 : r2;
	return right - left;
}
static float box_intersection(box a, box b)
{
	float w = overlap(a.x, a.w, b.x, b.w);
	float h = overlap(a.y, a.h, b.y, b.h);
	if (w < 0 || h < 0) return 0;
	float area = w * h;
	return area;
}
static float box_union(box a, box b)
{
	float i = box_intersection(a, b);
	float u = a.w*a.h + b.w*b.h - i;
	return u;
}
static float box_iou(box a, box b)
{
	return box_intersection(a, b) / box_union(a, b);
}
static float box_rmse(box a, box b)
{
	return sqrt(pow(a.x - b.x, 2) +
		pow(a.y - b.y, 2) +
		pow(a.w - b.w, 2) +
		pow(a.h - b.h, 2));
}
/* ============================== NMS ================================= */
typedef struct {
	int index;
	int class;
	float **probs;
} sortable_bbox;
static int nms_comparator(const void *pa, const void *pb)
{
	sortable_bbox a = *(sortable_bbox *)pa;
	sortable_bbox b = *(sortable_bbox *)pb;
	float diff = a.probs[a.index][b.class] - b.probs[b.index][b.class];
	if (diff < 0) return 1;
	else if (diff > 0) return -1;
	return 0;
}
static void do_nms_obj(box *boxes, float **probs, int total, int classes, float thresh)
{
	int i, j, k;
	sortable_bbox *s = calloc(total, sizeof(sortable_bbox));
	for (i = 0; i < total; ++i) {
		s[i].index = i;
		s[i].class = classes;
		s[i].probs = probs;
	}

	qsort(s, total, sizeof(sortable_bbox), nms_comparator);
	for (i = 0; i < total; ++i) {
		if (probs[s[i].index][classes] == 0) continue;
		box a = boxes[s[i].index];
		for (j = i + 1; j < total; ++j) {
			box b = boxes[s[j].index];
			if (box_iou(a, b) > thresh) {
				for (k = 0; k < classes + 1; ++k) {
					probs[s[j].index][k] = 0;
				}
			}
		}
	}
	free(s);
}
static void do_nms_sort(box *boxes, float **probs, int total, int classes, float thresh)
{
	int i, j, k;
	sortable_bbox *s = calloc(total, sizeof(sortable_bbox));

	for (i = 0; i < total; ++i) {
		s[i].index = i;
		s[i].class = 0;
		s[i].probs = probs;
	}

	for (k = 0; k < classes; ++k) {
		for (i = 0; i < total; ++i) {
			s[i].class = k;
		}
		qsort(s, total, sizeof(sortable_bbox), nms_comparator);
		for (i = 0; i < total; ++i) {
			if (probs[s[i].index][k] == 0) continue;
			box a = boxes[s[i].index];
			for (j = i + 1; j < total; ++j) {
				box b = boxes[s[j].index];
				if (box_iou(a, b) > thresh) {
					probs[s[j].index][k] = 0;
				}
			}
		}
	}
	free(s);
}
/* =============================================================== Region =============================================================== */
static float get_hierarchy_probability(float *x, tree *hier, int c)
{
	float p = 1;
	while (c >= 0) {
		p = p * x[c];
		c = hier->parent[c];
	}
	return p;
}
static void delta_region_class(float *output, float *delta, int index, int class, int classes, tree *hier, float scale, float *avg_cat)
{
	int i, n;
	if (hier) {
		float pred = 1;
		while (class >= 0) {
			pred *= output[index + class];
			int g = hier->group[class];
			int offset = hier->group_offset[g];
			for (i = 0; i < hier->group_size[g]; ++i) {
				delta[index + offset + i] = scale * (0 - output[index + offset + i]);
			}
			delta[index + class] = scale * (1 - output[index + class]);

			class = hier->parent[class];
		}
		*avg_cat += pred;
	}
	else {
		for (n = 0; n < classes; ++n) {
			delta[index + n] = scale * (((n == class) ? 1 : 0) - output[index + n]);
			if (n == class) *avg_cat += output[index + n];
		}
	}
}
static box get_region_box(float *x, float *biases, int n, int index, int i, int j, int w, int h)
{
	box b;
	b.x = (i + logistic_activate(x[index + 0])) / w;
	b.y = (j + logistic_activate(x[index + 1])) / h;
	b.w = exp(x[index + 2]) * biases[2 * n] / w;
	b.h = exp(x[index + 3]) * biases[2 * n + 1] / h;
	return b;
}
static float delta_region_box(box truth, float *x, float *biases, int n, int index, int i, int j, int w, int h, float *delta, float scale)
{
	box pred = get_region_box(x, biases, n, index, i, j, w, h);
	float iou = box_iou(pred, truth);

	float tx = (truth.x*w - i);
	float ty = (truth.y*h - j);
	float tw = log(truth.w*w / biases[2 * n]);
	float th = log(truth.h*h / biases[2 * n + 1]);

	delta[index + 0] = scale * (tx - logistic_activate(x[index + 0])) * logistic_gradient(logistic_activate(x[index + 0]));
	delta[index + 1] = scale * (ty - logistic_activate(x[index + 1])) * logistic_gradient(logistic_activate(x[index + 1]));
	delta[index + 2] = scale * (tw - x[index + 2]);
	delta[index + 3] = scale * (th - x[index + 3]);
	return iou;
}
static void hierarchy_predictions(float *predictions, int n, tree *hier, int only_leaves)
{
	int j;
	for (j = 0; j < n; ++j) {
		int parent = hier->parent[j];
		if (parent >= 0) {
			predictions[j] *= predictions[parent];
		}
	}
	if (only_leaves) {
		for (j = 0; j < n; ++j) {
			if (!hier->leaf[j]) predictions[j] = 0;
		}
	}
}
static int hierarchy_top_prediction(float *predictions, tree *hier, float thresh)
{
	float p = 1;
	int group = 0;
	int i;
	while (1) {
		float max = 0;
		int max_i = 0;

		for (i = 0; i < hier->group_size[group]; ++i) {
			int index = i + hier->group_offset[group];
			float val = predictions[i + hier->group_offset[group]];
			if (val > max) {
				max_i = index;
				max = val;
			}
		}
		if (p*max > thresh) {
			p = p * max;
			group = hier->child[max_i];
			if (hier->child[max_i] < 0) return max_i;
		}
		else {
			return hier->parent[hier->group_offset[group]];
		}
	}
	return 0;
}
static void get_region_boxes(layer l, int w, int h, float thresh, float **probs, box *boxes, int only_objectness, int *map, float tree_thresh)
{
	int i, j, n;
	float *predictions = l.output;
	for (i = 0; i < l.w*l.h; ++i) {
		int row = i / l.w;
		int col = i % l.w;
		for (n = 0; n < l.n; ++n) {
			int index = i * l.n + n;
			int p_index = index * (l.classes + 5) + 4;
			float scale = predictions[p_index];
			int box_index = index * (l.classes + 5);
			boxes[index] = get_region_box(predictions, l.biases, n, box_index, col, row, l.w, l.h);
			boxes[index].x *= w;
			boxes[index].y *= h;
			boxes[index].w *= w;
			boxes[index].h *= h;

			int class_index = index * (l.classes + 5) + 5;
			if (l.softmax_tree) {
				hierarchy_predictions(predictions + class_index, l.classes, l.softmax_tree, 0);
				if (map) {
					for (j = 0; j < 200; ++j) {
						float prob = scale * predictions[class_index + map[j]];
						probs[index][j] = (prob > thresh) ? prob : 0;
					}
				}
				else {
					j = hierarchy_top_prediction(predictions + class_index, l.softmax_tree, tree_thresh);
					probs[index][j] = (scale > thresh) ? scale : 0;
					probs[index][l.classes] = scale;
				}
			}
			else {
				for (j = 0; j < l.classes; ++j) {
					float prob = scale * predictions[class_index + j];
					probs[index][j] = (prob > thresh) ? prob : 0;
				}
			}
			if (only_objectness) {
				probs[index][0] = scale;
			}
		}
	}
}
static void forward_region_layer(const layer l, network_state state)
{
	int i, j, b, t, n;
	int size = l.coords + l.classes + 1;
	memcpy(l.output, state.input, l.outputs*l.batch * sizeof(float));
#ifndef SOD_GPU
	flatten(l.output, l.w*l.h, size*l.n, l.batch, 1);
#endif
	for (b = 0; b < l.batch; ++b) {
		for (i = 0; i < l.h*l.w*l.n; ++i) {
			int index = size * i + b * l.outputs;
			l.output[index + 4] = logistic_activate(l.output[index + 4]);
		}
	}
#ifndef SOD_GPU
	if (l.softmax_tree) {
		for (b = 0; b < l.batch; ++b) {
			for (i = 0; i < l.h*l.w*l.n; ++i) {
				int index = size * i + b * l.outputs;
				softmax_tree(l.output + index + 5, 1, 0, 1, l.softmax_tree, l.output + index + 5);
			}
		}
	}
	else if (l.softmax) {
		for (b = 0; b < l.batch; ++b) {
			for (i = 0; i < l.h*l.w*l.n; ++i) {
				int index = size * i + b * l.outputs;
				softmax(l.output + index + 5, l.classes, 1, l.output + index + 5);
			}
		}
	}
#endif
	if (!state.train) return;
	memset(l.delta, 0, l.outputs * l.batch * sizeof(float));
	float avg_iou = 0;
	float recall = 0;
	float avg_cat = 0;
	float avg_obj = 0;
	float avg_anyobj = 0;
	int count = 0;
	int class_count = 0;
	*(l.cost) = 0;
	for (b = 0; b < l.batch; ++b) {
		if (l.softmax_tree) {
			int onlyclass = 0;
			for (t = 0; t < 30; ++t) {
				box truth = float_to_box(state.truth + t * 5 + b * l.truths);
				if (!truth.x) break;
				int class = state.truth[t * 5 + b * l.truths + 4];
				float maxp = 0;
				int maxi = 0;
				if (truth.x > 100000 && truth.y > 100000) {
					for (n = 0; n < l.n*l.w*l.h; ++n) {
						int index = size * n + b * l.outputs + 5;
						float scale = l.output[index - 1];
						l.delta[index - 1] = l.noobject_scale * ((0 - l.output[index - 1]) * logistic_gradient(l.output[index - 1]));
						float p = scale * get_hierarchy_probability(l.output + index, l.softmax_tree, class);
						if (p > maxp) {
							maxp = p;
							maxi = n;
						}
					}
					int index = size * maxi + b * l.outputs + 5;
					delta_region_class(l.output, l.delta, index, class, l.classes, l.softmax_tree, l.class_scale, &avg_cat);
					if (l.output[index - 1] < .3) l.delta[index - 1] = l.object_scale * ((.3 - l.output[index - 1]) * logistic_gradient(l.output[index - 1]));
					else  l.delta[index - 1] = 0;
					++class_count;
					onlyclass = 1;
					break;
				}
			}
			if (onlyclass) continue;
		}
		for (j = 0; j < l.h; ++j) {
			for (i = 0; i < l.w; ++i) {
				for (n = 0; n < l.n; ++n) {
					int index = size * (j*l.w*l.n + i * l.n + n) + b * l.outputs;
					box pred = get_region_box(l.output, l.biases, n, index, i, j, l.w, l.h);
					float best_iou = 0;
					for (t = 0; t < 30; ++t) {
						box truth = float_to_box(state.truth + t * 5 + b * l.truths);
						if (!truth.x) break;
						float iou = box_iou(pred, truth);
						if (iou > best_iou) {
							best_iou = iou;
						}
					}
					avg_anyobj += l.output[index + 4];
					l.delta[index + 4] = l.noobject_scale * ((0 - l.output[index + 4]) * logistic_gradient(l.output[index + 4]));
					if (best_iou > l.thresh) {
						l.delta[index + 4] = 0;
					}

					if (*(state.net->seen) < 12800) {
						box truth = { 0 };
						truth.x = (i + .5) / l.w;
						truth.y = (j + .5) / l.h;
						truth.w = l.biases[2 * n] / l.w;
						truth.h = l.biases[2 * n + 1] / l.h;
						delta_region_box(truth, l.output, l.biases, n, index, i, j, l.w, l.h, l.delta, .01);
					}
				}
			}
		}
		for (t = 0; t < 30; ++t) {
			box truth = float_to_box(state.truth + t * 5 + b * l.truths);

			if (!truth.x) break;
			float best_iou = 0;
			int best_index = 0;
			int best_n = 0;
			i = (truth.x * l.w);
			j = (truth.y * l.h);

			box truth_shift = truth;
			truth_shift.x = 0;
			truth_shift.y = 0;

			for (n = 0; n < l.n; ++n) {
				int index = size * (j*l.w*l.n + i * l.n + n) + b * l.outputs;
				box pred = get_region_box(l.output, l.biases, n, index, i, j, l.w, l.h);
				if (l.bias_match) {
					pred.w = l.biases[2 * n] / l.w;
					pred.h = l.biases[2 * n + 1] / l.h;
				}

				pred.x = 0;
				pred.y = 0;
				float iou = box_iou(pred, truth_shift);
				if (iou > best_iou) {
					best_index = index;
					best_iou = iou;
					best_n = n;
				}
			}
			float iou = delta_region_box(truth, l.output, l.biases, best_n, best_index, i, j, l.w, l.h, l.delta, l.coord_scale);
			if (iou > .5) recall += 1;
			avg_iou += iou;

			/*l.delta[best_index + 4] = iou - l.output[best_index + 4];*/
			avg_obj += l.output[best_index + 4];
			l.delta[best_index + 4] = l.object_scale * (1 - l.output[best_index + 4]) * logistic_gradient(l.output[best_index + 4]);
			if (l.rescore) {
				l.delta[best_index + 4] = l.object_scale * (iou - l.output[best_index + 4]) * logistic_gradient(l.output[best_index + 4]);
			}
			int class = state.truth[t * 5 + b * l.truths + 4];
			if (l.map) class = l.map[class];
			delta_region_class(l.output, l.delta, best_index + 5, class, l.classes, l.softmax_tree, l.class_scale, &avg_cat);
			++count;
			++class_count;
		}
	}
#ifndef SOD_GPU
	flatten(l.delta, l.w*l.h, size*l.n, l.batch, 0);
#endif
	*(l.cost) = pow(mag_array(l.delta, l.outputs * l.batch), 2);
}
static void backward_region_layer(const layer l, network_state state)
{
	axpy_cpu(l.batch*l.inputs, 1, l.delta, 1, state.delta, 1);
}
static layer make_region_layer(int batch, int w, int h, int n, int classes, int coords)
{
	layer l = { 0 };
	l.type = REGION;

	l.n = n;
	l.batch = batch;
	l.h = h;
	l.w = w;
	l.classes = classes;
	l.coords = coords;
	l.cost = calloc(1, sizeof(float));
	l.biases = calloc(n * 2, sizeof(float));
	l.bias_updates = calloc(n * 2, sizeof(float));
	l.outputs = h * w*n*(classes + coords + 1);
	l.inputs = l.outputs;
	l.truths = 30 * (5);
	l.delta = calloc(batch*l.outputs, sizeof(float));
	l.output = calloc(batch*l.outputs, sizeof(float));
	int i;
	for (i = 0; i < n * 2; ++i) {
		l.biases[i] = .5;
	}

	l.forward = forward_region_layer;
	l.backward = backward_region_layer;
#if 0 /* SOD_GPU */
	l.forward_gpu = forward_region_layer_gpu;
	l.backward_gpu = backward_region_layer_gpu;
	l.output_gpu = cuda_make_array(l.output, batch*l.outputs);
	l.delta_gpu = cuda_make_array(l.delta, batch*l.outputs);
#endif

	srand(0);

	return l;
}
static int *read_map(char *filename)
{
	int n = 0;
	int *map = 0;
	char *str;
	FILE *file = fopen(filename, "r");
	if (!file) {
		return 0;
	}
	while ((str = fgetl(file)) != 0) {
		++n;
		map = realloc(map, n * sizeof(int));
		map[n - 1] = atoi(str);
	}
	return map;
}
/* Forward declaration */
static tree *read_tree(char *filename);
static int parse_region(layer *l, list *options, size_params params)
{
	int coords = option_find_int(options, "coords", 4);
	int classes = option_find_int(options, "classes", 20);
	int num = option_find_int(options, "num", 1);

	*l = make_region_layer(params.batch, params.w, params.h, num, classes, coords);

	l->log = option_find_int(options, "log", 0);
	l->sqrt = option_find_int(options, "sqrt", 0);

	l->softmax = option_find_int(options, "softmax", 0);
	l->background = option_find_int(options, "background", 0);
	l->max_boxes = option_find_int(options, "max", 30);
	l->jitter = option_find_float(options, "jitter", .2);
	l->rescore = option_find_int(options, "rescore", 0);

	l->thresh = option_find_float(options, "thresh", .5);
	l->classfix = option_find_int(options, "classfix", 0);
	l->absolute = option_find_int(options, "absolute", 0);
	l->random = option_find_int(options, "random", 0);

	l->coord_scale = option_find_float(options, "coord_scale", 1);
	l->object_scale = option_find_float(options, "object_scale", 1);
	l->noobject_scale = option_find_float(options, "noobject_scale", 1);
	l->mask_scale = option_find_float(options, "mask_scale", 1);
	l->class_scale = option_find_float(options, "class_scale", 1);
	l->bias_match = option_find_int(options, "bias_match", 0);

	char *tree_file = option_find_str(options, "tree", 0);
	if (tree_file) l->softmax_tree = read_tree(tree_file);
	char *map_file = option_find_str(options, "map", 0);
	if (map_file) l->map = read_map(map_file);

	char *a = option_find_str(options, "anchors", 0);
	if (a) {
		int len = (int)strlen(a);
		int n = 1;
		int i;
		for (i = 0; i < len; ++i) {
			if (a[i] == ',') ++n;
		}
		for (i = 0; i < n; ++i) {
			float bias = atof(a);
			l->biases[i] = bias;
			a = strchr(a, ',') + 1;
		}
	}
	return SOD_OK;
}
/* =============================================================== Detection  =============================================================== */
typedef layer detection_layer;

static void forward_detection_layer(const detection_layer l, network_state state)
{
	int locations = l.side*l.side;
	int i, j;
	int b;
	memcpy(l.output, state.input, l.outputs*l.batch * sizeof(float));
	/*if(l.reorg) reorg(l.output, l.w*l.h, size*l.n, l.batch, 1);*/

	if (l.softmax) {
		for (b = 0; b < l.batch; ++b) {
			int index = b * l.inputs;
			for (i = 0; i < locations; ++i) {
				int offset = i * l.classes;
				softmax(l.output + index + offset, l.classes, 1,
					l.output + index + offset);
			}
		}
	}
	if (state.train) {
		float avg_iou = 0;
		float avg_cat = 0;
		float avg_allcat = 0;
		float avg_obj = 0;
		float avg_anyobj = 0;
		int count = 0;
		int size = l.inputs * l.batch;
		*(l.cost) = 0;

		memset(l.delta, 0, size * sizeof(float));
		for (b = 0; b < l.batch; ++b) {
			int index = b * l.inputs;
			for (i = 0; i < locations; ++i) {
				int truth_index = (b*locations + i)*(1 + l.coords + l.classes);
				int is_obj = state.truth[truth_index];
				int best_index;
				float best_iou;
				float best_rmse;
				int class_index;
				box truth;
				for (j = 0; j < l.n; ++j) {
					int p_index = index + locations * l.classes + i * l.n + j;
					l.delta[p_index] = l.noobject_scale*(0 - l.output[p_index]);
					*(l.cost) += l.noobject_scale*pow(l.output[p_index], 2);
					avg_anyobj += l.output[p_index];
				}

				best_index = -1;
				best_iou = 0;
				best_rmse = 20;

				if (!is_obj) {
					continue;
				}

				class_index = index + i * l.classes;
				for (j = 0; j < l.classes; ++j) {
					l.delta[class_index + j] = l.class_scale * (state.truth[truth_index + 1 + j] - l.output[class_index + j]);
					*(l.cost) += l.class_scale * pow(state.truth[truth_index + 1 + j] - l.output[class_index + j], 2);
					if (state.truth[truth_index + 1 + j]) avg_cat += l.output[class_index + j];
					avg_allcat += l.output[class_index + j];
				}

				truth = float_to_box(state.truth + truth_index + 1 + l.classes);
				truth.x /= l.side;
				truth.y /= l.side;

				for (j = 0; j < l.n; ++j) {
					float iou;
					float rmse;
					int box_index = index + locations * (l.classes + l.n) + (i*l.n + j) * l.coords;
					box out = float_to_box(l.output + box_index);
					out.x /= l.side;
					out.y /= l.side;

					if (l.sqrt) {
						out.w = out.w*out.w;
						out.h = out.h*out.h;
					}

					iou = box_iou(out, truth);

					rmse = box_rmse(out, truth);
					if (best_iou > 0 || iou > 0) {
						if (iou > best_iou) {
							best_iou = iou;
							best_index = j;
						}
					}
					else {
						if (rmse < best_rmse) {
							best_rmse = rmse;
							best_index = j;
						}
					}
				}

				if (l.forced) {
					if (truth.w*truth.h < .1) {
						best_index = 1;
					}
					else {
						best_index = 0;
					}
				}
				if (l.random && *(state.net->seen) < 64000) {
					best_index = rand() % l.n;
				}

				int box_index = index + locations * (l.classes + l.n) + (i*l.n + best_index) * l.coords;
				int tbox_index = truth_index + 1 + l.classes;

				box out = float_to_box(l.output + box_index);
				out.x /= l.side;
				out.y /= l.side;
				if (l.sqrt) {
					out.w = out.w*out.w;
					out.h = out.h*out.h;
				}
				float iou = box_iou(out, truth);

				int p_index = index + locations * l.classes + i * l.n + best_index;
				*(l.cost) -= l.noobject_scale * pow(l.output[p_index], 2);
				*(l.cost) += l.object_scale * pow(1 - l.output[p_index], 2);
				avg_obj += l.output[p_index];
				l.delta[p_index] = l.object_scale * (1. - l.output[p_index]);

				if (l.rescore) {
					l.delta[p_index] = l.object_scale * (iou - l.output[p_index]);
				}

				l.delta[box_index + 0] = l.coord_scale*(state.truth[tbox_index + 0] - l.output[box_index + 0]);
				l.delta[box_index + 1] = l.coord_scale*(state.truth[tbox_index + 1] - l.output[box_index + 1]);
				l.delta[box_index + 2] = l.coord_scale*(state.truth[tbox_index + 2] - l.output[box_index + 2]);
				l.delta[box_index + 3] = l.coord_scale*(state.truth[tbox_index + 3] - l.output[box_index + 3]);
				if (l.sqrt) {
					l.delta[box_index + 2] = l.coord_scale*(sqrt(state.truth[tbox_index + 2]) - l.output[box_index + 2]);
					l.delta[box_index + 3] = l.coord_scale*(sqrt(state.truth[tbox_index + 3]) - l.output[box_index + 3]);
				}

				*(l.cost) += pow(1 - iou, 2);
				avg_iou += iou;
				++count;
			}
		}
		*(l.cost) = pow(mag_array(l.delta, l.outputs * l.batch), 2);

		/*if(l.reorg) reorg(l.delta, l.w*l.h, size*l.n, l.batch, 0);*/
	}
}
static void backward_detection_layer(const detection_layer l, network_state state)
{
	axpy_cpu(l.batch*l.inputs, 1, l.delta, 1, state.delta, 1);
}
#if 0 /* SOD_GPU */

static void forward_detection_layer_gpu(const detection_layer l, network_state state)
{
	if (!state.train) {
		copy_ongpu(l.batch*l.inputs, state.input, 1, l.output_gpu, 1);
		return;
	}

	float *in_cpu = calloc(l.batch*l.inputs, sizeof(float));
	float *truth_cpu = 0;
	if (state.truth) {
		int num_truth = l.batch*l.side*l.side*(1 + l.coords + l.classes);
		truth_cpu = calloc(num_truth, sizeof(float));
		cuda_pull_array(state.truth, truth_cpu, num_truth);
	}
	cuda_pull_array(state.input, in_cpu, l.batch*l.inputs);
	network_state cpu_state = state;
	cpu_state.train = state.train;
	cpu_state.truth = truth_cpu;
	cpu_state.input = in_cpu;
	forward_detection_layer(l, cpu_state);
	cuda_push_array(l.output_gpu, l.output, l.batch*l.outputs);
	cuda_push_array(l.delta_gpu, l.delta, l.batch*l.inputs);
	free(cpu_state.input);
	if (cpu_state.truth) free(cpu_state.truth);
}
static void backward_detection_layer_gpu(detection_layer l, network_state state)
{
	axpy_ongpu(l.batch*l.inputs, 1, l.delta_gpu, 1, state.delta, 1);
	/*copy_ongpu(l.batch*l.inputs, l.delta_gpu, 1, state.delta, 1);*/
}
#endif
static void get_detection_boxes(layer l, int w, int h, float thresh, float **probs, box *boxes, int only_objectness)
{
	int i, j, n;
	float *predictions = l.output;

	for (i = 0; i < l.side*l.side; ++i) {
		int row = i / l.side;
		int col = i % l.side;
		for (n = 0; n < l.n; ++n) {
			int index = i * l.n + n;
			int p_index = l.side*l.side*l.classes + i * l.n + n;
			float scale = predictions[p_index];
			int box_index = l.side*l.side*(l.classes + l.n) + (i*l.n + n) * 4;
			boxes[index].x = (predictions[box_index + 0] + col) / l.side * w;
			boxes[index].y = (predictions[box_index + 1] + row) / l.side * h;
			boxes[index].w = pow(predictions[box_index + 2], (l.sqrt ? 2 : 1)) * w;
			boxes[index].h = pow(predictions[box_index + 3], (l.sqrt ? 2 : 1)) * h;
			for (j = 0; j < l.classes; ++j) {
				int class_index = i * l.classes;
				float prob = scale * predictions[class_index + j];
				probs[index][j] = (prob > thresh) ? prob : 0;
			}
			if (only_objectness) {
				probs[index][0] = scale;
			}
		}
	}
}
static detection_layer make_detection_layer(int batch, int inputs, int n, int side, int classes, int coords, int rescore)
{
	detection_layer l = { 0 };
	l.type = DETECTION;

	l.n = n;
	l.batch = batch;
	l.inputs = inputs;
	l.classes = classes;
	l.coords = coords;
	l.rescore = rescore;
	l.side = side;
	l.w = side;
	l.h = side;

	l.cost = calloc(1, sizeof(float));
	l.outputs = l.inputs;
	l.truths = l.side*l.side*(1 + l.coords + l.classes);
	l.output = calloc(batch*l.outputs, sizeof(float));
	l.delta = calloc(batch*l.outputs, sizeof(float));

	l.forward = forward_detection_layer;
	l.backward = backward_detection_layer;
#if 0 /* SOD_GPU */
	l.forward_gpu = forward_detection_layer_gpu;
	l.backward_gpu = backward_detection_layer_gpu;
	l.output_gpu = cuda_make_array(l.output, batch*l.outputs);
	l.delta_gpu = cuda_make_array(l.delta, batch*l.outputs);
#endif
	return l;
}
static int parse_detection(detection_layer *l, list *options, size_params params)
{
	int coords = option_find_int(options, "coords", 1);
	int classes = option_find_int(options, "classes", 1);
	int rescore = option_find_int(options, "rescore", 0);
	int num = option_find_int(options, "num", 1);
	int side = option_find_int(options, "side", 7);
	*l = make_detection_layer(params.batch, params.inputs, num, side, classes, coords, rescore);

	l->softmax = option_find_int(options, "softmax", 0);
	l->sqrt = option_find_int(options, "sqrt", 0);

	l->max_boxes = option_find_int(options, "max", 30);
	l->coord_scale = option_find_float(options, "coord_scale", 1);
	l->forced = option_find_int(options, "forced", 0);
	l->object_scale = option_find_float(options, "object_scale", 1);
	l->noobject_scale = option_find_float(options, "noobject_scale", 1);
	l->class_scale = option_find_float(options, "class_scale", 1);
	l->jitter = option_find_float(options, "jitter", .2);
	l->random = option_find_int(options, "random", 0);
	l->reorg = option_find_int(options, "reorg", 0);
	return SOD_OK;
}
/* =============================================================== Softmax  =============================================================== */
static tree *read_tree(char *filename)
{
	tree t = { 0 };
	FILE *fp = fopen(filename, "r");

	char *line;
	int last_parent = -1;
	int group_size = 0;
	int groups = 0;
	int n = 0;
	while ((line = fgetl(fp)) != 0) {
		char *id = calloc(256, sizeof(char));
		int parent = -1;
		sscanf(line, "%s %d", id, &parent);
		t.parent = realloc(t.parent, (n + 1) * sizeof(int));
		t.parent[n] = parent;

		t.child = realloc(t.child, (n + 1) * sizeof(int));
		t.child[n] = -1;

		t.name = realloc(t.name, (n + 1) * sizeof(char *));
		t.name[n] = id;
		if (parent != last_parent) {
			++groups;
			t.group_offset = realloc(t.group_offset, groups * sizeof(int));
			t.group_offset[groups - 1] = n - group_size;
			t.group_size = realloc(t.group_size, groups * sizeof(int));
			t.group_size[groups - 1] = group_size;
			group_size = 0;
			last_parent = parent;
		}
		t.group = realloc(t.group, (n + 1) * sizeof(int));
		t.group[n] = groups;
		if (parent >= 0) {
			t.child[parent] = groups;
		}
		++n;
		++group_size;
	}
	++groups;
	t.group_offset = realloc(t.group_offset, groups * sizeof(int));
	t.group_offset[groups - 1] = n - group_size;
	t.group_size = realloc(t.group_size, groups * sizeof(int));
	t.group_size[groups - 1] = group_size;
	t.n = n;
	t.groups = groups;
	t.leaf = calloc(n, sizeof(int));
	int i;
	for (i = 0; i < n; ++i) t.leaf[i] = 1;
	for (i = 0; i < n; ++i) if (t.parent[i] >= 0) t.leaf[t.parent[i]] = 0;

	fclose(fp);
	tree *tree_ptr = calloc(1, sizeof(tree));
	*tree_ptr = t;

	return tree_ptr;
}
static int parse_softmax(softmax_layer *l, list *options, size_params params)
{
	int groups = option_find_int(options, "groups", 1);
	*l = make_softmax_layer(params.batch, params.inputs, groups);
	l->temperature = option_find_float(options, "temperature", 1);
	char *tree_file = option_find_str(options, "tree", 0);
	if (tree_file) l->softmax_tree = read_tree(tree_file);
	return SOD_OK;
}
/* =============================================================== Normalization  =============================================================== */
static void forward_normalization_layer(const layer layer, network_state state)
{
	int k, b;
	int w = layer.w;
	int h = layer.h;
	int c = layer.c;
	scal_cpu(w*h*c*layer.batch, 0, layer.squared, 1);

	for (b = 0; b < layer.batch; ++b) {
		float *squared = layer.squared + w * h*c*b;
		float *norms = layer.norms + w * h*c*b;
		float *input = state.input + w * h*c*b;
		pow_cpu(w*h*c, 2, input, 1, squared, 1);

		const_cpu(w*h, layer.kappa, norms, 1);
		for (k = 0; k < layer.size / 2; ++k) {
			axpy_cpu(w*h, layer.alpha, squared + w * h*k, 1, norms, 1);
		}

		for (k = 1; k < layer.c; ++k) {
			copy_cpu(w*h, norms + w * h*(k - 1), 1, norms + w * h*k, 1);
			int prev = k - ((layer.size - 1) / 2) - 1;
			int next = k + (layer.size / 2);
			if (prev >= 0)      axpy_cpu(w*h, -layer.alpha, squared + w * h*prev, 1, norms + w * h*k, 1);
			if (next < layer.c) axpy_cpu(w*h, layer.alpha, squared + w * h*next, 1, norms + w * h*k, 1);
		}
	}
	pow_cpu(w*h*c*layer.batch, -layer.beta, layer.norms, 1, layer.output, 1);
	mul_cpu(w*h*c*layer.batch, state.input, 1, layer.output, 1);
}
static void backward_normalization_layer(const layer layer, network_state state)
{
	/* TODO This is approximate ;-)
	* Also this should add in to delta instead of overwritting.
	*/
	int w = layer.w;
	int h = layer.h;
	int c = layer.c;
	pow_cpu(w*h*c*layer.batch, -layer.beta, layer.norms, 1, state.delta, 1);
	mul_cpu(w*h*c*layer.batch, layer.delta, 1, state.delta, 1);
}

#if 0 /* SOD_GPU */
void forward_normalization_layer_gpu(const layer layer, network_state state)
{
	int k, b;
	int w = layer.w;
	int h = layer.h;
	int c = layer.c;
	scal_ongpu(w*h*c*layer.batch, 0, layer.squared_gpu, 1);

	for (b = 0; b < layer.batch; ++b) {
		float *squared = layer.squared_gpu + w * h*c*b;
		float *norms = layer.norms_gpu + w * h*c*b;
		float *input = state.input + w * h*c*b;
		pow_ongpu(w*h*c, 2, input, 1, squared, 1);

		const_ongpu(w*h, layer.kappa, norms, 1);
		for (k = 0; k < layer.size / 2; ++k) {
			axpy_ongpu(w*h, layer.alpha, squared + w * h*k, 1, norms, 1);
		}

		for (k = 1; k < layer.c; ++k) {
			copy_ongpu(w*h, norms + w * h*(k - 1), 1, norms + w * h*k, 1);
			int prev = k - ((layer.size - 1) / 2) - 1;
			int next = k + (layer.size / 2);
			if (prev >= 0)      axpy_ongpu(w*h, -layer.alpha, squared + w * h*prev, 1, norms + w * h*k, 1);
			if (next < layer.c) axpy_ongpu(w*h, layer.alpha, squared + w * h*next, 1, norms + w * h*k, 1);
		}
	}
	pow_ongpu(w*h*c*layer.batch, -layer.beta, layer.norms_gpu, 1, layer.output_gpu, 1);
	mul_ongpu(w*h*c*layer.batch, state.input, 1, layer.output_gpu, 1);
}

void backward_normalization_layer_gpu(const layer layer, network_state state)
{
	// TODO This is approximate ;-)

	int w = layer.w;
	int h = layer.h;
	int c = layer.c;
	pow_ongpu(w*h*c*layer.batch, -layer.beta, layer.norms_gpu, 1, state.delta, 1);
	mul_ongpu(w*h*c*layer.batch, layer.delta_gpu, 1, state.delta, 1);
}
#endif
static layer make_normalization_layer(int batch, int w, int h, int c, int size, float alpha, float beta, float kappa)
{
	layer layer = { 0 };
	layer.type = NORMALIZATION;
	layer.batch = batch;
	layer.h = layer.out_h = h;
	layer.w = layer.out_w = w;
	layer.c = layer.out_c = c;
	layer.kappa = kappa;
	layer.size = size;
	layer.alpha = alpha;
	layer.beta = beta;
	layer.output = calloc(h * w * c * batch, sizeof(float));
	layer.delta = calloc(h * w * c * batch, sizeof(float));
	layer.squared = calloc(h * w * c * batch, sizeof(float));
	layer.norms = calloc(h * w * c * batch, sizeof(float));
	layer.inputs = w * h*c;
	layer.outputs = layer.inputs;

	layer.forward = forward_normalization_layer;
	layer.backward = backward_normalization_layer;
#if 0 /* SOD_GPU */
	layer.forward_gpu = forward_normalization_layer_gpu;
	layer.backward_gpu = backward_normalization_layer_gpu;

	layer.output_gpu = cuda_make_array(layer.output, h * w * c * batch);
	layer.delta_gpu = cuda_make_array(layer.delta, h * w * c * batch);
	layer.squared_gpu = cuda_make_array(layer.squared, h * w * c * batch);
	layer.norms_gpu = cuda_make_array(layer.norms, h * w * c * batch);
#endif
	return layer;
}
static int parse_normalization(layer *l, list *options, size_params params)
{
	float alpha = option_find_float(options, "alpha", .0001);
	float beta = option_find_float(options, "beta", .75);
	float kappa = option_find_float(options, "kappa", 1);
	int size = option_find_int(options, "size", 5);
	*l = make_normalization_layer(params.batch, params.w, params.h, params.c, size, alpha, beta, kappa);
	return SOD_OK;
}
/* =============================================================== BATCHNORM  =============================================================== */
#if 0 /* SOD_GPU */

static void pull_batchnorm_layer(layer l)
{
	cuda_pull_array(l.scales_gpu, l.scales, l.c);
	cuda_pull_array(l.rolling_mean_gpu, l.rolling_mean, l.c);
	cuda_pull_array(l.rolling_variance_gpu, l.rolling_variance, l.c);
}
static void push_batchnorm_layer(layer l)
{
	cuda_push_array(l.scales_gpu, l.scales, l.c);
	cuda_push_array(l.rolling_mean_gpu, l.rolling_mean, l.c);
	cuda_push_array(l.rolling_variance_gpu, l.rolling_variance, l.c);
}
static void forward_batchnorm_layer_gpu(layer l, network_state state)
{
	if (l.type == BATCHNORM) copy_ongpu(l.outputs*l.batch, state.input, 1, l.output_gpu, 1);
	if (l.type == CONNECTED) {
		l.out_c = l.outputs;
		l.out_h = l.out_w = 1;
	}
	if (state.train) {
		fast_mean_gpu(l.output_gpu, l.batch, l.out_c, l.out_h*l.out_w, l.mean_gpu);
		fast_variance_gpu(l.output_gpu, l.mean_gpu, l.batch, l.out_c, l.out_h*l.out_w, l.variance_gpu);

		scal_ongpu(l.out_c, .99, l.rolling_mean_gpu, 1);
		axpy_ongpu(l.out_c, .01, l.mean_gpu, 1, l.rolling_mean_gpu, 1);
		scal_ongpu(l.out_c, .99, l.rolling_variance_gpu, 1);
		axpy_ongpu(l.out_c, .01, l.variance_gpu, 1, l.rolling_variance_gpu, 1);

		copy_ongpu(l.outputs*l.batch, l.output_gpu, 1, l.x_gpu, 1);
		normalize_gpu(l.output_gpu, l.mean_gpu, l.variance_gpu, l.batch, l.out_c, l.out_h*l.out_w);
		copy_ongpu(l.outputs*l.batch, l.output_gpu, 1, l.x_norm_gpu, 1);
	}
	else {
		normalize_gpu(l.output_gpu, l.rolling_mean_gpu, l.rolling_variance_gpu, l.batch, l.out_c, l.out_h*l.out_w);
	}

	scale_bias_gpu(l.output_gpu, l.scales_gpu, l.batch, l.out_c, l.out_h*l.out_w);
}
static void backward_batchnorm_layer_gpu(const layer l, network_state state)
{
	backward_scale_gpu(l.x_norm_gpu, l.delta_gpu, l.batch, l.out_c, l.out_w*l.out_h, l.scale_updates_gpu);

	scale_bias_gpu(l.delta_gpu, l.scales_gpu, l.batch, l.out_c, l.out_h*l.out_w);

	fast_mean_delta_gpu(l.delta_gpu, l.variance_gpu, l.batch, l.out_c, l.out_w*l.out_h, l.mean_delta_gpu);
	fast_variance_delta_gpu(l.x_gpu, l.delta_gpu, l.mean_gpu, l.variance_gpu, l.batch, l.out_c, l.out_w*l.out_h, l.variance_delta_gpu);
	normalize_delta_gpu(l.x_gpu, l.mean_gpu, l.variance_gpu, l.mean_delta_gpu, l.variance_delta_gpu, l.batch, l.out_c, l.out_w*l.out_h, l.delta_gpu);
	if (l.type == BATCHNORM) copy_ongpu(l.outputs*l.batch, l.delta_gpu, 1, state.delta, 1);
}
#endif
static layer make_batchnorm_layer(int batch, int w, int h, int c)
{
	layer layer = { 0 };
	layer.type = BATCHNORM;
	layer.batch = batch;
	layer.h = layer.out_h = h;
	layer.w = layer.out_w = w;
	layer.c = layer.out_c = c;
	layer.output = calloc(h * w * c * batch, sizeof(float));
	layer.delta = calloc(h * w * c * batch, sizeof(float));
	layer.inputs = w * h*c;
	layer.outputs = layer.inputs;

	layer.scales = calloc(c, sizeof(float));
	layer.scale_updates = calloc(c, sizeof(float));
	int i;
	for (i = 0; i < c; ++i) {
		layer.scales[i] = 1;
	}

	layer.mean = calloc(c, sizeof(float));
	layer.variance = calloc(c, sizeof(float));

	layer.rolling_mean = calloc(c, sizeof(float));
	layer.rolling_variance = calloc(c, sizeof(float));

	layer.forward = forward_batchnorm_layer;
	layer.backward = backward_batchnorm_layer;
#if 0 /* SOD_GPU */
	layer.forward_gpu = forward_batchnorm_layer_gpu;
	layer.backward_gpu = backward_batchnorm_layer_gpu;

	layer.output_gpu = cuda_make_array(layer.output, h * w * c * batch);
	layer.delta_gpu = cuda_make_array(layer.delta, h * w * c * batch);

	layer.scales_gpu = cuda_make_array(layer.scales, c);
	layer.scale_updates_gpu = cuda_make_array(layer.scale_updates, c);

	layer.mean_gpu = cuda_make_array(layer.mean, c);
	layer.variance_gpu = cuda_make_array(layer.variance, c);

	layer.rolling_mean_gpu = cuda_make_array(layer.mean, c);
	layer.rolling_variance_gpu = cuda_make_array(layer.variance, c);

	layer.mean_delta_gpu = cuda_make_array(layer.mean, c);
	layer.variance_delta_gpu = cuda_make_array(layer.variance, c);

	layer.x_gpu = cuda_make_array(layer.output, layer.batch*layer.outputs);
	layer.x_norm_gpu = cuda_make_array(layer.output, layer.batch*layer.outputs);
#endif
	return layer;
}
static int parse_batchnorm(layer *l, list *options, size_params params)
{
	(void)options; /* cc warn on unused var */
	*l = make_batchnorm_layer(params.batch, params.w, params.h, params.c);
	return SOD_OK;
}
/* =============================================================== MAXPOOL  =============================================================== */
typedef layer maxpool_layer;
static void forward_maxpool_layer(const maxpool_layer l, network_state state)
{
	int b, i, j, k, m, n;
	int w_offset = -l.pad;
	int h_offset = -l.pad;

	int h = l.out_h;
	int w = l.out_w;
	int c = l.c;

	for (b = 0; b < l.batch; ++b) {
		for (k = 0; k < c; ++k) {
			for (i = 0; i < h; ++i) {
				for (j = 0; j < w; ++j) {
					int out_index = j + w * (i + h * (k + c * b));
					float max = -FLT_MAX;
					int max_i = -1;
					for (n = 0; n < l.size; ++n) {
						for (m = 0; m < l.size; ++m) {
							int cur_h = h_offset + i * l.stride + n;
							int cur_w = w_offset + j * l.stride + m;
							int index = cur_w + l.w*(cur_h + l.h*(k + b * l.c));
							int valid = (cur_h >= 0 && cur_h < l.h &&
								cur_w >= 0 && cur_w < l.w);
							float val = (valid != 0) ? state.input[index] : -FLT_MAX;
							max_i = (val > max) ? index : max_i;
							max = (val > max) ? val : max;
						}
					}
					l.output[out_index] = max;
					l.indexes[out_index] = max_i;
				}
			}
		}
	}
}
static void backward_maxpool_layer(const maxpool_layer l, network_state state)
{
	int i;
	int h = l.out_h;
	int w = l.out_w;
	int c = l.c;
	for (i = 0; i < h*w*c*l.batch; ++i) {
		int index = l.indexes[i];
		state.delta[index] += l.delta[i];
	}
}
static maxpool_layer make_maxpool_layer(int batch, int h, int w, int c, int size, int stride, int padding)
{
	maxpool_layer l = { 0 };
	l.type = MAXPOOL;
	l.batch = batch;
	l.h = h;
	l.w = w;
	l.c = c;
	l.pad = padding;
	l.out_w = (w + 2 * padding) / stride;
	l.out_h = (h + 2 * padding) / stride;
	l.out_c = c;
	l.outputs = l.out_h * l.out_w * l.out_c;
	l.inputs = h * w*c;
	l.size = size;
	l.stride = stride;
	int output_size = l.out_h * l.out_w * l.out_c * batch;
	l.indexes = calloc(output_size, sizeof(int));
	l.output = calloc(output_size, sizeof(float));
	l.delta = calloc(output_size, sizeof(float));
	l.forward = forward_maxpool_layer;
	l.backward = backward_maxpool_layer;
#if 0 /* SOD_GPU */
	l.forward_gpu = forward_maxpool_layer_gpu;
	l.backward_gpu = backward_maxpool_layer_gpu;
	l.indexes_gpu = cuda_make_int_array(output_size);
	l.output_gpu = cuda_make_array(l.output, output_size);
	l.delta_gpu = cuda_make_array(l.delta, output_size);
#endif
	return l;
}
static int parse_maxpool(maxpool_layer *l, list *options, size_params params, sod_cnn *pNet)
{
	int stride = option_find_int(options, "stride", 1);
	int size = option_find_int(options, "size", stride);
	int padding = option_find_int(options, "padding", (size - 1) / 2);

	int batch, h, w, c;
	h = params.h;
	w = params.w;
	c = params.c;
	batch = params.batch;
	if (!(h && w && c)) {
		pNet->zErr = "Layer before maxpool layer must output image.";
		pNet->nErr++;
		return -1;
	}
	*l = make_maxpool_layer(batch, h, w, c, size, stride, padding);
	return SOD_OK;
}
/* =============================================================== REORG =============================================================== */
static void forward_reorg_layer(const layer l, network_state state)
{
	if (l.reverse) {
		reorg_cpu(state.input, l.w, l.h, l.c, l.batch, l.stride, 1, l.output);
	}
	else {
		reorg_cpu(state.input, l.w, l.h, l.c, l.batch, l.stride, 0, l.output);
	}
}
static void backward_reorg_layer(const layer l, network_state state)
{
	if (l.reverse) {
		reorg_cpu(l.delta, l.w, l.h, l.c, l.batch, l.stride, 0, state.delta);
	}
	else {
		reorg_cpu(l.delta, l.w, l.h, l.c, l.batch, l.stride, 1, state.delta);
	}
}
#if 0 /* SOD_GPU */
static void forward_reorg_layer_gpu(layer l, network_state state)
{
	if (l.reverse) {
		reorg_ongpu(state.input, l.w, l.h, l.c, l.batch, l.stride, 1, l.output_gpu);
	}
	else {
		reorg_ongpu(state.input, l.w, l.h, l.c, l.batch, l.stride, 0, l.output_gpu);
	}
}
static void backward_reorg_layer_gpu(layer l, network_state state)
{
	if (l.reverse) {
		reorg_ongpu(l.delta_gpu, l.w, l.h, l.c, l.batch, l.stride, 0, state.delta);
	}
	else {
		reorg_ongpu(l.delta_gpu, l.w, l.h, l.c, l.batch, l.stride, 1, state.delta);
	}
}
#endif
static layer make_reorg_layer(int batch, int w, int h, int c, int stride, int reverse)
{
	layer l = { 0 };
	l.type = REORG;
	l.batch = batch;
	l.stride = stride;
	l.h = h;
	l.w = w;
	l.c = c;
	if (reverse) {
		l.out_w = w * stride;
		l.out_h = h * stride;
		l.out_c = c / (stride*stride);
	}
	else {
		l.out_w = w / stride;
		l.out_h = h / stride;
		l.out_c = c * (stride*stride);
	}
	l.reverse = reverse;

	l.outputs = l.out_h * l.out_w * l.out_c;
	l.inputs = h * w*c;
	int output_size = l.out_h * l.out_w * l.out_c * batch;
	l.output = calloc(output_size, sizeof(float));
	l.delta = calloc(output_size, sizeof(float));

	l.forward = forward_reorg_layer;
	l.backward = backward_reorg_layer;
#if 0 /* SOD_GPU */
	l.forward_gpu = forward_reorg_layer_gpu;
	l.backward_gpu = backward_reorg_layer_gpu;

	l.output_gpu = cuda_make_array(l.output, output_size);
	l.delta_gpu = cuda_make_array(l.delta, output_size);
#endif
	return l;
}
static int parse_reorg(layer *l, list *options, size_params params, sod_cnn *pNet)
{
	int stride = option_find_int(options, "stride", 1);
	int reverse = option_find_int(options, "reverse", 0);

	int batch, h, w, c;
	h = params.h;
	w = params.w;
	c = params.c;
	batch = params.batch;
	if (!(h && w && c)) {
		pNet->zErr = "Layer before reorg layer must output image.";
		pNet->nErr++;
		return -1;
	}
	*l = make_reorg_layer(batch, w, h, c, stride, reverse);
	return SOD_OK;
}
/* =============================================================== AVGPOOL =============================================================== */
typedef layer avgpool_layer;
static void forward_avgpool_layer(const avgpool_layer l, network_state state)
{
	int b, i, k;

	for (b = 0; b < l.batch; ++b) {
		for (k = 0; k < l.c; ++k) {
			int out_index = k + b * l.c;
			l.output[out_index] = 0;
			for (i = 0; i < l.h*l.w; ++i) {
				int in_index = i + l.h*l.w*(k + b * l.c);
				l.output[out_index] += state.input[in_index];
			}
			l.output[out_index] /= l.h*l.w;
		}
	}
}
static void backward_avgpool_layer(const avgpool_layer l, network_state state)
{
	int b, i, k;

	for (b = 0; b < l.batch; ++b) {
		for (k = 0; k < l.c; ++k) {
			int out_index = k + b * l.c;
			for (i = 0; i < l.h*l.w; ++i) {
				int in_index = i + l.h*l.w*(k + b * l.c);
				state.delta[in_index] += l.delta[out_index] / (l.h*l.w);
			}
		}
	}
}
static avgpool_layer make_avgpool_layer(int batch, int w, int h, int c)
{
	avgpool_layer l = { 0 };
	l.type = AVGPOOL;
	l.batch = batch;
	l.h = h;
	l.w = w;
	l.c = c;
	l.out_w = 1;
	l.out_h = 1;
	l.out_c = c;
	l.outputs = l.out_c;
	l.inputs = h * w*c;
	int output_size = l.outputs * batch;
	l.output = calloc(output_size, sizeof(float));
	l.delta = calloc(output_size, sizeof(float));
	l.forward = forward_avgpool_layer;
	l.backward = backward_avgpool_layer;
#if 0 /* SOD_GPU */
	l.forward_gpu = forward_avgpool_layer_gpu;
	l.backward_gpu = backward_avgpool_layer_gpu;
	l.output_gpu = cuda_make_array(l.output, output_size);
	l.delta_gpu = cuda_make_array(l.delta, output_size);
#endif
	return l;
}
static int parse_avgpool(avgpool_layer *l, list *options, size_params params, sod_cnn *pNet)
{
	int batch, w, h, c;
	w = params.w;
	h = params.h;
	c = params.c;
	batch = params.batch;
	if (!(h && w && c)) {
		pNet->zErr = "Layer before avgpool layer must output image.";
		pNet->nErr++;
		options = 0; /* cc warn on unused var */
		return -1;
	}
	*l = make_avgpool_layer(batch, w, h, c);

	return SOD_OK;
}
/* =============================================================== ROUTE =============================================================== */
typedef layer route_layer;
static void forward_route_layer(const route_layer l, network_state state)
{
	int i, j;
	int offset = 0;
	for (i = 0; i < l.n; ++i) {
		int index = l.input_layers[i];
		float *input = state.net->layers[index].output;
		int input_size = l.input_sizes[i];
		for (j = 0; j < l.batch; ++j) {
			copy_cpu(input_size, input + j * input_size, 1, l.output + offset + j * l.outputs, 1);
		}
		offset += input_size;
	}
}
static void backward_route_layer(const route_layer l, network_state state)
{
	int i, j;
	int offset = 0;
	for (i = 0; i < l.n; ++i) {
		int index = l.input_layers[i];
		float *delta = state.net->layers[index].delta;
		int input_size = l.input_sizes[i];
		for (j = 0; j < l.batch; ++j) {
			axpy_cpu(input_size, 1, l.delta + offset + j * l.outputs, 1, delta + j * input_size, 1);
		}
		offset += input_size;
	}
}
#if 0 /* SOD_GPU */
static void forward_route_layer_gpu(const route_layer l, network_state state)
{
	int i, j;
	int offset = 0;
	for (i = 0; i < l.n; ++i) {
		int index = l.input_layers[i];
		float *input = state.net.layers[index].output_gpu;
		int input_size = l.input_sizes[i];
		for (j = 0; j < l.batch; ++j) {
			copy_ongpu(input_size, input + j * input_size, 1, l.output_gpu + offset + j * l.outputs, 1);
		}
		offset += input_size;
	}
}
static void backward_route_layer_gpu(const route_layer l, network_state state)
{
	int i, j;
	int offset = 0;
	for (i = 0; i < l.n; ++i) {
		int index = l.input_layers[i];
		float *delta = state.net.layers[index].delta_gpu;
		int input_size = l.input_sizes[i];
		for (j = 0; j < l.batch; ++j) {
			axpy_ongpu(input_size, 1, l.delta_gpu + offset + j * l.outputs, 1, delta + j * input_size, 1);
		}
		offset += input_size;
	}
}
#endif
static route_layer make_route_layer(int batch, int n, int *input_layers, int *input_sizes)
{

	route_layer l = { 0 };
	l.type = ROUTE;
	l.batch = batch;
	l.n = n;
	l.input_layers = input_layers;
	l.input_sizes = input_sizes;
	int i;
	int outputs = 0;
	for (i = 0; i < n; ++i) {

		outputs += input_sizes[i];
	}

	l.outputs = outputs;
	l.inputs = outputs;
	l.delta = calloc(outputs*batch, sizeof(float));
	l.output = calloc(outputs*batch, sizeof(float));;

	l.forward = forward_route_layer;
	l.backward = backward_route_layer;
#if 0 /* SOD_GPU */
	l.forward_gpu = forward_route_layer_gpu;
	l.backward_gpu = backward_route_layer_gpu;

	l.delta_gpu = cuda_make_array(l.delta, outputs*batch);
	l.output_gpu = cuda_make_array(l.output, outputs*batch);
#endif
	return l;
}
static int parse_route(route_layer *lr, list *options, size_params params, network *net, sod_cnn *pNet)
{
	char *l = option_find(options, "layers");
	if (!l) {
		pNet->zErr = "Route Layer must specify input layers";
		pNet->nErr++;
		return -1;
	}
	int len = (int)strlen(l);
	int n = 1;
	int i;
	for (i = 0; i < len; ++i) {
		if (l[i] == ',') ++n;
	}

	int *layers = calloc(n, sizeof(int));
	int *sizes = calloc(n, sizeof(int));
	for (i = 0; i < n; ++i) {
		int index = atoi(l);
		l = strchr(l, ',') + 1;
		if (index < 0) index = params.index + index;
		layers[i] = index;
		sizes[i] = net->layers[index].outputs;
	}
	int batch = params.batch;

	route_layer layer = make_route_layer(batch, n, layers, sizes);

	convolutional_layer first = net->layers[layers[0]];
	layer.out_w = first.out_w;
	layer.out_h = first.out_h;
	layer.out_c = first.out_c;
	for (i = 1; i < n; ++i) {
		int index = layers[i];
		convolutional_layer next = net->layers[index];
		if (next.out_w == first.out_w && next.out_h == first.out_h) {
			layer.out_c += next.out_c;
		}
		else {
			layer.out_h = layer.out_w = layer.out_c = 0;
		}
	}
	*lr = layer;
	return SOD_OK;
}
/* =============================================================== SHORTCUT =============================================================== */
static void forward_shortcut_layer(const layer l, network_state state)
{
	copy_cpu(l.outputs*l.batch, state.input, 1, l.output, 1);
	shortcut_cpu(l.batch, l.w, l.h, l.c, state.net->layers[l.index].output, l.out_w, l.out_h, l.out_c, l.output);
	activate_array(l.output, l.outputs*l.batch, l.activation);
}
static void backward_shortcut_layer(const layer l, network_state state)
{
	gradient_array(l.output, l.outputs*l.batch, l.activation, l.delta);
	axpy_cpu(l.outputs*l.batch, 1, l.delta, 1, state.delta, 1);
	shortcut_cpu(l.batch, l.out_w, l.out_h, l.out_c, l.delta, l.w, l.h, l.c, state.net->layers[l.index].delta);
}

#if 0 /* SOD_GPU */
static void forward_shortcut_layer_gpu(const layer l, network_state state)
{
	copy_ongpu(l.outputs*l.batch, state.input, 1, l.output_gpu, 1);
	shortcut_gpu(l.batch, l.w, l.h, l.c, state.net.layers[l.index].output_gpu, l.out_w, l.out_h, l.out_c, l.output_gpu);
	activate_array_ongpu(l.output_gpu, l.outputs*l.batch, l.activation);
}
static void backward_shortcut_layer_gpu(const layer l, network_state state)
{
	gradient_array_ongpu(l.output_gpu, l.outputs*l.batch, l.activation, l.delta_gpu);
	axpy_ongpu(l.outputs*l.batch, 1, l.delta_gpu, 1, state.delta, 1);
	shortcut_gpu(l.batch, l.out_w, l.out_h, l.out_c, l.delta_gpu, l.w, l.h, l.c, state.net.layers[l.index].delta_gpu);
}
#endif
static layer make_shortcut_layer(int batch, int index, int w, int h, int c, int w2, int h2, int c2)
{
	layer l = { 0 };
	l.type = SHORTCUT;
	l.batch = batch;
	l.w = w2;
	l.h = h2;
	l.c = c2;
	l.out_w = w;
	l.out_h = h;
	l.out_c = c;
	l.outputs = w * h*c;
	l.inputs = l.outputs;

	l.index = index;

	l.delta = calloc(l.outputs*batch, sizeof(float));
	l.output = calloc(l.outputs*batch, sizeof(float));;

	l.forward = forward_shortcut_layer;
	l.backward = backward_shortcut_layer;
#if 0 /* SOD_GPU */
	l.forward_gpu = forward_shortcut_layer_gpu;
	l.backward_gpu = backward_shortcut_layer_gpu;

	l.delta_gpu = cuda_make_array(l.delta, l.outputs*batch);
	l.output_gpu = cuda_make_array(l.output, l.outputs*batch);
#endif
	return l;
}
static int parse_shortcut(layer *rl, list *options, size_params params, network *net)
{
	char *l = option_find(options, "from");
	int index = atoi(l);
	if (index < 0) index = params.index + index;

	int batch = params.batch;
	layer from = net->layers[index];

	*rl = make_shortcut_layer(batch, index, params.w, params.h, params.c, from.out_w, from.out_h, from.out_c);
	rl->activation = get_activation(option_find_str(options, "activation", "linear"));

	return SOD_OK;
}
/* =============================================================== DROPOUT =============================================================== */
typedef layer dropout_layer;
static void forward_dropout_layer(dropout_layer l, network_state state)
{
	int i;
	if (!state.train) return;
	for (i = 0; i < l.batch * l.inputs; ++i) {
		float r = rand_uniform(0, 1);
		l.rand[i] = r;
		if (r < l.probability) state.input[i] = 0;
		else state.input[i] *= l.scale;
	}
}
static void backward_dropout_layer(dropout_layer l, network_state state)
{
	int i;
	if (!state.delta) return;
	for (i = 0; i < l.batch * l.inputs; ++i) {
		float r = l.rand[i];
		if (r < l.probability) state.delta[i] = 0;
		else state.delta[i] *= l.scale;
	}
}
static dropout_layer make_dropout_layer(int batch, int inputs, float probability)
{
	dropout_layer l = { 0 };
	l.type = DROPOUT;
	l.probability = probability;
	l.inputs = inputs;
	l.outputs = inputs;
	l.batch = batch;
	l.rand = calloc(inputs*batch, sizeof(float));
	l.scale = 1. / (1. - probability);
	l.forward = forward_dropout_layer;
	l.backward = backward_dropout_layer;
#if 0 /* SOD_GPU */
	l.forward_gpu = forward_dropout_layer_gpu;
	l.backward_gpu = backward_dropout_layer_gpu;
	l.rand_gpu = cuda_make_array(l.rand, inputs*batch);
#endif

	return l;
}
static int parse_dropout(dropout_layer *l, list *options, size_params params)
{
	float probability = option_find_float(options, "probability", .5);
	*l = make_dropout_layer(params.batch, params.inputs, probability);
	l->out_w = params.w;
	l->out_h = params.h;
	l->out_c = params.c;
	return SOD_OK;
}
/*
* Network architecture Parser & Processor.
*/
static int parse_network_cfg(const char *zConf, sod_cnn *pNet)
{
	list *sections = read_cfg(zConf);
	node *n = sections->front;
	network net;
	size_params params;
	section *s;
	list *options;
	layer l;

	if (!n) {
		pNet->nErr++;
		pNet->zErr = "Model has no sections";
		free_list(sections);
		return SOD_UNSUPPORTED;
	}
	make_network(sections->size - 1, &net);
	net.pNet = pNet;
	net.gpu_index = 0;

	s = (section *)n->val;
	options = s->options;
	if (!is_network(s)) {
		pNet->nErr++;
		pNet->zErr = "First section must be [net] or [network]";
		free_section(s);
		free_list(sections);
		return -1;
	}
	parse_net_options(options, &net);

	params.h = net.h;
	params.w = net.w;
	params.c = net.c;
	params.inputs = net.inputs;
	params.batch = net.batch;
	params.time_steps = net.time_steps;
	params.net = net;

	size_t workspace_size = 0;
	n = n->next;
	int count = 0;
	free_section(s);

	while (n) {
		memset(&l, 0, sizeof(layer));
		params.index = count;
		s = (section *)n->val;
		options = s->options;

		SOD_CNN_LAYER_TYPE lt = string_to_layer_type(s->type);
		switch (lt) {
		case CONVOLUTIONAL: {
			parse_convolutional(&l, options, params, pNet);
		}
							break;
		case LOCAL: {
			parse_local(&l, options, params, pNet);
		}
					break;
		case ACTIVE: {
			parse_activation(&l, options, params);
		}
					 break;
		case RNN: {
			parse_rnn(&l, options, params);
			pNet->flags |= SOD_LAYER_RNN;
		}
				  break;
		case GRU: {
			parse_gru(&l, options, params);
		}
				  break;
		case CRNN: {
			parse_crnn(&l, options, params);
		}
				   break;
		case CONNECTED: {
			parse_connected(&l, options, params);
		}
						break;
		case CROP: {
			parse_crop(&l, options, params, pNet);
		}
				   break;
		case COST: {
			parse_cost(&l, options, params);
		}
				   break;
		case REGION: {
			parse_region(&l, options, params);
		}
					 break;
		case DETECTION: {
			parse_detection(&l, options, params);
		}
						break;

		case SOFTMAX: {
			parse_softmax(&l, options, params);
			net.hierarchy = l.softmax_tree;
		}
					  break;
		case NORMALIZATION: {
			parse_normalization(&l, options, params);
		}
							break;
		case BATCHNORM: {
			parse_batchnorm(&l, options, params);
		}
						break;
		case MAXPOOL: {
			parse_maxpool(&l, options, params, pNet);
		}
					  break;
		case REORG: {
			parse_reorg(&l, options, params, pNet);
		}
					break;
		case AVGPOOL: {
			parse_avgpool(&l, options, params, pNet);
		}
					  break;
		case ROUTE: {
			parse_route(&l, options, params, &net, pNet);
		}
					break;
		case SHORTCUT: {
			parse_shortcut(&l, options, params, &net);
		}
					   break;
		case DROPOUT: {
			parse_dropout(&l, options, params);
			l.output = net.layers[count - 1].output;
			l.delta = net.layers[count - 1].delta;
#if 0 /* SOD_GPU */
			l.output_gpu = net.layers[count - 1].output_gpu;
			l.delta_gpu = net.layers[count - 1].delta_gpu;
#endif
		}
					  break;
		default:
			/* Unknown layer, simply ignore */
			break;
		}
		l.dontload = option_find_int(options, "dontload", 0);
		l.dontloadscales = option_find_int(options, "dontloadscales", 0);
		net.layers[count] = l;
		if (l.workspace_size > workspace_size) workspace_size = l.workspace_size;
		free_section(s);
		n = n->next;
		++count;
		if (n) {
			params.h = l.out_h;
			params.w = l.out_w;
			params.c = l.out_c;
			params.inputs = l.outputs;
		}
	}
	free_list(sections);
	net.outputs = get_network_output_size(&net);
	net.output = get_network_output(&net);
	net.workspace_size = workspace_size;
	if (workspace_size) {
#if 0 /* SOD_GPU */
		if (gpu_index >= 0) {
			net.workspace = cuda_make_array(0, (workspace_size - 1) / sizeof(float) + 1);
		}
		else {
			net.workspace = calloc(1, workspace_size);
		}
#else
		net.workspace = calloc(1, workspace_size);
#endif
	}
	pNet->net = net;
	return pNet->nErr > 0 ? SOD_ABORT : SOD_OK;
}
/*
* Convolutional/Recurrent Neural Networks (CNN/RNN) Exported Interfaces.
*/
/*
* Each magic word is defined by an instance of the following structure.
*/
struct sod_cnn_magic
{
	const char *zMagic; /* Magic work */
	const char *zModel; /* Associated model */
	const char **azNameSet; /* Class names */
};
/*
* Seed a RNN network.
*/
static void prepare_rnn_seed(sod_cnn *pNet, int len) {
	int i;
	for (i = 0; i < len - 1; ++i) {
		pNet->c_rnn = pNet->zRnnSeed[i];
		pNet->aInput[pNet->c_rnn] = 1;
		network_predict(&pNet->net, pNet->aInput);
		pNet->aInput[pNet->c_rnn] = 0;
	}
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
int sod_cnn_create(sod_cnn **ppOut, const char *zArch, const char *zModelPath, const char **pzErr)
{
	sod_cnn *pNet = malloc(sizeof(sod_cnn));
	void *pMap = 0;
	size_t sz;
	int rc, j;
	if (pNet == 0 || zArch == 0 || zArch[0] == 0) {
		if (pzErr) {
			*pzErr = "Out of Memory/Empty network architecture";
		}
		*ppOut = 0;
		free(pNet);
		return SOD_OUTOFMEM;
	}
	*ppOut = pNet;
	/* Zero */
	memset(pNet, 0, sizeof(sod_cnn));
	/* Export the built-in VFS */
	pNet->pVfs = sodExportBuiltinVfs();
	srand((unsigned int)pNet->pVfs->xTicks());
	/* Check if we are dealing with a memory buffer or with a file path */
	while (isspace(zArch[0])) zArch++;
	/* Assume a null terminated memory buffer by default */
	sz = strlen(zArch);
	if (zArch[0] == ':') {
		/* Magic word */
		static const struct sod_cnn_magic aMagic[] = {
			{ "fast",  zTinyVoc, zVoc },
			{ "voc",  zTinyVoc, zVoc },
			{ "tiny80", zTiny, zCoco },
			{ "coco", zTiny, zCoco },
			{ "tiny", zTiny, zCoco },
			{ "full", zYolo,   zCoco },
			{ "yolo", zYolo,   zCoco },
			{ "face", zfaceCnn, zFace },
			{ "rnn",  zRnn,     0 },
			{ 0, 0, 0 } /* Marker */
		};
		for (j = 0; aMagic[j].zMagic != 0; j++) {
			if (sy_strcmp(aMagic[j].zMagic, &zArch[1]) == 0) {
				zArch = aMagic[j].zModel;
				pNet->azNames = aMagic[j].azNameSet;
				break;
			}
		}
		if (aMagic[j].zModel == 0) {
			if (pzErr) *pzErr = "No such model for the given magic keyword";
			*ppOut = 0;
			free(pNet);
			return SOD_UNSUPPORTED;
		}
	}
	else if (zArch[0] != '[' || zArch[0] != '#' || sz < 170) {
		/* Assume a file path, open read-only  */
		rc = pNet->pVfs->xMmap(zArch, &pMap, &sz);
		if (rc != SOD_OK) {
			if (pzErr) *pzErr = "Error loading network architecture file";
			*ppOut = 0;
			free(pNet);
			return SOD_IOERR;
		}
		zArch = (const char *)pMap;
	}
	/* Parse the network architecture */
	rc = parse_network_cfg(zArch, pNet);
	if (pMap) {
		pNet->pVfs->xUnmap((void *)zArch, (int64_t)sz);
	}
	if (rc != SOD_OK) {
		goto fail;
	}
	if (zModelPath) {
		rc = load_weights(&pNet->net, zModelPath);
		if (rc != SOD_OK) {
			goto fail;
		}
	}
	/* Fill with default configuration */
	SySetInit(&pNet->aBoxes, sizeof(sod_box));
	SySetAlloc(&pNet->aBoxes, 8);
	SyBlobInit(&pNet->sRnnConsumer);
	SyBlobInit(&pNet->sLogConsumer);
	pNet->det = pNet->net.layers[pNet->net.n - 1];
	if ((pNet->flags & SOD_LAYER_RNN) == 0) {
		set_batch_network(&pNet->net, 1);
	}
	pNet->nInput = get_network_input_size(&pNet->net);
	pNet->nms = .45;
	pNet->thresh = .24;
	pNet->hier_thresh = .5;
	pNet->temp = .7;
#define S_RNN_SEED "\n\n"
	pNet->zRnnSeed = S_RNN_SEED;
	pNet->rnn_gen_len = 100;
	if (pNet->det.classes > 0) {
		pNet->sRz = sod_make_image(pNet->net.w, pNet->net.h, pNet->net.c);
		if (pNet->det.type == REGION || pNet->det.type == DETECTION) {
			pNet->boxes = calloc(pNet->det.w*pNet->det.h*pNet->det.n, sizeof(box));
		}
		pNet->probs = calloc(pNet->det.w*pNet->det.h*pNet->det.n, sizeof(float *));
		for (j = 0; j < pNet->det.w*pNet->det.h*pNet->det.n; ++j) pNet->probs[j] = calloc(pNet->det.classes + 1, sizeof(float));
	}
	if (pNet->flags & SOD_LAYER_RNN) {
		int i;
		for (i = 0; i < pNet->net.n; ++i) {
			pNet->net.layers[i].temperature = pNet->temp;
		}
		pNet->aInput = (float *)calloc(pNet->nInput, sizeof(float));
		/*prepare_rnn_seed(pNet, sizeof(S_RNN_SEED) - 1);*/
		pNet->c_rnn = pNet->zRnnSeed[sizeof(S_RNN_SEED) - 2];
	}
	pNet->state = SOD_NET_STATE_READY;
	return SOD_OK;
fail:
	if (pzErr) {
		*pzErr = pNet->zErr ? pNet->zErr : "Invalid network model";
	}
	*ppOut = 0;
	free_network(&pNet->net);
	free(pNet);
#ifdef SOD_MEM_DEBUG
	_CrtDumpMemoryLeaks();
#endif /* SOD_MEM_DEBUG */
	return rc;
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
int sod_cnn_config(sod_cnn * pNet, SOD_CNN_CONFIG conf, ...)
{
	int rc = SOD_OK;
	va_list ap;
	va_start(ap, conf);
	switch (conf) {
	case SOD_CNN_NETWORK_OUTPUT: {
		float **vector = va_arg(ap, float **);
		int *pCount = va_arg(ap, int *);
		*vector = pNet->pOut;
		if (pCount) {
			*pCount = pNet->nOuput;
		}
	}
		break;
	case SOD_CNN_LOG_CALLBACK: {
		/* Log callback */
		ProcLogCallback xLog = va_arg(ap, ProcLogCallback);
		void *pLogData = va_arg(ap, void *);
		/* Register the callback */
		pNet->xLog = xLog;
		pNet->pLogData = pLogData;
	}
			break;
	case SOD_CNN_DETECTION_THRESHOLD: {
		double thresh = va_arg(ap, double);
		pNet->thresh = (float)thresh;
	}
			break;
	case SOD_CNN_NMS: {
		double nms = va_arg(ap, double);
		pNet->nms = (float)nms;
	}
									  break;
	case SOD_RNN_CALLBACK: {
		/* RNN callback */
		ProcRnnCallback xCB = va_arg(ap, ProcRnnCallback);
		void *pUserdata = va_arg(ap, void *);
		/* Register the callback */
		pNet->xRnn = xCB;
		pNet->pRnnData = pUserdata;
	}
						   break;
	case SOD_RNN_TEXT_LENGTH:
	case SOD_RNN_DATA_LENGTH: {
		/* Maximum text length to be generated */
		int len = va_arg(ap, int);
		if (len > 0) {
			pNet->rnn_gen_len = len;
		}
	}
							  break;
	case SOD_RNN_SEED: {
		/* Seed for the rnn */
		const char *zSeed = va_arg(ap, const char *); /* Must be null terminated */
		if (zSeed) {
			int len = (int)strlen(zSeed);
			if (len > 0) {
				pNet->zRnnSeed = zSeed;
				prepare_rnn_seed(pNet, len);
				pNet->c_rnn = pNet->zRnnSeed[len - 1];
			}
		}
	}
					   break;
	case SOD_CNN_TEMPERATURE: {
		double temp = va_arg(ap, double);
		int i;
		pNet->temp = (float)temp;
		for (i = 0; i < pNet->net.n; ++i) {
			pNet->net.layers[i].temperature = pNet->temp;
		}
	}
							  break;
	default:
		rc = SOD_UNSUPPORTED;
		break;
	}
	va_end(ap);
	return rc;
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
void sod_cnn_destroy(sod_cnn *pNet)
{
	if (pNet->state > 0) {
		if (pNet->probs) {
			int j;
			for (j = 0; j < pNet->det.w*pNet->det.h*pNet->det.n; ++j) free(pNet->probs[j]);
			free(pNet->probs);
		}
		if (pNet->boxes) {
			free(pNet->boxes);
		}
		if (pNet->aInput) {
			free(pNet->aInput);
		}
		free_network(&pNet->net);
		SySetRelease(&pNet->aBoxes);
		SyBlobRelease(&pNet->sRnnConsumer);
		SyBlobRelease(&pNet->sLogConsumer);
		sod_free_image(pNet->sTmpim);
		sod_free_image(pNet->sRz);
		sod_free_image(pNet->sPart);
		free(pNet);
	}
#ifdef SOD_MEM_DEBUG
	_CrtDumpMemoryLeaks();
#endif /* SOD_MEM_DEBUG */
}
/* Forward declaration */
static void sodFastImageResize(sod_img im, sod_img resized, sod_img part, int w, int h);
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
float * sod_cnn_prepare_image(sod_cnn *pNet, sod_img in)
{
	sod_img *pCur;
	if (pNet->state != SOD_NET_STATE_READY) {
		return 0;
	}
	if (pNet->net.w < 1 && pNet->net.h < 1) {
		/* Not a detection network */
		return 0;
	}
	if (in.c != pNet->net.c) {
		/* Must comply with the trained channels for this network */
		return 0;
	}
	pNet->ow = in.w;
	pNet->oh = in.h;
	pCur = &pNet->sRz;
	if (in.h != pNet->net.h || in.w != pNet->net.w) {
		sod_md_alloc_dyn_img(pCur, pNet->net.w, pNet->net.h, pNet->net.c);
		sod_md_alloc_dyn_img(&pNet->sPart, pNet->net.w, in.h, pNet->net.c);
		sodFastImageResize(in, pNet->sRz, pNet->sPart, pNet->net.w, pNet->net.h);
	}
	return pCur->data;
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
int sod_cnn_get_network_size(sod_cnn * pNet, int * pWidth, int * pHeight, int * pChannels)
{
	if (pNet->state != SOD_NET_STATE_READY) {
		return -1;
	}
	if (pWidth) *pWidth = pNet->net.w;
	if (pHeight) *pHeight = pNet->net.h;
	if (pChannels) *pChannels = pNet->net.c;
	return SOD_OK;
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
int sod_cnn_predict(sod_cnn * pNet, float * pInput, sod_box **paBox, int *pnBox)
{
	int i, j, nRun = 1;
	if (pNet->flags & SOD_LAYER_RNN) {
		pInput = pNet->aInput;
		nRun = pNet->rnn_gen_len;
		SyBlobReset(&pNet->sRnnConsumer);
	}
	for (i = 0; i < nRun; ++i) {
		if (pNet->flags & SOD_LAYER_RNN) {
			pNet->aInput[pNet->c_rnn] = 1;
		}
		pNet->pOut = network_predict(&pNet->net, pInput);
		if (pNet->flags & SOD_LAYER_RNN) {
			pNet->aInput[pNet->c_rnn] = 0;
			for (j = 0; j < pNet->nInput; ++j) {
				if (pNet->pOut[j] < .0001) pNet->pOut[j] = 0;
			}
			pNet->c_rnn = sample_array(pNet->pOut, pNet->nInput);
			/* Append character output */
			SyBlobAppend(&pNet->sRnnConsumer, (const void *)&pNet->c_rnn, sizeof(char));
		}
	}
	if (pnBox) {
		sod_box sBox;
		SySetReset(&pNet->aBoxes);
		if (pNet->det.classes > 0) {
			if (pNet->det.type == REGION) {
				get_region_boxes(pNet->det, 1, 1, pNet->thresh, pNet->probs, pNet->boxes, 0, 0, pNet->hier_thresh);
				if (pNet->det.softmax_tree && pNet->nms) {
					do_nms_obj(pNet->boxes, pNet->probs, pNet->det.w*pNet->det.h*pNet->det.n, pNet->det.classes, pNet->nms);
				}
				else if (pNet->nms) {
					do_nms_sort(pNet->boxes, pNet->probs, pNet->det.w*pNet->det.h*pNet->det.n, pNet->det.classes, pNet->nms);
				}
			}
			else if (pNet->det.type == DETECTION) {
				get_detection_boxes(pNet->det, 1, 1, pNet->thresh, pNet->probs, pNet->boxes, 0);
				if (pNet->nms) {
					do_nms_sort(pNet->boxes, pNet->probs, pNet->det.side*pNet->det.side*pNet->det.n, pNet->det.classes, pNet->nms);
				}
			}
			for (i = 0; i < pNet->det.w*pNet->det.h*pNet->det.n; ++i) {
				float max = pNet->probs[i][0];
				int class = 0, v;
				float prob;
				for (v = 1; v < pNet->det.classes; ++v) {
					if (pNet->probs[i][v] > max) {
						max = pNet->probs[i][v];
						class = v;
					}
				}
				prob = pNet->probs[i][class];
				if (prob > pNet->thresh) {
					box b = pNet->boxes[i];
					int left = (b.x - b.w / 2.)*pNet->ow;
					int top = (b.y - b.h / 2.)*pNet->oh;
					int right = (b.x + b.w / 2.)*pNet->ow;
					int bot = (b.y + b.h / 2.)*pNet->oh;
					if (left < 0) left = 0;
					if (top < 0) top = 0;
					if (right > pNet->ow - 1) right = pNet->ow - 1;
					if (bot > pNet->oh - 1) bot = pNet->oh - 1;
					sBox.score = prob;
					sBox.x = left;
					sBox.y = top;
					sBox.w = right - left;
					sBox.h = bot - top;
					if (pNet->azNames) {
						/* WARNING: azNames[] must hold at least n 'class' entries. This is fine with the
						* built-in magic words such as :tiny, :full, etc. Otherwise, expect a SEGFAULT.
						*/
						sBox.zName = pNet->azNames[class];
					}
					else {
						sBox.zName = "object";
					}
					sBox.pUserData = 0;
					/* Insert in the set */
					SySetPut(&pNet->aBoxes, &sBox);

				}
			}
		}
		if (paBox) {
			*paBox = (sod_box *)SySetBasePtr(&pNet->aBoxes);
		}
		*pnBox = (int)SySetUsed(&pNet->aBoxes);
	}
	if (pNet->flags & SOD_LAYER_RNN) {
		SyBlobNullAppend(&pNet->sRnnConsumer); /* Append the null terminator */
		if (pNet->xRnn) {
			/* Run the user callback */
			pNet->xRnn((const char *)SyBlobData(&pNet->sRnnConsumer), SyBlobLength(&pNet->sRnnConsumer), pNet->pRnnData);
		}
	}
	return SOD_OK;
}
#endif /* SOD_DISABLE_CNN */
/*
* Image Processing Interfaces.
*/
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
sod_img sod_make_empty_image(int w, int h, int c)
{
	sod_img out;
	out.data = 0;
	out.h = h;
	out.w = w;
	out.c = c;
	return out;
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
sod_img sod_make_image(int w, int h, int c)
{
	sod_img out = sod_make_empty_image(w, h, c);
	out.data = calloc(h*w*c, sizeof(float));
	return out;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_grow_image(sod_img *pImg, int w, int h, int c)
{
	sod_md_alloc_dyn_img(&(*pImg), w, h, c);
	return pImg->data == 0 ? SOD_OUTOFMEM : SOD_OK;
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
sod_img sod_make_random_image(int w, int h, int c)
{
	sod_img out = sod_make_empty_image(w, h, c);
	out.data = calloc(h*w*c, sizeof(float));
	if (out.data) {
		int i;
		for (i = 0; i < w*h*c; ++i) {
			out.data[i] = (rand_normal() * .25) + .5;
		}
	}
	return out;
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
sod_img sod_img_get_layer(sod_img m, int l)
{
	sod_img out = sod_make_image(m.w, m.h, 1);
	int i;
	if (out.data && l >= 0 && l < m.c) {
		for (i = 0; i < m.h*m.w; ++i) {
			out.data[i] = m.data[i + l * m.h*m.w];
		}
	}
	return out;
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
sod_img sod_copy_image(sod_img m)
{
	sod_img copy = m;
	copy.data = calloc(m.h*m.w*m.c, sizeof(float));
	if (copy.data && m.data) {
		memcpy(copy.data, m.data, m.h*m.w*m.c * sizeof(float));
	}
	return copy;
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
void sod_free_image(sod_img m)
{
	if (m.data) {
		free(m.data);
	}
}
static inline float get_pixel(sod_img m, int x, int y, int c)
{
	return (m.data ? m.data[c*m.h*m.w + y * m.w + x] : 0.0f);
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
float sod_img_get_pixel(sod_img m, int x, int y, int c)
{
	if (x < 0) x = 0;
	if (x >= m.w) x = m.w - 1;
	if (y < 0) y = 0;
	if (y >= m.h) y = m.h - 1;
	if (c < 0 || c >= m.c) return 0;
	return get_pixel(m, x, y, c);
}
static inline void set_pixel(sod_img m, int x, int y, int c, float val)
{
	/* x, y, c are already validated by upper layers */
	if (m.data)
		m.data[c*m.h*m.w + y * m.w + x] = val;
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
void sod_img_set_pixel(sod_img m, int x, int y, int c, float val)
{
	if (x < 0 || y < 0 || c < 0 || x >= m.w || y >= m.h || c >= m.c) return;
	set_pixel(m, x, y, c, val);
}
static inline void add_pixel(sod_img m, int x, int y, int c, float val)
{
	/* x, y, c are already validated by upper layers */
	if (m.data)
		m.data[c*m.h*m.w + y * m.w + x] += val;
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
void sod_img_add_pixel(sod_img m, int x, int y, int c, float val)
{
	if (!(x < m.w && y < m.h && c < m.c)) {
		return;
	}
	add_pixel(m, x, y, c, val);
}
static inline float three_way_max(float a, float b, float c)
{
	return (a > b) ? ((a > c) ? a : c) : ((b > c) ? b : c);
}
static inline float three_way_min(float a, float b, float c)
{
	return (a < b) ? ((a < c) ? a : c) : ((b < c) ? b : c);
}
/* http://www.cs.rit.edu/~ncs/color/t_convert.html */
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
void sod_img_rgb_to_hsv(sod_img im)
{
	int i, j;
	float r, g, b;
	float h, s, v;
	if (im.c != 3) {
		return;
	}
	for (j = 0; j < im.h; ++j) {
		for (i = 0; i < im.w; ++i) {
			r = get_pixel(im, i, j, 0);
			g = get_pixel(im, i, j, 1);
			b = get_pixel(im, i, j, 2);
			float max = three_way_max(r, g, b);
			float min = three_way_min(r, g, b);
			float delta = max - min;
			v = max;
			if (max == 0) {
				s = 0;
				h = 0;
			}
			else {
				s = delta / max;
				if (r == max) {
					h = (g - b) / delta;
				}
				else if (g == max) {
					h = 2 + (b - r) / delta;
				}
				else {
					h = 4 + (r - g) / delta;
				}
				if (h < 0) h += 6;
				h = h / 6.;
			}
			set_pixel(im, i, j, 0, h);
			set_pixel(im, i, j, 1, s);
			set_pixel(im, i, j, 2, v);
		}
	}
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
void sod_img_hsv_to_rgb(sod_img im)
{
	int i, j;
	float r, g, b;
	float h, s, v;
	float f, p, q, t;
	if (im.c != 3) {
		return;
	}
	for (j = 0; j < im.h; ++j) {
		for (i = 0; i < im.w; ++i) {
			h = 6 * get_pixel(im, i, j, 0);
			s = get_pixel(im, i, j, 1);
			v = get_pixel(im, i, j, 2);
			if (s == 0) {
				r = g = b = v;
			}
			else {
				int index = floor(h);
				f = h - index;
				p = v * (1 - s);
				q = v * (1 - s * f);
				t = v * (1 - s * (1 - f));
				if (index == 0) {
					r = v; g = t; b = p;
				}
				else if (index == 1) {
					r = q; g = v; b = p;
				}
				else if (index == 2) {
					r = p; g = v; b = t;
				}
				else if (index == 3) {
					r = p; g = q; b = v;
				}
				else if (index == 4) {
					r = t; g = p; b = v;
				}
				else {
					r = v; g = p; b = q;
				}
			}
			set_pixel(im, i, j, 0, r);
			set_pixel(im, i, j, 1, g);
			set_pixel(im, i, j, 2, b);
		}
	}
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
sod_img sod_grayscale_image(sod_img im)
{
	if (im.c != 1) {
		int i, j, k;
		sod_img gray = sod_make_image(im.w, im.h, 1);
		if (gray.data) {
			float scale[] = { 0.587, 0.299, 0.114 };
			for (k = 0; k < im.c; ++k) {
				for (j = 0; j < im.h; ++j) {
					for (i = 0; i < im.w; ++i) {
						gray.data[i + im.w*j] += scale[k] * get_pixel(im, i, j, k);
					}
				}
			}
		}
		return gray;
	}
	return sod_copy_image(im); /* Already grayscaled */
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
void sod_grayscale_image_3c(sod_img im)
{
	if (im.c == 3) {
		int i, j, k;
		float scale[] = { 0.299, 0.587, 0.114 };
		for (j = 0; j < im.h; ++j) {
			for (i = 0; i < im.w; ++i) {
				float val = 0;
				for (k = 0; k < 3; ++k) {
					val += scale[k] * get_pixel(im, i, j, k);
				}
				im.data[0 * im.h*im.w + im.w*j + i] = val;
				im.data[1 * im.h*im.w + im.w*j + i] = val;
				im.data[2 * im.h*im.w + im.w*j + i] = val;
			}
		}
	}
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
sod_img sod_threshold_image(sod_img im, float thresh)
{
	sod_img t = sod_make_image(im.w, im.h, im.c);
	if (t.data) {
		int i;
		for (i = 0; i < im.w*im.h*im.c; ++i) {
			t.data[i] = im.data[i] > thresh ? 1 : 0;
		}
	}
	return t;
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
sod_img sod_otsu_binarize_image(sod_img im)
{
#define OTSU_GRAYLEVEL 256
	sod_img t = sod_make_image(im.w, im.h, im.c);
	if (t.data) {
		/* binarization by Otsu's method based on maximization of inter-class variance */
		int hist[OTSU_GRAYLEVEL];
		double prob[OTSU_GRAYLEVEL], omega[OTSU_GRAYLEVEL]; /* prob of graylevels */
		double myu[OTSU_GRAYLEVEL];   /* mean value for separation */
		double max_sigma, sigma[OTSU_GRAYLEVEL]; /* inter-class variance */
		float threshold; /* threshold for binarization */
		int i; /* Loop variable */

			   /* Histogram generation */
		for (i = 0; i < OTSU_GRAYLEVEL; i++) hist[i] = 0;
		for (i = 0; i < im.w*im.h*im.c; ++i) {
			hist[(unsigned char)(255. * im.data[i])]++;
		}
		/* calculation of probability density */
		for (i = 0; i < OTSU_GRAYLEVEL; i++) {
			prob[i] = (double)hist[i] / (im.w * im.h);
		}
		omega[0] = prob[0];
		myu[0] = 0.0;       /* 0.0 times prob[0] equals zero */
		for (i = 1; i < OTSU_GRAYLEVEL; i++) {
			omega[i] = omega[i - 1] + prob[i];
			myu[i] = myu[i - 1] + i * prob[i];
		}

		/* sigma maximization
		sigma stands for inter-class variance
		and determines optimal threshold value */
		threshold = 0.0;
		max_sigma = 0.0;
		for (i = 0; i < OTSU_GRAYLEVEL - 1; i++) {
			if (omega[i] != 0.0 && omega[i] != 1.0)
				sigma[i] = pow(myu[OTSU_GRAYLEVEL - 1] * omega[i] - myu[i], 2) /
				(omega[i] * (1.0 - omega[i]));
			else
				sigma[i] = 0.0;
			if (sigma[i] > max_sigma) {
				max_sigma = sigma[i];
				threshold = (float)i;
			}
		}
		threshold /= 255.;
		/* binarization output */
		for (i = 0; i < im.w*im.h*im.c; ++i) {
			t.data[i] = im.data[i] > threshold ? 1 : 0;
		}
	}
	return t;
}
/*
* Taken from the libpipi project. http://caca.zoy.org/browser/libpipi/trunk/pipi/filter/dilate.c
* License: WTFPL
* http://sam.zoy.org/wtfpl/COPYING for more details.
*/
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_dilate_image(sod_img im, int times)
{
	sod_img out;
	if (im.c != SOD_IMG_GRAYSCALE) {
		/* Only grayscale or binary images */
		return sod_make_empty_image(0, 0, 0);
	}
	out = sod_make_image(im.w, im.h, im.c);
	if (out.data && im.data) {
		int x, y, w, h;
		float *srcdata = im.data;
		float *dstdata = out.data;
		float *tmp = malloc(im.w*im.h * sizeof(float));
		w = im.w;
		h = im.h;
		if (tmp) {
			while (times-- > 0) {
				for (y = 0; y < h; y++)
				{
					for (x = 0; x < w; x++)
					{
						float t;
						int x2, y2, x3, y3;

						y2 = y - 1;
						if (y2 < 0) y2 = h - 1;
						y3 = y + 1;
						if (y3 >= h) y3 = 0;

						x2 = x - 1;
						if (x2 < 0) x2 = w - 1;
						x3 = x + 1;
						if (x3 >= w) x3 = 0;


						t = srcdata[y * w + x];
						if (srcdata[y2 * w + x] > t) t = srcdata[y2 * w + x];
						if (srcdata[y3 * w + x] > t) t = srcdata[y3 * w + x];
						if (srcdata[y * w + x2] > t) t = srcdata[y * w + x2];
						if (srcdata[y * w + x3] > t) t = srcdata[y * w + x3];
						dstdata[y * w + x] = t;

					}
				}
				memcpy(tmp, dstdata, w*h * sizeof(float));
				srcdata = tmp;
			}
			free(tmp);
		}
	}
	return out;
}
/*
* Taken from the libpipi project. http://caca.zoy.org/browser/libpipi/trunk/pipi/filter/dilate.c
* License: WTFPL
* http://sam.zoy.org/wtfpl/COPYING for more details.
*/
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_erode_image(sod_img im, int times)
{
	sod_img out;
	if (im.c != SOD_IMG_GRAYSCALE) {
		/* Only grayscale or binary images */
		return sod_make_empty_image(0, 0, 0);
	}
	out = sod_make_image(im.w, im.h, im.c);
	if (out.data && im.data) {
		int x, y, w, h;
		float *srcdata = im.data;
		float *dstdata = out.data;
		float *tmp = malloc(im.w*im.h * sizeof(float));
		w = im.w;
		h = im.h;
		if (tmp) {
			while (times-- > 0) {
				for (y = 0; y < h; y++)
				{
					for (x = 0; x < w; x++)
					{
						float t;
						int x2, y2, x3, y3;

						y2 = y - 1;
						if (y2 < 0) y2 = h - 1;
						y3 = y + 1;
						if (y3 >= h) y3 = 0;

						x2 = x - 1;
						if (x2 < 0) x2 = w - 1;
						x3 = x + 1;
						if (x3 >= w) x3 = 0;


						t = srcdata[y * w + x];
						if (srcdata[y2 * w + x] < t) t = srcdata[y2 * w + x];
						if (srcdata[y3 * w + x] < t) t = srcdata[y3 * w + x];
						if (srcdata[y * w + x2] < t) t = srcdata[y * w + x2];
						if (srcdata[y * w + x3] < t) t = srcdata[y * w + x3];
						dstdata[y * w + x] = t;
					}
				}
				memcpy(tmp, dstdata, w*h * sizeof(float));
				srcdata = tmp;
			}
			free(tmp);
		}
	}
	return out;
}
/* Based on the work: http://cis.k.hosei.ac.jp/~wakahara/ */
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_sharpen_filtering_image(sod_img im)
{
	/* Spatial filtering of image data */
	/* Sharpening filter by 8-neighbor Laplacian subtraction */
	sod_img out;
	if (im.c != SOD_IMG_GRAYSCALE || !im.data) {
		/* Only grayscale image */
		return sod_make_empty_image(0, 0, 0);
	}
	/* Clone the input image */
	out = sod_copy_image(im);
	if (out.data) {
		/* Definition of sharpening filter */
		int weight[3][3] = { { 1,  1,  1 },
		{ 1,  -8,  1 },
		{ 1,  1,  1 } };
		const double alpha = 0.2;
		double pixel_value;
		int x, y, i, j;  /* Loop variable */
		int new_value;
		/* Original image minus Laplacian image */
		for (y = 1; y < im.h - 1; y++) {
			for (x = 1; x < im.w - 1; x++) {
				pixel_value = 0.0;
				for (j = -1; j < 2; j++) {
					for (i = -1; i < 2; i++) {
						pixel_value += weight[j + 1][i + 1] * im.data[((y + j) * im.w) + x + i];
					}
				}
				new_value = (int)(im.data[y * im.w + x] - alpha * pixel_value);
				if (new_value < 0) new_value = 0;
				if (new_value > 1) new_value = 1;
				out.data[y * out.w + x] = (float)new_value;
			}
		}
	}
	return out;
}
/* Based on the work: http://cis.k.hosei.ac.jp/~wakahara/ */
/* connectivity detection for each point */
static int hilditch_func_nc8(int *b)
{
	int n_odd[4] = { 1, 3, 5, 7 };  /* odd-number neighbors */
	int i, j, sum, d[10];           /* control variable */

	for (i = 0; i <= 9; i++) {
		j = i;
		if (i == 9) j = 1;
		if (abs(*(b + j)) == 1) {
			d[i] = 1;
		}
		else {
			d[i] = 0;
		}
	}
	sum = 0;
	for (i = 0; i < 4; i++) {
		j = n_odd[i];
		sum = sum + d[j] - d[j] * d[j + 1] * d[j + 2];
	}
	return sum;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_hilditch_thin_image(sod_img im)
{
	/* thinning of binary image via Hilditch's algorithm */
	int offset[9][2] = { { 0,0 },{ 1,0 },{ 1,-1 },{ 0,-1 },{ -1,-1 },
	{ -1,0 },{ -1,1 },{ 0,1 },{ 1,1 } }; /* offsets for neighbors */
	int n_odd[4] = { 1, 3, 5, 7 };      /* odd-number neighbors */
	int px, py;                         /* X/Y coordinates  */
	int b[9];                           /* gray levels for 9 neighbors */
	int condition[6];                   /* valid for conditions 1-6 */
	int counter;                        /* number of changing points  */
	int i, x, y, copy, sum;             /* control variable          */
	sod_img out;

	if (im.data == 0 || im.c != SOD_IMG_GRAYSCALE) {
		/* Must be a binary image (canny_edge, thresholding, etc..) */
		return sod_make_empty_image(0, 0, 0);
	}
	/* initialization of output */
	out = sod_copy_image(im);
	if (out.data == 0) {
		return sod_make_empty_image(0, 0, 0);
	}
	/* processing starts */
	do {
		counter = 0;
		for (y = 0; y < im.h; y++) {
			for (x = 0; x < im.w; x++) {
				/* substitution of 9-neighbor gray values */
				for (i = 0; i < 9; i++) {
					b[i] = 0;
					px = x + offset[i][0];
					py = y + offset[i][1];
					if (px >= 0 && px < im.w &&
						py >= 0 && py < im.h) {
						if (out.data[py * im.w + px] == 0) {
							b[i] = 1;
						}
						else if (out.data[py * im.w + px] == 2 /* Temp marker */) {
							b[i] = -1;
						}
					}
				}
				for (i = 0; i < 6; i++) {
					condition[i] = 0;
				}

				/* condition 1: figure point */
				if (b[0] == 1) condition[0] = 1;

				/* condition 2: boundary point */
				sum = 0;
				for (i = 0; i < 4; i++) {
					sum = sum + 1 - abs(b[n_odd[i]]);
				}
				if (sum >= 1) condition[1] = 1;

				/* condition 3: endpoint conservation */
				sum = 0;
				for (i = 1; i <= 8; i++) {
					sum = sum + abs(b[i]);
				}
				if (sum >= 2) condition[2] = 1;

				/* condition 4: isolated point conservation */
				sum = 0;
				for (i = 1; i <= 8; i++) {
					if (b[i] == 1) sum++;
				}
				if (sum >= 1) condition[3] = 1;

				/* condition 5: connectivity conservation */
				if (hilditch_func_nc8(b) == 1) condition[4] = 1;

				/* condition 6: one-side elimination for line-width of two */
				sum = 0;
				for (i = 1; i <= 8; i++) {
					if (b[i] != -1) {
						sum++;
					}
					else {
						copy = b[i];
						b[i] = 0;
						if (hilditch_func_nc8(b) == 1) sum++;
						b[i] = copy;
					}
				}
				if (sum == 8) condition[5] = 1;

				/* final decision */
				if (condition[0] && condition[1] && condition[2] &&
					condition[3] && condition[4] && condition[5]) {
					out.data[y * im.w + x] = 2; /* Temp */
					counter++;
				}
			} /* end of x */
		} /* end of y */

		if (counter != 0) {
			for (y = 0; y < im.h; y++) {
				for (x = 0; x < im.w; x++) {
					if (out.data[y * im.w + x] == 2) out.data[y *im.w + x] = 1;
				}
			}
		}
	} while (counter != 0);

	return out;
}
/* Based on the work: http://cis.k.hosei.ac.jp/~wakahara/ */
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_sobel_image(sod_img im)
{
	sod_img out;
	int weight[3][3] = { { -1,  0,  1 },
	{ -2,  0,  2 },
	{ -1,  0,  1 } };
	double pixel_value;
	double min, max;
	int x, y, i, j;  /* Loop variable */

	if (!im.data || im.c != SOD_IMG_GRAYSCALE) {
		/* Only grayscale images */
		return sod_make_empty_image(im.w, im.h, im.c);
	}
	out = sod_make_image(im.w, im.h, im.c);
	if (!out.data) {
		return sod_make_empty_image(im.w, im.h, im.c);
	}
	/* Maximum values calculation after filtering*/
	min = DBL_MAX;
	max = -DBL_MAX;
	for (y = 1; y < im.h - 1; y++) {
		for (x = 1; x < im.w - 1; x++) {
			pixel_value = 0.0;
			for (j = -1; j <= 1; j++) {
				for (i = -1; i <= 1; i++) {
					pixel_value += weight[j + 1][i + 1] * im.data[(im.w * (y + j)) + x + i];
				}
			}
			if (pixel_value < min) min = pixel_value;
			if (pixel_value > max) max = pixel_value;
		}
	}
	if ((int)(max - min) == 0) {
		return out;
	}
	/* Generation of image2 after linear transformation */
	for (y = 1; y < out.h - 1; y++) {
		for (x = 1; x < out.w - 1; x++) {
			pixel_value = 0.0;
			for (j = -1; j <= 1; j++) {
				for (i = -1; i <= 1; i++) {
					pixel_value += weight[j + 1][i + 1] * im.data[(im.w * (y + j)) + x + i];
				}
			}
			pixel_value = (pixel_value - min) / (max - min);
			out.data[out.w * y + x] = (float)pixel_value;
		}
	}
	return out;
}
/*
* Connected Component Labeling.
*/
typedef struct sod_label_coord sod_label_coord;
struct sod_label_coord
{
	int xmin;
	int xmax;
	int ymin;
	int ymax;
	sod_label_coord *pNext; /* Next recorded label on the list */
};
/*
* License CPOL, https://www.codeproject.com/Articles/825200/An-Implementation-Of-The-Connected-Component-Label
*/
#define CALL_LabelComponent(x,y,returnLabel) { STACK[SP] = x; STACK[SP+1] = y; STACK[SP+2] = returnLabel; SP += 3; goto START; }
#define ST_RETURN { SP -= 3;                \
                 switch (STACK[SP+2])    \
                 {                       \
                 case 1 : goto RETURN1;  \
                 case 2 : goto RETURN2;  \
                 case 3 : goto RETURN3;  \
                 case 4 : goto RETURN4;  \
                 default: return;        \
                 }                       \
               }
#define XS (STACK[SP-3])
#define YS (STACK[SP-2])
static void LabelComponent(uint16_t* STACK, uint16_t width, uint16_t height, float* input, sod_label_coord **output, sod_label_coord *pCord, uint16_t x, uint16_t y)
{
	STACK[0] = x;
	STACK[1] = y;
	STACK[2] = 0;  /* return - component is labeled */
	int SP = 3;
	int index;

START: /* Recursive routine starts here */

	index = XS + width * YS;
	if (input[index] == 0) ST_RETURN;   /* This pixel is not part of a component */
	if (output[index] != 0) ST_RETURN;   /* This pixel has already been labeled  */
	output[index] = pCord;

	if (pCord->xmin > XS) pCord->xmin = XS;
	if (pCord->xmax < XS) pCord->xmax = XS;
	if (pCord->ymin > YS) pCord->ymin = YS;
	if (pCord->ymax < YS) pCord->ymax = YS;

	if (XS > 0) CALL_LabelComponent(XS - 1, YS, 1);   /* left  pixel */
RETURN1:

	if (XS < width - 1) CALL_LabelComponent(XS + 1, YS, 2);   /* right pixel */
RETURN2:

	if (YS > 0) CALL_LabelComponent(XS, YS - 1, 3);   /* upper pixel */
RETURN3:

	if (YS < height - 1) CALL_LabelComponent(XS, YS + 1, 4);   /* lower pixel */
RETURN4:

	ST_RETURN;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
static sod_label_coord * LabelImage(sod_img *pImg)
{
	sod_label_coord **apCord, *pEntry, *pList = 0;
	uint16_t width = (uint16_t)pImg->w;
	uint16_t height = (uint16_t)pImg->h;
	uint16_t* STACK;
	int labelNo = 0;
	int index = -1;
	float *input;
	uint16_t x, y;
	STACK = (uint16_t *)malloc(3 * sizeof(uint16_t)*(width*height + 1));
	if (STACK == 0) return 0;
	apCord = (sod_label_coord **)malloc(width * height * sizeof(sod_label_coord *));
	if (apCord == 0) {
		free(STACK);
		return 0;
	}
	memset(apCord, 0, width * height * sizeof(sod_label_coord *));
	input = pImg->data;
	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			index++;
			if (input[index] == 0) continue;   /* This pixel is not part of a component */
			if (apCord[index] != 0) continue;   /* This pixel has already been labeled  */
												/* New component found */
			pEntry = (sod_label_coord *)malloc(sizeof(sod_label_coord));
			if (pEntry == 0) {
				goto finish;
			}
			labelNo++;
			pEntry->xmax = pEntry->ymax = -100;
			pEntry->xmin = pEntry->ymin = 2147483647;
			pEntry->pNext = pList;
			pList = pEntry;
			LabelComponent(STACK, width, height, input, apCord, pEntry, x, y);
		}
	}
finish:
	free(STACK);
	free(apCord);
	return pList;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_image_find_blobs(sod_img im, sod_box ** paBox, int * pnBox, int(*xFilter)(int width, int height))
{
	if (im.c != SOD_IMG_GRAYSCALE || im.data == 0) {
		/* Must be a binary image */
		if (pnBox) {
			*pnBox = 0;
		}
		return SOD_UNSUPPORTED;
	}
	if (pnBox) {
		sod_label_coord *pList, *pNext;
		sod_box sRect;
		SySet aBox;
		/* Label that image */
		pList = LabelImage(&im);
		SySetInit(&aBox, sizeof(sod_box));
		while (pList) {
			pNext = pList->pNext;
			if (pList->xmax >= 0) {
				int allow = 1;
				int h = pList->ymax - pList->ymin;
				int w = pList->xmax - pList->xmin;
				if (xFilter) {
					allow = xFilter(w, h);
				}
				if (allow) {
					sRect.pUserData = 0;
					sRect.score = 0;
					sRect.zName = "blob";
					sRect.x = pList->xmin;
					sRect.y = pList->ymin;
					sRect.w = w;
					sRect.h = h;
					/* Save the box */
					SySetPut(&aBox, (const void *)&sRect);
				}
			}
			free(pList);
			pList = pNext;
		}
		*pnBox = (int)SySetUsed(&aBox);
		if (paBox) {
			*paBox = (sod_box *)SySetBasePtr(&aBox);
		}
		else {
			SySetRelease(&aBox);
		}
	}
	return SOD_OK;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_image_blob_boxes_release(sod_box * pBox)
{
	free(pBox);
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_blend_image(sod_img fore, sod_img back, float alpha)
{
	sod_img blend = sod_make_image(fore.w, fore.h, fore.c);
	if (!(fore.w == back.w && fore.h == back.h && fore.c == back.c)) {
		return blend;
	}
	int i, j, k;
	for (k = 0; k < fore.c; ++k) {
		for (j = 0; j < fore.h; ++j) {
			for (i = 0; i < fore.w; ++i) {
				float val = alpha * get_pixel(fore, i, j, k) + (1 - alpha)* get_pixel(back, i, j, k);
				set_pixel(blend, i, j, k, val);
			}
		}
	}
	return blend;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_scale_image_channel(sod_img im, int c, float v)
{
	int i, j;
	for (j = 0; j < im.h; ++j) {
		for (i = 0; i < im.w; ++i) {
			float pix = get_pixel(im, i, j, c);
			pix = pix * v;
			set_pixel(im, i, j, c, pix);
		}
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_translate_image_channel(sod_img im, int c, float v)
{
	int i, j;
	for (j = 0; j < im.h; ++j) {
		for (i = 0; i < im.w; ++i) {
			float pix = get_pixel(im, i, j, c);
			pix = pix + v;
			set_pixel(im, i, j, c, pix);
		}
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_binarize_image(sod_img im, int reverse)
{
	sod_img c = sod_copy_image(im);
	if (c.data) {
		int i;
		for (i = 0; i < im.w * im.h * im.c; ++i) {
			if (c.data[i] > .5) c.data[i] = reverse ? 1 : 0;
			else c.data[i] = reverse ? 0 : 1;
		}
	}
	return c;
}
static float get_pixel_extend(sod_img m, int x, int y, int c)
{
	if (x < 0 || x >= m.w || y < 0 || y >= m.h) return 0;
	if (c < 0 || c >= m.c) return 0;
	return get_pixel(m, x, y, c);
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_composite_image(sod_img source, sod_img dest, int dx, int dy)
{
	int x, y, k;
	for (k = 0; k < source.c; ++k) {
		for (y = 0; y < source.h; ++y) {
			for (x = 0; x < source.w; ++x) {
				float val = get_pixel(source, x, y, k);
				float val2 = get_pixel_extend(dest, dx + x, dy + y, k);
				set_pixel(dest, dx + x, dy + y, k, val * val2);
			}
		}
	}
}
static void sodFastImageResize(sod_img im, sod_img resized, sod_img part, int w, int h)
{
	int r, c, k;
	float w_scale = (float)(im.w - 1) / (w - 1);
	float h_scale = (float)(im.h - 1) / (h - 1);
	for (k = 0; k < im.c; ++k) {
		for (r = 0; r < im.h; ++r) {
			for (c = 0; c < w; ++c) {
				float val = 0;
				if (c == w - 1 || im.w == 1) {
					val = get_pixel(im, im.w - 1, r, k);
				}
				else {
					float sx = c * w_scale;
					int ix = (int)sx;
					float dx = sx - ix;
					val = (1 - dx) * get_pixel(im, ix, r, k) + dx * get_pixel(im, ix + 1, r, k);
				}
				set_pixel(part, c, r, k, val);
			}
		}
	}
	for (k = 0; k < im.c; ++k) {
		for (r = 0; r < h; ++r) {
			float sy = r * h_scale;
			int iy = (int)sy;
			float dy = sy - iy;
			for (c = 0; c < w; ++c) {
				float val = (1 - dy) * get_pixel(part, c, iy, k);
				set_pixel(resized, c, r, k, val);
			}
			if (r == h - 1 || im.h == 1) continue;
			for (c = 0; c < w; ++c) {
				float val = dy * get_pixel(part, c, iy + 1, k);
				add_pixel(resized, c, r, k, val);
			}
		}
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_resize_image(sod_img im, int w, int h)
{
	sod_img resized = sod_make_image(w, h, im.c);
	sod_img part = sod_make_image(w, im.h, im.c);
	sodFastImageResize(im, resized, part, w, h);
	sod_free_image(part);
	return resized;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_resize_max(sod_img im, int max)
{
	int w = im.w;
	int h = im.h;
	if (w > h) {
		h = (h * max) / w;
		w = max;
	}
	else {
		w = (w * max) / h;
		h = max;
	}
	if (w == im.w && h == im.h) return im;
	sod_img resized = sod_resize_image(im, w, h);
	return resized;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_resize_min(sod_img im, int min)
{
	int w = im.w;
	int h = im.h;
	if (w < h) {
		h = (h * min) / w;
		w = min;
	}
	else {
		w = (w * min) / h;
		h = min;
	}
	if (w == im.w && h == im.h) return im;
	sod_img resized = sod_resize_image(im, w, h);
	return resized;
}
static float bilinear_interpolate(sod_img im, float x, float y, int c)
{
#if defined(__BORLANDC__) && defined(_WIN32)
	int ix = (int)floor(x);
	int iy = (int)floor(y);
#else
	int ix = (int)floorf(x);
	int iy = (int)floorf(y);
#endif
	float dx = x - ix;
	float dy = y - iy;
	float val = (1 - dy) * (1 - dx) * get_pixel_extend(im, ix, iy, c) +
		dy * (1 - dx) * get_pixel_extend(im, ix, iy + 1, c) +
		(1 - dy) *   dx   * get_pixel_extend(im, ix + 1, iy, c) +
		dy * dx   * get_pixel_extend(im, ix + 1, iy + 1, c);
	return val;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_rotate_crop_image(sod_img im, float rad, float s, int w, int h, float dx, float dy, float aspect)
{
	int x, y, c;
	float cx = im.w / 2.;
	float cy = im.h / 2.;
	sod_img rot = sod_make_image(w, h, im.c);
	if (rot.data) {
		for (c = 0; c < im.c; ++c) {
			for (y = 0; y < h; ++y) {
				for (x = 0; x < w; ++x) {
					float rx = cos(rad)*((x - w / 2.) / s * aspect + dx / s * aspect) - sin(rad)*((y - h / 2.) / s + dy / s) + cx;
					float ry = sin(rad)*((x - w / 2.) / s * aspect + dx / s * aspect) + cos(rad)*((y - h / 2.) / s + dy / s) + cy;
					float val = bilinear_interpolate(im, rx, ry, c);
					set_pixel(rot, x, y, c, val);
				}
			}
		}
	}
	return rot;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_rotate_image(sod_img im, float rad)
{
	int x, y, c;
	float cx = im.w / 2.;
	float cy = im.h / 2.;
	sod_img rot = sod_make_image(im.w, im.h, im.c);
	if (rot.data) {
		for (c = 0; c < im.c; ++c) {
			for (y = 0; y < im.h; ++y) {
				for (x = 0; x < im.w; ++x) {
					float rx = cos(rad)*(x - cx) - sin(rad)*(y - cy) + cx;
					float ry = sin(rad)*(x - cx) + cos(rad)*(y - cy) + cy;
					float val = bilinear_interpolate(im, rx, ry, c);
					set_pixel(rot, x, y, c, val);
				}
			}
		}
	}
	return rot;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_translate_image(sod_img m, float s)
{
	if (m.data) {
		int i;
		for (i = 0; i < m.h*m.w*m.c; ++i) m.data[i] += s;
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_scale_image(sod_img m, float s)
{
	if (m.data) {
		int i;
		for (i = 0; i < m.h*m.w*m.c; ++i) m.data[i] *= s;
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_crop_image(sod_img im, int dx, int dy, int w, int h)
{
	sod_img cropped = sod_make_image(w, h, im.c);
	int i, j, k;
	for (k = 0; k < im.c; ++k) {
		for (j = 0; j < h; ++j) {
			for (i = 0; i < w; ++i) {
				int r = j + dy;
				int c = i + dx;
				float val = 0;
				r = constrain_int(r, 0, im.h - 1);
				c = constrain_int(c, 0, im.w - 1);
				if (r >= 0 && r < im.h && c >= 0 && c < im.w) {
					val = get_pixel(im, c, r, k);
				}
				set_pixel(cropped, i, j, k, val);
			}
		}
	}
	return cropped;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_random_crop_image(sod_img im, int w, int h)
{
	int dx = rand_int(0, im.w - w);
	int dy = rand_int(0, im.h - h);
	sod_img crop = sod_crop_image(im, dx, dy, w, h);
	return crop;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_transpose_image(sod_img im)
{
	int n, m;
	int c;
	if (im.w != im.h) {
		return;
	}
	if (im.data) {
		for (c = 0; c < im.c; ++c) {
			for (n = 0; n < im.w - 1; ++n) {
				for (m = n + 1; m < im.w; ++m) {
					float swap = im.data[m + im.w*(n + im.h*c)];
					im.data[m + im.w*(n + im.h*c)] = im.data[n + im.w*(m + im.h*c)];
					im.data[n + im.w*(m + im.h*c)] = swap;
				}
			}
		}
	}
}
#define GRAYLEVEL 256
#define FINAL_LEVEL 64  /* No. of final gray levels */
/* Based on the work: http://cis.k.hosei.ac.jp/~wakahara/ */
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
sod_img sod_equalize_histogram(sod_img im)
{
	sod_img out;
	int hist1[GRAYLEVEL], hist2[GRAYLEVEL];
	int trans_table[GRAYLEVEL]; /* Table of gray level transformation */
	int i, x, y;      /* Loop variable */
	int target_value; /* Target occurrences after transformation */
	int gray;
	double gray_step; /* Descriptive gray level interval */
	float min, max;
	int x_size2, y_size2;

	if (im.data == 0 || im.c != SOD_IMG_GRAYSCALE) {
		/* Must be a grayscale image */
		return sod_make_empty_image(0, 0, 0);
	}
	out = sod_make_image(im.w, im.h, im.c);
	if (out.data == 0) {
		return sod_make_empty_image(0, 0, 0);
	}

	/* Generation of gray level histogram of input image */
	for (i = 0; i < GRAYLEVEL; i++) hist1[i] = 0;
	for (y = 0; y < im.h; y++) {
		for (x = 0; x < im.w; x++) {
			hist1[(unsigned char)(255 * im.data[y * im.w + x])]++;
		}
	}

	/* Generation of transformation table */
	target_value = (int)((double)(im.w * im.h) / FINAL_LEVEL);
	for (i = 0; i < FINAL_LEVEL; i++)
		hist2[i] = 0;

	gray = 0;
	for (i = 0; i < GRAYLEVEL; i++) {
		if (abs(target_value - hist2[gray]) <
			abs(target_value - (hist2[gray] + hist1[i]))) {
			gray++;
			if (gray >= FINAL_LEVEL) gray = FINAL_LEVEL - 1;
		}
		trans_table[i] = gray;
		hist2[gray] = hist2[gray] + hist1[i];
	}

	/* Output of image2 subject to histogram equalization */
	x_size2 = im.w;
	y_size2 = im.h;
	gray_step = (double)GRAYLEVEL / FINAL_LEVEL;
	for (y = 0; y < y_size2; y++) {
		for (x = 0; x < x_size2; x++) {
			out.data[y * out.w + x] = (trans_table[(unsigned char)(255 * im.data[y * im.w + x])] * gray_step) / 255.;
		}
	}
	/* linear transformation of gray level histogram of output image */
	max = min = out.data[0];
	for (y = 0; y < y_size2; y++) {
		for (x = 0; x < x_size2; x++) {
			if (min > out.data[y * out.w + x]) min = out.data[y * out.w + x];
			if (max < out.data[y * out.w + x]) max = out.data[y * out.w + x];
		}
	}
	if ((max - min) == 0) {
		return out;
	}
	for (y = 0; y < y_size2; y++) {
		for (x = 0; x < x_size2; x++) {
			out.data[y * out.w + x] = ((out.data[y * out.w + x] - min) / (max - min));
		}
	}
	return out;
}

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif /* M_PI */
/*
Adapted from the FAST-EDGE Repo

Copyright (c) 2009 Benjamin C. Haynor

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/
static void canny_calc_gradient_sobel(sod_img * img_in, int *g, int *dir) {
	int w, h, x, y, max_x, max_y, g_x, g_y;
	float g_div;
	w = img_in->w;
	h = img_in->h;
	max_x = w - 3;
	max_y = w * (h - 3);
	for (y = w * 3; y < max_y; y += w) {
		for (x = 3; x < max_x; x++) {
			g_x = (int)(255 * (2 * img_in->data[x + y + 1]
				+ img_in->data[x + y - w + 1]
				+ img_in->data[x + y + w + 1]
				- 2 * img_in->data[x + y - 1]
				- img_in->data[x + y - w - 1]
				- img_in->data[x + y + w - 1]));
			g_y = (int)(255 * (2 * img_in->data[x + y - w]
				+ img_in->data[x + y - w + 1]
				+ img_in->data[x + y - w - 1]
				- 2 * img_in->data[x + y + w]
				- img_in->data[x + y + w + 1]
				- img_in->data[x + y + w - 1]));

			g[x + y] = sqrt(g_x * g_x + g_y * g_y);

			if (g_x == 0) {
				dir[x + y] = 2;
			}
			else {
				g_div = g_y / (float)g_x;
				if (g_div < 0) {
					if (g_div < -2.41421356237) {
						dir[x + y] = 0;
					}
					else {
						if (g_div < -0.414213562373) {
							dir[x + y] = 1;
						}
						else {
							dir[x + y] = 2;
						}
					}
				}
				else {
					if (g_div > 2.41421356237) {
						dir[x + y] = 0;
					}
					else {
						if (g_div > 0.414213562373) {
							dir[x + y] = 3;
						}
						else {
							dir[x + y] = 2;
						}
					}
				}
			}
		}
	}
}
/*
NON_MAX_SUPPRESSION
using the estimates of the Gx and Gy image gradients and the edge direction angle determines whether the magnitude of the gradient assumes a local  maximum in the gradient direction
if the rounded edge direction angle is 0 degrees, checks the north and south directions
if the rounded edge direction angle is 45 degrees, checks the northwest and southeast directions
if the rounded edge direction angle is 90 degrees, checks the east and west directions
if the rounded edge direction angle is 135 degrees, checks the northeast and southwest directions
*/
static void canny_non_max_suppression(sod_img * img, int *g, int *dir) {

	int w, h, x, y, max_x, max_y;
	w = img->w;
	h = img->h;
	max_x = w;
	max_y = w * h;
	for (y = 0; y < max_y; y += w) {
		for (x = 0; x < max_x; x++) {
			switch (dir[x + y]) {
			case 0:
				if (g[x + y] > g[x + y - w] && g[x + y] > g[x + y + w]) {
					if (g[x + y] > 255) {
						img->data[x + y] = 255.;
					}
					else {
						img->data[x + y] = (float)g[x + y];
					}
				}
				else {
					img->data[x + y] = 0;
				}
				break;
			case 1:
				if (g[x + y] > g[x + y - w - 1] && g[x + y] > g[x + y + w + 1]) {
					if (g[x + y] > 255) {
						img->data[x + y] = 255.;
					}
					else {
						img->data[x + y] = (float)g[x + y];
					}
				}
				else {
					img->data[x + y] = 0;
				}
				break;
			case 2:
				if (g[x + y] > g[x + y - 1] && g[x + y] > g[x + y + 1]) {
					if (g[x + y] > 255) {
						img->data[x + y] = 255.;
					}
					else {
						img->data[x + y] = (float)g[x + y];
					}
				}
				else {
					img->data[x + y] = 0;
				}
				break;
			case 3:
				if (g[x + y] > g[x + y - w + 1] && g[x + y] > g[x + y + w - 1]) {
					if (g[x + y] > 255) {
						img->data[x + y] = 255.;
					}
					else {
						img->data[x + y] = (float)g[x + y];
					}
				}
				else {
					img->data[x + y] = 0;
				}
				break;
			default:
				break;
			}
		}
	}
}
#define LOW_THRESHOLD_PERCENTAGE 0.8 /* percentage of the high threshold value that the low threshold shall be set at */
#define HIGH_THRESHOLD_PERCENTAGE 0.10 /* percentage of pixels that meet the high threshold - for example 0.15 will ensure that at least 15% of edge pixels are considered to meet the high threshold */
/*
* ESTIMATE_THRESHOLD
* estimates hysteresis threshold, assuming that the top X% (as defined by the HIGH_THRESHOLD_PERCENTAGE) of edge pixels with the greatest
* intensity are true edges and that the low threshold is equal to the quantity of the high threshold plus the total number of 0s at
* the low end of the histogram divided by 2.
*/
static void canny_estimate_threshold(sod_img * img, int * high, int * low) {

	int i, max, pixels, high_cutoff;
	int histogram[256];
	max = img->w * img->h;
	for (i = 0; i < 256; i++) {
		histogram[i] = 0;
	}
	for (i = 0; i < max; i++) {
		histogram[(int)img->data[i]]++;
	}
	pixels = (max - histogram[0]) * HIGH_THRESHOLD_PERCENTAGE;
	high_cutoff = 0;
	i = 255;
	while (high_cutoff < pixels) {
		high_cutoff += histogram[i];
		i--;
	}
	*high = i;
	i = 1;
	while (histogram[i] == 0) {
		i++;
	}
	*low = (*high + i) * LOW_THRESHOLD_PERCENTAGE;
}
static int canny_range(sod_img * img, int x, int y)
{
	if ((x < 0) || (x >= img->w)) {
		return(0);
	}
	if ((y < 0) || (y >= img->h)) {
		return(0);
	}
	return(1);
}
static int canny_trace(int x, int y, int low, sod_img * img_in, sod_img * img_out)
{
	int y_off, x_off;
	if (img_out->data[y * img_out->w + x] == 0)
	{
		img_out->data[y * img_out->w + x] = 1;
		for (y_off = -1; y_off <= 1; y_off++)
		{
			for (x_off = -1; x_off <= 1; x_off++)
			{
				if (!(y == 0 && x_off == 0) && canny_range(img_in, x + x_off, y + y_off) && (int)(img_in->data[(y + y_off) * img_out->w + x + x_off]) >= low) {
					if (canny_trace(x + x_off, y + y_off, low, img_in, img_out))
					{
						return(1);
					}
				}
			}
		}
		return(1);
	}
	return(0);
}
static void canny_hysteresis(int high, int low, sod_img * img_in, sod_img * img_out)
{
	int x, y, n, max;
	max = img_in->w * img_in->h;
	for (n = 0; n < max; n++) {
		img_out->data[n] = 0;
	}
	for (y = 0; y < img_out->h; y++) {
		for (x = 0; x < img_out->w; x++) {
			if ((int)(img_in->data[y * img_out->w + x]) >= high) {
				canny_trace(x, y, low, img_in, img_out);
			}
		}
	}
}
/* Based on the work: http://cis.k.hosei.ac.jp/~wakahara/ */
static int minutiae_crossnumber(float *pixels,int y, int x, int w)
{
	int i, data[8];
	int cross;

	data[0] = pixels[y * w + x + 1] == 0 ? 1 : 0;
	data[1] = pixels[(y - 1) * w + x + 1] == 0 ? 1 : 0;
	data[2] = pixels[(y - 1) * w + x] == 0 ? 1 : 0;
	data[3] = pixels[(y - 1) * w + (x - 1)] == 0 ? 1 : 0;
	data[4] = pixels[y * w + (x - 1)] == 0 ? 1 : 0;
	data[5] = pixels[(y + 1) * w + (x - 1)] == 0 ? 1 : 0;
	data[6] = pixels[(y + 1) * w + x] == 0 ? 1 : 0;
	data[7] = pixels[(y + 1) * w + x + 1] == 0 ? 1 : 0;
	cross = 0;
	for (i = 0; i < 8; i++) {
		cross += abs(data[(i + 1) % 8] - data[i]);
	}
	cross /= 2;
	return cross;
}
/*
 * CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
 */
SOD_APIEXPORT sod_img sod_minutiae(sod_img bin, int *pTotal, int *pEp, int *pBp)
{
	if (pTotal) {
		*pTotal = 0;
	}
	if (pEp) {
		*pEp = 0;
	}
	if (pBp) {
		*pBp = 0;
	}
	/* Extraction of minutiae candidates in skeletonized fingerprint image */
	if (bin.data == 0 || bin.c != SOD_IMG_GRAYSCALE) {
		/* Must be a binary image  processed via sod_hilditch_thin_image() */
		return sod_make_empty_image(0, 0, 0);
	}
	sod_img out = sod_make_image(bin.w, bin.h, bin.c);
	if (out.data) {
		int x, y;
		int total, np1, np2;  /* number of black and minutiae points */
		int cross;
		int i;
		for (i = 0; i < out.w*out.h; i++) {
			if (bin.data[i] == 1) {
				out.data[i] = 200;
			}
			else {
				out.data[i] = 1;
			}
		}
		/* finding minutiae in 3 x 3 window 
		 * Minutiae extraction is applied to skeletonized fingerprint.
		 */
		total = 0;
		np1 = 0;  /* number of ending points */
		np2 = 0;  /* number of bifurcations */
		for (y = 1; y < bin.h - 1; y++) {
			for (x = 1; x < bin.w - 1; x++) {
				if (bin.data[y * bin.w + x] == 0) {
					total++;
					cross = minutiae_crossnumber(bin.data, y, x, bin.w);
					if (cross == 1) {
						np1++;
						out.data[y * bin.w + x] = 0;
					}
					else if (cross >= 3) {
						np2++;
						out.data[y * bin.w + x] = 0;
					}
				}
			}
		}
		if (pTotal) {
			*pTotal = total;
		}
		if (pEp) {
			*pEp = np1;
		}
		if (pBp) {
			*pBp = np2;
		}	
	}
	return out;
}
/*
* Gaussian Noise Reduce on a grayscale image.
* apply 5x5 Gaussian convolution filter, shrinks the image by 4 pixels in each direction, using Gaussian filter found here:
* http://en.wikipedia.org/wiki/Canny_edge_detector
*/
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_gaussian_noise_reduce(sod_img grayscale)
{
	int w, h, x, y, max_x, max_y;
	sod_img img_out;
	if (!grayscale.data || grayscale.c != SOD_IMG_GRAYSCALE) {
		return sod_make_empty_image(0, 0, 0);
	}
	w = grayscale.w;
	h = grayscale.h;
	img_out = sod_make_image(w, h, 1);
	if (img_out.data) {
		max_x = w - 2;
		max_y = w * (h - 2);
		for (y = w * 2; y < max_y; y += w) {
			for (x = 2; x < max_x; x++) {
				img_out.data[x + y] = (2 * grayscale.data[x + y - 2 - w - w] +
					4 * grayscale.data[x + y - 1 - w - w] +
					5 * grayscale.data[x + y - w - w] +
					4 * grayscale.data[x + y + 1 - w - w] +
					2 * grayscale.data[x + y + 2 - w - w] +
					4 * grayscale.data[x + y - 2 - w] +
					9 * grayscale.data[x + y - 1 - w] +
					12 * grayscale.data[x + y - w] +
					9 * grayscale.data[x + y + 1 - w] +
					4 * grayscale.data[x + y + 2 - w] +
					5 * grayscale.data[x + y - 2] +
					12 * grayscale.data[x + y - 1] +
					15 * grayscale.data[x + y] +
					12 * grayscale.data[x + y + 1] +
					5 * grayscale.data[x + y + 2] +
					4 * grayscale.data[x + y - 2 + w] +
					9 * grayscale.data[x + y - 1 + w] +
					12 * grayscale.data[x + y + w] +
					9 * grayscale.data[x + y + 1 + w] +
					4 * grayscale.data[x + y + 2 + w] +
					2 * grayscale.data[x + y - 2 + w + w] +
					4 * grayscale.data[x + y - 1 + w + w] +
					5 * grayscale.data[x + y + w + w] +
					4 * grayscale.data[x + y + 1 + w + w] +
					2 * grayscale.data[x + y + 2 + w + w]) / 159;
			}
		}
	}
	return img_out;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_canny_edge_image(sod_img im, int reduce_noise)
{
	if (im.data && im.c == SOD_IMG_GRAYSCALE) {
		sod_img out, sobel, clean;
		int high, low, *g, *dir;
		if (reduce_noise) {
			clean = sod_gaussian_noise_reduce(im);
			if (!clean.data)return sod_make_empty_image(0, 0, 0);
		}
		else {
			clean = im;
		}
		sobel = sod_make_image(im.w, im.h, 1);
		out = sod_make_image(im.w, im.h, 1);
		g = malloc(im.w * im.h * sizeof(int));
		dir = malloc(im.w * im.h * sizeof(int));
		if (g && dir && sobel.data && out.data) {
			canny_calc_gradient_sobel(&clean, g, dir);
			canny_non_max_suppression(&sobel, g, dir);
			canny_estimate_threshold(&sobel, &high, &low);
			canny_hysteresis(high, low, &sobel, &out);
		}
		if (g)free(g);
		if (dir)free(dir);
		if (reduce_noise)sod_free_image(clean);
		sod_free_image(sobel);
		return out;
	}
	/* Make a grayscale version of your image using sod_grayscale_image() or sod_img_load_grayscale() first */
	return sod_make_empty_image(0, 0, 0);
}
/*
* Hough Transform - Portion based on the work of Bruno Keymolen under the BSD License.
* see: http://www.keymolen.com/2013/05/hough-transformation-c-implementation.html
*/
/* Copyright (c) 2013, Bruno Keymolen, email: bruno.keymolen@gmail.com
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this
* list of conditions and the following disclaimer in the documentation and/or other
* materials provided with the distribution.
* Neither the name of "Bruno Keymolen" nor the names of its contributors may be
* used to endorse or promote products derived from this software without specific
* prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_pts * sod_hough_lines_detect(sod_img im, int threshold, int *nPts)
{
#define DEG2RAD 0.017453293f
	double center_x, center_y;
	unsigned int *accu;
	int accu_w, accu_h;
	int img_w, img_h;
	double hough_h;
	SySet aLines;
	sod_pts pts;
	int x, y;
	int r, t;

	if (!im.data || im.c != SOD_IMG_GRAYSCALE) {
		/* Require a binary image using sod_canny_edge_image() */
		*nPts = 0;
		return 0;
	}
	SySetInit(&aLines, sizeof(sod_pts));
	img_w = im.w;
	img_h = im.h;

	hough_h = ((sqrt(2.0) * (double)(im.h>im.w ? im.h : im.w)) / 2.0);
	accu_h = hough_h * 2.0; /* -r -> +r */
	accu_w = 180;

	accu = (unsigned int*)calloc(accu_h * accu_w, sizeof(unsigned int));
	if (accu == 0) {
		*nPts = 0;
		return 0;
	}
	center_x = im.w / 2;
	center_y = im.h / 2;
	for (y = 0; y < img_h; y++)
	{
		for (x = 0; x < img_w; x++)
		{
			if (im.data[y * img_w + x] == 1 /*> 250/255.*/)
			{
				for (t = 0; t < 180; t++)
				{
					double ra = (((double)x - center_x) * cos((double)t * DEG2RAD)) + (((double)y - center_y) * sin((double)t * DEG2RAD));
					accu[(int)((round(ra + hough_h) * 180.0)) + t]++;
				}
			}
		}
	}
	if (threshold < 1) threshold = im.w > im.h ? im.w / 3 : im.h / 3;
	for (r = 0; r < accu_h; r++)
	{
		for (t = 0; t < accu_w; t++)
		{
			if ((int)accu[(r*accu_w) + t] >= threshold)
			{
				int ly, lx;
				/* Is this point a local maxima (9x9) */
				int max = (int)accu[(r*accu_w) + t];
				for (ly = -4; ly <= 4; ly++)
				{
					for (lx = -4; lx <= 4; lx++)
					{
						if ((ly + r >= 0 && ly + r < accu_h) && (lx + t >= 0 && lx + t < accu_w))
						{
							if ((int)accu[((r + ly)*accu_w) + (t + lx)] > max)
							{
								max = (int)accu[((r + ly)*accu_w) + (t + lx)];
								ly = lx = 5;
							}
						}
					}
				}
				if (max >(int)accu[(r*accu_w) + t])
					continue;


				int x1, y1, x2, y2;
				x1 = y1 = x2 = y2 = 0;

				if (t >= 45 && t <= 135)
				{
					/*y = (r - x cos(t)) / sin(t)*/
					x1 = 0;
					y1 = ((double)(r - (accu_h / 2)) - ((x1 - (img_w / 2)) * cos(t * DEG2RAD))) / sin(t * DEG2RAD) + (img_h / 2);
					x2 = img_w - 0;
					y2 = ((double)(r - (accu_h / 2)) - ((x2 - (img_w / 2)) * cos(t * DEG2RAD))) / sin(t * DEG2RAD) + (img_h / 2);
				}
				else
				{
					/* x = (r - y sin(t)) / cos(t);*/
					y1 = 0;
					x1 = ((double)(r - (accu_h / 2)) - ((y1 - (img_h / 2)) * sin(t * DEG2RAD))) / cos(t * DEG2RAD) + (img_w / 2);
					y2 = img_h - 0;
					x2 = ((double)(r - (accu_h / 2)) - ((y2 - (img_h / 2)) * sin(t * DEG2RAD))) / cos(t * DEG2RAD) + (img_w / 2);
				}
				pts.x = x1; pts.y = y1;
				SySetPut(&aLines, &pts);
				pts.x = x2; pts.y = y2;
				SySetPut(&aLines, &pts);
			}
		}
	}
	free(accu);
	*nPts = (int)SySetUsed(&aLines);
	return (sod_pts *)SySetBasePtr(&aLines);
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_hough_lines_release(sod_pts * pLines)
{
	free(pLines);
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_random_augment_image(sod_img im, float angle, float aspect, int low, int high, int size)
{
	aspect = rand_scale(aspect);
	int r = rand_int(low, high);
	int min = (im.h < im.w*aspect) ? im.h : im.w*aspect;
	float scale = (float)r / min;
	float rad = rand_uniform(-angle, angle) * TWO_PI / 360.;
	float dx = (im.w*scale / aspect - size) / 2.;
	float dy = (im.h*scale - size) / 2.;
	if (dx < 0) dx = 0;
	if (dy < 0) dy = 0;
	dx = rand_uniform(-dx, dx);
	dy = rand_uniform(-dy, dy);
	sod_img crop = sod_rotate_crop_image(im, rad, scale, size, size, dx, dy, aspect);
	return crop;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_img_rgb_to_bgr(sod_img im)
{
	int i;
	if (im.c != 3 || !im.data) {
		return;
	}
	for (i = 0; i < im.w*im.h; ++i) {
		float swap = im.data[i];
		im.data[i] = im.data[i + im.w*im.h * 2];
		im.data[i + im.w*im.h * 2] = swap;
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_img_bgr_to_rgb(sod_img im)
{
	int i;
	if (im.c != 3 || !im.data) {
		return;
	}
	for (i = 0; i < im.w*im.h; ++i) {
		float swap = im.data[i + im.w*im.h * 2];
		im.data[i + im.w*im.h * 2] = im.data[i];
		im.data[i] = swap;
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_img_yuv_to_rgb(sod_img im)
{
	int i, j;
	float r, g, b;
	float y, u, v;
	if (im.c != 3) {
		return;
	}
	for (j = 0; j < im.h; ++j) {
		for (i = 0; i < im.w; ++i) {
			y = get_pixel(im, i, j, 0);
			u = get_pixel(im, i, j, 1);
			v = get_pixel(im, i, j, 2);

			r = y + 1.13983*v;
			g = y + -.39465*u + -.58060*v;
			b = y + 2.03211*u;

			set_pixel(im, i, j, 0, r);
			set_pixel(im, i, j, 1, g);
			set_pixel(im, i, j, 2, b);
		}
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_img_rgb_to_yuv(sod_img im)
{
	int i, j;
	float r, g, b;
	float y, u, v;
	if (im.c != 3) {
		return;
	}
	for (j = 0; j < im.h; ++j) {
		for (i = 0; i < im.w; ++i) {
			r = get_pixel(im, i, j, 0);
			g = get_pixel(im, i, j, 1);
			b = get_pixel(im, i, j, 2);

			y = .299*r + .587*g + .114*b;
			u = -.14713*r + -.28886*g + .436*b;
			v = .615*r + -.51499*g + -.10001*b;

			set_pixel(im, i, j, 0, y);
			set_pixel(im, i, j, 1, u);
			set_pixel(im, i, j, 2, v);
		}
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_flip_image(sod_img input)
{
	int i, j, k;
	for (k = 0; k < input.c; ++k) {
		for (i = 0; i < input.h; ++i) {
			for (j = 0; j < input.w / 2; ++j) {
				int index = j + input.w*(i + input.h*(k));
				int flip = (input.w - j - 1) + input.w*(i + input.h*(k));
				float swap = input.data[flip];
				input.data[flip] = input.data[index];
				input.data[index] = swap;
			}
		}
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_image_distance(sod_img a, sod_img b)
{
	int i, j;
	sod_img dist = sod_make_image(a.w, a.h, 1);
	if (dist.data && b.c >= a.c && b.h >= a.h && b.w >= a.w) {
		for (i = 0; i < a.c; ++i) {
			for (j = 0; j < a.h*a.w; ++j) {
				dist.data[j] += pow(a.data[i*a.h*a.w + j] - b.data[i*a.h*a.w + j], 2);
			}
		}
		for (j = 0; j < a.h*a.w; ++j) {
			dist.data[j] = sqrt(dist.data[j]);
		}
	}
	return dist;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_embed_image(sod_img source, sod_img dest, int dx, int dy)
{
	int x, y, k;
	for (k = 0; k < source.c; ++k) {
		for (y = 0; y < source.h; ++y) {
			for (x = 0; x < source.w; ++x) {
				float val = get_pixel(source, x, y, k);
				set_pixel(dest, dx + x, dy + y, k, val);
			}
		}
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_image_draw_box_grayscale(sod_img im, int x1, int y1, int x2, int y2, float g)
{
	if (im.data) {
		int i;
			if (x1 < 0) x1 = 0;
		if (x1 >= im.w) x1 = im.w - 1;
		if (x2 < 0) x2 = 0;
		if (x2 >= im.w) x2 = im.w - 1;

		if (y1 < 0) y1 = 0;
		if (y1 >= im.h) y1 = im.h - 1;
		if (y2 < 0) y2 = 0;
		if (y2 >= im.h) y2 = im.h - 1;

		for (i = x1; i <= x2; ++i) {
			im.data[i + y1 * im.w] = g;
			im.data[i + y2 * im.w] = g;
		}
		for (i = y1; i <= y2; ++i) {
			im.data[x1 + i * im.w] = g;
			im.data[x2 + i * im.w] = g;
		}
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_image_draw_circle(sod_img im, int x0, int y0, int radius, float r, float g, float b)
{
#define plot(x, y) sod_img_set_pixel(im,x,y,0,r);sod_img_set_pixel(im,x,y,1,g);sod_img_set_pixel(im,x,y,2,b);
	int f, ddF_x, ddF_y, x, y;
	if (im.data) {
		r = r / 255.;
		g = g / 255.;
		b = b / 255.;
		if (im.c == 1) {
			/* Draw on grayscale image */
			r = b * 0.114 + g * 0.587 + r * 0.299;
		}
		f = 1 - radius;
		ddF_x = 0;
		ddF_y = -2 * radius;
		x = 0;
		y = radius;

		plot(x0, y0 + radius);
		plot(x0, y0 - radius);
		plot(x0 + radius, y0);
		plot(x0 - radius, y0);

		while (x < y)
		{
			if (f >= 0)
			{
				y--;
				ddF_y += 2;
				f += ddF_y;
			}
			x++;
			ddF_x += 2;
			f += ddF_x + 1;
			plot(x0 + x, y0 + y);
			plot(x0 - x, y0 + y);
			plot(x0 + x, y0 - y);
			plot(x0 - x, y0 - y);
			plot(x0 + y, y0 + x);
			plot(x0 - y, y0 + x);
			plot(x0 + y, y0 - x);
			plot(x0 - y, y0 - x);
		}
	}
#undef plot
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_image_draw_box(sod_img im, int x1, int y1, int x2, int y2, float r, float g, float b)
{
	r = r / 255.;
	g = g / 255.;
	b = b / 255.;
	if (im.c == 1) {
		/* Draw on grayscale image */
		sod_image_draw_box_grayscale(im, x1, y1, x2, y2, b * 0.114 + g * 0.587 + r * 0.299);
		return;
	}
	if (im.data) {
		int i;
		if (x1 < 0) x1 = 0;
		if (x1 >= im.w) x1 = im.w - 1;
		if (x2 < 0) x2 = 0;
		if (x2 >= im.w) x2 = im.w - 1;

		if (y1 < 0) y1 = 0;
		if (y1 >= im.h) y1 = im.h - 1;
		if (y2 < 0) y2 = 0;
		if (y2 >= im.h) y2 = im.h - 1;

		for (i = x1; i <= x2; ++i) {
			im.data[i + y1 * im.w + 0 * im.w*im.h] = r;
			im.data[i + y2 * im.w + 0 * im.w*im.h] = r;

			im.data[i + y1 * im.w + 1 * im.w*im.h] = g;
			im.data[i + y2 * im.w + 1 * im.w*im.h] = g;

			im.data[i + y1 * im.w + 2 * im.w*im.h] = b;
			im.data[i + y2 * im.w + 2 * im.w*im.h] = b;
		}
		for (i = y1; i <= y2; ++i) {
			im.data[x1 + i * im.w + 0 * im.w*im.h] = r;
			im.data[x2 + i * im.w + 0 * im.w*im.h] = r;

			im.data[x1 + i * im.w + 1 * im.w*im.h] = g;
			im.data[x2 + i * im.w + 1 * im.w*im.h] = g;

			im.data[x1 + i * im.w + 2 * im.w*im.h] = b;
			im.data[x2 + i * im.w + 2 * im.w*im.h] = b;
		}
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_image_draw_bbox(sod_img im, sod_box bbox, float r, float g, float b)
{
	sod_image_draw_box(im, bbox.x, bbox.y, bbox.x + bbox.w, bbox.y + bbox.h, r, g, b);
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_image_draw_bbox_width(sod_img im, sod_box bbox, int width, float r, float g, float b)
{
	int i;
	for (i = 0; i < width; i++) {
		sod_image_draw_box(im, bbox.x + i, bbox.y + i, (bbox.x + bbox.w) - i, (bbox.y + bbox.h) - i, r, g, b);
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_image_draw_circle_thickness(sod_img im, int x0, int y0, int radius, int width, float r, float g, float b)
{
	int i;
	for (i = 0; i < width; i++) {
		sod_image_draw_circle(im, x0, y0, radius - i, r, g, b);
	}
	/* @chm: Fill empty pixels */
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_image_draw_line(sod_img im, sod_pts start, sod_pts end, float r, float g, float b)
{
	int x1, x2, y1, y2, dx, dy, err, sx, sy, e2;
	r = r / 255.;
	g = g / 255.;
	b = b / 255.;
	if (im.c == 1) {
		/* Draw on grayscale image */
		r = b * 0.114 + g * 0.587 + r * 0.299;
	}
	x1 = start.x;
	x2 = end.x;
	y1 = start.y;
	y2 = end.y;
	if (x1 < 0) x1 = 0;
	if (x1 >= im.w) x1 = im.w - 1;
	if (x2 < 0) x2 = 0;
	if (x2 >= im.w) x2 = im.w - 1;

	if (y1 < 0) y1 = 0;
	if (y1 >= im.h) y1 = im.h - 1;
	if (y2 < 0) y2 = 0;
	if (y2 >= im.h) y2 = im.h - 1;

	dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
	dy = abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
	err = (dx > dy ? dx : -dy) / 2;

	for (;;) {
		set_pixel(im, x1, y1, 0, r);
		if (im.c == 3) {
			set_pixel(im, x1, y1, 1, g);
			set_pixel(im, x1, y1, 2, b);
		}
		if (x1 == x2 && y1 == y2) break;
		e2 = err;
		if (e2 > -dx) { err -= dy; x1 += sx; }
		if (e2 < dy) { err += dx; y1 += sy; }
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_normalize_image(sod_img p)
{
	if (p.data) {
		int i;
		float min = 9999999;
		float max = -999999;

		for (i = 0; i < p.h*p.w*p.c; ++i) {
			float v = p.data[i];
			if (v < min) min = v;
			if (v > max) max = v;
		}
		if (max - min < .000000001) {
			min = 0;
			max = 1;
		}
		for (i = 0; i < p.c*p.w*p.h; ++i) {
			p.data[i] = (p.data[i] - min) / (max - min);
		}
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
unsigned char * sod_image_to_blob(sod_img im)
{
	unsigned char *data = 0;
	int i, k;
	if (im.data) {
		data = calloc(im.w*im.h*im.c, sizeof(unsigned char));
		if (data) {
			for (k = 0; k < im.c; ++k) {
				for (i = 0; i < im.w*im.h; ++i) {
					data[i*im.c + k] = (unsigned char)(255 * im.data[i + k * im.w*im.h]);
				}
			}
		}
	}
	return data;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_image_free_blob(unsigned char *zBlob)
{
	free(zBlob);
}
/*
* The RealNets training layer implementation.
*/
#ifdef SOD_ENABLE_NET_TRAIN
#ifdef SOD_DISABLE_IMG_READER
#error The training layer require that the SOD_DISABLE_IMG_READER compile-time directive be enabled, please remove the SOD_DISABLE_IMG_READER compile-time directive from this build
#endif /* SOD_DISABLE_IMG_READER */
/*
* Each configuration (key/value) entry is identified by an instance of the following structure.
*/
typedef struct sod_config_node sod_config_node;
struct sod_config_node {
	SyString sKey;
	SyString sVal;
};
/*
* Each collected path is stored in an instance of the following structure.
*/
typedef struct sod_paths sod_paths;
struct sod_paths
{
	size_t max_samples_collect; /* Maximum samples to collect (Optional) */
	SyString sRootPath; /* Root path of any */
	SyString sPosPath;  /* Where the positive samples reside */
	SyString sNegPath;  /* Where the negative samples reside */
	SyString sTestPath; /* Where the test samples reside */
	int recurse;        /* True to recurse on sub-directory */
};
/*
* Each registered layer is identified by an instance of the following structure.
*/
typedef int(*ProcLayerLoad)(sod_realnet_trainer *, sod_config_node *, int, void **);
typedef int(*ProcLayerExec)(void *, sod_paths *);
typedef void(*ProcLayerRelease)(void *);

typedef struct sod_layer sod_layer;
struct sod_layer {
	SyString sName;
	void *pLayerData;          /* Layer private data */
	ProcLayerLoad xLoad;       /* Load callback */
	ProcLayerExec xExec;       /* Training exec callback */
	ProcLayerRelease xRelease; /* Clean-up callback */
};
/*
* Each configuration layer is identified by an instance of the following structure.
*/
typedef struct sod_config_layer sod_config_layer;
struct sod_config_layer {
	SyString sName; /* Layer name */
	SySet   aNode;  /* Array of 'sod_config_node' entries*/
	sod_config_layer *pNext; /* Next Layer on the list */
	sod_config_layer *pPrev; /* Previous Layer on the list */
};
typedef enum {
	SOD_TR_SAMPLE_NEG = 0, /* Negative Sample */
	SOD_TR_SAMPLE_POS,     /* Positive Sample */
	SOD_TR_SAMPLE_TEST     /* Test Sample */
}SOD_TR_SAMPLE_TYPE;
/*
* Each training sample is identified by an instance of the following structure.
*/
typedef struct sod_tr_sample sod_tr_sample;
struct sod_tr_sample
{
	SOD_TR_SAMPLE_TYPE id;
	sod_img sRaw;
	float tv;
	float score;
	int rs;
	int cs;
	int ss;
};
/*
* Current training state is held in an instance of this structure.
*/
struct sod_realnet_trainer
{
	const sod_vfs *pVfs;
	sod_config_layer *pFirst;
	sod_config_layer *pLast;
	int nLayers;
	SySet aBuiltin; /* Built-in layers */
	SyBlob sWorker;
	ProcLogCallback xLog;
	void *pLogData;
	const char *zOutPath; /* Where to store the output cascade */
	SySet aSample;        /* Collected samples for this epoch */
	SySet aPos;
	SySet aNeg;
	SySet aTest;
	int nEpoch;
	/*[paths]*/
	SySet aPaths;
};
/*
* @Training Code Implementation
*/
static inline void sod_config_log_msg(sod_realnet_trainer *pTrainer, const char *zFmt, ...)
{
	va_list ap;
	va_start(ap, zFmt);
	if (pTrainer->xLog) {
		SyBlobReset(&pTrainer->sWorker);
		SyBlobFormatAp(&pTrainer->sWorker, zFmt, ap);
		pTrainer->xLog((const char *)SyBlobData(&pTrainer->sWorker), SyBlobLength(&pTrainer->sWorker), pTrainer->pLogData);
	}
	va_end(ap);
}
/* Forward declaration defined below: Make sure to compile WITHOUT this directive defined: SOD_DISABLE_IMG_READER */
static int ExtractPathInfo(const char *zPath, size_t nByte, sod_path_info *pOut);
/* forward declaration */
static int sy_strnicmp(const char *zA, const char *zB, size_t len);
/*
* Point to the next line from the given memory buffer.
*/
static int sod_config_get_next_line(const char **pzPtr, const char *zEnd, SyString *pBuf, int *pLine)
{
	const char *zPtr = *pzPtr;
	const char *zCur;
	/* Trim leading white spaces */
	while (zPtr < zEnd && isspace(zPtr[0])) {
		if (zPtr[0] == '\n') (*pLine)++;
		zPtr++;
	}
	zCur = zPtr;
	while (zPtr < zEnd && zPtr[0] != '\n') zPtr++;
	size_t n = (size_t)(zPtr - zCur);
	if (n < 1) {
		return -1; /* EOF */
	}
	/* Next Line  */
	*pzPtr = zPtr;
	/* Trailing white spaces shall be trimmed later */
	n = (size_t)(zPtr - zCur);
	pBuf->zString = zCur;
	pBuf->nByte = n;
	return SOD_OK;
}
/*
* Extract a layer name if any.
*/
static void sod_config_get_layer_name(SyString *pBuf)
{
	const char *zIn = pBuf->zString;
	const char *zEnd = &zIn[pBuf->nByte];
	const char *zCur;
	while (zIn < zEnd && (isspace(zIn[0]) || !isalnum(zIn[0]))) zIn++;
	zCur = zIn;
	while (zIn < zEnd && (isalnum(zIn[0]) || zIn[0] == '_'))  zIn++;
	pBuf->zString = zCur;
	pBuf->nByte = (size_t)(zIn - zCur);
}
/*
* Extract a name/value pair.
*/
static void sod_config_get_name_value_pair(SyString *pBuf, SyString *pName, SyString *pVal, sod_realnet_trainer *pTrainer, int line)
{
	const char *zIn = pBuf->zString;
	const char *zEnd = &zIn[pBuf->nByte];
	const char *zCur;
	zCur = zIn;
	pVal->nByte = 0;
	pVal->zString = 0;
	/* White space already trimmed */
	while (zIn < zEnd && (isalnum(zIn[0]) || zIn[0] == '_')) zIn++;
	pName->zString = zCur;
	pName->nByte = (size_t)(zIn - zCur);
	while (zIn < zEnd && isspace(zIn[0])) zIn++;
	if (zIn < zEnd) {
		if (zIn[0] != '=') {
			sod_config_log_msg(&(*pTrainer), "[Line: %d] Expecting '=' next to the key: '%z', got '%c'. Any remaining value will be ignored\n", line, pName, zIn[0]);
		}
		else {
			zIn++;
			while (zIn < zEnd && isspace(zIn[0])) zIn++;
			pVal->zString = zIn;
			pVal->nByte = (size_t)(zEnd - zIn);
			zEnd--;
			while (zEnd > zIn && isspace(zEnd[0]) && pVal->nByte > 0) {
				/* Trailing white spaces */
				pVal->nByte--;
				zEnd--;
			}
		}
	}
}
/*
* Allocate a new configuration layer.
*/
static int sod_config_create_new_layer(sod_realnet_trainer *pTrainer, SyString *pName)
{
	sod_config_layer *pLayer = malloc(sizeof(sod_config_layer));
	if (pLayer == 0) {
		sod_config_log_msg(&(*pTrainer), "Running out of memory\n");
		return SOD_OUTOFMEM;
	}
	memset(pLayer, 0, sizeof(sod_config_layer));
	pLayer->sName = *pName;
	SySetInit(&pLayer->aNode, sizeof(sod_config_node));
	/* Push */
	pLayer->pNext = pTrainer->pLast;
	if (pTrainer->pLast) {
		pTrainer->pLast->pPrev = pLayer;
	}
	pTrainer->pLast = pLayer;
	if (pTrainer->pFirst == 0) {
		pTrainer->pFirst = pLayer;
	}
	pTrainer->nLayers++;
	return SOD_OK;
}
/*
* Parse the whole configuration buffer.
*/
static int sod_parse_config(sod_realnet_trainer *pTrainer, const char *zConf, size_t conf_len)
{
	const char *zIn = zConf;
	const char *zEnd = &zConf[conf_len];
	SyString sEntry, sKey, sVal;
	int line = 1;
	sod_config_log_msg(&(*pTrainer), "Parsing training configuration...\n");
	while (SOD_OK == sod_config_get_next_line(&zIn, zEnd, &sEntry, &line)) {
		sod_config_layer *pLayer;
		/* Discard any comment */
		if (sEntry.zString[0] == '#' || sEntry.zString[0] == ';') continue;
		if (sEntry.zString[0] == '[') {
			/* Extract layer name */
			sod_config_get_layer_name(&sEntry);
			/* Log */
			if (sEntry.nByte < 1) {
				sod_config_log_msg(&(*pTrainer), "[Line: %d] Empty layer found..ignoring\n", line);
			}
			else {
				sod_config_log_msg(&(*pTrainer), "[Line: %d] new layer found: '%z'\n", line, &sEntry);
				/* Register this layer */
				if (SOD_OK != sod_config_create_new_layer(&(*pTrainer), &sEntry)) {
					return SOD_OUTOFMEM;
				}
			}
			continue;
		}
		/* Extract key/value pair */
		sod_config_get_name_value_pair(&sEntry, &sKey, &sVal, &(*pTrainer), line);
		if (sKey.nByte < 1) {
			sod_config_log_msg(&(*pTrainer), "[Line: %d] Missing configuration key..discarding\n", line);
			continue;
		}
		/* Extract the upper layer */
		pLayer = pTrainer->pLast;
		if (pLayer == 0) {
			sod_config_log_msg(&(*pTrainer), "[Line: %d] No upper layer associated with the key: '%z'..discarding\n", line, &sKey);
		}
		else {
			sod_config_node sNode;
			/* Store the key/value pair in the node set */
			sNode.sKey = sKey;
			sNode.sVal = sVal;
			SySetPut(&pLayer->aNode, (const void *)&sNode);
		}
	}
	return SOD_OK;
}
/*
* Convert a non-nil terminated string to double value.
*/
static int SyStrToDouble(const char *zSrc, size_t nLen, double * pOutVal, const char **zRest)
{
#define SXDBL_DIG        15
#define SXDBL_MAX_EXP    308
#define SXDBL_MIN_EXP_PLUS	307
	static const double aTab[] = {
		10,
		1.0e2,
		1.0e4,
		1.0e8,
		1.0e16,
		1.0e32,
		1.0e64,
		1.0e128,
		1.0e256
	};
	short int neg = 0;
	double Val = 0.0;
	const char *zEnd;
	int Lim, exp;
	double *p = 0;

	zEnd = &zSrc[nLen];
	while (zSrc < zEnd && isspace(zSrc[0])) {
		zSrc++;
	}
	if (zSrc < zEnd && (zSrc[0] == '-' || zSrc[0] == '+')) {
		neg = zSrc[0] == '-' ? 1 : 0;
		zSrc++;
	}
	Lim = SXDBL_DIG;
	for (;;) {
		if (zSrc >= zEnd || !Lim || !isdigit(zSrc[0])) break; Val = Val * 10.0 + (zSrc[0] - '0'); zSrc++; --Lim;
		if (zSrc >= zEnd || !Lim || !isdigit(zSrc[0])) break; Val = Val * 10.0 + (zSrc[0] - '0'); zSrc++; --Lim;
		if (zSrc >= zEnd || !Lim || !isdigit(zSrc[0])) break; Val = Val * 10.0 + (zSrc[0] - '0'); zSrc++; --Lim;
		if (zSrc >= zEnd || !Lim || !isdigit(zSrc[0])) break; Val = Val * 10.0 + (zSrc[0] - '0'); zSrc++; --Lim;
	}
	if (zSrc < zEnd && (zSrc[0] == '.' || zSrc[0] == ',')) {
		double dec = 1.0;
		zSrc++;
		for (;;) {
			if (zSrc >= zEnd || !Lim || !isdigit(zSrc[0])) break; Val = Val * 10.0 + (zSrc[0] - '0'); dec *= 10.0; zSrc++; --Lim;
			if (zSrc >= zEnd || !Lim || !isdigit(zSrc[0])) break; Val = Val * 10.0 + (zSrc[0] - '0'); dec *= 10.0; zSrc++; --Lim;
			if (zSrc >= zEnd || !Lim || !isdigit(zSrc[0])) break; Val = Val * 10.0 + (zSrc[0] - '0'); dec *= 10.0; zSrc++; --Lim;
			if (zSrc >= zEnd || !Lim || !isdigit(zSrc[0])) break; Val = Val * 10.0 + (zSrc[0] - '0'); dec *= 10.0; zSrc++; --Lim;
		}
		Val /= dec;
	}
	if (neg == 1 && Val != 0.0) {
		Val = -Val;
	}
	if (Lim <= 0) {
		/* jump overflow digit */
		while (zSrc < zEnd) {
			if (zSrc[0] == 'e' || zSrc[0] == 'E') {
				break;
			}
			zSrc++;
		}
	}
	neg = 0;
	if (zSrc < zEnd && (zSrc[0] == 'e' || zSrc[0] == 'E')) {
		zSrc++;
		if (zSrc < zEnd && (zSrc[0] == '-' || zSrc[0] == '+')) {
			neg = zSrc[0] == '-' ? 1 : 0;
			zSrc++;
		}
		exp = 0;
		while (zSrc < zEnd && isdigit(zSrc[0]) && exp < SXDBL_MAX_EXP) {
			exp = exp * 10 + (zSrc[0] - '0');
			zSrc++;
		}
		if (neg) {
			if (exp > SXDBL_MIN_EXP_PLUS) exp = SXDBL_MIN_EXP_PLUS;
		}
		else if (exp > SXDBL_MAX_EXP) {
			exp = SXDBL_MAX_EXP;
		}
		for (p = (double *)aTab; exp; exp >>= 1, p++) {
			if (exp & 01) {
				if (neg) {
					Val /= *p;
				}
				else {
					Val *= *p;
				}
			}
		}
	}
	while (zSrc < zEnd && isspace(zSrc[0])) {
		zSrc++;
	}
	if (zRest) {
		*zRest = zSrc;
	}
	if (pOutVal) {
		*pOutVal = Val;
	}
	return zSrc >= zEnd ? SOD_OK : -1;
}
/*
* Convert a string to 64-bit integer.
*/
#define SXINT32_MIN_STR		"2147483648"
#define SXINT32_MAX_STR		"2147483647"
static int SyStrToInt32(const char *zSrc, size_t nLen, int32_t *pOutVal, const char **zRest)
{
	const char *zEnd;
	int isNeg = 0;
	int32_t nVal;
	short int i;

	zEnd = &zSrc[nLen];
	while (zSrc < zEnd && isspace(zSrc[0])) {
		zSrc++;
	}
	if (zSrc < zEnd && (zSrc[0] == '-' || zSrc[0] == '+')) {
		isNeg = (zSrc[0] == '-') ? 1 : 0;
		zSrc++;
	}
	/* Skip leading zero */
	while (zSrc < zEnd && zSrc[0] == '0') {
		zSrc++;
	}
	i = 10;
	if ((size_t)(zEnd - zSrc) >= 10) {
		i = memcmp(zSrc, isNeg ? SXINT32_MIN_STR : SXINT32_MAX_STR, 10) <= 0 ? 10 : 9;
	}
	nVal = 0;
	for (;;) {
		if (zSrc >= zEnd || !i || !isdigit(zSrc[0])) { break; } nVal = nVal * 10 + (zSrc[0] - '0'); --i; zSrc++;
		if (zSrc >= zEnd || !i || !isdigit(zSrc[0])) { break; } nVal = nVal * 10 + (zSrc[0] - '0'); --i; zSrc++;
		if (zSrc >= zEnd || !i || !isdigit(zSrc[0])) { break; } nVal = nVal * 10 + (zSrc[0] - '0'); --i; zSrc++;
		if (zSrc >= zEnd || !i || !isdigit(zSrc[0])) { break; } nVal = nVal * 10 + (zSrc[0] - '0'); --i; zSrc++;
	}
	/* Skip trailing spaces */
	while (zSrc < zEnd && isspace(zSrc[0])) {
		zSrc++;
	}
	if (zRest) {
		*zRest = (char *)zSrc;
	}
	if (pOutVal) {
		if (isNeg && nVal != 0) {
			nVal = -nVal;
		}
		*pOutVal = nVal;
	}
	return (zSrc >= zEnd) ? SOD_OK : -1;
}
/* Forward declaration */
static int CmpSyString(SyString *pStr, const char *zIn);
/*
* Duplicate a non-nil terminated string.
*/
static int DupSyString(SyString *pIn, SyString *pOut)
{
	char *z = 0;
	pOut->nByte = 0; /* Marker */
	if (pOut->zString != 0) {
		z = (char *)pOut->zString;
		z = realloc(z, pIn->nByte);
	}
	else {
		z = malloc(pIn->nByte + 1);
		pOut->nByte = 0; /* Marker */
	}
	if (z == 0) return SOD_OUTOFMEM;
	memcpy(z, (const void *)pIn->zString, pIn->nByte);
	z[pIn->nByte] = 0;
	pOut->zString = z;
	pOut->nByte = pIn->nByte;
	return SOD_OK;
}
/*
* Release a dynamically allocated string.
*/
static void FreeSyString(SyString *pIn)
{
	if (pIn->nByte > 0) {
		char *z = (char *)pIn->zString;
		free(z);
	}
}
static void PathConfRelease(sod_paths *pPath)
{
	FreeSyString(&pPath->sRootPath);
	FreeSyString(&pPath->sPosPath);
	FreeSyString(&pPath->sNegPath);
	FreeSyString(&pPath->sTestPath);
}
/*
* @ [paths] layer implementation.
*/
/*
* Load method for the [paths] layer. Get the of list of supplied paths.
* Actually, the [paths] is a special one and works directly with the trainer pointer. That is,
* it does not store any private data.
*/
static int paths_layer_load(sod_realnet_trainer *pTrainer, sod_config_node *aNode, int nNode, void **ppPrivate)
{
	static const SyString aName[] = {
	{ "root",     sizeof("root") - 1 },
	{ "recurse",  sizeof("recurse") - 1 },
	{ "pos",      sizeof("pos") - 1 },
	{ "positive", sizeof("positive") - 1 },
	{ "neg",      sizeof("neg") - 1 },
	{ "negative",  sizeof("negative") - 1 },
	{ "background",  sizeof("background") - 1 },
	{ "test",        sizeof("test") - 1 },
	{ "max_samples",        sizeof("max_samples") - 1 }
	};
	sod_paths sPath;
	int i, j;
	*ppPrivate = 0;
	memset(&sPath, 0, sizeof(sod_paths));
	/* Iterate all over the available entries */
	for (i = 0; i < nNode; i++) {
		sod_config_node *pNode = &aNode[i];
		SyString *pKey = &pNode->sKey;
		for (j = 0; j < (int)sizeof(aName) / sizeof(aName[0]); ++j) {
			const SyString *pName = &aName[j];
			if (SyStringCmp(pName, pKey, sy_strnicmp) == 0) {
				int c = pName->zString[0];
				int32_t iVal = 0;
				switch (c) {
				case 'm':
					/* Max samples to collect */
					SyStrToInt32(pNode->sVal.zString, pNode->sVal.nByte, &iVal, 0);
					if (iVal < 10) {
						sod_config_log_msg(&(*pTrainer), "Minimum samples to collect must be greater than 10\n");
					}
					else {
						sPath.max_samples_collect = (size_t)iVal;
					}
					break;
				case 'r':
					if (pName->zString[1] == 'e') {
						/* Recurse on sub-directory  */
						if (CmpSyString(&pNode->sVal, "true") == 0 || CmpSyString(&pNode->sVal, "1") == 0) {
							sPath.recurse = 1;
						}
					}
					else {
						/* Root path */
						DupSyString(&pNode->sVal, &sPath.sRootPath);
					}
					break;
				case 'p':
					/* Positive samples path */
					DupSyString(&pNode->sVal, &sPath.sPosPath);
					break;
				case 'n':
				case 'b':
					/* Negative samples path */
					DupSyString(&pNode->sVal, &sPath.sNegPath);
					break;
				case 't':
					/* Test samples path */
					DupSyString(&pNode->sVal, &sPath.sTestPath);
					break;
				default:
					/*Can't happen*/
					break;
				}
			}
		}
	}
	/* Validate the configuration */
	if (sPath.sPosPath.nByte < 1 || sPath.sNegPath.nByte < 1) {
		PathConfRelease(&sPath);
		sod_config_log_msg(&(*pTrainer), "Missing positive or negatives samples path..aborting\n");
		return SOD_ABORT;
	}
	if (SOD_OK != SySetPut(&pTrainer->aPaths, (const void *)&sPath)) {
		PathConfRelease(&sPath);
		sod_config_log_msg(&(*pTrainer), "Running out of memory for the [paths] layer\n");
		return SOD_ABORT;
	}
	return SOD_OK;
}
/*
* @ [detector] layer implementation.
*/
typedef struct sod_realnet_detector sod_realnet_detector;
/*
* Each built tree is identified by instance of the following structure.
*/
typedef struct sod_tree sod_tree;
struct sod_tree
{
	sod_realnet_detector *pDet;
	int depth;
	float threshold;
	float *aLeafs; /* Lead table */
	int *aNodes;   /* Internal node table */
};
struct sod_realnet_detector
{
	sod_realnet_trainer *pTrainer;
	int flag; /* Control flags */
	int min_tree_depth; /* Minimum tree depth */
	int max_tree_depth; /* Maximum tree depth */
	int max_trees;      /* Maximum decision trees for the output model */
	char bbox[4];
	int version; /* Output model version format */
	float tpr;   /* True positive rate */
	float fpr;   /* False positive rate */
	float target_fpr; /* Target false positive rate to achieve (i.e. 1e-6)*/
	int data_aug;     /* True to perform some random perturbation on the training set. */
	int norm_samples; /* Normalize positive samples */
	SyString sName;   /* Classifier name */
	SyString sCopyright; /* Copyright info associated with this classifier if any */
	SySet aTree;         /* Regression trees */
	size_t max_samples_collect; /* Maximum samples to collect if any */
};
#define SOD_DET_PROCESS_STARTED 0x001
/*
* List of supported feature extraction algorithms.
*/
#define SOD_DET_PIXEL_ALGO 1 /* Pixel intensity (default) */
#define SOD_DET_LBP_ALGO   2 /* LBP (Support dropped) */
/*
* Load method for the [detector] layer.
*/
enum sod_realnet_entries {
	SOD_DET_STEP_SIZE = 1,    /* Step size (percentage) [step_size = 10%] */
	SOD_DET_SCALE_FACTOR,     /* Scale factor (float) [scale_factor = 1.1] */
	SOD_DET_MIN_TREE_DEPTH,   /* Minimum tree depth [min_tree_depth = 6]*/
	SOD_DET_MAX_TREE_DEPTH,   /* Maximum tree depth [max_tree_depth = 12] */
	SOD_DET_MAX_TREES,        /* Maximum trees to generate [max_trees = 2048] */
	SOD_DET_DATA_AUGMENT,     /* Introduce small perturbation to the input positive samples */
	SOD_DET_TPR,              /* True positive rate [tpr  = 0.9900f] */
	SOD_DET_FPR,              /* False positive rate [fpr = 0.5f] */
	SOD_DET_TARGET_FPR,       /* Target FPR to achieve [target_fpr = 0.01]*/
	SOD_DET_NORMALIZE_SAMPLES,/* Normalize the training positive samples  [normalize = false]*/
	SOD_DET_NAME,             /* Name of the target classifier [name = 'object'] */
	SOD_DET_ABOUT             /* Description & Copyright information about this classifier if any */
};
static int detector_layer_load(sod_realnet_trainer *pTrainer, sod_config_node *aNode, int nNode, void **ppPrivate)
{
	static const struct sod_det_conf {
		const char *zConf;
		int nId;
	}aConf[] = {
	{ "min_tree_depth",      SOD_DET_MIN_TREE_DEPTH },
	{ "max_tree_depth",      SOD_DET_MAX_TREE_DEPTH },
	{ "max_trees",           SOD_DET_MAX_TREES },
	{ "tpr",                 SOD_DET_TPR },
	{ "fpr",                 SOD_DET_FPR },
	{ "data_augment",        SOD_DET_DATA_AUGMENT },
	{ "target_fpr",          SOD_DET_TARGET_FPR },
	{ "normalize",           SOD_DET_NORMALIZE_SAMPLES },
	{ "name",                SOD_DET_NAME },
	{ "about",               SOD_DET_ABOUT }
	};
	sod_realnet_detector *pDet;
	int i, j;
	int nErr = 0;
	/* Allocate the layer private data */
	pDet = malloc(sizeof(sod_realnet_detector));
	if (pDet == 0) {
		sod_config_log_msg(&(*pTrainer), "Running out of memory for the [detector] layer\n");
		return SOD_ABORT;
	}
	/* Fill with default values */
	memset(pDet, 0, sizeof(sod_realnet_detector));
	pDet->pTrainer = pTrainer;
	pDet->fpr = 0.5f;
	pDet->tpr = 0.9975f;
	pDet->min_tree_depth = 6;
	pDet->max_tree_depth = 12;
	pDet->max_trees = 2048;
	pDet->norm_samples = 0;
	pDet->version = 3; /* Increment on each new format */
	pDet->bbox[0] = -127;
	pDet->bbox[1] = +127;
	pDet->bbox[2] = -127;
	pDet->bbox[3] = +127;
	pDet->target_fpr = 1e-6f; /* Stop when the false positive rate (FPR) fall below this value */
	SyStringInitFromBuf(&pDet->sName, "object", sizeof("object") - 1);
	SySetInit(&pDet->aTree, sizeof(sod_tree));
	/* Iterate all over the available entries */
	for (i = 0; i < nNode; i++) {
		sod_config_node *pNode = &aNode[i];
		SyString *pKey = &pNode->sKey;
		for (j = 0; j < (int)sizeof(aConf) / sizeof(aConf[0]); j++) {
			if (CmpSyString(pKey, aConf[j].zConf) == 0) {
				SyString *pVal = &pNode->sVal;
				double dVal = 0.0;
				int32_t iVal = 0;
				/* Process this configuration entry */
				switch (aConf[j].nId) {
				case SOD_DET_NAME:
					if (SyStringLength(&pNode->sVal) > 0) {
						SyStringDupPtr(&pDet->sName, &pNode->sVal);
					}
					break;
				case SOD_DET_ABOUT:
					if (SyStringLength(&pNode->sVal) > 0) {
						SyStringDupPtr(&pDet->sCopyright, &pNode->sVal);
					}
					break;
				case SOD_DET_NORMALIZE_SAMPLES:
					if (CmpSyString(&pNode->sVal, "true") == 0 || CmpSyString(&pNode->sVal, "1") == 0) {
						pDet->norm_samples = 1;
					}
					break;
				case SOD_DET_FPR:
					/* False positive rate */
					SyStrToDouble(pVal->zString, pVal->nByte, &dVal, 0);
					if (dVal >= 1 || dVal < 0.1) {
						sod_config_log_msg(&(*pTrainer), "Maximum False Positive Rate (FPR) must be a float value set between 0.1 .. 1\n");
						nErr++;
					}
					else {
						pDet->fpr = (float)dVal;
					}
					break;
				case SOD_DET_TPR:
					/* True positive rate */
					SyStrToDouble(pVal->zString, pVal->nByte, &dVal, 0);
					if (dVal >= 1 || dVal < 0.1) {
						sod_config_log_msg(&(*pTrainer), "Minimum True Positive Rate (TPR) must be a float value set between 0.1 .. 1\n");
						nErr++;
					}
					else {
						pDet->tpr = (float)dVal;
					}
					break;
				case SOD_DET_DATA_AUGMENT:
					if (CmpSyString(&pNode->sVal, "true") == 0 || CmpSyString(&pNode->sVal, "1") == 0) {
						pDet->data_aug = 1;
					}
					break;
				case SOD_DET_TARGET_FPR:
					/* Target false positive rate */
					SyStrToDouble(pVal->zString, pVal->nByte, &dVal, 0);
					if (dVal < 0.0) {
						sod_config_log_msg(&(*pTrainer), "Target false Positive Rate (TFPR) cannot take a negative value\n");
						nErr++;
					}
					else {
						pDet->target_fpr = (float)dVal;
					}
					break;
				case SOD_DET_MAX_TREE_DEPTH:
					/* Max tree depth */
					SyStrToInt32(pVal->zString, pVal->nByte, &iVal, 0);
					if (iVal < 5 || iVal > 30) {
						sod_config_log_msg(&(*pTrainer), "Maximum tree depth must be an integer set between 5 .. 30\n");
						nErr++;
					}
					else {
						pDet->max_tree_depth = iVal;
					}
					break;
				case SOD_DET_MIN_TREE_DEPTH:
					/* Min tree depth */
					SyStrToInt32(pVal->zString, pVal->nByte, &iVal, 0);
					if (iVal < 1 || iVal > 12) {
						sod_config_log_msg(&(*pTrainer), "Minimum tree depth must be an integer set between 1 .. 12\n");
						nErr++;
					}
					else {
						pDet->min_tree_depth = iVal;
					}
					break;
				case SOD_DET_MAX_TREES:
					/* Max trees */
					SyStrToInt32(pVal->zString, pVal->nByte, &iVal, 0);
					if (iVal < 100) {
						sod_config_log_msg(&(*pTrainer), "Maximum number of trees allowed is: 100\n");
						nErr++;
					}
					else {
						pDet->max_trees = iVal;
					}
					break;
				default:
					break;
				}

			}
		}
	}
	if (nErr > 0) {
		/* Invalid configuration entry */
		sod_config_log_msg(&(*pTrainer), "%d error(s) were recorded. Please check your network configuration again\n", nErr);
		free(pDet);
		return SOD_ABORT;
	}
	*ppPrivate = pDet;
	return SOD_OK;
}
#ifndef SQR
#define SQR(x) ((x)*(x))
#endif /* SQR */
#define SOD_CMP_FIXED_PT 256
/*
* Pixel intensity comparison function. MIT Licensed from the pico project.
*/
static int BinTest(int iNode, int rs, int cs, int ss, sod_img *pTarget)
{
	const char *zNode = (const char *)&iNode;
	int r1, r2, c1, c2, p1, p2;
	r1 = (rs * SOD_CMP_FIXED_PT + zNode[0] * ss) / SOD_CMP_FIXED_PT;
	c1 = (cs * SOD_CMP_FIXED_PT + zNode[1] * ss) / SOD_CMP_FIXED_PT;
	r2 = (rs * SOD_CMP_FIXED_PT + zNode[2] * ss) / SOD_CMP_FIXED_PT;
	c2 = (cs * SOD_CMP_FIXED_PT + zNode[3] * ss) / SOD_CMP_FIXED_PT;
	/* Check for overflow/underflow */
	r1 = MIN(MAX(0, r1), pTarget->h - 1);
	c1 = MIN(MAX(0, c1), pTarget->w - 1);
	r2 = MIN(MAX(0, r2), pTarget->h - 1);
	c2 = MIN(MAX(0, c2), pTarget->w - 1);
	/* Finally, compare */
	p1 = (unsigned char)(255 * pTarget->data[r1 * pTarget->w + c1]);
	p2 = (unsigned char)(255 * pTarget->data[r2 * pTarget->w + c2]);
	return p1 <= p2;
}
/*
* Run across the tree and get its output.
*/
static float DetectorGetTreeOutput(sod_tree *pTree, int rs, int cs, int ss, sod_img *pTarget)
{
	int j, idx = 1;
	for (j = 0; j < pTree->depth; j++) {
		int iNode = pTree->aNodes[idx - 1];
		idx = 2 * idx + BinTest(iNode, rs, cs, ss, &(*pTarget));
	}
	return pTree->aLeafs[idx - (1 << pTree->depth)];
}
/*
* Classify an image using the currently built tree
*/
static inline int DetectorClassifyBlob(sod_realnet_detector *pDet, float *score, int rs, int cs, int ss, sod_img *pTarget)
{
	sod_tree *aTree = (sod_tree *)SySetBasePtr(&pDet->aTree);
	size_t n;
	*score = 0.0f;
	for (n = 0; n < SySetUsed(&pDet->aTree); n++) {
		*score += DetectorGetTreeOutput(&aTree[n], rs, cs, ss, &(*pTarget));
		if (*score <= aTree[n].threshold)
			return -1;
	}
	/* False/True positive, upper layers will verify that. */
	return 1;
}
/*
* Split the training samples into two groups using the selected pair of pixels (iCode).
*/
static uint32_t SplitNodes(int iCode, uint32_t *aInd, sod_tr_sample *aSample, uint32_t nSample)
{
	uint32_t i = 0, j = nSample - 1;
	for (;;) {
		while (!BinTest(iCode, aSample[aInd[i]].rs, aSample[aInd[i]].cs, aSample[aInd[i]].ss, &aSample[aInd[i]].sRaw)) {
			if (i == j)
				break;
			i++;
		}
		while (BinTest(iCode, aSample[aInd[j]].rs, aSample[aInd[j]].cs, aSample[aInd[j]].ss, &aSample[aInd[j]].sRaw)) {
			if (i == j)
				break;
			j--;
		}
		if (i >= j) {
			break;
		}
		else {
			aInd[i] = aInd[i] ^ aInd[j];
			aInd[j] = aInd[i] ^ aInd[j];
			aInd[i] = aInd[i] ^ aInd[j];
		}
	}
	j = 0;
	for (i = 0; i < nSample; ++i) {
		if (!BinTest(iCode, aSample[aInd[i]].rs, aSample[aInd[i]].cs, aSample[aInd[i]].ss, &aSample[aInd[i]].sRaw)) {
			j++;
		}
	}
	return j;
}
/*
* Compute split entropy.
*/
static float ComputeSplitEntropy(int iCode, double *ws, uint32_t *inds, sod_tr_sample *aSample, uint32_t nSample)
{
	double wtvalsum0, wtvalsumsqr0, wtvalsum1, wtvalsumsqr1;
	double wsum, wsum0, wsum1;
	double wmse0, wmse1;
	uint32_t i;
	wsum = wsum0 = wsum1 = wtvalsum0 = wtvalsum1 = wtvalsumsqr0 = wtvalsumsqr1 = 0.0;
	for (i = 0; i < nSample; ++i) {
		sod_tr_sample *pSample = &aSample[inds[i]];
		if (BinTest(iCode, pSample->rs, pSample->cs, pSample->ss, &pSample->sRaw)) {
			wsum1 += ws[inds[i]];
			wtvalsum1 += ws[inds[i]] * pSample->tv;
			wtvalsumsqr1 += ws[inds[i]] * SQR(pSample->tv);
		}
		else {
			wsum0 += ws[inds[i]];
			wtvalsum0 += ws[inds[i]] * pSample->tv;
			wtvalsumsqr0 += ws[inds[i]] * SQR(pSample->tv);
		}
		wsum += ws[inds[i]];
	}
	wmse0 = wtvalsumsqr0 - SQR(wtvalsum0) / wsum0;
	wmse1 = wtvalsumsqr1 - SQR(wtvalsum1) / wsum1;
	return (float)((wmse0 + wmse1) / wsum);
}
/*
* Generate random internal node codes.
*/
static int GenerateRandomNodeCodes(const char *zBox)
{
	int iCode = 0; /* cc warning */
	char *z;
	z = (char *)&iCode;
	z[0] = zBox[0] + rand() % (zBox[1] - zBox[0] + 1);
	z[1] = zBox[2] + rand() % (zBox[3] - zBox[2] + 1);
	z[2] = zBox[0] + rand() % (zBox[1] - zBox[0] + 1);
	z[3] = zBox[2] + rand() % (zBox[3] - zBox[2] + 1);
	return iCode;
}
/*
* Gen table
*/
struct gen_table
{
	int iCode; /* Random code */
	float es;  /* Error split probability */
};
/*
* Grow a regression tree.
*/
static void DetectorGrowTree(sod_tree *pTree, int nodeidx, int cur_depth, int max_depth, double *wt, uint32_t *aInd, sod_tr_sample *aSample, uint32_t nSample)
{
	uint32_t i;
	if (cur_depth >= max_depth) {
		int tidx = nodeidx - ((1 << max_depth) - 1); /* Index of this node on the lookup table */
		double sum, tvacc;
		sum = tvacc = 0.0;
		/* Compute the average */
		for (i = 0; i < nSample; i++) {
			tvacc += wt[aInd[i]] * aSample[aInd[i]].tv;
			sum += wt[aInd[i]];
		}
		if (sum == 0.0)
			pTree->aLeafs[tidx] = 0.0f;
		else
			pTree->aLeafs[tidx] = (float)(tvacc / sum);
	}
	else if (nSample < 2) {
		pTree->aNodes[nodeidx] = 0;
		/* Recurse on this subtree until we reach a leaf node */
		DetectorGrowTree(pTree, 2 * nodeidx + 1, cur_depth + 1, max_depth, wt, aInd, aSample, nSample);
		DetectorGrowTree(pTree, 2 * nodeidx + 2, cur_depth + 1, max_depth, wt, aInd, aSample, nSample);
	}
	else {
		int best;
		float emin;
#define GEN_TABLE_RAND 1024
		struct gen_table aGen[GEN_TABLE_RAND];
		for (i = 0; i < GEN_TABLE_RAND; i++) {
			struct gen_table *pGen = &aGen[i];
			pGen->iCode = GenerateRandomNodeCodes(pTree->pDet->bbox);
			pGen->es = ComputeSplitEntropy(pGen->iCode, wt, aInd, aSample, nSample);
		}
		/* Grab the best candidate for this internal node */
		best = aGen[0].iCode;
		emin = aGen[0].es;
		for (i = 1; i < GEN_TABLE_RAND; i++) {
			if (emin > aGen[i].es) {
				emin = aGen[i].es;
				best = aGen[i].iCode;
			}
		}
		pTree->aNodes[nodeidx] = best;
		/* Split the subtree */
		i = SplitNodes(best, aInd, aSample, nSample);
		DetectorGrowTree(&(*pTree), 2 * nodeidx + 1, cur_depth + 1, max_depth, wt, &aInd[0], aSample, i);
		DetectorGrowTree(&(*pTree), 2 * nodeidx + 2, cur_depth + 1, max_depth, wt, &aInd[i], aSample, nSample - i);
	}
}
/*
* Build a regression tree for the current epoch.
*/
static int DetectorBuildTree(sod_tree *pTree, int depth, double *wt, sod_tr_sample *aSample, size_t nSample)
{
	uint32_t nNode = (1 << depth) - 1;
	uint32_t i, *aInd;
	float *table;
	int *pBin;
	/* Allocate the necessary tables */
	pBin = (int *)malloc(nNode * sizeof(int));
	if (pBin == 0) return SOD_ABORT;
	table = (float *)malloc((1 << depth) * sizeof(float));
	if (table == 0) {
		free(pBin);
		return SOD_ABORT;
	}
	aInd = (uint32_t *)malloc((uint32_t)nSample * sizeof(uint32_t));
	if (aInd == 0) {
		free(pBin);
		free(table);
		return SOD_ABORT;
	}
	/* Initialize */
	memset(pBin, 0, nNode * sizeof(int));
	memset(table, 0, (1 << depth) * sizeof(float)); /* Please ignore the annoying VS2017 compiler warning on 32-bit shift */
	pTree->aNodes = pBin;
	pTree->aLeafs = table;
	pTree->depth = depth;
	/* Build */
	for (i = 0; i < (uint32_t)nSample; i++) {
		aInd[i] = i;
	}
	DetectorGrowTree(&(*pTree), 0, 0, depth, wt, aInd, aSample, (uint32_t)nSample);
	/* Cleanup */
	free(aInd);
	return SOD_OK;
}
/*
* Learn a new stage.
*/
static int DetectorLearnNewStage(sod_realnet_detector *pDet, float mintpr, float maxfpr, size_t maxtree, size_t nPos, size_t nNeg)
{
	sod_realnet_trainer *pTrainer = pDet->pTrainer;
	sod_tr_sample *aSample = (sod_tr_sample *)SySetBasePtr(&pTrainer->aSample);
	size_t i, nSample = SySetUsed(&pTrainer->aSample);
	float threshold = 0.0f;
	float tpr, fpr;
	double *wt;
	/* Allocate the weights table */
	wt = (double *)malloc(nSample * sizeof(double));
	if (wt == 0) {
		sod_config_log_msg(pTrainer, "Running out-of-memory...aborting\n");
		return SOD_ABORT;
	}
	maxtree = SySetUsed(&pDet->aTree) + maxtree;
	fpr = 1.0f;
	nNeg = SySetUsed(&pTrainer->aSample) - nPos;
	/* Start the learning process */
	sod_config_log_msg(pTrainer, "Learning new detection stage for epoch#%d\n", pTrainer->nEpoch);
	while (SySetUsed(&pDet->aTree) < maxtree && fpr > maxfpr) {
		float end, start = pTrainer->pVfs->xTicks();
		sod_tr_sample *pEntry;
		double ws = 0.0;
		sod_tree sTree;
		/* Compute the weights first */
		for (i = 0; i < nSample; ++i) {
			pEntry = &aSample[i];
			if (pEntry->tv > 0) {
				wt[i] = exp(-1.0 * pEntry->score) / nPos;
			}
			else {
				wt[i] = exp(+1.0 * pEntry->score) / nNeg;
			}
			ws += wt[i];
		}
		/* Compute the average */
		for (i = 0; i < nSample; ++i) wt[i] /= ws;
		/* Build the tree */
		memset(&sTree, 0, sizeof(sod_tree));
		sTree.pDet = pDet;
		if (SOD_OK != DetectorBuildTree(&sTree, pDet->min_tree_depth, wt, aSample, nSample)) {
			/* Mostly out of memory, abort */
			free(wt);
			return SOD_ABORT;
		}
		/* Set a low confidence threshold first */
		sTree.threshold = -1337.0f /* -FLT_MAX */;
		/* Insert */
		if (SOD_OK != SySetPut(&pDet->aTree, &sTree)) {
			free(sTree.aNodes);
			free(sTree.aLeafs);
			free(wt);
			sod_config_log_msg(pTrainer, "Running out-of-memory...aborting\n");
			return SOD_ABORT;
		}
		/* Update score */
		for (i = 0; i < nSample; ++i) {
			float score;
			pEntry = &aSample[i];
			score = DetectorGetTreeOutput(&sTree, pEntry->rs, pEntry->cs, pEntry->ss, &pEntry->sRaw);
			pEntry->score += score;
		}
		/* Calculate threshold */
		threshold = 5.0f;
		do {
			uint32_t ntps, nfps;
			ntps = nfps = 0;
			/* Adjust threshold */
			threshold -= 0.005f;
			for (i = 0; i < nSample; ++i) {
				pEntry = &aSample[i];
				if (pEntry->tv > 0 && pEntry->score > threshold) ntps++;
				if (pEntry->tv < 0 && pEntry->score > threshold) nfps++;
			}
			tpr = ntps / (float)nPos;
			fpr = nfps / (float)nNeg;
		} while (tpr < mintpr);
		/* Tree generated, log that */
		end = pTrainer->pVfs->xTicks();
		sod_config_log_msg(pTrainer, "Tree#%d of depth %d built in %d seconds: FPR: %f TPR: %f\n",
		(int)SySetUsed(&pDet->aTree),
		pDet->min_tree_depth,
		(int)(end - start),
		fpr,
		tpr
		);
	}
	if (SySetUsed(&pDet->aTree) > 0) {
		/* Set the final threshold */
		sod_tree *aTree = (sod_tree *)SySetBasePtr(&pDet->aTree);
		aTree[SySetUsed(&pDet->aTree) - 1].threshold = threshold;
		sod_config_log_msg(&(*pTrainer), "Final threshold value for tree#%d set to: %f\n",
			(int)SySetUsed(&pDet->aTree),
			threshold
		);
	}
	if (fpr > maxfpr && pDet->min_tree_depth < pDet->max_tree_depth) {
		/* Increment the tree depth due to high false positive rate */
		/* 
		pDet->min_tree_depth++;
		sod_config_log_msg(&(*pTrainer), "Tree depth incremented to %d due to high false positive rate\n", pDet->min_tree_depth);
		*/
	}
	/* Cleanup */
	free(wt);
	return SOD_OK;
}
/* Directory recursion limit while gathering samples. */
#ifndef SOD_MAX_RECURSE_COUNT
#define SOD_MAX_RECURSE_COUNT 3
#endif /* SOD_MAX_RECURSE_COUNT */
static float aug_rand(float M, float N)
{
	return M + (rand() / (RAND_MAX / (N - M)));
}
/*
* Collect image samples from a given path.
* Only png, jpeg, bmp, pgm, pbm image format are allowed.
*/
static int DetectorCollectSamples(sod_realnet_detector *pDet, const char *zPath, SOD_TR_SAMPLE_TYPE iType, int recurse, int rec_count)
{
	static const char *zAllowed[] = { "png","jpg","jpeg","bmp","pgm","ppm","pbm", 0 /*Marker*/ };
	sod_realnet_trainer *pTrainer = pDet->pTrainer;
	const sod_vfs *pVfs = pTrainer->pVfs;
	sod_path_info sPath;
	SySet *	pTarget;
	SyBlob sReader;
	void *pHandle;
	sod_img sRaw;
	int i, rc;
	/* Iterate over the target directory */
	rc = pVfs->xOpenDir(zPath, &pHandle);
	if (rc != SOD_OK) {
		sod_config_log_msg(&(*pTrainer), "IO error while entering directory: '%s'\n", zPath);
		return rc;
	}
	SyBlobInit(&sReader);
	sod_config_log_msg(&(*pTrainer), "Entering directory: '%s'..\n", zPath);
	pVfs->xChdir(zPath);
	if (iType == SOD_TR_SAMPLE_POS) {
		pTarget = &pTrainer->aPos;
	}
	else {
		pTarget = (iType == SOD_TR_SAMPLE_TEST ? &pTrainer->aTest : &pTrainer->aNeg);
	}
	while (pVfs->xDirRead(pHandle, &sReader) == SOD_OK) {
		const char *zEntry = (const char *)SyBlobData(&sReader);
		size_t nByte = SyBlobLength(&sReader);
		sod_img tmp;
		/* Reset the blob */
		SyBlobReset(&sReader);
		if (pVfs->xIsdir(zEntry)) {
			/* Entry is a directory, recurse if allowed */
			if (recurse) {
				if (rec_count < SOD_MAX_RECURSE_COUNT) {
					/* Recurse */
					rec_count++;
					if (SOD_OK == DetectorCollectSamples(&(*pDet), zEntry, iType, 1, rec_count)) {
						/* Return to the upper directory */
						pVfs->xChdir("../");
					}
					rec_count--;
				}
				else {
					sod_config_log_msg(&(*pTrainer), "Recursion limit (%d) reached while entering directory: '%s'..aborting\n", SOD_MAX_RECURSE_COUNT, zEntry);
				}
			}
			/* Ignore */
			continue;
		}
		/* Make sure the file is of the correct extension */
		ExtractPathInfo(zEntry, nByte, &sPath);
		rc = 0;
		for (i = 0; zAllowed[i] != 0; i++) {
			if (CmpSyString(&sPath.sExtension, zAllowed[i]) == 0) {
				rc = 1;
				break;
			}
		}
		if (!rc) {
			sod_config_log_msg(&(*pTrainer), "Discarding file not of the expected extension: '%s'\n", zEntry);
			continue;
		}
		if (pDet->max_samples_collect > 0 && pTarget->nUsed >= pDet->max_samples_collect) {
			sod_config_log_msg(&(*pTrainer), "Maximum samples to collect limit reached: %u..stopping\n", (unsigned int)pTarget->nUsed);
			break;
		}
		/* Prepare the insertion */
		memset(&sRaw, 0, sizeof(sod_img));
		/* Load the image */
		sRaw = sod_img_load_from_file(zEntry, 1);
		if (sRaw.data == 0) {
			sod_config_log_msg(&(*pTrainer), "Cannot load sample: '%s'..ignoring\n", zEntry);
			continue;
		}
		/* Insert in the target set */
		rc = SySetPut(pTarget, (const void *)&sRaw);
		if (rc != SOD_OK) {
			/* Avoid memory leaks */
			sod_free_image(sRaw);
		}
		else {
			if (iType == SOD_TR_SAMPLE_POS) {
				/* Sum-up the average */
				if (pDet->data_aug) {
					float rand = aug_rand(1.0f, 40.0f);
					tmp = sod_rotate_image(sRaw, rand);
					if (tmp.data) {
						sRaw = tmp;
						rc = SySetPut(pTarget, (const void *)&sRaw);
						if (rc != SOD_OK) {
							sod_free_image(sRaw);
						}
					}
					tmp = sod_copy_image(sRaw);
					if (tmp.data) {
						sod_flip_image(tmp);
						sRaw = tmp;
						rc = SySetPut(pTarget, (const void *)&sRaw);
						if (rc != SOD_OK) {
							sod_free_image(tmp);
						}
					}
					tmp = sod_copy_image(sRaw);
					if (tmp.data) {
						rand = aug_rand(0.1f, 1.0f);
						sod_scale_image(tmp, rand);
						sRaw = tmp;
						rc = SySetPut(pTarget, (const void *)&sRaw);
						if (rc != SOD_OK) {
							sod_free_image(tmp);
						}
					}
					tmp = sod_copy_image(sRaw);
					if (tmp.data) {
						rand = aug_rand(0.1f, 1.0f);
						sod_translate_image(tmp, rand);
						sRaw = tmp;
						rc = SySetPut(pTarget, (const void *)&sRaw);
						if (rc != SOD_OK) {
							sod_free_image(tmp);
						}
					}
				}
			}
			if (pTarget->nUsed > 0 && ((pTarget->nUsed % 300) == 0)) {
				sod_config_log_msg(&(*pTrainer), "Over %u %s samples were collected so far..\n", (unsigned int)pTarget->nUsed, (iType == SOD_TR_SAMPLE_POS ? "positive" : "negative/test"));
			}
		}
	}
	/* Cleanup */
	pVfs->xCloseDir(pHandle);
	SyBlobRelease(&sReader);
	if (rec_count < 1) {
		if (pTarget->nUsed < 1) {
			if (iType == SOD_TR_SAMPLE_POS) {
				sod_config_log_msg(&(*pTrainer), "no positive samples were collected on directory: '%s'..aborting\n", zPath);
				return SOD_ABORT;
			}
			/* Does not hurt when working with test samples */
			return SOD_OK;
		}
		/* log */
		sod_config_log_msg(&(*pTrainer), "%u %s samples were collected on directory: '%s'\n", (unsigned int)pTarget->nUsed, (iType == SOD_TR_SAMPLE_POS ? "positive" : "negative/test"), zPath);
	}
	return SOD_OK;
}
/*
* Arrange the N elements of ARRAY in random order. Only effective if N is much smaller than RAND_MAX;
* if this may not be the case, use a better random number generator.
*/
static void shuffleImgArray(sod_img *array, size_t n)
{
	if (n > 1)
	{
		size_t i;
		for (i = 0; i < n - 1; i++)
		{
			size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
			sod_img t = array[j];
			array[j] = array[i];
			array[i] = t;
		}
	}
}
/*
* Load training sample for the current Epoch.
*/
static float DetectorPrepareEpochSamples(sod_realnet_detector *pDet, size_t *nPos, size_t *nNeg)
{
	sod_realnet_trainer *pTrainer = pDet->pTrainer;
	float score = 0.0f;
	sod_tr_sample sTr;
	float etpr, efpr;
	sod_img *aRaw;
	size_t nc, i;
	int round, rc;
	sod_config_log_msg(&(*pTrainer), "Sampling started [epoch#%d]\n", ++pTrainer->nEpoch);
	/* Reset the collector */
	SySetReset(&pTrainer->aSample);
	/* Iterate through the positive sample first */
	aRaw = (sod_img *)SySetBasePtr(&pTrainer->aPos);
	for (i = 0; i < SySetUsed(&pTrainer->aPos); ++i) {
		sod_img *pRaw = &aRaw[i];
		/* Prepare classification */
		memset(&sTr, 0, sizeof(sod_tr_sample));
		sTr.id = SOD_TR_SAMPLE_POS;
		sTr.score = score;
		sTr.tv = +1;
		sTr.sRaw = *pRaw;
		sTr.rs = pRaw->h / 2;
		sTr.cs = pRaw->w / 2;
		sTr.ss = 2 * MIN(pRaw->w, pRaw->h) / 3;
		/* Classify */
		rc = DetectorClassifyBlob(&(*pDet), &score, sTr.rs, sTr.cs, sTr.ss, pRaw);
		if (rc != 1) {
			/* Sadly mistaken as a false negative, cannot recover from this */
			continue;
		}
		SySetPut(&pTrainer->aSample, (const void *)&sTr);
	}
	/* Total positive samples for this epoch */
	*nPos = SySetUsed(&pTrainer->aSample);
	sod_config_log_msg(&(*pTrainer), "%u positive samples were prepared for epoch#%d\n", (unsigned int)*nPos, pTrainer->nEpoch);
	*nNeg = nc = 0;
	if (SySetUsed(&pTrainer->aNeg) > 0) {
		/* Shuffle the negative samples array */
		aRaw = (sod_img *)SySetBasePtr(&pTrainer->aNeg);
		shuffleImgArray(aRaw, SySetUsed(&pTrainer->aNeg));
		/* Iterate through the negative samples */
		round = 0;
		i = 0;
		for (;;) {
			sod_img *pRaw = &aRaw[i];
			sod_tr_sample *pSam;
			int r, c, s;
			/* Data mine hard negatives */
			r = rand() % pRaw->h;
			c = rand() % pRaw->w;
			/* sample the size of a random object in the pool */
			pSam = SySetFetch(&pTrainer->aSample, rand() % *nPos);
			s = pSam->ss;
			/* Classify */
			rc = DetectorClassifyBlob(&(*pDet), &score, r, c, s, pRaw);
			if (rc == 1) {
				if (*nNeg >= *nPos) {
					break;
				}
				/* We have a false positive, prepare the insertion */
				memset(&sTr, 0, sizeof(sod_tr_sample));
				sTr.id = SOD_TR_SAMPLE_NEG;
				sTr.score = score;
				sTr.tv = -1;
				sTr.sRaw = *pRaw;
				sTr.rs = r;
				sTr.cs = c;
				sTr.ss = s;
				SySetPut(&pTrainer->aSample, (const void *)&sTr);
				(*nNeg)++;
			}
			nc++;
			i++;
			if (i >= SySetUsed(&pTrainer->aNeg)) {
				round++;
				if (round > 10000 ) {
					sod_config_log_msg(&(*pTrainer), "Too many background image collection rounds (> 10K). Perhaps should you stop training and test the accuracy of your model?\n");
					round = 0;
				}
				/* Reset back */
				i = 0;
				/* Shuffle again */
				shuffleImgArray(aRaw, SySetUsed(&pTrainer->aNeg));
			}
		}
	}
	else {
		nc = 1;
	}
	/* Calculate the epoch true/false positive rate */
	etpr = *nPos / ((float)SySetUsed(&pTrainer->aPos));
	efpr = (float)(*nNeg / (double)nc);

	sod_config_log_msg(&(*pTrainer),
		"sampling finished [Epoch#%d]:\n\tTrue Positive Rate (TPR): %0.8f\n\tFalse Positive Rate (FPR): %0.8f\n",
		pTrainer->nEpoch,
		etpr,
		efpr
	);
	return efpr;
}
/*
* Generate a binary classifier to be used with the realnet network.
*/
static int DetectorSaveCascadetoDisk(sod_realnet_detector *pDet)
{
	int nTrees = (int)SySetUsed(&pDet->aTree);
	sod_tree *aTrees;
	int i;
	FILE* file;
	if (pDet->pTrainer->zOutPath == 0) {
		/* No output model path were specified, return */
		return SOD_OK;
	}
	if (nTrees < 1) {
		/* No regression trees were actually generated, return immediately */
		return SOD_OK;
	}
	file = fopen(pDet->pTrainer->zOutPath, "wb");
	if (!file) {
		return SOD_ABORT;
	}
	/* Magic headers */
	fwrite(&pDet->version, sizeof(int32_t), 1, file);
	fwrite(&pDet->bbox[0], sizeof(int8_t), 4, file);
	fwrite(&pDet->min_tree_depth, sizeof(int), 1, file);
	fwrite(&nTrees, sizeof(int), 1, file);
	/* Generated tress */
	aTrees = SySetBasePtr(&pDet->aTree);
	for (i = 0; i<nTrees; ++i)
	{
		sod_tree *pTree = &aTrees[i];
		/* Nodes, lookup table and thresholds. */
		fwrite(pTree->aNodes, sizeof(int32_t), (1 << pTree->depth) - 1, file);
		fwrite(pTree->aLeafs, sizeof(float), 1 << pTree->depth, file);
		fwrite(&pTree->threshold, sizeof(float), 1, file);
	}
	fclose(file);
	return SOD_OK;
}
/*
* Exec method for the [detector] layer.
*/
static int detector_layer_exec(void *pPrivate, sod_paths *pPath)
{
	sod_realnet_detector *pDet = (sod_realnet_detector *)pPrivate;
	sod_realnet_trainer *pTr = pDet->pTrainer;
	float xEnd, xStart;
	size_t nPos, nNeg;
	float fpr;
	int rc;
	if (pPath == 0) {
		sod_config_log_msg(pTr, "Missing training samples paths for detector [%z]..aborting\n", &pDet->sName);
		return SOD_ABORT;
	}
	/* Change root path if any */
	if (SyStringLength(&pPath->sRootPath) > 0) {
		pTr->pVfs->xChdir(pPath->sRootPath.zString);
	}
	pDet->max_samples_collect = pPath->max_samples_collect;
	/* Collect samples first */
	sod_config_log_msg(pTr, "Collecting positive samples..\n");
	rc = DetectorCollectSamples(pDet, pPath->sPosPath.zString, SOD_TR_SAMPLE_POS, pPath->recurse, 0);
	if (rc != SOD_OK) return rc;
	sod_config_log_msg(pTr, "Collecting negative samples..\n");
	rc = DetectorCollectSamples(pDet, pPath->sNegPath.zString, SOD_TR_SAMPLE_NEG, pPath->recurse, 0);
	if (rc != SOD_OK) return rc;
	if (SyStringLength(&pPath->sTestPath) > 0) {
		sod_config_log_msg(pTr, "Collecting test samples..\n");
		/* Not so fatal if this fail */
		DetectorCollectSamples(pDet, pPath->sTestPath.zString, SOD_TR_SAMPLE_TEST, pPath->recurse, 0);
	}
	/* Start the learning process */
	sod_config_log_msg(pTr, "Learning process started for classifier [%z]\n", &pDet->sName);
	xStart = pTr->pVfs->xTicks();

	DetectorPrepareEpochSamples(pDet, &nPos, &nNeg);
	rc = DetectorLearnNewStage(pDet, 0.9800f, 0.5f, 4, nPos, nNeg);
	if (rc != SOD_OK) return rc;

	DetectorPrepareEpochSamples(pDet, &nPos, &nNeg);
	rc = DetectorLearnNewStage(pDet, 0.9850f, 0.5f, 8, nPos, nNeg);
	if (rc != SOD_OK) return rc;

	DetectorPrepareEpochSamples(pDet, &nPos, &nNeg);
	rc = DetectorLearnNewStage(pDet, 0.9900f, 0.5f, 16, nPos, nNeg);
	if (rc != SOD_OK) return rc;

	DetectorPrepareEpochSamples(pDet, &nPos, &nNeg);
	rc = DetectorLearnNewStage(pDet, 0.9950f, 0.5f, 32, nPos, nNeg);
	if (rc != SOD_OK) return rc;
	/* Save the cascade */
	if (SOD_OK != DetectorSaveCascadetoDisk(pDet)) {
		sod_config_log_msg(pTr, "IO error while saving cascade to disk..stopping\n");
		return rc;
	}

	/* Train until the target FPR is reached */
	for (;;) {
		fpr = DetectorPrepareEpochSamples(pDet, &nPos, &nNeg);
		if (fpr <= pDet->target_fpr) {
			break;
		}
		rc = DetectorLearnNewStage(pDet, pDet->tpr, pDet->fpr, 64, nPos, nNeg);
		if (rc != SOD_OK) return rc;
		if (SOD_OK != DetectorSaveCascadetoDisk(pDet)) {
			sod_config_log_msg(pTr, "IO error while saving cascade to disk\n");
		}
		if (pDet->max_trees > 0 && (int)SySetUsed(&pDet->aTree) > pDet->max_trees) {
			sod_config_log_msg(pTr, "Maximum number of allowed tree in the classifier [%z] is reached..stopping\n", &pDet->sName);
			break;
		}
	}
	xEnd = pTr->pVfs->xTicks();
	sod_config_log_msg(pTr, "Target FPR for the classifier [%z] reached in %u seconds. Final cascade classifier (%u Trees) already generated..training done!", &pDet->sName, (unsigned int)(xEnd - xStart), (unsigned int)SySetUsed(&pDet->aTree));
	return SOD_OK;
}
/*
* Release method for the [detector] layer.
*/
static void detector_layer_release(void *pPrivate)
{
	if (pPrivate) {
		sod_realnet_detector *pDet = (sod_realnet_detector *)pPrivate;
		sod_tree *aTree = (sod_tree *)SySetBasePtr(&pDet->aTree);
		size_t i;
		for (i = 0; i < SySetUsed(&pDet->aTree); i++) {
			sod_tree *pTree = &aTree[i];
			free(pTree->aNodes);
			free(pTree->aLeafs);
		}
		SySetRelease(&pDet->aTree);
		free(pDet);
	}
}
/*
* Each built-in layer is defined by an instance of the following
* structure.
*/
typedef struct sod_builtin_layer sod_builtin_layer;
struct sod_builtin_layer {
	const char *zName; /* Layer name */
	ProcLayerLoad    xLoad; /* Load callback */
	ProcLayerExec    xExec; /* Exec callback */
	ProcLayerRelease xRelease; /* Release callback */
}aBuiltLayers[] = {
	{ "paths", paths_layer_load, 0, 0 },
	{ "path",  paths_layer_load, 0, 0 }, /* Alias for [paths] */
	{ "detector", detector_layer_load, detector_layer_exec, detector_layer_release },
	{ "det",      detector_layer_load, detector_layer_exec, detector_layer_release }, /* Alias for [detector] */
};
/*
* Register the built-in layers.
*/
static void sod_realnet_trainer_register_builtin_layers(sod_realnet_trainer *pTrainer)
{
	size_t n;
	for (n = 0; n < sizeof(aBuiltLayers) / sizeof(aBuiltLayers[0]); ++n) {
		sod_layer sLayer;
		/* Fill in */
		SyStringInitFromBuf(&sLayer.sName, aBuiltLayers[n].zName, strlen(aBuiltLayers[n].zName));
		sLayer.xLoad = aBuiltLayers[n].xLoad;
		sLayer.xExec = aBuiltLayers[n].xExec;
		sLayer.xRelease = aBuiltLayers[n].xRelease;
		sLayer.pLayerData = 0;
		/* Register this one */
		SySetPut(&pTrainer->aBuiltin, (const void *)&sLayer);
	}
}
/*
* Find a layer from the set of built-in one.
*/
static sod_layer * sod_find_builtin_layer(sod_realnet_trainer *pTrainer, SyString *pConf)
{
	sod_layer *aLayers = SySetBasePtr(&pTrainer->aBuiltin);
	size_t n;
	for (n = 0; n < SySetUsed(&pTrainer->aBuiltin); ++n) {
		sod_layer *pLayer = &aLayers[n];
		SyString *pName = &pLayer->sName;
		if (pConf->nByte == pName->nByte && sy_strnicmp(pConf->zString, pName->zString, pConf->nByte) == 0) {
			/* found layer */
			return pLayer;
		}
	}
	/* No such layer */
	return 0;
}
/*
* Release the training samples from a given set.
*/
static void sod_realnet_release_samples(SySet *pSet)
{
	sod_img *aSample = (sod_img *)SySetBasePtr(pSet);
	size_t n;
	for (n = 0; n < SySetUsed(pSet); n++) {
		sod_free_image(aSample[n]);
	}
	SySetRelease(&(*pSet));
}
/*
* @Trainer Public Interface
*/
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_realnet_train_init(sod_realnet_trainer ** ppOut)
{
	sod_realnet_trainer *pTrainer = malloc(sizeof(sod_realnet_trainer));
	*ppOut = pTrainer;
	if (pTrainer == 0) {
		return SOD_OUTOFMEM;
	}
	/* Zero the structure */
	memset(pTrainer, 0, sizeof(sod_realnet_trainer));
	/* Init */
	SySetInit(&pTrainer->aBuiltin, sizeof(sod_layer));
	SySetInit(&pTrainer->aSample, sizeof(sod_tr_sample));
	SyBlobInit(&pTrainer->sWorker);
	SySetInit(&pTrainer->aPaths, sizeof(sod_paths));
	/* Training samples */
	SySetInit(&pTrainer->aPos, sizeof(sod_img));
	SySetInit(&pTrainer->aNeg, sizeof(sod_img));
	SySetInit(&pTrainer->aTest, sizeof(sod_img));
	pTrainer->pVfs = sodExportBuiltinVfs();
	/* Register the built-in layers */
	sod_realnet_trainer_register_builtin_layers(&(*pTrainer));
	return SOD_OK;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_realnet_train_config(sod_realnet_trainer * pTrainer, SOD_REALNET_TRAINER_CONFIG op, ...)
{
	int rc = SOD_OK;
	va_list ap;
	va_start(ap, op);
	switch (op) {
	case SOD_REALNET_TR_LOG_CALLBACK: {
		/* Trainer log callback */
		ProcLogCallback xLog = va_arg(ap, ProcLogCallback);
		void *pLogData = va_arg(ap, void *);
		/* Register the callback */
		pTrainer->xLog = xLog;
		pTrainer->pLogData = pLogData;
	}
	break;
	case SOD_REALNET_TR_OUTPUT_MODEL: {
		const char *zPath = va_arg(ap, const char *);
		pTrainer->zOutPath = zPath;
	}
	break;
	default:
		rc = SOD_UNSUPPORTED;
		break;
	}
	va_end(ap);
	return rc;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_realnet_train_start(sod_realnet_trainer *pTrainer, const char * zConf)
{
	sod_config_layer *pConf;
	void *pMap = 0;
	int nAbort = 0;
	size_t sz;
	int rc;
	static const char zBanner[] = {
		"============================================================\n"
		"RealNets Model Training                                     \n"
		"         Copyright (C) Symisc Systems. All rights reserved  \n"
		"============================================================\n"
	};
	/* Check if we are dealing with a memory buffer or with a file path */
	while (isspace(zConf[0])) zConf++;
	/* Assume a null terminated memory buffer */
	sz = strlen(zConf);
	if (zConf[0] != '[' || zConf[0] != '#' || zConf[0] != ';' || sz < 170) {
		/* Assume a file path, open read-only  */
		rc = pTrainer->pVfs->xMmap(zConf, &pMap, &sz);
		if (rc != SOD_OK) {
			sod_config_log_msg(&(*pTrainer), "Error while reading training configuration file: '%s'\n", zConf);
			return SOD_IOERR;
		}
		zConf = (const char *)pMap;
	}
	sod_config_log_msg(&(*pTrainer), zBanner);
	/* Parse the configuration */
	rc = sod_parse_config(&(*pTrainer), zConf, sz);
	if (rc != SOD_OK) {
		/* Something went wrong, log should tell you more */
		sod_config_log_msg(&(*pTrainer), "Parsing finished with errors..aborting\n");
		nAbort++;
	}
	else {
		sod_config_log_msg(&(*pTrainer), "Parsing done. Processing network layers now..\n");
		srand((unsigned int)pTrainer->pVfs->xTicks());
		pConf = pTrainer->pFirst;
		/* Process our config */
		while (pConf) {
			/* Get the target layer */
			sod_layer *pLayer = sod_find_builtin_layer(&(*pTrainer), &pConf->sName);
			if (pLayer == 0) {
				/* No such layer, discard */
				sod_config_log_msg(&(*pTrainer), "No built-in layer(s) were found for this configuration: '%z'..discarding\n", &pConf->sName);
			}
			else {
				sod_config_log_msg(&(*pTrainer), "Processing layer: '%z'..\n", &pConf->sName);
				/* Run the init callback */
				rc = pLayer->xLoad(&(*pTrainer), (sod_config_node *)SySetBasePtr(&pConf->aNode), (int)SySetUsed(&pConf->aNode), &pLayer->pLayerData);
				if (rc == SOD_OK && pLayer->xExec) {
					/* Run the exec callback */
					rc = pLayer->xExec(pLayer->pLayerData, (sod_paths *)SySetPeek(&pTrainer->aPaths)/* Peek the last path */);
				}
				if (rc != SOD_OK) {
					/* Callback requested an operation abort for invalid parameters...*/
					nAbort++;
					break;
				}
			}
			pConf = pConf->pPrev;
		}
	}
	/* Clean up */
	pConf = pTrainer->pLast;
	while (pTrainer->nLayers > 0 && pConf) {
		sod_config_layer *pNext = pConf->pNext;
		sod_layer *pLayer = sod_find_builtin_layer(&(*pTrainer), &pConf->sName);
		if (pLayer && pLayer->xRelease) {
			pLayer->xRelease(pLayer->pLayerData);
		}
		/* Release */
		SySetRelease(&pConf->aNode);
		free(pConf);
		pConf = pNext;
		pTrainer->nLayers--;
	}
	if (pMap) {
		/* Discard the memory view */
		pTrainer->pVfs->xUnmap(pMap, sz);
	}
	return nAbort > 0 ? SOD_ABORT /* Callback request an operation abort, check log */ : SOD_OK;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_realnet_train_release(sod_realnet_trainer * pTrainer)
{
	sod_paths *aPaths = (sod_paths *)SySetBasePtr(&pTrainer->aPaths);
	size_t i;

	SyBlobRelease(&pTrainer->sWorker);
	for (i = 0; i < SySetUsed(&pTrainer->aPaths); i++) {
		PathConfRelease(&aPaths[i]);
	}
	SySetRelease(&pTrainer->aPaths);
	SySetRelease(&pTrainer->aBuiltin);
	sod_realnet_release_samples(&pTrainer->aPos);
	sod_realnet_release_samples(&pTrainer->aNeg);
	sod_realnet_release_samples(&pTrainer->aTest);
	SySetRelease(&pTrainer->aSample);
	free(pTrainer);
#ifdef SOD_MEM_DEBUG
	_CrtDumpMemoryLeaks();
#endif /* #ifdSOD_MEM_DEBUG */
}
#endif /* SOD_ENABLE_NET_TRAIN */
struct sod_realnet
{
	SySet aModels;     /* Set of loaded models */
	SySet aBox;        /* Detection box */
};
typedef enum {
	SOD_REALNET_DETECTION = 1 /* An object detection network */
}SOD_REALNET_NET_TYPE;
typedef struct sod_realnet_model sod_realnet_model;
struct sod_realnet_model
{
	const char *zName; /* Model name if any */
	const char *zAbout;
	SOD_REALNET_NET_TYPE iType;
	/* Decision trees */
	int depth;
	int ntrees;
	int version;
	int bbox;
	void *pTrees;
	void *pMmap;
	size_t mapSz;
	size_t offset;
	/* Detection parameters */
	int minsize;
	int maxsize;
	float scalefactor;
	float stridefactor;
	float threshold;
	float nms;
};
/*
 * Parse a given binary cascade trained for detection tasks.
 */
static int RealnetParseDetectionCascade(const void *pRaw, size_t nSz, sod_realnet_model *pModel, int is_mmaped)
{
	int *cascade = (int *)pRaw;
	if (nSz < 100) return SOD_ABORT;
	pModel->version = cascade[0];
	if (pModel->version != 3 && pModel->version != 4) {
		/* Corrupt model */
		return SOD_ABORT;
	}
	pModel->bbox = cascade[1];
	if (pModel->version == 3) {
		pModel->depth = cascade[2];
	}
	pModel->ntrees = cascade[3];
	/* Calculate the offset */
	pModel->pTrees = (void *)&cascade[4];
	if (is_mmaped) {
		pModel->pMmap = (void *)pRaw;
		pModel->mapSz = nSz;
	}
	else {
		/* Already zeroed */
		pModel->pMmap = 0; 
	}
	pModel->offset = ((1 << pModel->depth) - 1) * sizeof(int) /* Node table */ + (1 << pModel->depth) * sizeof(float)/* Leaf table*/ + 1 * sizeof(float) /* Tree threshold */;
	if (nSz < (4 *sizeof(int) + pModel->ntrees * pModel->offset) ) {
		/* Corrupt model */
		return SOD_ABORT;
	}
	pModel->zName = "object";
	return SOD_OK;
}
/* 
 * Fill a Realnet model with default values.
 */
static void RealnetFillModel(sod_realnet_model *pModel)
{
	pModel->minsize = 128;
	pModel->maxsize = 1024;
	pModel->scalefactor = 1.1f;
	pModel->stridefactor = 0.1f;
	pModel->threshold = 5.0f;
	pModel->nms = 0.4f;
	pModel->iType = SOD_REALNET_DETECTION;
}
/*
 * Run a Realnet cascade for object detection tasks. 
 * Implementation based on the work on Nenad Markus pico project. License MIT.
 */
static int RealnetRunDetectionCascade(sod_realnet_model *pModel, int r, int c, int s, float *threshold, const unsigned char *zPixels, int w, int h)
{
	const char *zTree = (const char*)pModel->pTrees;
	float tree_thresh;
	int i;
	r = r * 256;
	c = c * 256;
	if ((r + 128 * s) / 256 >= h || (r - 128 * s) / 256 < 0 || (c + 128 * s) / 256 >= w || (c - 128 * s) / 256 < 0) {
		return -1;
	}
	tree_thresh = 0.0f;
	for (i = 0; i < pModel->ntrees; i++) {
		const char *zNodes = zTree - 4;
		float *aLeafs = (float *)(zTree + ((1 << pModel->depth) - 1) * sizeof(int));
		int idx = 1;
		int j;
		tree_thresh = *(float *)(zTree + ((1 << pModel->depth) - 1) * sizeof(int) + (1 << pModel->depth) * sizeof(float));
		for (j = 0; j < pModel->depth; ++j) {
			idx = 2 * idx + (zPixels[((r + zNodes[4 * idx + 0] * s) / 256) * w + (c + zNodes[4 * idx + 1] * s) / 256] <= zPixels[((r + zNodes[4 * idx + 2] * s) / 256) * w + (c + zNodes[4 * idx + 3] * s) / 256]);
		}
		*threshold = *threshold + aLeafs[idx - (1 << pModel->depth)];
		if (*threshold <= tree_thresh) {
			return -1;
		}
		zTree += pModel->offset;
	}
	*threshold = *threshold - tree_thresh;
	return +1;
}
/*
 * Non-Maximum Suppression (NMS) on sod_boxes.
 */
static float sodBoxOverlap(int x1, int w1, int x2, int w2)
{
	float l1 = (float)x1 - w1 / 2;
	float l2 = (float)x2 - w2 / 2;
	float left = l1 > l2 ? l1 : l2;
	float r1 = x1 + w1 / 2;
	float r2 = x2 + w2 / 2;
	float right = r1 < r2 ? r1 : r2;
	return right - left;
}
static float sodBoxInter(sod_box a, sod_box b)
{
	float w = sodBoxOverlap(a.x, a.w, b.x, b.w);
	float h = sodBoxOverlap(a.y, a.h, b.y, b.h);
	if (w < 0 || h < 0) return 0;
	float area = w * h;
	return area;
}
static float sodBoxUnion(sod_box a, sod_box b)
{
	float i = sodBoxInter(a, b);
	float u = a.w*a.h + b.w*b.h - i;
	return u;
}
static float sodBoxIou(sod_box a, sod_box b)
{
	return sodBoxInter(a, b) / sodBoxUnion(a, b);
}
static int sodBoxNmsCmp(const void *pa, const void *pb)
{
	sod_box *a = (sod_box *)pa;
	sod_box *b = (sod_box *)pb;
	float diff = a->score - b->score;
	if (diff < 0) return 1;
	else if (diff > 0) return -1;
	return 0;
}
static void sodBoxesdoNms(sod_box *aBoxes, size_t nCount, float thresh)
{
	size_t i, j;
	qsort(aBoxes, nCount, sizeof(sod_box), sodBoxNmsCmp);
	for (i = 0; i < nCount; ++i) {
		sod_box a = aBoxes[i];
		if (a.score == 0) continue;
		for (j = i + 1; j < nCount; ++j) {
			sod_box b = aBoxes[j];
			if (sodBoxIou(a, b) > thresh) {
				aBoxes[j].score = 0;
			}
		}
	}
}
static void sodNmsDiscardBoxes(SySet *pBox)
{
	sod_box *aBoxes = SySetBasePtr(pBox);
	size_t nNewCount = 0;
	sod_box *pPtr, *pEnd;
	pPtr = &aBoxes[0];
	pEnd = &aBoxes[SySetUsed(pBox)];
	while(pPtr < pEnd){
		if (pPtr->score != 0) {
			nNewCount++;
			pPtr++;
			continue;
		}
		memmove(pPtr, &pPtr[1], sizeof(sod_box)*(pEnd - pPtr));
		pEnd--;
	}
	pBox->nUsed = nNewCount;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_realnet_create(sod_realnet **ppOut)
{
	sod_realnet *pNet = malloc(sizeof(sod_realnet));
	*ppOut = pNet;
	if (pNet == 0) {
		return SOD_OUTOFMEM;
	}
	memset(pNet, 0, sizeof(sod_realnet));
	SySetInit(&pNet->aModels, sizeof(sod_realnet_model));
	SySetAlloc(&pNet->aModels, 8);
	SySetInit(&pNet->aBox, sizeof(sod_box));
	SySetAlloc(&pNet->aBox, 16);
	return SOD_OK;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_realnet_load_model_from_mem(sod_realnet *pNet, const void * pModel, unsigned int nBytes, sod_realnet_model_handle *pOutHandle)
{
	sod_realnet_model sModel;
	int rc;
	/* Parse the model */
	memset(&sModel, 0, sizeof(sod_realnet_model));
	rc = RealnetParseDetectionCascade(pModel, nBytes, &sModel, 0);
	if (rc != SOD_OK) {
		/* Corrupt model */
		return rc;
	}
	/* Fill with default values */
	RealnetFillModel(&sModel);
	/* Register that model */
	if (SOD_OK != SySetPut(&pNet->aModels, &sModel)) {
		return SOD_OUTOFMEM;
	}
	if (pOutHandle) {
		*pOutHandle = (sod_realnet_model_handle)(SySetUsed(&pNet->aModels) - 1);
	}
	return SOD_OK;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_realnet_load_model_from_disk(sod_realnet *pNet, const char * zPath, sod_realnet_model_handle *pOutHandle)
{
	const sod_vfs *pVfs = sodExportBuiltinVfs();
	sod_realnet_model sModel;
	void *pMap = 0;
	size_t sz;
	int rc;
	if (SOD_OK != pVfs->xMmap(zPath, &pMap, &sz)) {
		return SOD_IOERR;
	}
	/* Parse the model */
	memset(&sModel, 0, sizeof(sod_realnet_model));
	rc = RealnetParseDetectionCascade(pMap, sz, &sModel, 1);
	if (rc != SOD_OK) {
		/* Corrupt model */
		return rc;
	}
	/* Fill with default values */
	RealnetFillModel(&sModel);
	/* Register that model */
	if (SOD_OK != SySetPut(&pNet->aModels, &sModel)) {
		return SOD_OUTOFMEM;
	}
	if (pOutHandle) {
		*pOutHandle = (sod_realnet_model_handle)(SySetUsed(&pNet->aModels) - 1);
	}
	return SOD_OK;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_realnet_model_config(sod_realnet *pNet, sod_realnet_model_handle handle, SOD_REALNET_MODEL_CONFIG conf, ...)
{
	sod_realnet_model *pModel;
	int rc = SOD_OK;
	va_list ap;
	/* Fetch the target model */
	pModel = SySetFetch(&pNet->aBox, handle);
	va_start(ap, conf);
	if (pModel) {
		switch (conf) {
		case SOD_REALNET_MODEL_NMS: {
			double nms = va_arg(ap, double);
			pModel->nms = (float)nms;

		}
													break;
		case SOD_RELANET_MODEL_DETECTION_THRESHOLD: {
			double thresh = va_arg(ap, double);
			pModel->threshold = (float)thresh;

		}
													break;
		case SOD_REALNET_MODEL_MINSIZE: {
			int minsize = va_arg(ap, int);
			if (minsize >= 8) pModel->minsize = minsize;
		}
										break;
		case SOD_REALNET_MODEL_MAXSIZE: {
			int maxsize = va_arg(ap, int);
			if (maxsize >= 16) pModel->maxsize = maxsize;
		}
										break;
		case SOD_REALNET_MODEL_SCALEFACTOR: {
			double scalefactor = va_arg(ap, double);
			pModel->scalefactor = (float)scalefactor;
		}
											break;
		case SOD_REALNET_MODEL_STRIDEFACTOR: {
			double stridefactor = va_arg(ap, double);
			pModel->stridefactor = (float)stridefactor;
		}
											 break;
		case SOD_REALNET_MODEL_NAME: {
			const char *zName = va_arg(ap, const char *);
			pModel->zName = zName;
		}
									 break;
		case SOD_REALNET_MODEL_ABOUT_INFO: {
			const char *zAbout = va_arg(ap, const char *);
			pModel->zAbout = zAbout;
		}
									 break;

		default:
			rc = SOD_UNSUPPORTED;
			break;
		}
	}
	else {
		rc = SOD_UNSUPPORTED;
	}
	va_end(ap);
	return rc;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_realnet_detect(sod_realnet *pNet, const unsigned char *zGrayImg, int width, int height, sod_box **apBox, int *pnBox)
{
	sod_realnet_model *aModel = (sod_realnet_model *)SySetBasePtr(&pNet->aModels);
	size_t n;
	SySetReset(&pNet->aBox);
	/* Loaded models */
	for (n = 0; n < SySetUsed(&pNet->aModels); ++n) {
		sod_realnet_model *pMl = &aModel[n];
		size_t nCur = SySetUsed(&pNet->aBox);
		float s;
		/* Start detection */
		s = pMl->minsize;
		while (s <= pMl->maxsize) {
			float r, c, dr, dc;
			dc = dr = MAX(s*pMl->stridefactor, 1.0f);
			for (r = s / 2 + 1; r <= height - s / 2 - 1; r += dr) {
				for (c = s / 2 + 1; c <= width - s / 2 - 1; c += dc) {
					float thresh = 0.0f; /* cc warning */
					if (1 == RealnetRunDetectionCascade(pMl, r, c, s, &thresh, zGrayImg, width, height) && thresh >= pMl->threshold) {
						sod_box bbox;
						bbox.score = thresh;
						bbox.zName = pMl->zName;
						bbox.pUserData = 0;
						bbox.x = MAX((int)(c - 0.5*s), 0);
						bbox.y = MAX((int)(r - 0.5*s), 0);
						bbox.w = MIN((int)(c + 0.5*s), width)  - bbox.x;
						bbox.h = MIN((int)(r + 0.5*s), height) - bbox.y;
						SySetPut(&pNet->aBox, &bbox);
					}
				}
			}
			s = s * pMl->scalefactor;
		}
		if (pMl->nms) {
			/* Non-Maximum Suppression */
			sodBoxesdoNms((sod_box *)SySetBasePtrJump(&pNet->aBox, nCur), SySetUsed(&pNet->aBox) - nCur, pMl->nms);
			sodNmsDiscardBoxes(&pNet->aBox);
		}
	}
	if (pnBox) {
		*pnBox = (int)SySetUsed(&pNet->aBox);
	}
	if (apBox) {
		*apBox = SySetBasePtr(&pNet->aBox);
	}
	return SOD_OK;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_realnet_destroy(sod_realnet * pNet)
{
	sod_realnet_model *aModel = (sod_realnet_model *)SySetBasePtr(&pNet->aModels);
	const sod_vfs *pVfs = sodExportBuiltinVfs();
	size_t n;
	/* Release the memory view if any for each loaded model */
	for (n = 0; n < SySetUsed(&pNet->aModels); ++n) {
		sod_realnet_model *pMl = &aModel[n];
		/* For models */
		if (pMl->pMmap) {
			pVfs->xUnmap(pMl->pMmap, pMl->mapSz);
		}
	}
	SySetRelease(&pNet->aBox);
	SySetRelease(&pNet->aModels);
	free(pNet);
#ifdef SOD_MEM_DEBUG
	_CrtDumpMemoryLeaks();
#endif /* #ifdSOD_MEM_DEBUG */
}
#ifndef SOD_DISABLE_IMG_READER
#ifdef _MSC_VER
/* Disable the nonstandard extension used: non-constant aggregate initializer warning */
#pragma warning(disable:4204)
#endif /* _MSC_VER */
#define STB_IMAGE_IMPLEMENTATION
#include "sod_img_reader.h"
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_img_load_from_mem(const unsigned char * zBuf, int buf_len, int nChannels)
{
	int w, h, c;
	int i, j, k;
	unsigned char *data = stbi_load_from_memory(zBuf, buf_len, &w, &h, &c, nChannels);
	if (!data) {
		return sod_make_empty_image(0, 0, 0);
	}
	if (nChannels) c = nChannels;
	sod_img im = sod_make_image(w, h, c);
	if (im.data) {
		for (k = 0; k < c; ++k) {
			for (j = 0; j < h; ++j) {
				for (i = 0; i < w; ++i) {
					int dst_index = i + w * j + w * h*k;
					int src_index = k + c * i + c * w*j;
					im.data[dst_index] = (float)data[src_index] / 255.;
				}
			}
		}
	}
	free(data);
	return im;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_img_load_from_file(const char *zFile, int nChannels)
{
	const sod_vfs *pVfs = sodExportBuiltinVfs();
	unsigned char *data;
	void *pMap = 0;
	size_t sz = 0; /* gcc warn */
	int w, h, c;
	int i, j, k;
	if (SOD_OK != pVfs->xMmap(zFile, &pMap, &sz)) {
		data = stbi_load(zFile, &w, &h, &c, nChannels);
	}
	else {
		data = stbi_load_from_memory((const unsigned char *)pMap, (int)sz, &w, &h, &c, nChannels);
	}
	if (!data) {
		return sod_make_empty_image(0, 0, 0);
	}
	if (nChannels) c = nChannels;
	sod_img im = sod_make_image(w, h, c);
	if (im.data) {
		for (k = 0; k < c; ++k) {
			for (j = 0; j < h; ++j) {
				for (i = 0; i < w; ++i) {
					int dst_index = i + w * j + w * h*k;
					int src_index = k + c * i + c * w*j;
					im.data[dst_index] = (float)data[src_index] / 255.;
				}
			}
		}
	}
	free(data);
	if (pMap) {
		pVfs->xUnmap(pMap, sz);
	}
	return im;
}
/*
* Extract path fields.
*/
static int ExtractPathInfo(const char *zPath, size_t nByte, sod_path_info *pOut)
{
	const char *zPtr, *zEnd = &zPath[nByte - 1];
	SyString *pCur;
	int c, d;
	c = d = '/';
#ifdef __WINNT__
	d = '\\';
#endif
	/* Zero the structure */
	memset(pOut, 0, sizeof(sod_path_info));
	/* Handle special case */
	if (nByte == sizeof(char) && ((int)zPath[0] == c || (int)zPath[0] == d)) {
#ifdef __WINNT__
		SyStringInitFromBuf(&pOut->sDir, "\\", sizeof(char));
#else
		SyStringInitFromBuf(&pOut->sDir, "/", sizeof(char));
#endif
		return SOD_OK;
	}
	/* Extract the basename */
	while (zEnd > zPath && ((int)zEnd[0] != c && (int)zEnd[0] != d)) {
		zEnd--;
	}
	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;
	zEnd = &zPath[nByte];
	/* dirname */
	pCur = &pOut->sDir;
	SyStringInitFromBuf(pCur, zPath, zPtr - zPath);
	if (pCur->nByte > 1) {
		SyStringTrimTrailingChar(pCur, '/');
#ifdef __WINNT__
		SyStringTrimTrailingChar(pCur, '\\');
#endif
	}
	else if ((int)zPath[0] == c || (int)zPath[0] == d) {
#ifdef __WINNT__
		SyStringInitFromBuf(&pOut->sDir, "\\", sizeof(char));
#else
		SyStringInitFromBuf(&pOut->sDir, "/", sizeof(char));
#endif
	}
	/* basename/filename */
	pCur = &pOut->sBasename;
	SyStringInitFromBuf(pCur, zPtr, zEnd - zPtr);
	SyStringTrimLeadingChar(pCur, '/');
#ifdef __WINNT__
	SyStringTrimLeadingChar(pCur, '\\');
#endif
	SyStringDupPtr(&pOut->sFilename, pCur);
	if (pCur->nByte > 0) {
		/* extension */
		zEnd--;
		while (zEnd > pCur->zString /*basename*/ && zEnd[0] != '.') {
			zEnd--;
		}
		if (zEnd > pCur->zString) {
			zEnd++; /* Jump leading dot */
			SyStringInitFromBuf(&pOut->sExtension, zEnd, &zPath[nByte] - zEnd);
			/* Fix filename */
			pCur = &pOut->sFilename;
			if (pCur->nByte > SyStringLength(&pOut->sExtension)) {
				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);
			}
		}
	}
	return SOD_OK;
}
/*
* Cross platform srtnicmp
*/
static int sy_strnicmp(const char *zA, const char *zB, size_t len)
{
	for (;;) {
		if (len < 1) break;
		int c = tolower(zA[0]);
		int d = tolower(zB[0]);
		int e = c - d;
		if (e != 0) return e;
		if (c == 0) break;
		zA++;
		zB++;
		len--;
	}
	return 0; /* Equal string */
}
/*
* Compare two strings. One is nil-terminated, the other may be not.
*/
static int CmpSyString(SyString *pStr, const char *zIn)
{
	SyString sStr;
	SyStringInitFromBuf(&sStr, zIn, strlen(zIn));
	return SyStringCmp(&sStr, pStr, sy_strnicmp);
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_img_set_load_from_directory(const char * zPath, sod_img ** apLoaded, int * pnLoaded, int max_entries)
{
	static const char *zAllowed[] = { "png","jpg","jpeg","bmp","pgm","ppm","pbm","hdr","psd","tga","pic", 0 /*Marker*/ };
	const sod_vfs *pVfs = sodExportBuiltinVfs();
	sod_path_info sPath;
	SySet aEntries;
	SyBlob sReader;
	void *pHandle;
	sod_img img;
	int i, rc;
	/* Open the target directory */
	rc = pVfs->xOpenDir(zPath, &pHandle);
	if (rc != SOD_OK) {
		*apLoaded = 0;
		*pnLoaded = 0;
		return rc;
	}
	SySetInit(&aEntries, sizeof(sod_img));
	SyBlobInit(&sReader);

	/* Iterate over the target directory */
	pVfs->xChdir(zPath);
	while (pVfs->xDirRead(pHandle, &sReader) == SOD_OK) {
		const char *zEntry = (const char *)SyBlobData(&sReader);
		size_t nByte = SyBlobLength(&sReader);
		/* Reset the blob */
		SyBlobReset(&sReader);
		if (pVfs->xIsdir(zEntry)) {
			/* Entry is a directory, ignore */
			continue;
		}
		/* Make sure the file is of the correct extension */
		ExtractPathInfo(zEntry, nByte, &sPath);
		rc = 0;
		for (i = 0; zAllowed[i] != 0; i++) {
			if (CmpSyString(&sPath.sExtension, zAllowed[i]) == 0) {
				rc = 1;
				break;
			}
		}
		if (!rc) {
			/* Not of the expected extension */
			continue;
		}
		if (max_entries > 0 && (int)SySetUsed(&aEntries) >= max_entries) {
			/* Maximum samples to collect limit reached */
			break;
		}
		/* Load the image */
		img = sod_img_load_from_file(zEntry, SOD_IMG_COLOR);
		if (img.data == 0) {
			/* Cannot load image, ignore */
			continue;
		}
		/* Insert in the target set */
		rc = SySetPut(&aEntries, (const void *)&img);
		if (rc != SOD_OK) {
			/* Avoid memory leaks */
			sod_free_image(img);
		}
	}
	/* Cleanup */
	pVfs->xCloseDir(pHandle);
	SyBlobRelease(&sReader);
	/* Total loaded images from this directory */
	*apLoaded = (sod_img *)SySetBasePtr(&aEntries);
	*pnLoaded = (int)SySetUsed(&aEntries);
	return SOD_OK;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_img_set_release(sod_img *aLoaded, int nEntries)
{
	int i;
	for (i = 0; i < nEntries; i++) {
		sod_free_image(aLoaded[i]);
	}
	free(aLoaded);
}
#ifndef SOD_DISABLE_IMG_WRITER
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include"sod_img_writer.h"
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_img_save_as_png(sod_img input, const char * zPath)
{
	unsigned char *zPng = sod_image_to_blob(input);
	int rc;
	if (zPng == 0) {
		return SOD_OUTOFMEM;
	}
	rc = stbi_write_png(zPath, input.w, input.h, input.c, (const void *)zPng, input.w * input.c);
	sod_image_free_blob(zPng);
	return rc ? SOD_OK : SOD_IOERR;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_img_save_as_jpeg(sod_img input, const char *zPath, int Quality)
{
	unsigned char *zJpeg = sod_image_to_blob(input);
	int rc;
	if (zJpeg == 0) {
		return SOD_OUTOFMEM;
	}
	rc = stbi_write_jpg(zPath, input.w, input.h, input.c, (const void *)zJpeg, Quality < 0 ? 100 : Quality);
	sod_image_free_blob(zJpeg);
	return rc ? SOD_OK : SOD_IOERR;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_img_blob_save_as_png(const char * zPath, const unsigned char *zBlob, int width, int height, int nChannels)
{
	int rc;
	rc = stbi_write_png(zPath, width, height, nChannels, (const void *)zBlob, width * nChannels);
	return rc ? SOD_OK : SOD_IOERR;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_img_blob_save_as_jpeg(const char * zPath, const unsigned char *zBlob, int width, int height, int nChannels, int Quality)
{
	int rc;
	rc = stbi_write_jpg(zPath, width, height, nChannels, (const void *)zBlob, Quality < 0 ? 100 : Quality);
	return rc ? SOD_OK : SOD_IOERR;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_img_blob_save_as_bmp(const char * zPath, const unsigned char *zBlob, int width, int height, int nChannels)
{
	int rc;
	rc = stbi_write_bmp(zPath, width, height, nChannels, (const void *)zBlob);
	return rc ? SOD_OK : SOD_IOERR;
}
#endif /* SOD_DISABLE_IMG_WRITER  */
#endif /* SOD_DISABLE_IMG_READER */
#ifdef SOD_ENABLE_OPENCV
/*
* OpenCV integration with the SOD library.
*/
static void ipl_into_image(IplImage* src, sod_img im)
{
	unsigned char *data = (unsigned char *)src->imageData;
	int h = src->height;
	int w = src->width;
	int c = src->nChannels;
	int step = src->widthStep;
	int i, j, k;

	for (i = 0; i < h; ++i) {
		for (k = 0; k < c; ++k) {
			for (j = 0; j < w; ++j) {
				im.data[k*w*h + i * w + j] = data[i*step + j * c + k] / 255.;
			}
		}
	}
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_img_load_cv_ipl(IplImage* src)
{
	int h = src->height;
	int w = src->width;
	int c = src->nChannels;
	sod_img out = sod_make_image(w, h, c);
	ipl_into_image(src, out);
	return out;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_img_load_from_cv(const char *filename, int channels)
{
	IplImage* src = 0;
	int flag = -1;
	if (channels == 0) flag = -1;
	else if (channels == 1) flag = 0;
	else if (channels == 3) flag = 1;
	else {
		return sod_make_empty_image(0, 0, 0);
	}

	if ((src = cvLoadImage(filename, flag)) == 0)
	{
		return sod_make_empty_image(0, 0, 0);
	}
	sod_img out = sod_img_load_cv_ipl(src);
	cvReleaseImage(&src);
	sod_img_rgb_to_bgr(out);
	return out;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
sod_img sod_img_load_from_cv_stream(CvCapture *cap)
{
	IplImage* src = cvQueryFrame(cap);
	if (!src) return sod_make_empty_image(0, 0, 0);
	sod_img im = sod_img_load_cv_ipl(src);
	sod_img_rgb_to_bgr(im);
	return im;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
int sod_img_fill_from_cv_stream(CvCapture *cap, sod_img *pImg)
{
	IplImage* src = cvQueryFrame(cap);
	if (!src) {
		return -1;
	}
	/* Make sure we have enough space to hold this chunk */
	sod_md_alloc_dyn_img(&(*pImg), src->width, src->height, src->nChannels);
	if (pImg->data == 0) {
		return SOD_OUTOFMEM;
	}
	ipl_into_image(src, *pImg);
	sod_img_rgb_to_bgr(*pImg); /* noop if grayscale IPL which is required for the real-time object detector */
	return SOD_OK;
}
/*
* CAPIREF: Refer to the official documentation at https://sod.pixlab.io/api.html for the expected parameters this interface takes.
*/
void sod_img_save_to_cv_jpg(sod_img im, const char *zPath)
{
	sod_img copy = sod_copy_image(im);
	if (im.c == 3) sod_img_rgb_to_bgr(copy);
	int x, y, k;

	IplImage *disp = cvCreateImage(cvSize(im.w, im.h), IPL_DEPTH_8U, im.c);
	int step = disp->widthStep;
	for (y = 0; y < im.h; ++y) {
		for (x = 0; x < im.w; ++x) {
			for (k = 0; k < im.c; ++k) {
				disp->imageData[y*step + x * im.c + k] = (unsigned char)(get_pixel(copy, x, y, k) * 255);
			}
		}
	}
	cvSaveImage(zPath, disp, 0);
	cvReleaseImage(&disp);
	sod_free_image(copy);
}
#endif /* SOD_ENABLE_OPENCV */
/*
 * SOD Embedded Release Information & Copyright Notice.
 */
const char * sod_lib_copyright(void)
{
	return SOD_LIB_INFO;
}
