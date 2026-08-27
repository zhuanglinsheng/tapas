#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
destination_root=${VSCODE_EXTENSIONS_DIR:-"$HOME/.vscode/extensions"}
destination="$destination_root/tapas-language.tapas-language-0.1.0"

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
copy_atomically "$script_dir/protocol.js" "$destination/protocol.js"
copy_atomically "$script_dir/language-configuration.json" \
  "$destination/language-configuration.json"
mkdir -p "$destination/syntaxes"
copy_atomically "$script_dir/syntaxes/tapas.tmLanguage.json" \
  "$destination/syntaxes/tapas.tmLanguage.json"

repository_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
server="$repository_root/build/bin/tapas-language-server"
if [ -x "$server" ]; then
  mkdir -p "$destination/server"
  copy_atomically "$server" \
    "$destination/server/tapas-language-server" executable
else
  printf '%s\n' "Warning: $server is not built; configure tapas.languageServer.path manually."
fi

runtime="$repository_root/build/bin/tapas"
if [ -x "$runtime" ]; then
  mkdir -p "$destination/runtime"
  copy_atomically "$runtime" "$destination/runtime/tapas" executable
else
  printf '%s\n' "Warning: $runtime is not built; configure tapas.runtime.path manually."
fi

printf '%s\n' "Installed Tapas Language Support in $destination"
printf '%s\n' "Restart VS Code, then open a .tap file."
