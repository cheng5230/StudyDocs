@echo off
cd .
set /A try_times=1

:START

set /A try_times=%try_times%+1
if %try_times% GTR 20 goto END 

tools\tlsr82xx\tcdb.exe rst > %2\a.log
set /p log=<%2\a.log
echo %log%
if "%log%" EQU "  Slave MCU Reset Failed" (
	tools\tlsr82xx\Cmd_download_tool.exe 8258 sws b0 10 b0 10
	ping -n 1 127.0.0.1 > %2\a.log
	goto START
)

tools\tlsr82xx\tcdb.exe wf 0 -s 512k -e
if %errorlevel% EQU 1 goto END
tools\tlsr82xx\tcdb.exe -i %1 -b
if %errorlevel% EQU 1 goto END
echo file dowload to 0001f000
tools\tlsr82xx\tcdb.exe wc 6f 20
if %errorlevel% EQU 1 goto END

:END

del %2\a.log
