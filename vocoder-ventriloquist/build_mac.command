#!/bin/bash
# Compile vocoder ventriloquist sur Mac et l'installe (VST3 + Audio Unit).
# Double-cliquer sur ce fichier, ou lancer : ./build_mac.command
set -e
cd "$(dirname "$0")"

if ! command -v cmake >/dev/null 2>&1; then
    echo "CMake est introuvable. Installez-le avec :  brew install cmake"
    echo "(ou depuis https://cmake.org/download/)"
    exit 1
fi
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Les outils de développement Apple sont introuvables. Lancez :  xcode-select --install"
    exit 1
fi

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel

echo
echo "Terminé. Le plugin est installé dans :"
echo "  ~/Library/Audio/Plug-Ins/VST3/vocoder ventriloquist.vst3"
echo "  ~/Library/Audio/Plug-Ins/Components/vocoder ventriloquist.component"
echo "Relancez votre DAW et rescannez les plugins."
