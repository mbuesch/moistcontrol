@echo off

echo ************************************************************
echo ************************************************************
echo INFO: If make.exe, avr-gcc.exe or other tools are not found,
echo       please make sure the AVR GCC toolchain is in your PATH
echo       environment variable.
echo ************************************************************
echo ************************************************************


rem Some bad guesses for toolchain paths...
set "PATH=%PATH%;%PROGRAMFILES%\Atmel\AVR Tools\AVR Toolchain\bin"
set "PATH=%PATH%;%PROGRAMFILES%\Atmel\Atmel Studio 6.1\shellUtils"
set "PATH=%PATH%;%PROGRAMFILES%\Atmel\Atmel Toolchain\AVR8 GCC\Native\3.4.2.1002\avr8-gnu-toolchain\bin"


echo Cleaning source tree...
make.exe distclean V=1 BINEXT=.exe NODEPS=1

echo Compiling...
make.exe V=1 BINEXT=.exe NODEPS=1

pause
