/*\
 * DateFile.cpp
 *
 * Copyright (c) 2014-2020 - Geoff R. McLane
 * Licence: GNU GPL version 2
 *
\*/
#ifndef  _DateFile_HHH_
#define  _DateFile_HHH_

#ifdef   UNICODE
#undef   UNICODE
#endif   // UNICODE
#ifdef   _UNICODE
#undef   _UNICODE
#endif   // _UNICODE

#ifndef  _CRT_SECURE_NO_DEPRECATE
#define  _CRT_SECURE_NO_DEPRECATE
#endif // #ifndef  _CRT_SECURE_NO_DEPRECATE

#define  PGM_DATE    "25 June, 2020"    // Due to a MSVC6 2019 compiler update
// PGM_DATE    "13 May, 2020"
// PGM_DATE    "14 January, 2008"
// PGM_DATE    "30 March, 2007"

#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <list>
#include <vector>
#include <map>

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#endif // #ifndef  _DateFile_HHH_
// eof - DateFile.h

