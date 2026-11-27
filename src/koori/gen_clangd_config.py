#!/usr/bin/env python
import argparse
import subprocess
import tempfile
import os
import os.path

if os.name == 'nt':
  _WSL = ['wsl.exe']
  _WSLPATH = 'wslpath'
  _WSLPATH_M = [_WSLPATH, '-m']
else:
  _WSL = []
  _WSLPATH = 'echo'
  _WSLPATH_M = [_WSLPATH]

def _wsl_cmd_common(x: list[str]) -> str:
  return subprocess.run(_WSL + x, check=True,
                        stdout=subprocess.PIPE,
                        text=True).stdout.removesuffix('\n')

def main():
  ap = argparse.ArgumentParser(description='''\
Generate a Clangd configuration for koori that is
 suitable for development under Windows with WSL toolchain.

This will call `winegcc` in WSL to extract CFLAGS and includes.

Output will contain path to Wine, Linux and MSVCRT headers.

This script requires default WSL distribution to be set.

Clang must be installed in WSL to extract triple.

You must run this script from Windows, not from WSL.

Note this script does not check if Linux headers are installed in WSL.

Possible to run this script under Linux if you use your IDE under Linux.''')
  ap.add_argument('-o', '--output', type=argparse.FileType('w'),
                  help='path to .clangd to be created')
  args = ap.parse_args()

  script_parent_dir = os.path.dirname(__file__)

  if args.output is None:
    default_out_path = os.path.join(script_parent_dir, '.clangd')
    args.output = open(default_out_path, 'w')

  clang_triple = _wsl_cmd_common(['clang', '-print-target-triple'])

  with tempfile.TemporaryDirectory() as tmpdir:
    c_src_winpath = os.path.join(tmpdir, 'dummy.c')
    with open(c_src_winpath, 'w'): pass

    c_src_wslpath = _wsl_cmd_common([_WSLPATH, c_src_winpath])
    dummy_out_path = f'{c_src_wslpath}.o'

    winegcc_log_lines = _wsl_cmd_common(['winegcc',
                                         '-v', '-c',
                                         '-mno-cygwin',
                                         c_src_wslpath,
                                         '-o',
                                         dummy_out_path]).splitlines()

    if len(winegcc_log_lines) > 1:
      raise ValueError('output is not a single line')

    winegcc_cmdline = winegcc_log_lines[0].split()

    if winegcc_cmdline[0].split('/')[-1] != 'gcc':
      raise ValueError('output is not a gcc cmdline')

    common_inc_dir = os.path.join(script_parent_dir, '..', '..', 'inc')

    args.output.write(f'''\
CompileFlags:
  Add:
    - -target
    - {clang_triple}
    - -I{os.path.abspath(common_inc_dir)}
''')

    for winegcc_arg in winegcc_cmdline:
      if winegcc_arg.startswith(('-D', '-f', '-m')):
        args.output.write(f'    - {winegcc_arg}\n')
      elif winegcc_arg.startswith('-isystem'):
        inc_wslpath = winegcc_arg.removeprefix('-isystem')
        inc_winpath = _wsl_cmd_common([*_WSLPATH_M, inc_wslpath])
        args.output.write(f'    - -isystem{inc_winpath}\n')

    sys_inc = _wsl_cmd_common([*_WSLPATH_M, '/usr/include'])
    args.output.write(f'    - -isystem{sys_inc}\n')

main()
