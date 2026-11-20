#!/usr/bin/env python3
"""Verify the small vendored QR encoder against the reviewed source lock."""
import hashlib,json,pathlib
root=pathlib.Path(__file__).resolve().parents[1]
lock=json.loads((root/'dependencies.lock.json').read_text())['qrcodegen']
for name,expected in lock['files'].items():
    assert pathlib.PurePosixPath(name).name==name
    if hashlib.sha256((root/'third_party/qrcodegen'/name).read_bytes()).hexdigest()!=expected:
        raise SystemExit('QR source differs from the pinned dependency: '+name)
