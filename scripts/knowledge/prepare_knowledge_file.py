#!/usr/bin/env python3
"""Convert a lawfully obtained official source file into .dmcknowledge.

This converter never scrapes or bypasses a login. It only processes a file the
user already obtained from the publisher and keeps source/licence metadata in
the resulting package.
"""

from __future__ import annotations

import argparse
import csv
import io
import json
import re
import sys
import xml.etree.ElementTree as ET
import zipfile
from collections import defaultdict
from datetime import date
from pathlib import Path
from typing import Any, Iterable, Iterator

from build_knowledge_module import ZIP_TIMESTAMP, zip_entry
from knowledge_runtime import load_catalog

MAX_SOURCE_BYTES = 2 * 1024 * 1024 * 1024
MAX_ZIP_ENTRIES = 20_000
MAX_RECORDS = 2_000_000
SOURCE_KINDS = {
    "bfarm.icd10gm": "icd10",
    "who.icd11": "icd11",
    "bfarm.ops": "ops",
    "bfarm.icf": "icf",
}
SOURCE_LEGAL = {
    "bfarm.icd10gm": (
        "BfArM download terms",
        "Herausgegeben vom Bundesinstitut für Arzneimittel und Medizinprodukte "
        "(BfArM) im Auftrag des Bundesministeriums für Gesundheit (BMG).",
    ),
    "bfarm.ops": (
        "BfArM download terms",
        "Herausgegeben vom Bundesinstitut für Arzneimittel und Medizinprodukte "
        "(BfArM) im Auftrag des Bundesministeriums für Gesundheit (BMG).",
    ),
    "bfarm.alphaidse": (
        "BfArM download terms",
        "Quelle: Bundesinstitut für Arzneimittel und Medizinprodukte (BfArM), Alpha-ID-SE.",
    ),
    "bfarm.icf": (
        "BfArM download terms",
        "Quelle: Bundesinstitut für Arzneimittel und Medizinprodukte (BfArM), "
        "deutsche ICF-Fassung.",
    ),
    "who.icd11": (
        "CC BY-ND 3.0 IGO",
        "ICD-11 content: World Health Organization. Licensed under "
        "CC BY-ND 3.0 IGO; official identifiers and titles are unchanged.",
    ),
    "orphadata.nomenclature": (
        "CC BY 4.0",
        "Orphadata: Free access data from Orphanet, INSERM 1999. "
        "Licensed under CC BY 4.0.",
    ),
    "loinc.core": (
        "LOINC License",
        "This material contains content from LOINC (http://loinc.org). "
        "LOINC is copyright Regenstrief Institute, Inc. and the LOINC Committee "
        "and is available at no cost under the LOINC License.",
    ),
    "snomed.international": (
        "SNOMED CT Affiliate License Agreement",
        "This module contains SNOMED CT content. Use is subject to the user's "
        "valid territory-specific SNOMED CT affiliate licence.",
    ),
}


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1].rsplit(":", 1)[-1]


def text_content(element: ET.Element | None) -> str:
    if element is None:
        return ""
    return re.sub(r"\s+", " ", " ".join(element.itertext())).strip()


def first_descendant(element: ET.Element, name: str) -> ET.Element | None:
    return next((child for child in element.iter() if local_name(child.tag) == name), None)


def iter_claml(stream: io.BufferedIOBase) -> Iterator[dict[str, Any]]:
    for _, element in ET.iterparse(stream, events=("end",)):
        if local_name(element.tag) != "Class":
            continue
        code = element.attrib.get("code", "").strip()
        if not code:
            element.clear()
            continue
        rubrics: dict[str, list[str]] = defaultdict(list)
        parent = ""
        children: list[str] = []
        for child in element:
            child_name = local_name(child.tag)
            if child_name == "SuperClass":
                parent = child.attrib.get("code", "").strip()
            elif child_name == "SubClass":
                value = child.attrib.get("code", "").strip()
                if value:
                    children.append(value)
            elif child_name == "Rubric":
                kind = child.attrib.get("kind", "other").strip().lower()
                label = first_descendant(child, "Label")
                value = text_content(label)
                if value:
                    rubrics[kind].append(value)
        title = next(
            (
                values[0]
                for key in ("preferred", "preferredlong", "text", "other")
                if (values := rubrics.get(key))
            ),
            code,
        )
        definition = " | ".join(rubrics.get("definition", []) + rubrics.get("note", []))
        inclusions = " | ".join(rubrics.get("inclusion", []))
        exclusions = " | ".join(rubrics.get("exclusion", []))
        yield {
            "id": code,
            "code": code,
            "title": title,
            "definition": definition,
            "inclusions": inclusions,
            "exclusions": exclusions,
            "parents": parent,
            "children": " ".join(children),
        }
        element.clear()


def select_zip_member(archive: zipfile.ZipFile, source_id: str) -> zipfile.ZipInfo:
    files = [entry for entry in archive.infolist() if not entry.is_dir()]
    if len(files) > MAX_ZIP_ENTRIES:
        raise ValueError("Source ZIP contains too many entries")
    if source_id == "loinc.core":
        matches = [
            entry for entry in files
            if entry.filename.replace("\\", "/").lower().endswith("/loinccore.csv")
            or entry.filename.replace("\\", "/").lower().endswith("/loinc.csv")
        ]
        if matches:
            return sorted(matches, key=lambda entry: ("core" not in entry.filename.lower(), entry.filename))[0]
    if source_id == "snomed.international":
        matches = [
            entry for entry in files
            if "description_" in entry.filename.lower()
            and "snapshot" in entry.filename.lower()
            and entry.filename.lower().endswith(".txt")
        ]
        if matches:
            return sorted(matches, key=lambda entry: entry.filename)[0]
    if source_id == "orphadata.nomenclature":
        matches = [
            entry for entry in files
            if entry.filename.lower().endswith(".xml")
            and any(token in entry.filename.lower() for token in ("product1", "nomenclature"))
        ]
        if matches:
            return sorted(matches, key=lambda entry: entry.filename)[0]
    matches = [
        entry for entry in files
        if entry.filename.lower().endswith((".xml", ".csv", ".txt", ".jsonl"))
    ]
    if not matches:
        raise ValueError("No supported source file was found inside the ZIP")
    for entry in matches:
        with archive.open(entry) as stream:
            prefix = stream.read(16_384).decode("utf-8", errors="ignore")
        if "<ClaML" in prefix or ":ClaML" in prefix:
            return entry
    return sorted(matches, key=lambda entry: entry.filename)[0]


def decode_table(payload: bytes) -> str:
    for encoding in ("utf-8-sig", "utf-16", "cp1252", "latin-1"):
        try:
            return payload.decode(encoding)
        except UnicodeDecodeError:
            continue
    raise ValueError("Could not decode table")


def detect_dialect(text: str) -> csv.Dialect:
    sample = text[:64_000]
    try:
        return csv.Sniffer().sniff(sample, delimiters=";,\t|")
    except csv.Error:
        return csv.excel_tab


def normalized_header(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", value.lower())


def first_value(row: dict[str, str], candidates: Iterable[str]) -> str:
    normalized = {normalized_header(key): value for key, value in row.items()}
    for candidate in candidates:
        value = normalized.get(normalized_header(candidate), "")
        if value and value.strip():
            return value.strip()
    return ""


def iter_table(payload: bytes, source_id: str) -> Iterator[dict[str, Any]]:
    text = decode_table(payload)
    reader = csv.DictReader(io.StringIO(text), dialect=detect_dialect(text))
    if not reader.fieldnames:
        raise ValueError("Table has no header")
    for row_number, row in enumerate(reader, 2):
        if source_id == "loinc.core":
            code = first_value(row, ("LOINC_NUM",))
            title = first_value(row, ("LONG_COMMON_NAME", "SHORTNAME", "COMPONENT"))
            definition = " | ".join(
                value for value in (
                    first_value(row, ("COMPONENT",)),
                    first_value(row, ("PROPERTY",)),
                    first_value(row, ("TIME_ASPCT",)),
                    first_value(row, ("SYSTEM",)),
                    first_value(row, ("SCALE_TYP",)),
                    first_value(row, ("METHOD_TYP",)),
                ) if value
            )
            copyright_notice = first_value(row, ("EXTERNAL_COPYRIGHT_NOTICE",))
            metadata = {
                "status": first_value(row, ("STATUS",)),
                "externalCopyrightNotice": copyright_notice,
            }
        else:
            code = first_value(
                row,
                ("code", "kode", "icd10gm", "icd10", "alphaid", "alpha-id", "orpha", "id"),
            )
            title = first_value(
                row,
                ("title", "name", "bezeichnung", "diagnosetext", "text", "term", "long_common_name"),
            )
            definition = first_value(row, ("definition", "beschreibung", "description"))
            metadata = {key: value for key, value in row.items() if value}
        if not code and not title:
            continue
        if not title:
            title = code
        yield {
            "id": code or f"row-{row_number}",
            "code": code,
            "title": title,
            "definition": definition,
            "metadata": metadata,
        }


def iter_orphadata(stream: io.BufferedIOBase) -> Iterator[dict[str, Any]]:
    for _, element in ET.iterparse(stream, events=("end",)):
        if local_name(element.tag) != "Disorder":
            continue
        code = text_content(first_descendant(element, "OrphaCode"))
        title = text_content(first_descendant(element, "Name"))
        if code and title:
            disorder_type = first_descendant(element, "DisorderType")
            disorder_group = first_descendant(element, "DisorderGroup")
            yield {
                "id": f"ORPHA:{code}",
                "code": f"ORPHA:{code}",
                "title": title,
                "definition": " | ".join(
                    value for value in (
                        text_content(first_descendant(disorder_type, "Name")) if disorder_type is not None else "",
                        text_content(first_descendant(disorder_group, "Name")) if disorder_group is not None else "",
                    ) if value
                ),
                "uri": f"https://www.orpha.net/en/disease/detail/{code}",
            }
        element.clear()


def iter_snomed(payload: bytes) -> Iterator[dict[str, Any]]:
    text = decode_table(payload)
    reader = csv.DictReader(io.StringIO(text), delimiter="\t")
    preferred: dict[str, tuple[int, dict[str, Any]]] = {}
    for row in reader:
        if row.get("active") != "1":
            continue
        concept_id = row.get("conceptId", "").strip()
        term = row.get("term", "").strip()
        if not concept_id or not term:
            continue
        type_id = row.get("typeId", "")
        priority = 2 if type_id == "900000000000013009" else 1
        current = preferred.get(concept_id)
        if current is None or priority > current[0]:
            preferred[concept_id] = (
                priority,
                {
                    "id": concept_id,
                    "code": concept_id,
                    "title": term,
                    "uri": f"http://snomed.info/id/{concept_id}",
                    "metadata": {
                        "effectiveTime": row.get("effectiveTime", ""),
                        "languageCode": row.get("languageCode", ""),
                        "typeId": type_id,
                    },
                },
            )
    for concept_id in sorted(preferred):
        yield preferred[concept_id][1]


def iter_jsonl(payload: bytes) -> Iterator[dict[str, Any]]:
    for line_number, line in enumerate(payload.decode("utf-8").splitlines(), 1):
        if not line.strip():
            continue
        record = json.loads(line)
        if not isinstance(record, dict):
            raise ValueError(f"JSONL line {line_number} is not an object")
        if not record.get("title"):
            raise ValueError(f"JSONL line {line_number} has no title")
        record.setdefault("id", record.get("code") or f"line-{line_number}")
        record.setdefault("code", "")
        yield record


def read_records(path: Path, source_id: str) -> list[dict[str, Any]]:
    if path.stat().st_size > MAX_SOURCE_BYTES:
        raise ValueError("Source file exceeds the 2 GiB conversion limit")
    suffix = path.suffix.lower()
    if suffix == ".zip":
        with zipfile.ZipFile(path) as archive:
            member = select_zip_member(archive, source_id)
            if member.file_size > MAX_SOURCE_BYTES:
                raise ValueError("Expanded source file exceeds the 2 GiB conversion limit")
            with archive.open(member) as source:
                payload = source.read()
            member_name = member.filename.lower()
    else:
        payload = path.read_bytes()
        member_name = path.name.lower()

    if source_id == "snomed.international":
        records = list(iter_snomed(payload))
    elif source_id == "orphadata.nomenclature":
        records = list(iter_orphadata(io.BytesIO(payload)))
    elif member_name.endswith(".jsonl"):
        records = list(iter_jsonl(payload))
    elif member_name.endswith((".csv", ".txt")):
        records = list(iter_table(payload, source_id))
    elif member_name.endswith(".xml"):
        prefix = payload[:32_768].decode("utf-8", errors="ignore")
        if "<ClaML" in prefix or ":ClaML" in prefix:
            records = list(iter_claml(io.BytesIO(payload)))
        elif source_id == "orphadata.nomenclature":
            records = list(iter_orphadata(io.BytesIO(payload)))
        else:
            raise ValueError("XML format is not recognized for the selected source")
    else:
        raise ValueError("Unsupported source format")
    if not records:
        raise ValueError("No knowledge records were found")
    if len(records) > MAX_RECORDS:
        raise ValueError("Converted record count exceeds the safety limit")
    return records


def infer_version(path: Path) -> str:
    match = re.search(r"20[0-9]{2}(?:[-_.][0-9]{1,2})?", path.name)
    return match.group(0).replace("_", "-").replace(".", "-") if match else date.today().isoformat()


def package_records(
    records: list[dict[str, Any]],
    output: Path,
    source: dict[str, Any],
    version: str,
    language: str,
    jurisdiction: str,
) -> None:
    license_name, attribution = SOURCE_LEGAL.get(
        source["id"],
        (
            f"{source['publisher']} source-specific terms",
            f"Source: {source['publisher']}. See {source['officialUrl']} and preserve record-level rights.",
        ),
    )
    module_id = f"{source['id']}.{language}".lower()
    manifest = {
        "formatVersion": 1,
        "id": module_id,
        "name": source["name"],
        "type": SOURCE_KINDS.get(source["id"], "generic"),
        "version": version,
        "language": language,
        "jurisdiction": jurisdiction,
        "sourceName": source["publisher"],
        "sourceUrl": source["officialUrl"],
        "licenseName": license_name,
        "licenseUrl": source["licenseUrl"],
        "attributionText": attribution,
        "officialContentUnmodified": True,
        "recordCount": len(records),
    }
    manifest_bytes = (
        json.dumps(manifest, ensure_ascii=False, sort_keys=True, indent=2) + "\n"
    ).encode("utf-8")
    records_bytes = "".join(
        json.dumps(record, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
        for record in records
    ).encode("utf-8")
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w") as archive:
        for name, payload in (("manifest.json", manifest_bytes), ("records.jsonl", records_bytes)):
            info, content = zip_entry(name, payload)
            archive.writestr(info, content)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Prepare an official local source file for Android and Windows DMC"
    )
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version")
    parser.add_argument("--language", default="de")
    parser.add_argument("--jurisdiction", default="DE")
    parser.add_argument(
        "--confirm-lawful-source",
        action="store_true",
        help="Confirm that the file was obtained lawfully and its current terms were reviewed.",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        if not args.confirm_lawful_source:
            raise ValueError("--confirm-lawful-source is required")
        catalog = load_catalog(args.catalog)
        source = next(
            (item for item in catalog["sources"] if item["id"] == args.source_id), None
        )
        if source is None:
            raise ValueError(f"Unknown source id: {args.source_id}")
        records = read_records(args.input, args.source_id)
        package_records(
            records,
            args.output,
            source,
            args.version or infer_version(args.input),
            args.language,
            args.jurisdiction,
        )
    except (OSError, ValueError, json.JSONDecodeError, ET.ParseError, csv.Error) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    print(f"Created {args.output} with {len(records)} records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
