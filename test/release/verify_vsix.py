"""Check the actual VSIX manifest, contents, and shipped JavaScript."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import zipfile
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser()
parser.add_argument('vsix', type=Path)
parser.add_argument('--version', required=True)
args = parser.parse_args()
with zipfile.ZipFile(args.vsix) as archive:
    assert archive.testzip() is None
    names = set(archive.namelist())
    for file in ('extension.js', 'formatter.js', 'protocol.js', 'package.json',
                 'readme.md', 'README_zh.md', 'LICENSE.txt', 'images/icon.png',
                 'language-configuration.json', 'syntaxes/tapas.tmLanguage.json'):
        assert 'extension/' + file in names, file
    assert not any(n.startswith(('extension/test/', 'extension/runtime/',
        'extension/server/', 'extension/stdlib/')) or '__pycache__' in n for n in names)
    package = json.loads(archive.read('extension/package.json'))
    assert package['version'] == args.version
    identity = next(e for e in ET.fromstring(archive.read('extension.vsixmanifest')).iter()
                    if e.tag.endswith('}Identity') or e.tag == 'Identity')
    assert identity.attrib['Version'] == args.version
    assert identity.attrib['Publisher'] == package['publisher']
    with tempfile.TemporaryDirectory(prefix='tapas-vsix-') as tmp:
        for file in ('extension.js', 'formatter.js', 'protocol.js'):
            path = Path(tmp) / file
            path.write_bytes(archive.read('extension/' + file))
            subprocess.run(['node', '--check', str(path)], check=True)
print(f'VSIX {args.version}: ZIP integrity, manifest, contents, JavaScript passed')
