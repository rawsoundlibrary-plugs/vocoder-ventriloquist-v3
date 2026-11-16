@echo off
rem Compile vocoder ventriloquist sur Windows (necessite Visual Studio 2022 avec "Developpement Desktop en C++" et CMake).
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake est introuvable. Installez-le depuis https://cmake.org/download/
    pause
    exit /b 1
)

cmake -B build
if errorlevel 1 goto :error
cmake --build build --config Release --parallel
if errorlevel 1 goto :error

echo.
echo Termine. Copiez le dossier :
echo   build\VocoderVentriloquist_artefacts\Release\VST3\vocoder ventriloquist.vst3
echo dans :
echo   C:\Program Files\Common Files\VST3\
echo puis relancez votre DAW et rescannez les plugins.
pause
exit /b 0

:error
echo.
echo La compilation a echoue. Le message d erreur est affiche ci-dessus.
pause
exit /b 1
