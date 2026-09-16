# Release validation

Use the existing `python3` on PATH. These checks do not create a virtual
environment. Solving checks require that interpreter to import OR-Tools.

From the repository root, after building:

```sh
python3 test/release/check_docs.py --tapas build/bin/tapas
python3 test/release/verify_core.py /path/to/extracted/tapas-core-VERSION-PLATFORM --version VERSION
python3 test/release/verify_vsix.py /path/to/tapas-language-VERSION.vsix --version VERSION
```

`check_docs.py` covers maintained documentation and both root READMEs. It runs
executable fences and standalone examples, compares recorded output (ignoring
heap addresses), checks local file links and public header paths, checks the
reserved-word lists, and tests selected positive/negative language contracts.
Draft documents, literature, partial `text`/EBNF examples, and host-dependent C
fragments are not executable specifications. Passing this check is evidence,
not a proof of every natural-language claim or external URL/anchor.

`verify_core.py` runs the installed runtime and server from a new temporary
working directory, clears interpreter and module-path overrides, and verifies
version, source/bytecode solving and sampling, formatter discovery, and LSP
initialization. The extracted archive must contain the dependency requirement
and third-party notices. Run it for each native target, not on a foreign binary.

`verify_vsix.py` inspects the actual ZIP and VSIX identity, required/excluded
files, and shipped JavaScript syntax. GUI activation still requires VS Code;
local release checks can install into isolated user-data and extension dirs.

The release workflow remains tag-only. It verifies dependencies, documentation,
and extracted archives on each Core target before uploading release assets.

Keep reusable validation and packaging changes on `main`. Prepare version
metadata and `CHANGELOG.md` on a permanent version branch (for example,
`0.2.0`), then create its matching tag (`v0.2.0`) on that branch. Ordinary
branch pushes do not run the release workflow.
