@echo off
cd /d "%~dp0"
call "%VS110COMNTOOLS%..\..\vc\vcvarsall.bat"
if not exist build mkdir build
cd build
del /q /s *
cmake -DCMAKE_BUILD_TYPE=MinSizeRel -G Ninja ..
ninja
pause
