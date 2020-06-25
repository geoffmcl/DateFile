/*\
 * DateFile.cpp
 *
 * Copyright (c) 2014-2020 - Geoff R. McLane
 * Licence: GNU GPL version 2
 *
\*/

#include "DateFile.h"
#include <windows.h>
#include <direct.h>
//#include <iostream>   // for cin, cout
#include <conio.h>   // for _getch()

#undef ADD_FOLDER
#undef DO_NT_TEST

#ifdef DO_NT_TEST
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#include <Winternl.h>
#endif


#define  VFH(a)   ( a && ( a != INVALID_HANDLE_VALUE ) )
#define  EndBuf(a)   ( a + strlen(a) )
#define  ISNUM(a) (( a >= '0' )&&( a <= '9' ))

using namespace std;


typedef struct tagFILES {
    string          given_path;  // as given
    string          path; // path of file
    WIN32_FIND_DATA fd;   // WIN32 information
} FILES, * PFILES;

typedef struct tagFULLPATH {
   TCHAR path[_MAX_PATH];
   TCHAR drive[_MAX_DRIVE];
   TCHAR dir[_MAX_DIR];
   TCHAR fname[_MAX_FNAME];
   TCHAR ext[_MAX_EXT];
} FULLPATH, * PFULLPATH;

// forward reference
void Add_Dir_Files( char * cp );
int Got_End_Path_Sep( PTSTR path );
void Add_2_List( PTSTR path, WIN32_FIND_DATA * pfd );

typedef vector<string> string_list;
typedef string_list::iterator siter;

typedef vector<PFILES> file_list;
typedef file_list::iterator fiter;

#define  CHKMEM(a)   if( !a ) { printf( "CRITICAL ERROR: MEMORY FAILED!\n" ); pgm_exit(2); }

int bDoUpdate = 0;
string_list files;
file_list   sfiles;     // final FILE (or FOLDER) list
//file_list   directs;    // directories to process -f<dir>
FULLPATH fullpath;

int   verbosity = 1;
#define  VERB  (verbosity > 0)
#define  VERB2  (verbosity > 1)
#define  VERB5  (verbosity > 4)
#define  VERB9  (verbosity > 8)

char * Reference = 0;
SYSTEMTIME  Target = { 0 };
FILETIME FileTime = { 0 };
int   bGotTime = 0;
int   bGotDate = 0;
int   bGotRef = 0;
int   bUseSystem = 0;   // use system time, not local time

char ErrMessage[1024];

void  pgm_exit( int ret )
{
   for( fiter fit = sfiles.begin(); fit != sfiles.end(); fit++ ) {
      PFILES pf = *fit;
      delete pf;
   }

   files.clear();
   sfiles.clear();
   exit(ret);
}

void  print_help( void )
{
   printf( "Informatique Rouge - SET FILE DATE/TIME UTILITY - " PGM_DATE "\n" );
   printf( "Usage: DateFile [Options] FileSpec [filespec...]\n" );
   printf( "Options: Each preceeded by '-' or '/', and space delimited\n" );
   printf( " -?           This brief help, and exit.\n" );
#ifdef ADD_FOLDER
   printf( " -fDirectory  Set Date/Time in this DIRECTORY.\n");
#endif
   printf( " -ddd:mm:yyyy Date to use for update.\n" );
   printf( " -thh:mm:ss   Time to use for update. Use only 24 hour time.\n" );
   printf( " -rRefFile    Reference file, used for update date and time.\n" );
   printf( " -s           Use system time for update date and time.\n" );
   printf( " -u           Update chosen files (Def=Off)\n" );
   printf( " -v[n]        Verbosity. Range 0-9. (Def=1)\n" );
   printf( "Multiple file specifications can be given, including wild cards, '*' or '?'\n" );
   printf( "If the file spec is a DIRECTORY, all files in that directory will be processed.\n");

}

void  give_help( void )
{
   print_help();
   pgm_exit(1);
}

void  no_input( void )
{
   printf( "ERROR: Enter a file, or file mask ...\n" );
   give_help();
}

void  err_cmd( char * cmd )
{
   printf( "ERROR: %s is NOT a valid command!\n", cmd );
   give_help();
}

typedef void (*FPUSHBACK)( string );

void push_back_file( string s )
{
   files.push_back(s);
}

FPUSHBACK fpb = &push_back_file;

char * strdupe( char * file )
{
   size_t len = strlen(file) + 1;
   char * cp = new char[len];
   CHKMEM(cp);
   strcpy(cp, file);
   return cp;
}

DWORD GetLastErrorMsg( PTSTR lpm, DWORD dwLen, DWORD dwErr )
{
   PVOID lpMsgBuf = 0;
   DWORD    dwr;

   dwr = FormatMessage( 
      FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dwErr,   //	GetLastError(),
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
      (LPTSTR) &lpMsgBuf,
		0,
		NULL );
   
   //dwr = strlen(lpMsgBuf);
   if( ( dwr == 0 ) || ( dwr >= dwLen ) )
      dwr = (DWORD)-1;
   else
      strcat(lpm, (const char *)lpMsgBuf);

   //	printf("%s:%s\n",lpm,(LPCTSTR)lpMsgBuf);
   // Free the buffer.
   if( lpMsgBuf )
      LocalFree( lpMsgBuf );

   return dwr;
}

PTSTR   GetI64Stg( PULARGE_INTEGER pli )
{
   static TCHAR _s_I64Stg[288];
   TCHAR    buf[64];
   int      i,j,k;
   ULARGE_INTEGER  li;
   PTSTR   lps = _s_I64Stg;
   li = *pli;
   i = sprintf(buf, "%I64u", li.QuadPart );
   *lps = 0;   // clear any previous
   if( i )  // get its length
   {
      k = 64;
      j = 0;
      lps[k+1] = 0;  // esure ZERO termination
      while( ( i > 0 ) && ( k >= 0 ) )
      {
         i--;     // back up one
         if( j == 3 )   // have we had a set of 3?
         {
            lps[k--] = ',';   // ok, add a comma
            j = 0;            // and restart count of digits
         }
         lps[k--] = buf[i];   // move the buffer digit
         j++;
      }
      k++;  // back to LAST position
      lps = &lps[k]; // pointer to beginning of 'nice" number
   }
   return lps;
}

void  Add_SysTime_Str( PTSTR ps, SYSTEMTIME * pst )
{
   sprintf(EndBuf(ps), "%02d/%02d/%04d %02d:%02d:%02d.%03d",
      pst->wDay, pst->wMonth, pst->wYear, pst->wHour, pst->wMinute,
      pst->wSecond, pst->wMilliseconds );
}

PTSTR  Get_SysTime_Stg( SYSTEMTIME * pst )
{
   static TCHAR _s_syst_buf[32];
   PTSTR ps = _s_syst_buf;
   *ps = 0;
   Add_SysTime_Str( ps, pst );
   return ps;
}

#define  MMINFILNM   32
#define  MMINSIZE    14

PTSTR Get_FD_Stg( WIN32_FIND_DATA * pf )
{
   static char _s_fd_buf[288*2];
   PTSTR cp = _s_fd_buf;
   ULARGE_INTEGER ul;
   SYSTEMTIME  st;

   strcpy(cp, pf->cFileName);
   strcat(cp, " ");  // add at least one space
   while( strlen(cp) < MMINFILNM )
      strcat(cp, " ");

   if( FileTimeToSystemTime( &pf->ftLastWriteTime, &st ) ) {
      Add_SysTime_Str( cp, &st );
   } else {
      strcat(cp, "??/??/???? ??:??:??.????");
   }
   strcat(cp, " "); // add space
   PTSTR ps;
#ifdef ADD_FOLDER
   if (pf->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
       ps = "<DIR>";
   } else {
       ul.HighPart = pf->nFileSizeHigh;
       ul.LowPart  = pf->nFileSizeLow;
       ps = GetI64Stg( &ul );
   }
#else
   ul.HighPart = pf->nFileSizeHigh;
   ul.LowPart  = pf->nFileSizeLow;
   ps = GetI64Stg( &ul );
#endif
   size_t len = strlen(ps);
   while( len < MMINSIZE ) {
      strcat(cp, " ");
      len++;
   }
   strcat(cp, ps);
   return cp;
}

PTSTR Get_PFILES_Stg( PFILES pf )
{
   static char _s_pfiles_buf[288*2];
   PTSTR cp = _s_pfiles_buf;
   strcpy(cp, Get_FD_Stg( &pf->fd ) );
   strcat(cp, " ");
   strcat(cp, pf->path.c_str());
   return cp;
}

// hh:mm[:sec]
int   get_TIME( char * cpin )
{
   char * cp = strdupe( cpin );
   char * p1 = strchr( cp, ':' );
   int   hr, min, sec;
   sec = 0;
   min = 0;
   if( p1 ) {
      *p1 = 0;
      p1++;
      sec = 0;
      char * p2 = strchr( p1, ':' );
      if( p2 ) {
         *p2 = 0;
         p2++;
         sec = atoi(p2);
      }
      min = atoi(p1);
   }
   hr = atoi(cp);
   bGotTime = 0;
   if(( hr >= 0 ) && ( hr < 24 ) && ( min >= 0 ) && ( min < 60 ) &&
      ( sec >= 0 ) && ( sec < 60 )) {
      Target.wSecond = sec;
      Target.wHour   = hr;
      Target.wMinute = min;
      bGotTime = 1;
   }
   delete cp;
   return bGotTime;
}

int   get_DATE( char * cpin )
{
   char * cp = strdupe( cpin );
   int   sep = ':';
   char * p1, * p2;
   int   day, mth, yr;

   day = mth = yr = 0;
   bGotDate = 0;
   p1 = strchr( cp, sep );
   if( !p1 ) {
      sep = '/';
      p1 = strchr(cp, sep);
   }
   if(p1) {
      *p1 = 0;
      p1++;
      p2 = strchr(p1, sep);
      if(p2) {
         *p2 = 0;
         p2++;
         day = atoi(cp);
         mth = atoi(p1);
         yr  = atoi(p2);
         if(( day >= 1 ) && ( day < 32 ) && ( mth >= 1 ) && ( mth <= 12 ) &&
            ( yr >= 1 ) ) {
            bGotDate = 1;
            Target.wDay = day;
            Target.wMonth = mth;
            Target.wYear = yr;
         }
      }
   }

   delete cp;
   return bGotDate;
}

int   Get_REF( char * file )
{
   WIN32_FIND_DATA   fd;
   SYSTEMTIME  st;
   HANDLE hFind = FindFirstFile( file, &fd );
   if( VFH(hFind) ) {
      FindClose(hFind);
      if( FileTimeToSystemTime( &fd.ftLastWriteTime, &st ) ) {
         memcpy( &Target, &st, sizeof(SYSTEMTIME) );
         bGotRef = 1;
         return 1;
      }
   }
   return 0;
}

bool Is_Wild( const char * cp )
{
   size_t len = strlen(cp);
   size_t i;
   for( i = 0; i < len; i++ )
   {
      int c = cp[i];
      if(( c == '?' )||( c == '*' ))
         return true;

   }
   return false;
}

void Add_Wild_Files( PFULLPATH pfp )
{
   WIN32_FIND_DATA   fd;
   HANDLE hfind = FindFirstFile( pfp->path, &fd );
   if(VERB9)
      printf("Add_Wild_Files: for [%s] %s\n", pfp->path,
      ( hfind && (hfind != INVALID_HANDLE_VALUE) ) ? "ok" : "No FIND!" );

   _splitpath( pfp->path, pfp->drive, pfp->dir, pfp->fname, pfp->ext );
   if( hfind && (hfind != INVALID_HANDLE_VALUE) ) {
      do {
         if( !( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) ) {
            strcpy( pfp->path, pfp->drive );
            strcat( pfp->path, pfp->dir );
            strcat( pfp->path, fd.cFileName );
            string s = pfp->path;
            if(VERB9)
               printf("Add_Wild_Files: Adding file [%s]\n", s.c_str());
            //files.push_back(s);
            fpb(s);

         }
      } while( FindNextFile( hfind, &fd ) );
      FindClose(hfind);
   }
}

void Process_Directories( PFULLPATH pfp )
{
   WIN32_FIND_DATA   fd;
   HANDLE hfind = FindFirstFile( pfp->path, &fd );
   if(VERB9)
      printf("Process_Directories: for [%s] %s\n", pfp->path,
      ( hfind && (hfind != INVALID_HANDLE_VALUE) ) ? "ok" : "Not FOUND!" );

   _splitpath( pfp->path, pfp->drive, pfp->dir, pfp->fname, pfp->ext );
   if( hfind && (hfind != INVALID_HANDLE_VALUE) ) {
      do {
         if( (strcmp( fd.cFileName, "." ) == 0) || (strcmp( fd.cFileName, ".." ) == 0) ) {
            // skip these
         } else if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
            PFULLPATH pfp2 = new FULLPATH;
            CHKMEM(pfp2);
            strcpy( pfp2->path, pfp->drive );
            strcat( pfp2->path, pfp->dir );
            strcat( pfp2->path, fd.cFileName );
            Add_Dir_Files( pfp2->path );
         }
      } while( FindNextFile( hfind, &fd ) );
      FindClose(hfind);
   }
}

void Add_Dir_Files( char * cp )
{
   PFULLPATH pfp = new FULLPATH;
   CHKMEM(pfp);
   strcpy( pfp->path, cp );
   _fullpath( pfp->path, cp, _MAX_PATH );
   size_t len = strlen(pfp->path);
   if(VERB9)
      printf( "Add_Dir_Files: for %s ...\n", cp );

   if(len) {
      if( !Got_End_Path_Sep( pfp->path ) )
         strcat(pfp->path, "\\");
      strcat(pfp->path,"*.*");
      Add_Wild_Files( pfp );  // add any FILES, from this DIRECTORY

      // Now process DIRECTORIES, in this DIRECTORY
      strcpy( pfp->path, cp );
      _fullpath( pfp->path, cp, _MAX_PATH );
      if( !Got_End_Path_Sep( pfp->path ) )
         strcat(pfp->path, "\\");
      strcat(pfp->path,"*.*");
      Process_Directories( pfp );

   } else {
      printf( "ERROR: Internal error - got string of zero length from %s!!!...\n", cp );
      pgm_exit(1);
   }
   delete pfp;
}

// process a BARE item on the command line
// could be a file, a directory, or a wild card
void get_files(char * cp)
{
    string s = cp;
    PFULLPATH pfp = &fullpath;
    if( _fullpath( pfp->path, cp, _MAX_PATH ) ) {
        s = pfp->path;
    } else {
        strcpy( pfp->path, cp );
    }
    if(VERB9)
        printf( "get_files: using %s...\n", cp );

    if( Is_Wild( s.c_str() ) ) {
        // deal with a WILD
        Add_Wild_Files( pfp );
    } else {
        struct stat buf;
        if( stat( cp, &buf ) == 0 ) {
            if( buf.st_mode & _S_IFDIR ) {
                if(VERB9)
                    printf( "get_files: Adding directory %s...\n", cp );
                Add_Dir_Files(cp);
            } else {
                //files.push_back(s);  // add to string set
                fpb(s);
            }
        } else {
            printf( "ERROR: Invalid file/directory name %s! ...\n", cp );
            pgm_exit(1);
        }
    }
}

#ifdef ADD_FOLDER
int add_directory( char * cp )
{
    string s = cp;
    PFULLPATH pfp = &fullpath;
    if( _fullpath( pfp->path, cp, _MAX_PATH ) ) {
        s = pfp->path;
    } else {
        strcpy( pfp->path, cp );
    }
    WIN32_FIND_DATA   fd;
    PTSTR ps = (PTSTR)s.c_str();
    HANDLE hFind = FindFirstFile( ps, &fd ); 
    if( !VFH(hFind) || !( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )) {
        printf( "ERROR: %s is NOT a valid directory ...\n", ps );
        if (VFH(hFind))
            FindClose( hFind );
        pgm_exit(1);
    }
    FindClose( hFind );
    Add_2_List( ps, &fd );
    return 1;   // OK
}
#endif // ADD_FOLDER

int is_all_nums(char * cp)
{
    size_t len = strlen(cp);
    size_t i;
    for (i = 0; i < len; i++) {
        if (!ISNUM(cp[i]))
            return 0;   // NOT a number
    }
    return 1;
}


void Process_Verbosity( int argc, char * argv[] )
{
    for( int i = 1; i < argc; i++ ) {
        char * cp = argv[i];
        int   c = *cp;
        if(( c== '-' ) || ( c == '/' )) {
            cp++;
            //c = toupper(*cp);
            c = *cp;
            switch(c)
            {
            case 'v':
                cp++;
                if( *cp ) {
                    if ( is_all_nums(cp) )
                        verbosity = atoi(cp);
                    else {
                       printf( "ERROR: Only a number can follow -v! Not [%s]!\n", cp );
                       pgm_exit(1);
                    }
                } else
                    verbosity++;
                break;
            }
        }
    }
}

void  Process_Input( int argc, char * argv[], string & cmd )
{
   int   i, c;
   char * cp;
   Process_Verbosity( argc, argv );
   if(VERB9)
      printf( "Processing input ... %d items ...\n", (argc - 1));
   for( i = 1; i < argc; i++ )
   {
      cp = argv[i];
      c = *cp;
      if( cmd.length() )
         cmd += " ";
      cmd += cp;
      if (( c== '-' ) || ( c == '/' )) {
          // process a SWITCH
         cp++;
         //c = toupper(*cp);
         c = *cp;
         switch(c)
         {
         case '?':
            give_help();
            break;
         case 's':
            bUseSystem = 1;
            break;
         case 't':
            // get TIME (* = leave current)
            cp++;
            if( !get_TIME( cp ) ) {
               printf( "ERROR: Invalid TIME %s! ...\n", argv[i] );
               pgm_exit(1);
            }
            break;
         case 'u':
            bDoUpdate = 1;
            break;
         case 'v':
            // already done
            break;
         case 'd':
            // get DATE (* = leave as current)
            cp++;
            if( !get_DATE( cp ) ) {
               printf( "ERROR: Invalid DATE %s! ...\n", argv[i] );
               pgm_exit(1);
            }
            break;
#ifdef ADD_FOLDER
         case 'f':
             cp++;
             if ( !add_directory( cp ) ) {
                 printf( "ERROR: Unable to ADD directory [%s]!\n", cp );
                 pgm_exit(1);
             }
             break;
#endif
         case 'r':
            // reference file - us its time/date for update
            cp++;
            if( Reference ) {
               printf( "ERROR: Already have REFERENCE FILE %s ...\n", Reference );
               pgm_exit(1);
            }
            // FIX20100618 - Abort if -r<file> fails...
            if (strlen(cp) == 0) {
               printf( "ERROR: NO REFERENCE FILE following -r!\n" );
               pgm_exit(1);
            }
            Reference = strdupe(cp);
            if( !Get_REF( cp ) ) {
               printf( "ERROR: FAILED to get REFERENCE FILE [%s]!\n", Reference );
               pgm_exit(1);
            }
            break;
         default:
            err_cmd( argv[i] );
            break;
         }

      } else {
          // assume a BARE item is a FILE
          get_files(cp);
      }
   }
}

int   Got_End_Path_Sep( PTSTR path )
{
   size_t len = strlen(path);
   if(len) {
      len--;   // back to last char
      if( strchr( "\\/", path[len] ) )
         return 1;
   }
   return 0;
}

int   Got_Wild( PTSTR file )
{
   size_t len = strlen(file);
   size_t i;
   for(i = 0; i < len; i++)
   {
      int   c = file[i];
      if(( c == '?' )||( c == '*' ))
         return 1;
   }
   return 0;
}

void Add_2_List( PTSTR path, WIN32_FIND_DATA * pfd )
{
   PFILES   pf = new FILES;
   CHKMEM(pf);
   ZeroMemory( &fullpath, sizeof(fullpath) );
   pf->given_path = path;
   _splitpath( path, fullpath.drive, fullpath.dir, fullpath.fname, fullpath.ext );
   pf->path = fullpath.drive;
   pf->path += fullpath.dir;
   memcpy( &pf->fd, pfd, sizeof(WIN32_FIND_DATA));
   sfiles.push_back(pf);
}

void Process_Directory( PTSTR path )
{
   string s = path;
   int i = Got_End_Path_Sep(path);
   if(!i) s += "\\";
   s += "*.*";
   WIN32_FIND_DATA   fd;
   HANDLE hFind = FindFirstFile( s.c_str(), &fd ); 
   if( VFH(hFind) ) {
      do
      {
         if( !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ) {
            PFILES   pf = new FILES;
            pf->path = path;
            if(!i) pf->path += "\\";
            memcpy( &pf->fd, &fd, sizeof(WIN32_FIND_DATA));
            sfiles.push_back(pf);
         }
      } while( FindNextFile( hFind, &fd ) );
      FindClose( hFind );
   }
}

int   Check_Do_Update( PTSTR msg )
{
   printf( msg );
   // int chr = cin.get();
   int chr = toupper(_getch());
   fflush(stdin);
   return chr;
}

void  Process_File( string & s )
{
   //string s = *it;
   WIN32_FIND_DATA   fd;
   PTSTR ps = (PTSTR)s.c_str();
   HANDLE hFind = FindFirstFile( ps, &fd ); 
   if( !VFH(hFind) ) {
      printf( "ERROR: %s is NOT a valid file, or directory ...\n", ps );
      pgm_exit(1);
   }
   if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
      FindClose( hFind );
      Process_Directory( ps );
   } else if( Got_Wild( ps ) ) {
      do
      {
         if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
            // do nothing
         } else {
            Add_2_List( ps, &fd );
         }
      } while( FindNextFile( hFind, &fd ) );
      FindClose( hFind );
   } else {
      FindClose( hFind );
      Add_2_List( ps, &fd );
   }
}

string Get_CWD_Stg( void )
{
   TCHAR _s_CWD_Buf[MAX_PATH];
   PTSTR ps = _s_CWD_Buf;
   _getcwd( ps, MAX_PATH );
   string s = ps;
   return s;
}

void Do_Update( void )
{
   DWORD dwErr;
   int cnt = 0;
   string s;
   for( fiter fit = sfiles.begin(); fit != sfiles.end(); fit++ ) {
      PFILES pf = *fit;
      s = pf->path;
      s += pf->fd.cFileName;
      HANDLE hf = CreateFile( s.c_str(),
         GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
         FILE_ATTRIBUTE_NORMAL, NULL);
      cnt++;
      if( VFH(hf) ) {
         if( SetFileTime( hf, &FileTime, &FileTime, &FileTime ) ) {
            printf( "%d Done SetFileTime on file %s ...\n",
               cnt, pf->fd.cFileName );
         } else {
            dwErr = GetLastError();
            ErrMessage[0] = 0;
            printf( "%d WARNING: SetFileTime on file %s FAILED!\n",
               cnt, pf->fd.cFileName );
            if( GetLastErrorMsg( ErrMessage, 1024, dwErr ) != -1 ) {
               printf( "ERROR Indication: %s ...\n", ErrMessage ); 
            }
         }
         CloseHandle(hf);      
      } else {
         dwErr = GetLastError();
         ErrMessage[0] = 0;
         printf( "%d WARNING: Unable to OPEN file %s to change date/time!\n",
            cnt, pf->fd.cFileName );
         if( GetLastErrorMsg( ErrMessage, 1024, dwErr ) != -1 ) {
            printf( "ERROR Indication: %s ...\n", ErrMessage ); 
         }
      }
   }
}

#ifdef DO_NT_TEST

#define NTSTATUS LONG 

#define OBJ_CASE_INSENSITIVE 0x00000040L 

#define OBJ_KERNEL_HANDLE 0x00000200L 

// Desire Mode 

#define DESIRE_ACCESS ( SYNCHRONIZE | FILE_READ_DATA | FILE_WRITE_DATA | STANDARD_RIGHTS_READ | STANDARD_RIGHTS_WRITE ) 

// Shard Mode 

#define SHARD_MODE ( FILE_SHARE_READ | FILE_SHARE_WRITE ) 

// Create Disposition 

#define FILE_OPEN 0x00000001 

// Create Options 

#define FILE_SYNCHRONOUS_IO_NONALERT 0x00000020 

#define FILE_NON_DIRECTORY_FILE 0x00000040 

#if 0
#if (!defined(ANSI_STRING) && !defined(_STRING))
typedef struct _STRING 
{ 
    USHORT Length; 
    USHORT MaximumLength; 
    PCHAR Buffer; 
} ANSI_STRING, *PANSI_STRING; 
#endif

#ifndef UNICODE_STRING
typedef struct _UNICODE_STRING 
{
    USHORT Length; 
    USHORT MaxLength; 
    PWSTR Buffer; 
} UNICODE_STRING, *PUNICODE_STRING; 
#endif

#ifndef IO_STATUS_BLOCK
typedef struct _IO_STATUS_BLOCK { 
    union { 
        NTSTATUS Status; 
        PVOID Pointer; 
    }; 

    ULONG_PTR Information; 
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK; 
#endif

#ifndef OBJECT_ATTRIBUTES
typedef struct _OBJECT_ATTRIBUTES
{
    ULONG Length; 
    HANDLE RootDirectory; 
    PUNICODE_STRING ObjectName; 
    ULONG Attributes; 
    PVOID SecurityDescriptor; 
    PVOID SecurityQualityOfService; 
} OBJECT_ATTRIBUTES; 
typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES; 
#endif
#endif // 0

typedef int (WINAPI * 
NtCreateFileFunc)(PHANDLE,DWORD,POBJECT_ATTRIBUTES,PVOID,PVOID,ULONG,ULONG,ULONG,ULONG,PVOID,ULONG); 

typedef DWORD (WINAPI *PfRtlAnsiStringToUnicodeString)(PUNICODE_STRING, 
PANSI_STRING, BOOL); 

typedef DWORD (WINAPI *PfRtlUnicodeStringToAnsiString)(PANSI_STRING, 
PUNICODE_STRING, BOOL); 

typedef DWORD (WINAPI *PfRtlCompareUnicodeString)(PUNICODE_STRING, 
PUNICODE_STRING, BOOL); 


void test_get_nt(void)
{
    HANDLE hDir = NULL; 
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    ANSI_STRING as;
    //const char szDir[] = "\\\?\?\\CdRom0"; 
    const char szDir[] = "C:\\FG\\newDir"; 
    HMODULE hModule = GetModuleHandle("ntdll.dll");
    if (hModule == NULL) {
        printf("FAILED: hModule;!\n");
        pgm_exit(1);
    }

    NtCreateFileFunc ntcreatefile = 
        (NtCreateFileFunc)GetProcAddress(hModule,"NtCreateFile"); 

    PfRtlAnsiStringToUnicodeString MyRtlAnsiStringToUnicodeString = 
        (PfRtlAnsiStringToUnicodeString)GetProcAddress(hModule,"RtlAnsiStringToUnicodeString"); 

    PfRtlUnicodeStringToAnsiString MyRtlUnicodeStringToAnsiString = 
        (PfRtlUnicodeStringToAnsiString)GetProcAddress(hModule,"RtlUnicodeStringToAnsiString"); 

    PfRtlCompareUnicodeString MyRtlCompareUnicodeString = 
        (PfRtlCompareUnicodeString)GetProcAddress(hModule, "RtlCompareUnicodeString"); 

    if ( !ntcreatefile ) {
        printf("FAILED: (NtCreateFileFunc)GetProcAddress(hModule,\"NtCreateFile\");!\n");
        pgm_exit(1);
    }
    if ( !MyRtlAnsiStringToUnicodeString ) {
        printf("FAILED: (PfRtlAnsiStringToUnicodeString)GetProcAddress(hModule,\"RtlAnsiStringToUnicodeString\");!\n"); 
        pgm_exit(1);
    }

    if ( !MyRtlUnicodeStringToAnsiString ) {
        printf("FAILED: (PfRtlUnicodeStringToAnsiString)GetProcAddress(hModule,\"RtlUnicodeStringToAnsiString\");!\n");
        pgm_exit(1);
    }

    if ( !MyRtlCompareUnicodeString ) {
        printf("FAILED: (PfRtlCompareUnicodeString)GetProcAddress(hModule, \"RtlCompareUnicodeString\");!\n");
        pgm_exit(1);
    }
    as.Buffer = (char *)malloc(strlen(szDir) + 1); 
    strcpy(as.Buffer,szDir); 
    as.Length = as.MaximumLength = strlen(szDir);
    us.MaximumLength = us.Length = strlen(szDir); 

    // convert directory name from ANSI to UNICODE 
    MyRtlAnsiStringToUnicodeString(&us, &as, TRUE); 

    oa.Length = sizeof(oa); 
    oa.RootDirectory = NULL; 
    oa.ObjectName = &us; 
    oa.Attributes = OBJ_CASE_INSENSITIVE ; 
    oa.SecurityDescriptor = NULL; 
    oa.SecurityQualityOfService = NULL; 
    PIO_STATUS_BLOCK pIO = 
        (PIO_STATUS_BLOCK)malloc(sizeof(IO_STATUS_BLOCK)); 
    pIO->Information = 0; 

    NTSTATUS nt = 
        ntcreatefile( &hDir, DESIRE_ACCESS, &oa, 
        pIO, 0, 0, SHARD_MODE, FILE_OPEN, 0, NULL, 
        0 ); 

    printf( "Rtn - %d\n", nt ); 

    FreeLibrary(hModule);

    pgm_exit(0);

}
#endif

int main(int argc, char * argv[])
{
   int   exit_value = 0;
   SYSTEMTIME  syst, loct;
   string s = "";

#ifdef DO_NT_TEST
   test_get_nt();
#endif

   Process_Input( argc, argv, s );

   if(VERB2)
      printf( "Processed input: %s\n", s.c_str() );

   if( !files.size() ) {
      //no_input();
#ifdef ADD_FOLDER
        if ( !sfiles.size() )
            files.push_back( Get_CWD_Stg() );
#else
      files.push_back( Get_CWD_Stg() );
#endif
   }
   GetLocalTime( &loct );
   printf( "Current local  time is %s ...\n", Get_SysTime_Stg( &loct ) );
   GetSystemTime( &syst );
   printf( "Current system time is %s ...\n", Get_SysTime_Stg( &syst ) );
   if( bGotRef ) {
      printf( "Using Reference file %s for date and time ...\n", Reference );
   } else if( !bGotTime && !bGotDate ) {
      printf( "No Date, Time or Reference input - " );
      if( bUseSystem )
      {
         memcpy( &Target, &syst, sizeof(SYSTEMTIME) );
         printf( "using system date/time ...\n" );
      }
      else
      {
         memcpy( &Target, &loct, sizeof(SYSTEMTIME) );
         printf( "using local date/time ...\n" );
      }
   } else {
      if( !bGotTime ) {
         if( bUseSystem ) {
            printf( "Using system time, on user date ...\n" );
            Target.wHour = syst.wHour;
            Target.wMinute = syst.wMinute;
            Target.wSecond = syst.wSecond;
            Target.wMilliseconds = syst.wMilliseconds;
            Target.wDayOfWeek = syst.wDayOfWeek;
         } else {
            printf( "Using local time, on user date ...\n" );
            Target.wHour = loct.wHour;
            Target.wMinute = loct.wMinute;
            Target.wSecond = loct.wSecond;
            Target.wMilliseconds = loct.wMilliseconds;
            Target.wDayOfWeek = loct.wDayOfWeek;
         }
      } else if( !bGotDate ) {
         if( bUseSystem ) {
            printf( "Using system date, on users time ...\n" );
            Target.wDay = syst.wDay;
            Target.wMonth = syst.wMonth;
            Target.wYear = syst.wYear;
         } else {
            printf( "Using local date, on users time ...\n" );
            Target.wDay = loct.wDay;
            Target.wMonth = loct.wMonth;
            Target.wYear = loct.wYear;
         }
      } else {
         printf( "Using user input date and time ...\n" );
      }
   }

   printf( "On update, will use %s ...\n", Get_SysTime_Stg( &Target ) );

   if( !SystemTimeToFileTime( &Target, &FileTime ) ) {
      printf( "ERROR: FAILED to convert system time to file time ...\n" );
      pgm_exit(1);
   }

   // redo each FILE found, adding the information as well
   for( siter it = files.begin(); it != files.end(); it++ ) {
      Process_File( *it );
   }
   size_t cnt = sfiles.size();
   printf( "Found %d file(s)...\n", (int)cnt );
   if (cnt == 0) {
       printf( "ERROR: NO files found from current command!\n");
       pgm_exit(1);
   }
   cnt = 0;
   for( fiter fit = sfiles.begin(); fit != sfiles.end(); fit++ )
   {
      PFILES pf = *fit;
      cnt++;
      printf( "%s (%d)\n", Get_PFILES_Stg(pf), (int)cnt);
   }

   if( bDoUpdate ) {
      int   up = Check_Do_Update( "Continue with update? Y/N : " );
      if( toupper(up) == 'Y' ) {
         printf( "Yes.\n" );
         Do_Update();
      } else {
         printf( "NO.\n" );
      }
   } else {
      printf( "WARNING: No update done, since no -u command located ...\n" );
      exit_value = 1;
   }

   pgm_exit(exit_value);   // never return

	return exit_value;
}

// eof - DateFile.cpp
