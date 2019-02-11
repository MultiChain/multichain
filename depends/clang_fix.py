import itertools
import logging
import shutil
import sys
from argparse import ArgumentParser, FileType

from pathlib2 import Path

module_name = Path(__file__).stem
logger = logging.getLogger(module_name)

TEMPORARY_FILE = "temporary_file"


def process_config_file(options):
    lines = zip(itertools.count(), options.input.readlines())
    default_warnings = next(idx for idx, line in lines if line.startswith('config("default_warnings") {'))
    in_clang_block = next(idx for idx, line in lines[default_warnings:] if line.startswith('  if (is_clang) {'))
    cflags_block = next(idx for idx, line in lines[in_clang_block:] if line.startswith('    cflags += ['))
    lines[cflags_block+1:cflags_block+1] = [
        (0, '      "-Wno-defaulted-function-deleted",\n'),
        (0, '      "-Wno-null-pointer-arithmetic",\n'),
        (0, '      "-Wno-class-memaccess",\n'),
    ]

    if options.inplace:
        with open(TEMPORARY_FILE, 'w') as f:
            for idx, line in lines:
                f.write(line)
        shutil.move(options.input.name, options.input.name + options.inplace)
        shutil.move(TEMPORARY_FILE, options.input.name)
    else:
        for idx, line in lines:
            print(line.rstrip())


def get_options():
    parser = ArgumentParser(description="Add clang flags to GN compiler config file")
    parser.add_argument("-i", "--inplace", metavar="EXT", action="store", const=".bak", nargs="?", default="",
                        help="replace file (default: '%(const)s' if given, output to stdout if not)")
    parser.add_argument("input", type=FileType(), nargs="?", default="build/config/compiler/BUILD.gn",
                        help="GN config file name (default: %(default)s)")

    options = parser.parse_args()

    logger.info("{} - {}".format(module_name, parser.description))
    logger.info("  In-place: {!r}".format(options.inplace))
    logger.info("  In-file:  {!r}".format(options.input.name))

    return options


def main():
    logging.basicConfig(stream=sys.stdout, level=logging.INFO, format="%(asctime) %(levelname)-7s %(message)s")
    options = get_options()
    process_config_file(options)
    return 0


if __name__ == '__main__':
    sys.exit(main())
