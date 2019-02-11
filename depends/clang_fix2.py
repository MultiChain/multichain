import itertools
import json
import logging
import os
import shutil
import sys
from argparse import ArgumentParser, FileType

from pathlib2 import Path

module_name = Path(__file__).stem
logger = logging.getLogger(module_name)

TEMPORARY_FILE = "temporary_file"


def process_config_file(options, spec):
    logger.debug("process_config_file {}".format(spec["file"]))
    config_name = str(spec["config"])
    lines = zip(itertools.count(), [line.rstrip() for line in open(spec["file"]).readlines()])
    config = next(idx for idx, line in lines if line.startswith('config("{}") {{'.format(config_name)))
    in_clang_block = next(idx for idx, line in lines[config:] if line.startswith('  if (is_clang) {'))
    cflags_block = next(idx for idx, line in lines[in_clang_block:] if line.startswith('    cflags += ['))
    lines[cflags_block+1:cflags_block+1] = [(0, '      "{}",'.format(flag)) for flag in spec["flags"]]

    if options.inplace:
        with open(TEMPORARY_FILE, 'w') as f:
            for idx, line in lines:
                f.write(line + '\n')
        shutil.move(spec["file"], spec["file"] + options.inplace)
        shutil.move(TEMPORARY_FILE, spec["file"])
    else:
        for idx, line in lines:
            print(line.rstrip())


def get_options():
    mc_path_default = str(Path.home() / "multichain")
    parser = ArgumentParser(description="Add clang flags to GN compiler config file")
    parser.add_argument("-v", "--verbose", action="store_true", help="write debug messages to log")
    parser.add_argument("-m", "--multichain", metavar="DIR", default=mc_path_default,
                        help="MultiChain path prefix (default: %(default)s)")
    parser.add_argument("-i", "--inplace", metavar="EXT", action="store", const=".bak", nargs="?", default="",
                        help="replace file (default: '%(const)s' if given, output to stdout if not)")
    parser.add_argument("-c", "--config", metavar="FILE", default="clang_fix.config",
                        help="configuration file name (default: %(default)s)")

    options = parser.parse_args()

    if options.verbose:
        logger.setLevel(logging.DEBUG)

    if not Path(options.multichain).exists():
        parser.error("{!r}: MultiChain path does not exist".format(options.multichain))

    config_file = Path(options.config)
    if not config_file.is_absolute():
        options.config = str(Path(__file__).parent / config_file)

    logger.info("{} - {}".format(module_name, parser.description))
    logger.info("  MultiChain: {!r}".format(options.multichain))
    logger.info("  In-place:   {!r}".format(options.inplace))
    logger.info("  Config:     {!r}".format(options.config))

    return options


def main():
    logging.basicConfig(stream=sys.stdout, level=logging.INFO, format="%(asctime)s %(levelname)-7s %(message)s")
    options = get_options()
    specs = json.load(open(options.config))
    os.chdir(str(Path(options.multichain) / "v8build" / "v8"))
    for spec in specs:
        process_config_file(options, spec)
    return 0


if __name__ == '__main__':
    sys.exit(main())
