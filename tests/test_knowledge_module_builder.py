import argparse
import importlib.util
import json
import zipfile
from pathlib import Path

import pytest


SCRIPT = Path(__file__).parents[1] / "scripts" / "knowledge" / "build_knowledge_module.py"
SPEC = importlib.util.spec_from_file_location("knowledge_builder", SCRIPT)
assert SPEC and SPEC.loader
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)


def arguments(tmp_path: Path, records: Path, **overrides) -> argparse.Namespace:
    values = {
        "type": "icd11",
        "input": records,
        "output": tmp_path / "module.dmcknowledge",
        "id": "who.icd11.test.en",
        "name": "ICD-11 test",
        "version": "2026-01",
        "language": "en",
        "jurisdiction": "international",
        "source_name": "World Health Organization (WHO)",
        "source_url": builder.WHO_SOURCE_URL,
        "license_name": builder.WHO_LICENSE_NAME,
        "license_url": builder.WHO_LICENSE_URL,
        "attribution": builder.WHO_ATTRIBUTION,
        "confirm_unmodified_official_content": True,
    }
    values.update(overrides)
    return argparse.Namespace(**values)


def write_records(path: Path, uri: str = "https://id.who.int/icd/entity/123") -> None:
    path.write_text(
        json.dumps({"id": "123", "code": "1A00", "title": "Cholera", "uri": uri}) + "\n",
        encoding="utf-8",
    )


def test_builds_deterministic_compliant_icd11_package(tmp_path: Path) -> None:
    records = tmp_path / "records.jsonl"
    write_records(records)
    first = arguments(tmp_path, records)
    builder.build_package(first)
    first_bytes = first.output.read_bytes()
    builder.build_package(first)
    assert first.output.read_bytes() == first_bytes

    with zipfile.ZipFile(first.output) as archive:
        manifest = json.loads(archive.read("manifest.json"))
        output_record = json.loads(archive.read("records.jsonl"))
    assert manifest["officialContentUnmodified"] is True
    assert manifest["licenseName"] == "CC BY-ND 3.0 IGO"
    assert manifest["recordCount"] == 1
    assert output_record["title"] == "Cholera"
    assert output_record["uri"].startswith("https://id.who.int/")


@pytest.mark.parametrize(
    ("override", "message"),
    [
        ({"confirm_unmodified_official_content": False}, "confirm-unmodified"),
        ({"source_url": "https://example.com/icd11"}, "who.int"),
        ({"license_name": "MIT"}, "CC BY-ND 3.0 IGO"),
    ],
)
def test_rejects_noncompliant_icd11_metadata(
    tmp_path: Path, override: dict, message: str
) -> None:
    records = tmp_path / "records.jsonl"
    write_records(records)
    with pytest.raises(ValueError, match=message):
        builder.build_package(arguments(tmp_path, records, **override))


def test_rejects_non_who_record_uri(tmp_path: Path) -> None:
    records = tmp_path / "records.jsonl"
    write_records(records, "https://example.com/entity/123")
    with pytest.raises(ValueError, match="id.who.int"):
        builder.build_package(arguments(tmp_path, records))


def test_generic_module_requires_its_own_provenance(tmp_path: Path) -> None:
    records = tmp_path / "records.jsonl"
    write_records(records)
    args = arguments(
        tmp_path,
        records,
        type="generic",
        source_name=None,
        source_url=None,
        license_name=None,
        license_url=None,
        attribution=None,
        confirm_unmodified_official_content=False,
    )
    with pytest.raises(ValueError, match="Generic modules require"):
        builder.build_package(args)
