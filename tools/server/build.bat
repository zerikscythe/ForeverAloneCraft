@echo off
setlocal

set REPO=C:\WOW_Private\ForeverAloneCraft
set BUILD=%REPO%\build
set BIN=%BUILD%\bin\Debug
set CMAKE="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set VCPKG=C:\WOW_Private\vcpkg
set MYSQL=C:\WOW_Private\mysql-8.0.46-winx64

echo.
echo [1/4] Pulling latest from git...
cd /d %REPO%
git pull
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: git pull failed.
    pause
    exit /b 1
)

echo.
echo [2/4] Running CMake configure (picks up new source files)...
%CMAKE% -B %BUILD% -S %REPO% -G "Visual Studio 17 2022" ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG%\scripts\buildsystems\vcpkg.cmake ^
    -DBoost_ROOT=%VCPKG%\installed\x64-windows ^
    -DCMAKE_PREFIX_PATH=%VCPKG%\installed\x64-windows ^
    -DMYSQL_INCLUDE_DIR="%MYSQL%\include" ^
    -DMYSQL_LIBRARY="%MYSQL%\lib\libmysql.lib"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configure failed.
    pause
    exit /b 1
)

echo.
echo [3/4] Building modules...
%CMAKE% --build %BUILD% --config Debug --target modules 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: modules build failed.
    pause
    exit /b 1
)

echo.
echo [4/4] Building worldserver...
%CMAKE% --build %BUILD% --config Debug --target worldserver 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: worldserver build failed.
    pause
    exit /b 1
)

echo.
echo Build complete. Executable: %BIN%\worldserver.exe
echo Run with: cd /d %BIN% ^& worldserver.exe
echo.
pause
