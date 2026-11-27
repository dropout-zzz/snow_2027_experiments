#!/usr/bin/env python
import argparse
import os.path
import subprocess
import tempfile

def _cmd(x: list[str], **kwargs):
  subprocess.run(x, check=True, **kwargs)

def _cc(x: list[str], **kwargs):
  _cmd(['gcc', '-c'] + x, **kwargs)

def main():
  ap = argparse.ArgumentParser(description='''\
Generate a stub library for a Windows DLL suitable for linking into a SNOW program.

This program takes a *.lst and writes a lib*.a.

Output file will be placed in same directory with input file.

DLL name is determined from input file name excluding the file extension.

Currently this program assumes you run it from Linux.

GCC and binutils should be present on system for this to work.

There must be no empty or white-space lines in *.lst.''')
  ap.add_argument('INPUT',
                  help='Path to a text list file containing function names to bind')
  args = ap.parse_args()

  if not args.INPUT.endswith('.lst'):
    ap.error('unrecognized input')

  dllname = os.path.basename(args.INPUT).removesuffix('.lst')

  with tempfile.TemporaryDirectory() as tmpdir:
    with open(os.path.join(tmpdir, 'resp'), 'w') as rspf:
      script_parent_dir = os.path.abspath(os.path.dirname(__file__))
      fl = ['-DBUILDING_STUB_LIB', '-include',
            os.path.join(script_parent_dir, '_snowin.h'),
            '-Wno-builtin-declaration-mismatch']

      with open(args.INPUT, 'r') as f:
        for i, fnname in enumerate(map(lambda x: x.removesuffix('\n'), f)):
          stub_src = f'{i}.c'

          with open(os.path.join(tmpdir, stub_src), 'w') as cf:
            cf.write(f'DEFINE_FUNC({dllname}, {fnname})\n')

          _cc([*fl, stub_src], cwd=tmpdir)
          rspf.write(f'{i}.o\n')

      with open(os.path.join(tmpdir, 's.c'), 'w') as cf:
        cf.write(f'DEFINE_LBRY({dllname})\n')

      _cc([*fl, 's.c'], cwd=tmpdir)
      rspf.write('s.o\n')

    out_dir = os.path.dirname(args.INPUT)
    out = os.path.join(out_dir, f'lib{dllname}.a')

    if os.path.exists(out):
      os.remove(out)

    _cmd(['ar', 'rcs', os.path.abspath(out),
          '@resp'], cwd=tmpdir)

main()
