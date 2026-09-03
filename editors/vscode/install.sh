#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if [ -n "${CODE_COMMAND:-}" ]; then
  code_command=$CODE_COMMAND
elif command -v code >/dev/null 2>&1; then
  code_command=$(command -v code)
elif [ -x "/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code" ]; then
  code_command="/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code"
elif [ -x "/Applications/Visual Studio Code - Insiders.app/Contents/Resources/app/bin/code" ]; then
  code_command="/Applications/Visual Studio Code - Insiders.app/Contents/Resources/app/bin/code"
else
  printf '%s\n' "Cannot find the Visual Studio Code CLI." >&2
  printf '%s\n' "Install the 'code' command in PATH or set CODE_COMMAND to its path." >&2
  exit 1
fi

temporary_directory=$(mktemp -d "${TMPDIR:-/tmp}/tapas-vscode.XXXXXX")
cleanup() {
  rm -rf -- "$temporary_directory"
}
trap cleanup EXIT HUP INT TERM

version=$(awk -F'"' '/^[[:space:]]*"version"[[:space:]]*:/ { print $4; exit }' \
  "$script_dir/package.json")
vsix="$temporary_directory/tapas-language-$version.vsix"

(
  cd "$script_dir"
  npx --yes @vscode/vsce package --no-dependencies --out "$vsix"
)

"$code_command" --install-extension "$vsix" --force

printf '%s\n' "Installed Tapas Language Support $version from a validated VSIX."
printf '%s\n' "Tapas Core must be installed separately or configured in VS Code settings."
printf '%s\n' "Run 'Developer: Reload Window' in Visual Studio Code."
