#!/bin/bash
# Installe Meghir Stress (VST3 + AU) pour l'utilisateur courant.
# Usage : ouvre le Terminal dans ce dossier, puis :  bash install.sh
cd "$(dirname "$0")" || exit 1

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

# Retire le blocage « téléchargé depuis internet » de macOS
xattr -cr "Meghir Stress.vst3" "Meghir Stress.component" 2>/dev/null

rm -rf "$VST3_DIR/Meghir Stress.vst3" "$AU_DIR/Meghir Stress.component"
cp -R "Meghir Stress.vst3" "$VST3_DIR/"
cp -R "Meghir Stress.component" "$AU_DIR/"

# Force macOS à relire les Audio Units
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "Installé. Relance Ableton puis scanne les plug-ins (Préférences > Plug-ins)."
