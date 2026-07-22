#!/usr/bin/env python3
"""Build a deterministic DMC offline-knowledge package from lawful local data."""

from __future__ import annotations

import argparse
import json
import sys
import zipfile
from pathlib import Path
from urllib.parse import urlparse

FORMAT_VERSION = 1
WHO_LICENSE_NAME = "CC BY-ND 3.0 IGO"
WHO_LICENSE_URL = "https://icd.who.int/en/docs/icd11-license.pdf"
WHO_SOURCE_URL = "https://icd.who.int/browse11"
WHO_ATTRIBUTION = (
    "International Classification of Diseases, Eleventh Revision (ICD-11), "
    "World Health Organization (WHO) 2019 https://icd.who.int/browse11. "
    "Licensed under the Creative Commons Attribution-NoDerivatives 3.0 IGO "
    "licence (CC BY-ND 3.0 IGO)."
)
ZIP_TIMESTAMP = (1980, 1, 1, 0, 0, 0)


def official_host(url: str, expected: str) -> bool:
    host = (urlparse(url).hostname or "").lower()
    return host == expected or host.endswith(f".{expected}")


def load_records(path: Path, module_type: str) -> list[dict]:
    records: list[dict] = []
    with path.open("r", encoding="utf-8") as source:
        for line_number, raw_line in enumerate(source, 1):
            if not raw_line.strip():
                continue
            try:
                record = json.loads(raw_line)
            except json.JSONDecodeError as error:
                raise ValueError(f"Invalid JSON on line {line_number}: {error}") from error
            if not isinstance(record, dict):
                raise ValueError(f"Record on line {line_number} is not an object")
            for key in ("id", "code", "title"):
                if not isinstance(record.get(key), str) or not record[key].strip():
                    raise ValueError(f"Record on line {line_number} requires a non-empty string {key}")
            if module_type == "icd11":
                uri = record.get("uri")
                if not isinstance(uri, str) or not official_host(uri, "id.who.int"):
                    raise ValueError(
                        f"ICD-11 record on line {line_number} requires its official id.who.int URI"
                    )
            records.append(record)
    if not records:
        raise ValueError("Input contains no records")
    return records


def zip_entry(name: str, content: bytes) -> tuple[zipfile.ZipInfo, bytes]:
    info = zipfile.ZipInfo(name, ZIP_TIMESTAMP)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o644 << 16
    return info, content


def build_package(args: argparse.Namespace) -> None:
    records = load_records(args.input, args.type)
    if args.type == "icd11":
        source_name = args.source_name or "World Health Organization (WHO)"
        source_url = args.source_url or WHO_SOURCE_URL
        license_name = args.license_name or WHO_LICENSE_NAME
        license_url = args.license_url or WHO_LICENSE_URL
        attribution = args.attribution or WHO_ATTRIBUTION
        if not args.confirm_unmodified_official_content:
            raise ValueError(
                "ICD-11 packaging requires --confirm-unmodified-official-content"
            )
        if not official_host(source_url, "who.int"):
            raise ValueError("ICD-11 source URL must use an official who.int host")
        if not official_host(license_url, "who.int"):
            raise ValueError("ICD-11 license URL must use an official who.int host")
        if WHO_LICENSE_NAME.lower() not in license_name.lower():
            raise ValueError(f"ICD-11 license must retain {WHO_LICENSE_NAME}")
        if "World Health Organization" not in attribution:
            raise ValueError("ICD-11 attribution must name the World Health Organization")
    else:
        values = {
            "--source-name": args.source_name,
            "--source-url": args.source_url,
            "--license-name": args.license_name,
            "--license-url": args.license_url,
            "--attribution": args.attribution,
        }
        missing = [option for option, value in values.items() if not value]
        if missing:
            raise ValueError("Generic modules require " + ", ".join(missing))
        source_name = args.source_name
        source_url = args.source_url
        license_name = args.license_name
        license_url = args.license_url
        attribution = args.attribution

    manifest = {
        "formatVersion": FORMAT_VERSION,
        "id": args.id,
        "name": args.name,
        "type": args.type,
        "version": args.version,
        "language": args.language,
        "jurisdiction": args.jurisdiction,
        "sourceName": source_name,
        "sourceUrl": source_url,
        "licenseName": license_name,
        "licenseUrl": license_url,
        "attributionText": attribution,
        "officialContentUnmodified": bool(args.confirm_unmodified_official_content),
        "recordCount": len(records),
    }
    manifest_bytes = (
        json.dumps(manifest, ensure_ascii=False, sort_keys=True, indent=2) + "\n"
    ).encode("utf-8")
    records_bytes = "".join(
        json.dumps(record, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
        for record in records
    ).encode("utf-8")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w") as archive:
        for name, content in (
            ("manifest.json", manifest_bytes),
            ("records.jsonl", records_bytes),
        ):
            info, payload = zip_entry(name, content)
            archive.writestr(info, payload)


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description=(
            "Package already lawfully obtained, unchanged reference records. "
            "This tool downloads no data and grants no redistribution rights."
        )
    )
    result.add_argument("--type", choices=("icd11", "generic"), required=True)
    result.add_argument("--input", type=Path, required=True)
    result.add_argument("--output", type=Path, required=True)
    result.add_argument("--id", required=True)
    result.add_argument("--name", required=True)
    result.add_argument("--version", required=True)
    result.add_argument("--language", required=True)
    result.add_argument("--jurisdiction", default="international")
    result.add_argument("--source-name")
    result.add_argument("--source-url")
    result.add_argument("--license-name")
    result.add_argument("--license-url")
    result.add_argument("--attribution")
    result.add_argument("--confirm-unmodified-official-content", action="store_true")
    return result


def main() -> int:
    args = parser().parse_args()
    try:
        build_package(args)
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    print(f"Created {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
