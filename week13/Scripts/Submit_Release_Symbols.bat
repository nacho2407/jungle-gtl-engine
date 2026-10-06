@echo off
title Submit Release Symbols (Custom Tool Path)
REM 뒤에 커스텀 경로를 인자로 추가해 줍니다. (\srcsrv 전 단계 폴더까지 지정)
call "%~dp0PublishSymbol.bat" Release "C:\Windows Kits\10\Debuggers\x64"