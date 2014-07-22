@echo OFF
rem ===========================================================================
rem                        bladeRF Windows build script
rem ---------------------------------------------------------------------------
rem Copright (C) 2014 Nuand, LLC.
rem
rem Permission is hereby granted, free of charge, to any person obtaining a
rem copy of this software and associated documentation files (the "Software"),
rem to deal in the Software without restriction, including without limitation
rem the rights to use, copy, modify, merge, publish, distribute, sublicense,
rem and/or sell copies of the Software, and to permit persons to whom the
rem Software is furnished to do so, subject to the following conditions:
rem
rem The above copyright notice and this permission notice shall be included in
rem all copies or substantial portions of the Software.
rem
rem THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
rem IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
rem FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
rem AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
rem LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
rem FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
rem DEALINGS IN THE SOFTWARE.
rem ===========================================================================

rem ---------------------------------------------------------------------------
rem  Environment Variables
rem ---------------------------------------------------------------------------

rem Upstream source
set GIT_REPO=https://github.com/Nuand/bladeRF.git

rem bladeRF git repo version to use
set GIT_REV=HEAD

rem Path to libusb binaries and headers
set LIBUSB="C:\Program Files (x86)\libusb-1.0.19"

rem Path to Win32 Pthreads
set PTHREADS="C:\Program Files (x86)\pthreads-win32"

rem Build type (Release vs Debug)
set BUILD_TYPE=Release

rem Visual Studio 10 build environment variables
set VS_VARS="C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"

rem CMake's name for its Visual Studio 10 generator
set CMAKE_GENERATOR_32="Visual Studio 10"
set CMAKE_GENERATOR_64="Visual Studio 10 Win64"

rem Working directory to perform builds in
set WORKDIR=work

rem 32-bit and 64-built build directories
set BUILDDIR32=build32
set BUILDDIR64=build64

rem Default to build only the detected arch. Pass /A to build
rem both 32-bit and 64-bit builds
set BUILD_ALL_ARCHS=0

rem Provide a means to override these defaults with an external script
if exist bladeRF-env.bat call bladeRF-env.bat

rem ---------------------------------------------------------------------------
rem  Handle command line args
rem ---------------------------------------------------------------------------

:CMD_ARGS_LOOP
if "%1"=="/A" set BUILD_ALL_ARCHS=1
if "%1"=="/?" goto EXIT_USAGE
if "%1"=="" goto CMD_ARGS_DONE
shift
goto CMD_ARGS_LOOP
:CMD_ARGS_DONE

rem ---------------------------------------------------------------------------
rem  Build
rem ---------------------------------------------------------------------------

rem Clean up any prior working directory and create a fresh one
if not exist %WORKDIR% goto mk_work_dir
echo[
echo Cleaning up previous build. Removing %WORKDIR%.
rd /S /Q %WORKDIR%
:MK_WORK_DIR
mkdir %WORKDIR%
pushd work

rem Fetch the desired code base revision from upstream
echo[
echo ========================= Cloning Respository ========================
git clone %GIT_REPO% bladeRF || goto EXIT_FAILURE
pushd bladeRF
echo Checking out %GIT_REV%
rem git checkout %GIT_REV%
popd

rem Source Visual Studio environment. We want a 32-bit env to ensure the
rem installer is a "one size fits all."
echo[
call %VS_VARS% x86
echo[

rem Test for a 64-bit machine and skip the 64-bit build on a 32-bit machine
rem This check is based upon: https://support.microsoft.com/kb/556009
set REG_QUERY=HKLM\Hardware\Description\System\CentralProcessor\0
reg.exe Query %REG_QUERY% > query.txt
find /i "x86" < query.txt > strcheck.txt
if %ERRORLEVEL% == 0 (
    echo 32-bit OS detected. Skipping 64-bit build
    goto BUILD_32
)

mkdir %BUILDDIR64%
pushd %BUILDDIR64%
echo ====================== Configuring 64-bit build ======================
cmake -G %CMAKE_GENERATOR_64% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
      -DLIBUSB_PATH=%LIBUSB% -DLIBPTHREADSWIN32_PATH=%PTHREADS% ^
      ..\bladeRF\host ^
        || goto EXIT_FAILURE

echo ======================= Performing 64-bit build ======================
msbuild bladeRF.sln /t:Rebuild /p:Configuration=%BUILD_TYPE% ^
    || goto EXIT_FAILURE
echo[
echo[
popd

rem User didn't want to perform 32-bit build
if %BUILD_ALL_ARCHS% == 0 goto EXIT_SUCCESS

:BUILD_32
mkdir %BUILDDIR32%
pushd %BUILDDIR32%
echo[
echo[
echo ====================== Configuring 32-bit build ======================
cmake -G %CMAKE_GENERATOR_32% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
      -DLIBUSB_PATH=%LIBUSB% -DLIBPTHREADSWIN32_PATH=%PTHREADS% ^
      ..\bladeRF\host ^
        || goto EXIT_FAILURE

echo ======================= Performing 32-bit build ======================
msbuild bladeRF.sln /t:Rebuild /p:Configuration=%BUILD_TYPE% ^
    || goto EXIT_FAILURE
echo[
echo[
popd

goto EXIT_SUCCESS

:EXIT_USAGE
echo[
echo Usage: build-bladeRF.bat [options]
echo[
echo Options:
echo    /A  -   Build all. On 64-bit machines, this will cause both
echo            64-bit and 32-bit builds to be performed. This has no
echo            effect on 32-bit machines.
echo.   /?  -   Show this help text
echo[

:EXIT_SUCCESS
popd
exit /b 0

:EXIT_FAILURE
echo Failed with error #%errorlevel%
popd
exit /b %errorlevel%
