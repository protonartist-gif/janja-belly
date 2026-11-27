#!/bin/bash
set -e
cd "$(dirname "$0")"

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCOPY_PLUGIN_AFTER_BUILD=OFF
cmake --build build --config Release --parallel

ART="build/JanjaBelly_artefacts/Release"
VST3="$ART/VST3/Janja Belly.vst3"
AU="$ART/AU/Janja Belly.component"
[ -d "$VST3" ] && [ -d "$AU" ] || { echo "Build output not found in $ART"; exit 1; }

codesign --force --deep -s - "$VST3"
codesign --force --deep -s - "$AU"

rm -rf dmg_root && mkdir -p dmg_root
cp -R "$VST3" "$AU" dmg_root/
ln -s "/Library/Audio/Plug-Ins/VST3" "dmg_root/Install VST3 here"
ln -s "/Library/Audio/Plug-Ins/Components" "dmg_root/Install AU here"
printf 'Drag the .vst3 onto "Install VST3 here" and the .component onto "Install AU here".\nIf macOS blocks it, run: xattr -cr "/Library/Audio/Plug-Ins/Components/Janja Belly.component"\n' > "dmg_root/READ ME.txt"

rm -f JanjaBelly-0.2.dmg
hdiutil create -volname "Janja Belly" -srcfolder dmg_root -ov -format UDZO JanjaBelly-0.2.dmg
echo "Done"
