# Installing Tapas Core

Extract the archive anywhere, install GNU Readline, and add its `bin` directory
to `PATH`. macOS users can install Readline with `brew install readline`.
Linux users need the GNU Readline runtime library provided by their distribution.
The VS Code extension is distributed separately as a VSIX.

Check the runtime with `tapas --version`. The formatter is included and can be
run with `tapas -m format --check example.tap`.

## Solving and sampling

Tapas uses the `python3` command found on `PATH`. A virtual environment is not
required. Check the interpreter you already use:

```sh
python3 -c "import sys, ortools; print(sys.executable, ortools.__version__)"
```

If OR-Tools is missing, install it using that interpreter's supported package
installation method. From the extracted archive directory:

```sh
python3 -m pip install -r share/tapas/solve/requirements.txt
```

The tested OR-Tools version is 9.14.6206. If your Python is managed by an OS
package manager, follow that environment's installation policy. You may use a
virtual environment if you prefer, but Tapas does not create or require one.
`TAPAS_SOLVE_PYTHON` optionally selects a different Python executable.
Ordinary language execution and Rule checking work without the solver backend.

Dense-array operations may require a compatible LP64 CBLAS library;
`TAPAS_BLAS_LIBRARY` can select its location.

See CHANGELOG.md for source migrations. Recompile bytecode with this runtime.
