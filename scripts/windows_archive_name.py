#!/usr/bin/env python3
"""Choose a descriptive Windows ZIP name without building or packaging anything."""
import argparse
from pathlib import Path
import re
import subprocess
import unicodedata

ROOT = Path(__file__).resolve().parent.parent
PREFIX = 'MotoGP0910-Windows-x64-'


def archive_name(label=None):
    if not label or not label.strip():
        try:
            label = subprocess.check_output(
                ['git', 'log', '-1', '--format=%s'], cwd=ROOT,
                text=True, encoding='utf-8', stderr=subprocess.DEVNULL).strip()
        except (OSError, subprocess.CalledProcessError) as error:
            raise ValueError('Supply --label with a short English description of the changes') from error
    label = re.sub(r'^[a-z]+(?:\([^)]*\))?!?:\s*', '', label.strip(), flags=re.I)
    label = unicodedata.normalize('NFKD', label).encode('ascii', 'ignore').decode().lower()
    label = re.sub(r'[^a-z0-9]+', '-', label).strip('-')[:72].rstrip('-')
    if not label:
        raise ValueError('The build label must contain English letters or digits')
    return f'{PREFIX}{label}.zip'


def available_archive(directory, filename):
    """Preserve earlier local packages when a descriptive label is reused."""
    archive = directory / filename
    attempt = 2
    while archive.exists():
        archive = directory / f'{Path(filename).stem}-{attempt}.zip'
        attempt += 1
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--label', help='Short English change description; default: latest commit subject')
    args = parser.parse_args()
    try:
        print(archive_name(args.label))
    except ValueError as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
