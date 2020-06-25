@setlocal
@set TMPPRJ=DateFile
@set TMPLOG=bldlog-1.txt
@set BUILD_RELDBG=0

@call chkmsvc %TMPPRJ%

@echo Built project %TMPPRJ%... all ouput to %TMPLOG%

@set CMOPTS=
@REM ***************************************************
@REM NOTE WELL: FIXME: NOTE SPECIAL INSTALL PREFIX
@REM ***************************************************
@set CMOPTS=%CMOPTS% -DCMAKE_INSTALL_PREFIX=C:/MDOS
@REM ***************************************************

@echo Commence build %TMPPRJ% %DATE% at %TIME% > %TMPLOG%

@echo Doing: 'cmake .. %CMOPTS%'
@echo Doing: 'cmake .. %CMOPTS%' >> %TMPLOG%
@cmake .. %CMOPTS% >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR1

@echo Doing: 'cmake --build . --config Debug'
@echo Doing: 'cmake --build . --config Debug' >> %TMPLOG%
@cmake --build . --config Debug >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR4

@if "%BUILD_RELDBG%x" == "0x" goto DNRELDBG
@echo Doing: 'cmake --build . --config RelWithDebInfo'
@echo Doing: 'cmake --build . --config RelWithDebInfo' >> %TMPLOG%
@cmake --build . --config RelWithDebInfo >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR2
:DNRELDBG

@echo Doing: 'cmake --build . --config Release'
@echo Doing: 'cmake --build . --config Release' >> %TMPLOG%
@cmake --build . --config Release >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR3
@echo.
@echo Appears successful...
@echo.
@echo No install is configured, but there is an updexe.bat
@echo which may be adjustable to your needs...
@echo.
@goto END

:ERR1
@echo ERROR: cmake config/gen
@goto ISERR

:ERR2
@echo ERROR: cmake build RelWitDebInfo
@goto ISERR

:ERR3
@echo ERROR: cmake build Release
@goto ISERR

:ERR4
@echo ERROR: cmake build Debug
@goto ISERR

:ISERR
@endlocal
@exit /b 1

:END
@endlocal
@exit /b 0

@REM eof
