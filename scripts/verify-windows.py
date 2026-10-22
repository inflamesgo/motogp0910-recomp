#!/usr/bin/env python3
"""Run Windows CPU/UI regression probes in isolated folders, preserving user settings.

Build the EXCLUDE_FROM_ALL probe targets first. On Windows omit --wine;
Linux/Wine validation additionally needs a display and the native VC++ runtime.
This suite does not verify rendered Windows GPU pixels or a full career race.
Use --probe NAME and --launcher-phase PHASE for focused reruns in fresh fixtures.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

PROBES = {
    'localization_probe': 'localization', 'file_compat_probe': 'game', 'runtime_stability_probe': 'game', 'nightly_input_probe': '',
    'fsr_provider_probe': '', 'engine_settings_probe': 'game', 'engine_advanced_probe': 'game',
    'game_language_probe': 'game', 'physics_asset_probe': 'game',
    'button_prompt_probe': 'game', 'livery_readback_probe': 'gpu',
    'motogp_headless_probe': 'game', 'launcher_runtime_probe': '',
    'display_settings_probe': '', 'button_prompt_input_probe': '',
    'launcher_probe': 'game', 'launcher_layout_probe': 'ui',
    'achievement_ui_probe': 'achievements', 'quit_menu_probe': 'quit',
    'in_game_settings_probe': 'settings', 'adaptive_triggers_probe': '',
    'debug_overlay_probe': 'debug', 'gpu_diagnostics_bridge_probe': 'gpu',
    'relay_crypto_probe': '',
    'config_file_probe': '',
}
FULL_DISC_PROBES = {'localization_probe', 'launcher_probe', 'launcher_layout_probe'}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, default=Path('out/windows-validation'))
    parser.add_argument('--wine', type=Path)
    parser.add_argument('--probe', action='append', choices=[*PROBES, 'input_defaults_probe'],
                        help='Run only this probe; repeat to select multiple probes')
    parser.add_argument('--launcher-phase', choices=['full', 'layout', 'remaining', 'sliders', 'graphics', 'iso', 'updates'],
                        help='Select a launcher UI phase; requires --probe launcher_layout_probe')
    parser.add_argument('--skip-ui', action='store_true')
    parser.add_argument('--compilation-inputs-only', action='store_true',
                        help='Report full-disc probes as skipped; run checks supported by XEX/patch inputs')
    parser.add_argument('--list-targets', action='store_true', help='Print selected build targets without running checks')
    args = parser.parse_args()
    requested = set(args.probe) if args.probe else {*PROBES, 'input_defaults_probe'}
    if args.launcher_phase and (not args.probe or 'launcher_layout_probe' not in requested or args.skip_ui or args.compilation_inputs_only):
        parser.error('--launcher-phase requires an enabled --probe launcher_layout_probe')
    include_inputs = 'input_defaults_probe' in requested
    skipped = {name: 'requires full original disc assets' for name in FULL_DISC_PROBES
               if args.compilation_inputs_only and name in requested}
    if args.skip_ui and 'launcher_layout_probe' in requested:
        skipped['launcher_layout_probe'] = 'interactive launcher UI check disabled'
    selected = {name: kind for name, kind in PROBES.items() if name in requested and name not in skipped}
    targets = [*selected, *(['input_defaults_probe'] if include_inputs else [])]
    if args.list_targets:
        print('\n'.join(targets))
        return 0
    build, game = args.build_dir.resolve(), args.game_root.resolve()
    for name in targets:
        if not (build / (name + '.exe')).is_file():
            parser.error(f'Build the missing probe target first: {name}')
    if not (game / 'default.xex').is_file():
        parser.error('Game root must contain default.xex and default.xexp')
    args.output_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='run-', dir=args.output_dir.resolve()))
    results = [{'probe': name, 'status': 'skipped', 'reason': reason, 'exit': None}
               for name, reason in sorted(skipped.items())]
    for result in results:
        print(f"{result['probe']}: SKIP ({result['reason']})", flush=True)
    (root / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    provenance = {file.name: hashlib.sha256(file.read_bytes()).hexdigest()
                  for file in (build / 'rexruntime.dll', build / 'rexgpu-xenos.dll',
                               build / 'amd_fidelityfx_dx12.dll')}
    (root / 'runtime-sha256.json').write_text(json.dumps(provenance, indent=2) + '\n')

    def target(path):
        path = str(path.resolve())
        return 'Z:' + path.replace('/', '\\') if args.wine else path

    def run(name, extra=None, config=None, environment=None, label=None):
        label = label or name
        fixture = root / label
        fixture.mkdir()
        # Exercise launcher configuration/resources in the public root while
        # loading every private binary and runtime DLL from bin.
        binaries=fixture/'bin' if name in ('launcher_probe','launcher_layout_probe') else fixture
        binaries.mkdir(exist_ok=True)
        for file in build.glob('*.dll'):
            shutil.copy2(file, binaries / file.name)
        shutil.copy2(build / (name + '.exe'), binaries / (name + '.exe'))
        # Launcher validation requires the real game EXE beside the probe.
        if name in ('launcher_probe', 'launcher_layout_probe'):
            shutil.copy2(build / 'motogp0910.exe', binaries / 'motogp0910.exe')
        if name == 'launcher_probe':
            shutil.copy2(build / 'MotoGP09-10Launcher.exe', fixture / 'MotoGP09-10Launcher.exe')
        if (build / 'resources').is_dir():
            shutil.copytree(build / 'resources', fixture / 'resources')
        if config is not None:
            (fixture / 'input-probe.toml').write_text(config, encoding='utf-8')
        command = ([str(args.wine.resolve())] if args.wine else []) + [target(binaries / (name + '.exe')), *(extra or [])]
        env = os.environ.copy()
        if args.wine:
            # Exercise the Microsoft CRT shipped with the application. Wine's
            # builtin MSVCP can give different condition-variable behavior.
            native_crt = 'msvcp140,msvcp140_atomic_wait,vcruntime140,vcruntime140_1=n'
            env['WINEDLLOVERRIDES'] = ';'.join(filter(None, (env.get('WINEDLLOVERRIDES', ''), native_crt)))
        for key in ('REX_MNK_MODE', 'REXGLUE_MNK_MODE'):
            env.pop(key, None)
        env.update(environment or {})
        start = time.monotonic()
        # The bilingual physical-input audit also resizes every launcher page
        # and tests ISO import; Wine can exceed the CPU probes' three minutes.
        timeout = 600 if name == 'launcher_layout_probe' else 180
        with (root / (label + '.log')).open('w', encoding='utf-8') as log:
            try:
                result = subprocess.run(command, cwd=fixture, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
                code = result.returncode
            except subprocess.TimeoutExpired:
                log.write(f'\nTimed out after {timeout} seconds\n')
                code = 124
        results.append({'probe': label, 'status': 'passed' if code == 0 else 'failed',
                        'exit': code, 'seconds': round(time.monotonic() - start, 3)})
        print(f'{label}: {"PASS" if code == 0 else "FAIL"}', flush=True)
        (root / 'results.json').write_text(json.dumps(results, indent=2) + '\n')

    for name, kind in selected.items():
        extra = [target(game)] if kind in ('game', 'ui') else [target(build / 'rexgpu-xenos.dll')] if kind == 'gpu' else []
        if kind == 'localization':
            extra = [target(game), target(build / 'resources/localization/tr/overlay')]
        if kind == 'achievements':
            extra = [target(build / 'resources/ui/OpenSans-Semibold.ttf'), target(root / 'achievement-preview')]
        if kind == 'debug':
            extra = [target(build / 'resources/ui/OpenSans-Semibold.ttf')]
        if kind == 'quit':
            extra = [target(game), target(build / 'resources/ui/OpenSans-Semibold.ttf')]
        if kind == 'settings':
            extra = [target(game), target(build / 'resources/ui/OpenSans-Semibold.ttf'), target(root / 'settings-preview')]
        if kind == 'ui':
            extra.append(target(root / 'screenshots'))
            if args.launcher_phase and args.launcher_phase != 'full':
                extra.append('--' + args.launcher_phase + '-only')
        run(name, extra)
    if include_inputs:
        for scenario in ('default', 'config-false', 'env-false', 'cli-false', 'cli-true'):
            extra = [scenario]
            if scenario.startswith('cli-'):
                extra.append('--mnk_mode=' + ('true' if scenario == 'cli-true' else 'false'))
            run('input_defaults_probe', extra, config='mnk_mode=false\n' if scenario == 'config-false' else None,
                environment={'REX_MNK_MODE':'false', 'REXGLUE_MNK_MODE':'false'} if scenario in ('env-false', 'cli-true') else None,
                label='input-' + scenario)
        run('input_defaults_probe', ['config-false'],
            config='mnk_mode=false\nkeybind_a="Semicolon,Space"\nkeybind_b="Quote,Backspace"\n', label='input-legacy-confirm')
        run('input_defaults_probe', ['config-false'],
            config='mnk_mode=false\nkeybind_a="Ctrl+K,Space"\nkeybind_b="Delete"\n', label='input-custom-confirm')
    print('Evidence:', root)
    return 1 if any(item['exit'] for item in results) else 0

if __name__ == '__main__':
    raise SystemExit(main())
