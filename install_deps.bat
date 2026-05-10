@echo off
setlocal
chcp 65001 >nul

REM Usage:
REM   install_deps.bat            -> Release
REM   install_deps.bat Debug      -> Debug
REM   install_deps.bat Debug "C:\path\to\python.exe"

set "BUILD_TYPE=%~1"
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=Release"
set "RUNTIME_TYPE=Release"
if /I "%BUILD_TYPE%"=="Debug" set "RUNTIME_TYPE=Debug"
set "PYTHON_EXE=%~2"

set "DEFAULT_PROFILE=%USERPROFILE%\.conan2\profiles\default"
set "COMPAT_PROFILE=%TEMP%\conan_cmake4_compat_profile"
if not exist "%DEFAULT_PROFILE%" (
  echo Conan default profile not found. Detecting one now...
  conan profile detect --force
  if errorlevel 1 (
    echo.
    echo Failed to detect Conan profile.
    exit /b 1
  )
)

if not "%PYTHON_EXE%"=="" (
  if not exist "%PYTHON_EXE%" (
    echo.
    echo Provided Python executable does not exist: "%PYTHON_EXE%"
    exit /b 1
  )
)

if "%PYTHON_EXE%"=="" if defined CONDA_PREFIX (
  if exist "%CONDA_PREFIX%\python.exe" set "PYTHON_EXE=%CONDA_PREFIX%\python.exe"
)

if "%PYTHON_EXE%"=="" (
  if exist "%USERPROFILE%\miniconda3\python.exe" set "PYTHON_EXE=%USERPROFILE%\miniconda3\python.exe"
)
if "%PYTHON_EXE%"=="" (
  if exist "%USERPROFILE%\anaconda3\python.exe" set "PYTHON_EXE=%USERPROFILE%\anaconda3\python.exe"
)
if "%PYTHON_EXE%"=="" (
  if exist "%LOCALAPPDATA%\miniconda3\python.exe" set "PYTHON_EXE=%LOCALAPPDATA%\miniconda3\python.exe"
)
if "%PYTHON_EXE%"=="" (
  if exist "%LOCALAPPDATA%\anaconda3\python.exe" set "PYTHON_EXE=%LOCALAPPDATA%\anaconda3\python.exe"
)

if "%PYTHON_EXE%"=="" (
  for /f "delims=" %%P in ('where python 2^>nul') do (
    echo %%P | findstr /I /C:"WindowsApps\\python.exe" >nul
    if errorlevel 1 (
      set "PYTHON_EXE=%%P"
      goto :python_found
    )
  )
)

:python_found
if "%PYTHON_EXE%"=="" (
  echo.
  echo Python executable not found.
  echo Activate your conda env first, or pass python path explicitly:
  echo   install_deps.bat %BUILD_TYPE% "C:\path\to\python.exe"
  exit /b 1
)

for %%I in ("%PYTHON_EXE%") do set "PYTHON_DIR=%%~dpI"
set "PATH=%PYTHON_DIR%;%PATH%"
set "PYTHON_EXE_CMAKE=%PYTHON_EXE:\=/%"

(
  echo [conf]
  echo tools.cmake:configure_args=["-DCMAKE_POLICY_VERSION_MINIMUM=3.5","-DPYTHON_EXECUTABLE=%PYTHON_EXE_CMAKE%","-DOIIO_BUILD_TOOLS=OFF"]
  echo tools.cmake.cmaketoolchain:extra_variables={"CMAKE_POLICY_VERSION_MINIMUM":"3.5"}
) > "%COMPAT_PROFILE%"
if errorlevel 1 (
  echo.
  echo Failed to create temporary Conan compatibility profile.
  exit /b 1
)

echo Installing Conan dependencies (build type: %BUILD_TYPE%, cppstd: 20)...
echo Applying CMake 4 compatibility workaround for older dependency CMakeLists...
echo Using Python executable: "%PYTHON_EXE%"
conan install . --output-folder=build --build=missing --build=boost/* --build=openimageio/* -pr:h "%DEFAULT_PROFILE%" -pr:h "%COMPAT_PROFILE%" -pr:b "%DEFAULT_PROFILE%" -pr:b "%COMPAT_PROFILE%" -s:h os=Windows -s:h arch=x86_64 -s:h compiler=msvc -s:h compiler.version=194 -s:h compiler.runtime=dynamic -s:h compiler.runtime_type=%RUNTIME_TYPE% -s:h build_type=%BUILD_TYPE% -s:h compiler.cppstd=20 -s:b os=Windows -s:b arch=x86_64 -s:b compiler=msvc -s:b compiler.version=194 -s:b compiler.runtime=dynamic -s:b compiler.runtime_type=%RUNTIME_TYPE% -s:b build_type=%BUILD_TYPE% -s:b compiler.cppstd=20
if errorlevel 1 (
  echo.
  echo Conan install failed.
  del /q "%COMPAT_PROFILE%" >nul 2>nul
  exit /b 1
)

echo.
echo Conan dependencies installed successfully.
del /q "%COMPAT_PROFILE%" >nul 2>nul
exit /b 0
