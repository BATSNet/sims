"""Wrapper to run idf.py from MSYS2/Git Bash by cleaning env."""
import sys
import os

# Remove MSYS/MinGW env vars that block idf.py
for k in list(os.environ.keys()):
    if 'MSYS' in k or 'MINGW' in k:
        del os.environ[k]

# Ensure platform detection works
if 'PROCESSOR_ARCHITECTURE' not in os.environ:
    os.environ['PROCESSOR_ARCHITECTURE'] = 'AMD64'

# Set IDF paths
idf_path = os.environ.get('IDF_PATH', r'C:\Users\pp\esp\v5.5-rc1\esp-idf')
tools_base = os.environ.get('IDF_TOOLS_PATH', os.path.expanduser('~\\.espressif'))
os.environ['IDF_PATH'] = idf_path
os.environ['IDF_TOOLS_PATH'] = tools_base

# Ensure all ESP-IDF tools are on PATH with Windows-style paths
tool_paths = [
    os.path.join(tools_base, 'python_env', 'idf5.5_py3.11_env', 'Scripts'),
    os.path.join(tools_base, 'tools', 'xtensa-esp-elf', 'esp-14.2.0_20251107', 'xtensa-esp-elf', 'bin'),
    os.path.join(tools_base, 'tools', 'riscv32-esp-elf', 'esp-14.2.0_20251107', 'riscv32-esp-elf', 'bin'),
    os.path.join(tools_base, 'tools', 'cmake', '3.30.2', 'bin'),
    os.path.join(tools_base, 'tools', 'ninja', '1.12.1'),
    os.path.join(tools_base, 'tools', 'ccache', '4.11.2', 'ccache-4.11.2-windows-x86_64'),
    r'C:\WINDOWS\system32',
    r'C:\WINDOWS',
]
os.environ['PATH'] = os.pathsep.join(tool_paths) + os.pathsep + os.environ.get('PATH', '')

# Forward argv
sys.argv = ['idf.py'] + sys.argv[1:]

# Import and run
sys.path.insert(0, os.path.join(idf_path, 'tools'))
from idf import main
main()
