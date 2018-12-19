import distutils.spawn
import logging
import os
import sys
from argparse import ArgumentParser
from itertools import chain
from subprocess import call

from pathlib2 import Path

module_name = Path(__file__).stem
logger = logging.getLogger("module_name")


def to_posix(path):
    parts = path.parts[1:]
    full_path = Path("/") / "mnt" / "c" / '/'.join(parts)
    return full_path.as_posix()


def check_dependencies():
    if not distutils.spawn.find_executable("bash.exe"):
        logger.error("Cannot find bash.exe")
        return False
    return True


def create_object(options, bin_file):
    out_file = (options.output / bin_file.stem).with_suffix(options.object_ext)
    if sys.platform == "win32":
        out_file = to_posix(out_file)
    cmd = "objcopy -I binary -O pe-x86-64 -B i386 {} {}".format(bin_file.name, out_file)
    logger.info(cmd)
    call(["bash", "-c", cmd])
    return out_file


def process_bin_files(options):
    out_files = []
    for f in chain(options.root.glob("*.bin"), options.root.glob("*.dat")):
        out_file = create_object(options, f)
        out_files.append(out_file)
    lib_file = options.output / options.lib_name
    cmd = "x86_64-w64-mingw32-ar -r {} {}".format(to_posix(lib_file), ' '.join(["'{}'".format(f) for f in out_files]))
    logger.info(cmd)
    call(["bash", "-c", cmd])


def get_options():
    parser = ArgumentParser(description="Embed V8 snapshots in v8_data.lib")
    parser.add_argument("-r", "--root", metavar="DIR", required=True,
                        help="V8 root build directory (e.g: .../out.gn/x86.release")
    parser.add_argument("-o", "--output", metavar="DIR", help="Output directory for library")

    options = parser.parse_args()

    options.root = Path(options.root)
    if not options.root.exists():
        parser.error("Root path {} does not exists".format(options.root))
    options.root = options.root.resolve()

    options.output = Path(options.output) if options.output else (options.root / "obj")
    if not options.output.exists():
        parser.error("Output path {} does not exists".format(options.output))
    options.output = options.output.resolve()

    if sys.platform == "win32":
        options.object_ext = ".obj"
        options.lib_name = "v8_data.lib"
    else:
        options.object_ext = ".o"
        options.lib_name = "libv8_data.a"

    logger.info("{} - {}".format(module_name, parser.description or ""))
    logger.info("  Root dir:   {}".format(options.root))
    logger.info("  Output dir: {}".format(options.output))

    return options


def main():
    logging.basicConfig(stream=sys.stdout, level=logging.INFO, format="%(asctime)s %(levelname)-7s %(message)s")
    options = get_options()

    if not check_dependencies():
        return 1

    os.chdir(str(options.root))
    process_bin_files(options)
    return 0


if __name__ == '__main__':
    sys.exit(main())
