#!/usr/bin/env python3
"""Fetch a private two-file compilation archive without publishing its URL."""
import hashlib
import io
import json
import os
from pathlib import Path
import sys
import urllib.parse
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parent.parent
DESTINATION = ROOT / 'assets/actions-inputs'
LIMIT = 128 * 1024 * 1024
EXPECTED = {
    'default.xex': '54245417e996ed0aa561a5a803fd810e87684bd0b43082f316db711b3ee786ae',
    'default.xexp': '0419205ba10dc65eaa7b0bb62abc64cada46119ece5ed749e4ba20c06a6bc079',
}


class HttpsRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if urllib.parse.urlsplit(newurl).scheme.lower() != 'https':
            raise ValueError('Private input redirects must use HTTPS.')
        redirected = super().redirect_request(req, fp, code, msg, headers, newurl)
        if redirected and urllib.parse.urlsplit(req.full_url).netloc.lower() != urllib.parse.urlsplit(newurl).netloc.lower():
            redirected.remove_header('Authorization')
        return redirected


def main():
    url = os.environ.get('GAME_INPUTS_URL', '').strip()
    if urllib.parse.urlsplit(url).scheme.lower() != 'https':
        raise ValueError('Set GAME_INPUTS_URL to a private direct HTTPS ZIP download.')
    headers = {'User-Agent': 'MotoGP0910-Actions', 'Accept': 'application/octet-stream'}
    token = os.environ.get('GAME_INPUTS_TOKEN', '').strip()
    if token:
        if urllib.parse.urlsplit(url).netloc.lower() != 'api.github.com':
            raise ValueError('GAME_INPUTS_TOKEN can only be sent to api.github.com.')
        headers['Authorization'] = f'Bearer {token}'
    try:
        opener = urllib.request.build_opener(HttpsRedirects())
        # A private release tag URL avoids requiring the owner to find asset IDs.
        if urllib.parse.urlsplit(url).netloc.lower() == 'api.github.com' and '/releases/tags/' in urllib.parse.urlsplit(url).path:
            metadata_headers = dict(headers, Accept='application/vnd.github+json')
            with opener.open(urllib.request.Request(url, headers=metadata_headers), timeout=90) as response:
                metadata = response.read(1024 * 1024 + 1)
            if len(metadata) > 1024 * 1024:
                raise RuntimeError('Release metadata exceeds the size limit.')
            matches = [asset for asset in json.loads(metadata)['assets'] if asset['name'] == 'build-inputs.zip']
            if len(matches) != 1:
                raise RuntimeError('The private release must contain build-inputs.zip.')
            url = matches[0]['url']
            if urllib.parse.urlsplit(url).scheme != 'https' or urllib.parse.urlsplit(url).netloc.lower() != 'api.github.com':
                raise RuntimeError('Invalid GitHub release asset address.')
        with opener.open(urllib.request.Request(url, headers=headers), timeout=90) as response:
            archive = response.read(LIMIT + 1)
    except Exception:
        # Network exceptions can contain the signed URL. Keep it out of logs.
        raise RuntimeError('Private input download failed; check GAME_INPUTS_URL and its expiry.') from None
    if len(archive) > LIMIT:
        raise ValueError('Private input archive exceeds the 128 MiB limit.')
    contents = {}
    with zipfile.ZipFile(io.BytesIO(archive)) as package:
        if sorted(package.namelist()) != sorted(EXPECTED):
            raise ValueError('The input ZIP must contain only default.xex and default.xexp at its root.')
        for name, expected in EXPECTED.items():
            if package.getinfo(name).file_size > LIMIT:
                raise ValueError('Compilation input exceeds the size limit.')
            data = package.read(name)
            if hashlib.sha256(data).hexdigest() != expected:
                raise ValueError(f'Original input SHA-256 mismatch: {name}')
            contents[name] = data
    # Verify the entire archive before writing either input; never extract paths.
    DESTINATION.mkdir(parents=True, exist_ok=True)
    for name, data in contents.items():
        (DESTINATION / name).write_bytes(data)
    print('Private compilation inputs downloaded and SHA-256 verified.')


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        # Do not emit downloaded content, credentials or external exception text.
        if isinstance(error, (ValueError, RuntimeError)):
            print(str(error), file=sys.stderr)
        else:
            print('Could not prepare the private compilation inputs.', file=sys.stderr)
        sys.exit(1)
