# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0
"""Keeps locales/*.json in step with the launcher's source.

  gen_locales.py                 rewrite locales/en-US.json from the source and tidy the other files
  gen_locales.py --check         fail if a file is stale, has a stray key or a wrong placeholder
  gen_locales.py --check --complete   also fail if a language lacks a translation
  gen_locales.py --export CODE   print "number<TAB>English" for every text CODE lacks
  gen_locales.py --import CODE FILE   merge "number<TAB>translation" lines into CODE
"""
import argparse
import glob
import io
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOCALES = os.path.join(ROOT, 'locales')
SOURCES = ['launcher/main.cc', 'launcher/core/*.cc', 'launcher/core/*.h', 'launcher/ui/*.cc', 'launcher/ui/*.h']
LANGUAGES = [
    ('en-US', 'English'),
    ('pt-BR', 'Português (Brasil)'),
    ('es-ES', 'Español'),
    ('ru-RU', 'Русский'),
    ('id-ID', 'Bahasa Indonesia'),
    ('fr-FR', 'Français'),
    ('de-DE', 'Deutsch'),
    ('tr-TR', 'Türkçe'),
    ('pl-PL', 'Polski'),
    ('vi-VN', 'Tiếng Việt'),
    ('th-TH', 'ไทย'),
    ('ja-JP', '日本語'),
    ('ko-KR', '한국어'),
    ('zh-CN', '简体中文'),
    ('zh-TW', '繁體中文'),
    ('ar-SA', 'العربية'),
]
PLURAL_CATEGORIES = {
    'ru': ['one', 'few', 'many'],
    'pl': ['one', 'few', 'many'],
    'ar': ['zero', 'one', 'two', 'few', 'many', 'other'],
}
CONTEXT = '##'
MARKERS = ('T', 'TF', 'TN', 'ELIXIR_TR')
ESCAPES = {'n': '\n', 'r': '\r', 't': '\t', '\\': '\\', '"': '"', "'": "'", '0': '\0'}
PLACEHOLDER = re.compile(r'\{[0-9]\}')


def unescape(body, where):
    out = []
    i = 0
    while i < len(body):
        c = body[i]
        if c != '\\':
            out.append(c)
            i += 1
            continue
        i += 1
        e = body[i]
        if e == 'u':
            out.append(chr(int(body[i + 1:i + 5], 16)))
            i += 5
        elif e == 'x':
            j = i + 1
            while j < len(body) and body[j] in '0123456789abcdefABCDEF':
                j += 1
            out.append(chr(int(body[i + 1:j], 16)))
            i = j
        elif e in ESCAPES:
            out.append(ESCAPES[e])
            i += 1
        else:
            raise SystemExit('%s: unknown escape \\%s' % (where, e))
    return ''.join(out)


def skip_space(text, i):
    while i < len(text) and text[i] in ' \t\r\n':
        i += 1
    return i


def read_literals(text, i, where):
    parts = []
    while True:
        i = skip_space(text, i)
        if not text.startswith('L"', i):
            break
        j = i + 2
        while text[j] != '"':
            j += 2 if text[j] == '\\' else 1
        parts.append(unescape(text[i + 2:j], where))
        i = j + 1
    return (''.join(parts) if parts else None), i


def skip_argument(text, i):
    depth = 0
    while i < len(text):
        c = text[i]
        if c == '"':
            i += 1
            while text[i] != '"':
                i += 2 if text[i] == '\\' else 1
        elif c == "'":
            i += 1
            while text[i] != "'":
                i += 2 if text[i] == '\\' else 1
        elif c in '([{':
            depth += 1
        elif c in ')]}':
            if depth == 0:
                return i
            depth -= 1
        elif c == ',' and depth == 0:
            return i
        i += 1
    return i


def scan(path, text, found):
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            end = text.find('\n', i)
            i = n if end < 0 else end
            continue
        if text.startswith('/*', i):
            end = text.find('*/', i + 2)
            i = n if end < 0 else end + 2
            continue
        if c == '"':
            i += 1
            while i < n and text[i] != '"':
                i += 2 if text[i] == '\\' else 1
            i += 1
            continue
        if c == "'":
            i += 1
            while i < n and text[i] != "'":
                i += 2 if text[i] == '\\' else 1
            i += 1
            continue
        if c.isalpha() or c == '_':
            j = i
            while j < n and (text[j].isalnum() or text[j] == '_'):
                j += 1
            word = text[i:j]
            if word in MARKERS and j < n and text[j] == '(' and word + '(' != 'L(':
                line = text.count('\n', 0, i) + 1
                where = '%s:%d' % (path, line)
                k = j + 1
                if word == 'TN':
                    k = skip_argument(text, k)
                    if k < n and text[k] == ',':
                        one, k = read_literals(text, k + 1, where)
                        k = skip_space(text, k)
                        other = None
                        if one is not None and k < n and text[k] == ',':
                            other, k = read_literals(text, k + 1, where)
                        if one is not None and other is not None:
                            found.append((one, where, None))
                            found.append((other, where, 'plural'))
                else:
                    key, k = read_literals(text, k, where)
                    if key is not None:
                        found.append((key, where, None))
            i = j
            continue
        i += 1


def collect():
    found = []
    for pattern in SOURCES:
        for path in sorted(glob.glob(os.path.join(ROOT, pattern))):
            with io.open(path, encoding='utf-8') as handle:
                scan(os.path.relpath(path, ROOT).replace('\\', '/'), handle.read(), found)
    keys = []
    seen = {}
    plurals = set()
    for key, where, kind in found:
        if key == '':
            continue
        if key not in seen:
            seen[key] = where
            keys.append(key)
        if kind == 'plural':
            plurals.add(key)
    return keys, seen, plurals


def english_of(key):
    return key.split(CONTEXT, 1)[0]


def locale_path(code):
    return os.path.join(LOCALES, code + '.json')


def read_locale(code):
    path = locale_path(code)
    if not os.path.isfile(path):
        return {}
    with io.open(path, encoding='utf-8') as handle:
        document = json.load(handle)
    strings = document.get('strings', {})
    return {key: value for key, value in strings.items() if isinstance(value, str)}


def allowed_keys(code, keys, plurals):
    allowed = list(keys)
    for category in PLURAL_CATEGORIES.get(code.split('-')[0], []):
        for key in keys:
            if key in plurals:
                allowed.append(key + CONTEXT + category)
    return allowed


def ordered(code, keys, plurals, strings):
    result = {}
    categories = PLURAL_CATEGORIES.get(code.split('-')[0], [])
    for key in keys:
        if key in strings and strings[key] != '':
            result[key] = strings[key]
        if key in plurals:
            for category in categories:
                variant = key + CONTEXT + category
                if variant in strings and strings[variant] != '':
                    result[variant] = strings[variant]
    return result


def dump(code, name, strings):
    document = {'code': code, 'language': name, 'strings': strings}
    return json.dumps(document, ensure_ascii=False, indent=2) + '\n'


def write_if_changed(path, text):
    current = None
    if os.path.isfile(path):
        with io.open(path, encoding='utf-8', newline='') as handle:
            current = handle.read().replace('\r\n', '\n')
    if current == text:
        return False
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with io.open(path, 'w', encoding='utf-8', newline='\n') as handle:
        handle.write(text)
    return True


def base_key(key, plurals):
    if CONTEXT in key:
        head, tail = key.rsplit(CONTEXT, 1)
        if head in plurals and tail in ('zero', 'one', 'two', 'few', 'many', 'other'):
            return head
    return key


def problems_of(code, keys, plurals, strings):
    problems = []
    allowed = set(allowed_keys(code, keys, plurals))
    for key, value in strings.items():
        if key not in allowed:
            problems.append('stray key %r' % key)
            continue
        source = english_of(base_key(key, plurals))
        wanted = set(PLACEHOLDER.findall(source))
        used = set(PLACEHOLDER.findall(value))
        counted_form = base_key(key, plurals) != key
        if used != wanted and not (counted_form and used < wanted):
            problems.append('placeholders differ in %r -> %r' % (key, value))
        if CONTEXT in value:
            problems.append('"##" inside the translation of %r' % key)
        if source.startswith(' ') != value.startswith(' ') or source.endswith(' ') != value.endswith(' '):
            problems.append('leading or trailing space differs in %r -> %r' % (key, value))
        if source.count('\n') != value.count('\n'):
            problems.append('line breaks differ in %r' % key)
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--complete', action='store_true')
    parser.add_argument('--export', metavar='CODE')
    parser.add_argument('--all', action='store_true', help='with --export: every text, not only the missing ones')
    parser.add_argument('--import', dest='import_', nargs=2, metavar=('CODE', 'FILE'))
    args = parser.parse_args()

    keys, seen, plurals = collect()
    names = dict(LANGUAGES)

    if args.export:
        strings = read_locale(args.export)
        for number, key in enumerate(keys, 1):
            if args.all or key not in strings:
                sys.stdout.write('%d\t%s\n' % (number, json.dumps(key, ensure_ascii=False)[1:-1]))
        return 0

    if args.import_:
        code, path = args.import_
        if code not in names or code == 'en-US':
            raise SystemExit('unknown language ' + code)
        strings = read_locale(code)
        with io.open(path, encoding='utf-8') as handle:
            for raw in handle:
                line = raw.rstrip('\r\n')
                if not line.strip():
                    continue
                parts = re.match(r'\s*(\d+(?:##\w+)?)[\t ]+(.*)$', line)
                if not parts:
                    raise SystemExit('%s: cannot read %r' % (path, line[:60]))
                head, body = parts.group(1), parts.group(2)
                suffix = ''
                if CONTEXT in head:
                    head, suffix = head.split(CONTEXT, 1)
                    suffix = CONTEXT + suffix
                number = int(head)
                if number < 1 or number > len(keys):
                    raise SystemExit('%s: no text number %d' % (path, number))
                strings[keys[number - 1] + suffix] = json.loads('"' + body + '"')
        problems = problems_of(code, keys, plurals, strings)
        for problem in problems:
            print('%s: %s' % (code, problem))
        if problems:
            return 1
        write_if_changed(locale_path(code), dump(code, names[code], ordered(code, keys, plurals, strings)))
        print('%s: %d of %d texts' % (code, sum(1 for key in keys if key in strings), len(keys)))
        return 0

    failed = False
    english = {key: english_of(key) for key in keys}
    for code, name in LANGUAGES:
        strings = english if code == 'en-US' else read_locale(code)
        problems = [] if code == 'en-US' else problems_of(code, keys, plurals, strings)
        text = dump(code, name, ordered(code, keys, plurals, strings) if code != 'en-US' else english)
        missing = [key for key in keys if key not in strings]
        if args.check:
            current = None
            if os.path.isfile(locale_path(code)):
                with io.open(locale_path(code), encoding='utf-8', newline='') as handle:
                    current = handle.read().replace('\r\n', '\n')
            if current != text and not problems:
                problems.append('the file is not what gen_locales.py writes; run it')
            if args.complete and missing:
                problems.append('%d texts lack a translation, first: %r' % (len(missing), missing[0]))
        elif not problems:
            write_if_changed(locale_path(code), text)
        for problem in problems[:20]:
            print('%s: %s' % (code, problem))
        failed = failed or bool(problems)
        print('%s: %d of %d texts' % (code, len(keys) - len(missing), len(keys)))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
