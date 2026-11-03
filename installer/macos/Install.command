#!/bin/bash
# Installe FREE ANTICIP8 (VST3 + AU) pour l'utilisateur courant.
set -euo pipefail
cd "$(dirname "$0")"

VST3_SRC=$(find . -maxdepth 1 -type d -name "*.vst3" | head -1)
AU_SRC=$(find . -maxdepth 1 -type d -name "*.component" | head -1)

if [[ -z "${VST3_SRC:-}" && -z "${AU_SRC:-}" ]]; then
  osascript -e 'display alert "FREE ANTICIP8" message "Aucun plugin .vst3 / .component trouve a cote de ce script." as critical'
  exit 1
fi

mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"

install_one () {
  local src="$1"
  local dest_dir="$2"
  local name
  name=$(basename "$src")
  echo "→ $name"
  xattr -cr "$src" 2>/dev/null || true
  rm -rf "$dest_dir/$name"
  cp -R "$src" "$dest_dir/"
  codesign --force --deep -s - "$dest_dir/$name" 2>/dev/null || true
}

[[ -n "${VST3_SRC:-}" ]] && install_one "$VST3_SRC" "$HOME/Library/Audio/Plug-Ins/VST3"
[[ -n "${AU_SRC:-}" ]] && install_one "$AU_SRC" "$HOME/Library/Audio/Plug-Ins/Components"

osascript -e 'display alert "FREE ANTICIP8" message "Installation terminee.\nRecharge / rescanne les plugins dans ta DAW."'
echo "OK."
