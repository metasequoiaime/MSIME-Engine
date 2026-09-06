@echo off

rem Save the original working directory
set "orig_dir=%CD%"

rem Get the script directory
set "script_dir=%~dp0"

rem Check if the script directory contains "\bin\" (case-insensitive)
echo %script_dir% | findstr /i "\\bin\\" >nul
if %ERRORLEVEL%==0 (
  rem If script is in a \bin\ directory, use the original working directory as default download path
  set "default_download_path=%orig_dir%"
) else (
  rem Otherwise, use script directory
  pushd %~dp0
  set "default_download_path=%CD%"
  popd
)

rem Set the root path to be the parent directory of the script
for %%d in (%~dp0..) do set "root_path=%%~fd"

rem Count number of arguments passed to script
set argc=0
for %%x in (%*) do set /A argc+=1

set models=tiny tiny-q5_1 tiny-q8_0 ^
tiny.en tiny.en-q5_1 tiny.en-q8_0 ^
base base-q5_1 base-q8_0 ^
base.en base.en-q5_1 base.en-q8_0 ^
small small-q5_1 small-q8_0 ^
small.en small.en-q5_1 small.en-q8_0 small.en-tdrz ^
medium medium-q5_0 medium-q8_0 ^
medium.en medium.en-q5_0 medium.en-q8_0 ^
large-v1 ^
large-v2 large-v2-q5_0 large-v2-q8_0 ^
large-v3 large-v3-q5_0 ^
large-v3-turbo large-v3-turbo-q5_0 large-v3-turbo-q8_0

rem If argc is not equal to 1 or 2, print usage information and exit
if %argc% NEQ 1 (
  if %argc% NEQ 2 (
    echo.
    echo Usage: download-ggml-model.cmd model [models_path]
    CALL :list_models
    goto :eof
  )
)

if %argc% EQU 2 (
  set models_path=%2
) else (
  set models_path=%default_download_path%
)

set model=%1

for %%b in (%models%) do (
  if "%%b"=="%model%" (
    CALL :download_model
    goto :eof
  )
)

echo Invalid model: %model%
CALL :list_models
goto :eof

:download_model
echo Downloading ggml model %model%...

if exist "%models_path%\\ggml-%model%.bin" (
  echo Model %model% already exists. Skipping download.
  goto :eof
)
rem Pinned revisions rather than the upstream "main" branch: "resolve/main" hands back whatever the model repository holds today, so the same command on two machines can produce two different files. The digests in ggml-models.sha256 were read from these exact revisions.
set "whisper_rev=5359861c739e955e79d9a303bcbc70fb988958b1"
set "tdrz_rev=d44ba793fc67e509623a88a409723311fa677744"

echo %model% | findstr tdrz
if %ERRORLEVEL% neq 0 (
 PowerShell -NoProfile -ExecutionPolicy Bypass -Command "Start-BitsTransfer -Source https://huggingface.co/ggerganov/whisper.cpp/resolve/%whisper_rev%/ggml-%model%.bin -Destination \"%models_path%\\ggml-%model%.bin\""
) else (
  PowerShell -NoProfile -ExecutionPolicy Bypass -Command "Start-BitsTransfer -Source https://huggingface.co/akashmjn/tinydiarize-whisper.cpp/resolve/%tdrz_rev%/ggml-%model%.bin -Destination \"%models_path%\\ggml-%model%.bin\""

)

if %ERRORLEVEL% neq 0 (
  echo Failed to download ggml model %model%
  echo Please try again later or download the original Whisper model files and convert them yourself.
  goto :eof
)

CALL :verify_model
if %ERRORLEVEL% neq 0 goto :eof

rem Check if 'whisper-cli' is available in the system PATH
where whisper-cli >nul 2>&1
if %ERRORLEVEL%==0 (
  rem If found, suggest 'whisper-cli' (relying on PATH resolution)
  set "whisper_cmd=whisper-cli"
) else (
  rem If not found, suggest the local build version
  set "whisper_cmd=%root_path%\build\bin\Release\whisper-cli.exe"
)

echo Done! Model %model% saved in %models_path%\ggml-%model%.bin
echo You can now use it like this:
echo %whisper_cmd% -m %models_path%\ggml-%model%.bin -f samples\jfk.wav

goto :eof

:verify_model
rem A model that fails this check is deleted rather than left on disk, so a partial or substituted download cannot be picked up by a later run that only tests for the file's existence.
set "manifest=%~dp0ggml-models.sha256"
if not exist "%manifest%" (
  echo Cannot verify %model%: %manifest% is missing.
  del /f /q "%models_path%\ggml-%model%.bin" >nul 2>&1
  exit /b 1
)

set "expected="
for /f "usebackq tokens=1" %%h in (`findstr /r /c:"  ggml-%model%\.bin$" "%manifest%"`) do set "expected=%%h"
if not defined expected (
  echo No pinned SHA256 for model %model% in ggml-models.sha256.
  del /f /q "%models_path%\ggml-%model%.bin" >nul 2>&1
  exit /b 1
)

set "actual="
for /f "usebackq skip=1 tokens=*" %%h in (`CertUtil -hashfile "%models_path%\ggml-%model%.bin" SHA256`) do if not defined actual set "actual=%%h"
set "actual=%actual: =%"

if /i not "%actual%"=="%expected%" (
  echo SHA256 mismatch for %model%.
  echo   expected %expected%
  echo   actual   %actual%
  del /f /q "%models_path%\ggml-%model%.bin" >nul 2>&1
  exit /b 1
)

echo SHA256 verified for %model%.
exit /b 0

:list_models
  echo.
  echo Available models:
  (for %%a in (%models%) do (
    echo %%a
  ))
  echo.
  exit /b
