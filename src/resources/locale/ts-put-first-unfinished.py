#!/usr/bin/env python3

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import sys
from xml.parsers import expat
from xml.sax.saxutils import escape


ENCODING_RE = re.compile(br"<\?xml[^>]*\bencoding\s*=\s*(['\"])([^'\"]+)\1")
UNFINISHED_TYPE_RE = re.compile(
    br"\s+type\s*=\s*(?P<quote>['\"])unfinished(?P=quote)"
)


class FoundTranslation(Exception):
    pass


@dataclass
class TranslationSpan:
    start: int
    start_tag_end: int
    end_tag_start: int
    end: int
    self_closing: bool


def local_name(name):
    return name.rsplit(":", 1)[-1].rsplit("}", 1)[-1]


def detect_encoding(data):
    match = ENCODING_RE.search(data[:256])
    if match is None:
        return "utf-8"

    return match.group(2).decode("ascii")


def find_tag_end(data, start):
    quote = None

    for index in range(start, len(data)):
        byte = data[index]
        if quote is not None:
            if byte == quote:
                quote = None
        elif byte == ord('"') or byte == ord("'"):
            quote = byte
        elif byte == ord(">"):
            return index + 1

    raise ValueError("unterminated XML tag")


def is_self_closing_tag(start_tag):
    return start_tag.rstrip().endswith(b"/>")


def remove_unfinished_type(start_tag):
    return UNFINISHED_TYPE_RE.sub(b"", start_tag, count=1)


def open_translation_tag(start_tag):
    start_tag = remove_unfinished_type(start_tag)
    stripped = start_tag.rstrip()

    if stripped.endswith(b"/>"):
        return stripped[:-2].rstrip() + b">"

    return start_tag


def first_unfinished_translation_span(data):
    parser = expat.ParserCreate()
    stack = []
    span = None

    def start_element(name, attrs):
        nonlocal span

        name = local_name(name)
        if (
            name == "translation"
            and stack
            and stack[-1] == "message"
            and attrs.get("type") == "unfinished"
        ):
            start = parser.CurrentByteIndex
            start_tag_end = find_tag_end(data, start)
            start_tag = data[start:start_tag_end]
            self_closing = is_self_closing_tag(start_tag)
            span = TranslationSpan(
                start=start,
                start_tag_end=start_tag_end,
                end_tag_start=start_tag_end,
                end=start_tag_end,
                self_closing=self_closing,
            )

            if self_closing:
                raise FoundTranslation

        stack.append(name)

    def end_element(name):
        nonlocal span

        name = local_name(name)
        if (
            span is not None
            and name == "translation"
            and stack
            and stack[-1] == "translation"
        ):
            span.end_tag_start = parser.CurrentByteIndex
            span.end = find_tag_end(data, span.end_tag_start)
            raise FoundTranslation

        stack.pop()

    parser.StartElementHandler = start_element
    parser.EndElementHandler = end_element

    try:
        parser.Parse(data, True)
    except FoundTranslation:
        return span

    return None


def put_first_unfinished(ts_file, translation):
    path = Path(ts_file)
    data = path.read_bytes()
    span = first_unfinished_translation_span(data)

    if span is None:
        return False

    encoding = detect_encoding(data)
    start_tag = data[span.start : span.start_tag_end]
    new_element = (
        open_translation_tag(start_tag)
        + escape(translation).encode(encoding)
        + b"</translation>"
    )
    path.write_bytes(data[: span.start] + new_element + data[span.end :])
    return True


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=(
            "Insert a translation into the first unfinished message in a Qt "
            "Linguist TS file."
        )
    )
    parser.add_argument("ts_file", help="Qt Linguist TS file to update")
    parser.add_argument("translation", help="translation text to insert")
    args = parser.parse_args(argv)

    try:
        put_first_unfinished(args.ts_file, args.translation)
    except (OSError, UnicodeError, ValueError, expat.ExpatError) as error:
        print(f"{parser.prog}: {error}", file=sys.stderr)
        return 2

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
