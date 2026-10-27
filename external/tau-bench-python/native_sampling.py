"""Optional, explicitly built CPython accelerator; pure Python needs no compiler."""
import os
try:
    if os.environ.get('SAMPLING_NATIVE') == '0':
        raise ImportError('Native sampling disabled')
    from _sampling_native import compile_draw, draw as native_draw
    BACKEND = 'c'
except ImportError:
    BACKEND = 'python'
    def compile_draw(specs):
        return None
    native_draw = None


def metadata():
    import hashlib
    from pathlib import Path
    import sys
    binary = Path(sys.modules['_sampling_native'].__file__) if BACKEND == 'c' else None
    return dict(backend=BACKEND, binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest() if binary else None)
