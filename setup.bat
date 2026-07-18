@echo off
setlocal

:: 1. Check for Docker Image
echo Have you already created the Docker image 'buildenv-krane'?
choice /c YN /m "Press Y for Yes, N for No"

if errorlevel 2 (
    echo.
    echo Building the Docker image...
    docker build -t buildenv-krane .\buildenv
    if %errorlevel% neq 0 (
        echo [ERROR] Docker build failed.
        pause
        exit /b 1
    )
)

:: 2. Check for Log Viewer
echo.
echo Do you have 'Log Viewer' installed?
choice /c YN /m "Press Y for Yes, N for No"

if errorlevel 2 (
    echo.
    echo Downloading Log Viewer...
    
    if not exist "vendor\ntsoftware\logviewer" mkdir "vendor\ntsoftware\logviewer"
    
    :: Using the direct link you found
    powershell -Command "Invoke-WebRequest -Uri 'https://master.dl.sourceforge.net/project/log-viewer-v1/Log%%20Viewer.exe?viasf=1&fid=503b0c47af98af1a' -OutFile 'vendor\ntsoftware\logviewer\Log Viewer.exe'"
    
    echo.
    echo Download complete.
)

:: 3. Run the project
echo.
echo Executing run.bat...
if exist "run.bat" (
    call run.bat
) else (
    echo [ERROR] run.bat not found in current directory.
    pause
)
exit /b