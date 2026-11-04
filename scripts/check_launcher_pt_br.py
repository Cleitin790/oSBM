"""Validate launcher PT-BR coverage without game assets or a build toolchain."""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]
LANG = ROOT / 'source/application/mobile/android/host/app/src/main/assets/lang'


def read_locale(path):
    result = {}
    for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        line = line.strip()
        if not line or line.startswith(('#', '//')):
            continue
        assert '=' in line, (path, number, 'missing separator')
        key, value = (part.strip() for part in line.split('=', 1))
        assert key and value and key not in result, (path, number, 'empty or duplicate entry')
        assert '\ufffd' not in value, (path, number, 'invalid Unicode')
        result[key] = value
    return result


def placeholders(value):
    return sorted(re.findall(r'\{[^{}]*\}|%(?:[-+0 #]*\d*(?:\.\d+)?)?[sduif]', value))


def main():
    english = read_locale(LANG / 'en_US.lang')
    portuguese = read_locale(LANG / 'pt_BR.lang')
    assert not (english.keys() - portuguese.keys()), english.keys() - portuguese.keys()
    for key, value in english.items():
        assert placeholders(value) == placeholders(portuguese[key]), (key, 'placeholder mismatch')
    pattern = r'launcherText(?:Static)?\(\s*"([^"\\]+)"\s*,\s*("(?:[^"\\]|\\.)*")'
    count = 0
    for name in ('StarMobilePlatform.cpp', 'StarMobileControls.cpp'):
        source = (ROOT / 'source/application/mobile' / name).read_text(encoding='utf-8')
        for key, literal in re.findall(pattern, source):
            assert key in portuguese, (name, key, 'missing PT-BR entry')
            assert placeholders(json.loads(literal)) == placeholders(portuguese[key]), (name, key)
            count += 1
        # Human-readable direct ImGui labels must use the translation API.
        raw = re.findall(r'ImGui::(?:Button|Text\w*|Checkbox|Slider\w*|BeginCombo|InputText\w*|Selectable)\("([A-Za-z][^"]*)"', source)
        assert not [label for label in raw if label not in ('v', 'x')], (name, raw)
    print(f'PASS: {len(portuguese)} PT-BR entries, {len(english)} English entries, {count} source lookups; placeholders and labels checked')


if __name__ == '__main__':
    main()
