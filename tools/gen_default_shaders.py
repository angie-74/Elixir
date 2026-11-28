# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0
"""Writes launcher/core/default_shaders.cc from assets/shaders.

  gen_default_shaders.py           rewrite the file
  gen_default_shaders.py --check   fail if the file is stale
"""
import io
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(ROOT, 'assets', 'shaders')
TARGET = os.path.join(ROOT, 'launcher', 'core', 'default_shaders.cc')
ORDER = [
    'FXAA.frag',
    'Sharpening.frag',
    'Bloom',
    'Lens flare.frag',
    'God rays.frag',
    'Tonemapping.frag',
    'Color correction.frag',
    'Chromatic aberration.frag',
    'Vignette.frag',
    'Film grain.frag',
    'Debanding.frag',
    'Dithering.frag',
]
DELIMITER = 'elixir'


def collect():
    files = []
    for name in sorted(os.listdir(SOURCE)):
        path = os.path.join(SOURCE, name)
        if os.path.isdir(path):
            for inner in sorted(os.listdir(path)):
                files.append((name + '\\' + inner, os.path.join(path, inner)))
        else:
            files.append((name, path))
    top = sorted({relative.split('\\')[0] for relative, _ in files})
    if sorted(ORDER) != top:
        raise SystemExit('ORDER and assets/shaders differ: %r vs %r' % (sorted(ORDER), top))
    return files


def render():
    out = ['// Copyright 2026 Elixir Project Authors\n',
           '// Licensed under the Apache License, Version 2.0\n',
           '#include "core/shaders.h"\n\n',
           'namespace elixir::launcher {\n\n',
           'const DefaultShaderFile kDefaultShaderFiles[] = {\n']
    files = collect()
    for relative, path in files:
        text = io.open(path, encoding='utf-8', newline='').read().replace('\r\n', '\n')
        if ')' + DELIMITER + '"' in text:
            raise SystemExit(relative + ' contains the raw string delimiter')
        out.append('    {L"%s", R"%s(%s)%s"},\n' % (relative.replace('\\', '\\\\'), DELIMITER, text, DELIMITER))
    out.append('};\n\n')
    out.append('const std::size_t kDefaultShaderFileCount = %d;\n\n' % len(files))
    out.append('const wchar_t* const kDefaultShaderOrder[] = {\n')
    for name in ORDER:
        out.append('    L"%s",\n' % name)
    out.append('};\n\n')
    out.append('const std::size_t kDefaultShaderOrderCount = %d;\n\n' % len(ORDER))
    out.append('}\n')
    return ''.join(out)


def main():
    text = render()
    if '--check' in sys.argv[1:]:
        current = io.open(TARGET, encoding='utf-8', newline='').read() if os.path.exists(TARGET) else ''
        if current != text:
            print('launcher/core/default_shaders.cc is stale: run tools/gen_default_shaders.py')
            return 1
        return 0
    io.open(TARGET, 'w', encoding='utf-8', newline='').write(text)
    print('wrote %s' % os.path.relpath(TARGET, ROOT))
    return 0


if __name__ == '__main__':
    sys.exit(main())
