## Project DateFile - Windows ONLY

Simple project to set/change the date, and time, of a file, or files...

It works somewhat like the unix `touch` command, supporting a `-r ref-file`, and `-d 20120101`,,,

The `--help` says it all -

```
Informatique Rouge - SET FILE DATE/TIME UTILITY - 13 May, 2020
Usage: DateFile [Options] FileSpec [filespec...]
Options: Each preceeded by '-' or '/', and space delimited
 -?           This brief help, and exit.
 -ddd:mm:yyyy Date to use for update.
 -thh:mm:ss   Time to use for update. Use only 24 hour time.
 -rRefFile    Reference file, used for update date and time.
 -s           Use system time for update date and time.
 -u           Update chosen files (Def=Off)
 -v[n]        Verbosity. Range 0-9. (Def=1)
Multiple file specifications can be given, including wild cards, '*' or '?'
If the file spec is a DIRECTORY, all files in that directory will be processed.
```

Note, the `-u` options **must** be given, and even then it will ask for a Y/N confirmation...

Enjoy...

Geoff  
20200513 - 20070330 - 20140819  

