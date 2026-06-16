#!/usr/bin/env python3

import argparse
import sys
import xml.etree.ElementTree as ET


def local_name(tag):
    return tag.rsplit("}", 1)[-1]


def find_direct_child(element, name):
    for child in element:
        if local_name(child.tag) == name:
            return child
    return None


def element_text(element):
    return "".join(element.itertext())


def first_unfinished_source(ts_file):
    tree = ET.parse(ts_file)

    for message in tree.getroot().iter():
        if local_name(message.tag) != "message":
            continue

        translation = find_direct_child(message, "translation")
        if translation is None or translation.get("type") != "unfinished":
            continue

        source = find_direct_child(message, "source")
        return "" if source is None else element_text(source)

    return None


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Print the first unfinished source message in a Qt Linguist TS file."
    )
    parser.add_argument("ts_file", help="Qt Linguist TS file")
    args = parser.parse_args(argv)

    try:
        source = first_unfinished_source(args.ts_file)
    except (OSError, ET.ParseError) as error:
        print(f"{parser.prog}: {error}", file=sys.stderr)
        return 2

    if source is None:
        return 1

    print(source)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
