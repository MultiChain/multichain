import logging
import os
import sys
from argparse import ArgumentParser
from itertools import chain
from subprocess import call

from pathlib2 import Path

logger = logging.getLogger(str(Path(__file__).stem))


def get_bin_type():
    if sys.platform == "darwin":
        bin_type = "mach-o-x86-64"
    elif sys.platform == "win32":
        bin_type = "pe-x86-64"
    else:
        bin_type = "elf64-x86-64"
    return bin_type


def process_bin_file(filepath):
    logger.debug("process_bin_file(filepath={})".format(filepath))
    assert filepath.exists()
    obj_name = filepath.with_suffix(".o").name
    obj_path = Path("obj") / obj_name
    cmd = ["objcopy", "-B", "i386", "-I", "binary", "-O", get_bin_type(), filepath.name, str(obj_path)]
    logger.info(' '.join(cmd))
    call(cmd)
    return obj_name


def process_bin_files():
    cwd = Path.cwd()
    obj_names = []
    for f in chain(cwd.glob("*.bin"), cwd.glob("*.dat")):
        obj_names.append(process_bin_file(f))
    os.chdir("obj")
    cmd = ["ar", "rvs", "libv8_data.a"]
    cmd += obj_names
    logger.info(' '.join(cmd))
    call(cmd)


def get_options():
    default_path = Path.cwd() / ".." / "v8build" / "v8" / "out.gn" / "x64.release"
    parser = ArgumentParser(description="Build v8_data.lib from .bin and .dat files")
    parser.add_argument("-v", "--verbose", action="store_true", help="print debug messages to log")
    parser.add_argument("-p", "--path", metavar="PATH", type=Path, default=default_path.resolve(),
                        help="path to the V8 build area (default: %(default)s)")
    parser.add_argument("-b", "--binutils", metavar="PATH", type=Path, help="path to binutils")

    options = parser.parse_args()

    if options.verbose:
        logger.setLevel(logging.DEBUG)

    logger.info("{} - {}".format(logger.name, parser.description))
    logger.info("  Path:     {}".format(options.path))
    if options.binutils:
        logger.info("  Binutils: {}".format(options.binutils))

    return options


def main():
    logging.basicConfig(stream=sys.stdout, level=logging.INFO, format="%(asctime)s %(levelname)-7s %(message)s")
    options = get_options()
    if options.binutils:
        os.environ["PATH"] = os.pathsep.join(str(options.binutils / "bin"), os.environ["PATH"])
    os.chdir(str(options.path))
    process_bin_files()
    return 0


if __name__ == '__main__':
    sys.exit(main())
