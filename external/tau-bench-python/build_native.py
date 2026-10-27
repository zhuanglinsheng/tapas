"""Build the optional local accelerator with this Python's compiler configuration."""
from pathlib import Path
import os
import shlex
import subprocess
import sysconfig


def main():
    here = Path(__file__).resolve().parent
    target = here / ('_sampling_native' + sysconfig.get_config_var('EXT_SUFFIX'))
    command = shlex.split(sysconfig.get_config_var('LDSHARED'))
    command += shlex.split(sysconfig.get_config_var('CFLAGS') or '')
    command += ['-O3', '-fno-fast-math', '-I'+sysconfig.get_paths()['include'],
                str(here/'sampling_native.c'), '-o', str(target)+'.tmp']
    # Never build on import; a failed build leaves the previous binary intact.
    try:
        subprocess.run(command, check=True)
        os.replace(str(target)+'.tmp', target)
    finally:
        Path(str(target)+'.tmp').unlink(missing_ok=True)
    print(target)


if __name__ == '__main__':
    main()
