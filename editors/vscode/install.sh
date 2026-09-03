#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
destination_root=${VSCODE_EXTENSIONS_DIR:-"$HOME/.vscode/extensions"}
version=$(awk -F'"' '/^[[:space:]]*"version"[[:space:]]*:/ { print $4; exit }' \
  "$script_dir/package.json")
destination="$destination_root/tapas-language.tapas-language-$version"

copy_atomically() {
  source_file=$1
  destination_file=$2
  temporary_file="${destination_file}.tmp.$$"
  rm -f "$temporary_file"
  cp "$source_file" "$temporary_file"
  if [ "${3:-}" = executable ]; then
    chmod +x "$temporary_file"
  fi
  mv -f "$temporary_file" "$destination_file"
}

mkdir -p "$destination"
copy_atomically "$script_dir/package.json" "$destination/package.json"
copy_atomically "$script_dir/extension.js" "$destination/extension.js"
copy_atomically "$script_dir/formatter.js" "$destination/formatter.js"
copy_atomically "$script_dir/protocol.js" "$destination/protocol.js"
copy_atomically "$script_dir/README.md" "$destination/README.md"
copy_atomically "$script_dir/README_en.md" "$destination/README_en.md"
copy_atomically "$script_dir/LICENSE" "$destination/LICENSE"
copy_atomically "$script_dir/language-configuration.json" \
  "$destination/language-configuration.json"
mkdir -p "$destination/syntaxes"
copy_atomically "$script_dir/syntaxes/tapas.tmLanguage.json" \
  "$destination/syntaxes/tapas.tmLanguage.json"

printf '%s\n' "Installed Tapas Language Support in $destination"
printf '%s\n' "Tapas Core must be installed separately or configured in VS Code settings."
printf '%s\n' "Restart VS Code, then open a .tap file."
