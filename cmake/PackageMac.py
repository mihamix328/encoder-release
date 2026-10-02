#!/usr/bin/env python3
"""Package both macOS GUI apps with Qt, dependency notices and ad-hoc signing."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys


def run(*args):
    return subprocess.run([str(arg) for arg in args], check=True, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout


def binaries(bundle):
    for path in bundle.rglob('*'):
        if path.is_file() and not path.is_symlink() and 'Mach-O' in run('file', '-b', path):
            yield path


def check_dependencies(bundle):
    frameworks = bundle / 'Contents/Frameworks'
    for binary in binaries(bundle):
        for line in run('otool', '-L', binary).splitlines()[1:]:
            dependency = line.strip().split(' (compatibility version', 1)[0]
            if dependency.startswith(('/System/Library/', '/usr/lib/')):
                continue
            if dependency.startswith('@rpath/'):
                candidate = frameworks / dependency[len('@rpath/'):]
            elif dependency.startswith('@loader_path/'):
                candidate = binary.parent / dependency[len('@loader_path/'):]
            elif dependency.startswith('@executable_path/'):
                candidate = bundle / 'Contents/MacOS' / dependency[len('@executable_path/'):]
            else:
                raise RuntimeError(f'External dependency in {binary}: {dependency}')
            candidate = candidate.resolve()
            if bundle.resolve() not in candidate.parents or not candidate.exists():
                raise RuntimeError(f'Missing bundled dependency in {binary}: {dependency}')
        for rpath in re.findall(r'cmd LC_RPATH\n.*?path (.*?) \(offset', run('otool', '-l', binary), re.S):
            if rpath.startswith('/') and not rpath.startswith(('/System/Library/', '/usr/lib/')):
                raise RuntimeError(f'External runtime search path in {binary}: {rpath}')


def copy_notices(bundle, prefix, supplied, overrides):
    destination = bundle / 'Contents/Resources/licenses'
    if supplied:
        shutil.copytree(supplied, destination)
        if not any(destination.rglob('*')):
            raise RuntimeError('Dependency license directory is empty')
        return
    library_names = {p.name for p in binaries(bundle)}
    records = []
    covered = set()
    for metadata in sorted((prefix / 'conda-meta').glob('*.json')):
        record = json.loads(metadata.read_text())
        matches = {Path(p).name for p in record.get('files', [])} & library_names
        if not matches:
            continue
        source = Path(record['link']['source']) / 'info/licenses'
        if overrides and (overrides / record['name']).is_dir():
            source = overrides / record['name']
        if not source.is_dir() or not any(source.rglob('*')):
            raise RuntimeError(f'Missing licenses for {record["name"]}: {source}')
        shutil.copytree(source, destination / record['name'])
        covered |= matches
        records.append({key: record.get(key) for key in ('name', 'version', 'build', 'license', 'url')})
    missing = {name for name in library_names - covered if name.endswith('.dylib')}
    if missing:
        raise RuntimeError(f'No dependency notices found for: {sorted(missing)}')
    (destination / 'DEPENDENCIES.json').write_text(json.dumps(records, ensure_ascii=False, indent=2) + '\n')
    (destination / 'README.txt').write_text(
        'Third-party libraries are dynamically linked and their license texts are included here.\n'
        'Qt is distributed under LGPLv3; users may replace its libraries with compatible versions.\n'
        'Qt source: https://download.qt.io/official_releases/qt/\n'
        'Conda-forge recipes and source URLs: https://github.com/conda-forge/qt6-main-feedstock\n'
        'Encoder source: https://github.com/mihamix328/encoder-release\n'
        'This software is based in part on the work of the FreeType Team.\n'
        'These local builds are ad-hoc signed. Modified bundles can be re-signed with codesign.\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--macdeployqt', required=True, type=Path)
    parser.add_argument('--dependency-prefix', type=Path, help='Conda dependency prefix (license records)')
    parser.add_argument('--licenses', type=Path, help='Alternative complete third-party license directory')
    parser.add_argument('--license-overrides', type=Path, help='Package-named license directories for split packages')
    parser.add_argument('--identity', default='-', help='codesign identity; default is local ad-hoc signing')
    args = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('Run this packager on macOS')
    if not args.licenses and not args.dependency_prefix:
        parser.error('Provide --licenses or --dependency-prefix to preserve dependency notices')
    build, output = args.build.resolve(), args.output.resolve()
    if output.exists() and any(output.iterdir()):
        parser.error('Output directory must be empty; previous builds are never overwritten')
    output.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(Path(__file__).resolve().parent.parent / 'docs/MACOS.md', output / 'README-macOS.md')
    checksums = []
    for component, name in [('client', 'Encoder'), ('admin', 'Encoder Admin')]:
        bundle = output / (name + '.app')
        source = build / component / bundle.name
        if not source.is_dir():
            raise RuntimeError(f'Build the GUI bundle first: {source}')
        run('ditto', source, bundle)
        deploy_args = [args.macdeployqt.resolve(), bundle, '-no-codesign', '-verbose=1']
        if args.dependency_prefix:
            deploy_args.append('-libpath=' + str(args.dependency_prefix.resolve() / 'lib'))
        result = run(*deploy_args)
        if 'ERROR:' in result:
            raise RuntimeError(result)
        check_dependencies(bundle)
        copy_notices(bundle, args.dependency_prefix, args.licenses, args.license_overrides)
        # Sign nested libraries first, then the app. No Developer ID is required locally.
        for binary in binaries(bundle):
            run('codesign', '--force', '--sign', args.identity, binary)
        run('codesign', '--force', '--sign', args.identity, bundle)
        run('codesign', '--verify', '--deep', '--strict', bundle)
        arch = run('lipo', '-archs', bundle / 'Contents/MacOS' / name).strip().replace(' ', '-')
        archive = output / f'encoder-{component}-macos-{arch}.zip'
        run('ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', bundle, archive)
        checksums.append(f'{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}')
        print(f'Created {bundle}\nCreated {archive}', flush=True)
    (output / 'SHA256SUMS.txt').write_text('\n'.join(checksums) + '\n')


if __name__ == '__main__':
    main()
