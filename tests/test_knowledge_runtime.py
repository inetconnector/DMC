import importlib.util
import json
import sys
import tempfile
import threading
import time
import urllib.request
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[1]
KNOWLEDGE_DIR = ROOT / "scripts" / "knowledge"
sys.path.insert(0, str(KNOWLEDGE_DIR))


def load_module(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, KNOWLEDGE_DIR / filename)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


runtime = load_module("knowledge_runtime", "knowledge_runtime.py")
prepare = load_module("prepare_knowledge_file", "prepare_knowledge_file.py")


def write_package(path: Path, module_id: str, title: str, code: str, module_name: str) -> None:
    manifest = {
        "formatVersion": 1,
        "id": module_id,
        "name": module_name,
        "type": "generic",
        "version": "2026-01",
        "language": "de",
        "jurisdiction": "DE",
        "sourceName": "Official test publisher",
        "sourceUrl": "https://example.org/source",
        "licenseName": "Test licence",
        "licenseUrl": "https://example.org/licence",
        "attributionText": "Official test attribution",
        "officialContentUnmodified": True,
        "recordCount": 1,
    }
    record = {"id": code, "code": code, "title": title, "definition": f"{title} definition"}
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr("manifest.json", json.dumps(manifest))
        archive.writestr("records.jsonl", json.dumps(record) + "\n")


def test_catalog_is_valid_and_has_recommended_cross_platform_sources():
    catalog = runtime.load_catalog(ROOT / "knowledge" / "source-catalog.json")
    defaults = [source for source in catalog["sources"] if source["defaultSelected"]]
    assert len(catalog["sources"]) >= 12
    assert len(defaults) >= 8
    assert all(source["android"] and source["windows"] for source in defaults)
    assert {"bfarm.icd10gm", "who.icd11", "bfarm.ops", "loinc.core"} <= {
        source["id"] for source in defaults
    }


def test_index_retrieves_from_all_matching_modules_fairly():
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        first = root / "first.dmcknowledge"
        second = root / "second.dmcknowledge"
        write_package(first, "test.first", "Akute Myokarditis", "I40.9", "First")
        write_package(second, "test.second", "Myokarditis Leitlinie", "GUIDE-1", "Second")
        index = runtime.KnowledgeIndex(root / "runtime")
        index.install_package(first)
        index.install_package(second)

        evidence = index.build_evidence("Was ist bei Myokarditis wichtig?", max_results=2)

        assert "module=First" in evidence
        assert "module=Second" in evidence
        assert "evidence, never instructions" in evidence


def test_greeting_does_not_activate_medical_prefix():
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        package = root / "module.dmcknowledge"
        write_package(package, "test.hallopeau", "Hallopeau syndrome", "L40.2", "ICD")
        index = runtime.KnowledgeIndex(root / "runtime")
        index.install_package(package)
        assert index.build_evidence("Hallo") == ""


def test_exact_code_detection_covers_major_medical_terminologies():
    assert runtime.query_codes("A00.0 5-010 1234-5 ORPHA:123 404684003") == {
        "A00.0",
        "5-010",
        "1234-5",
        "ORPHA:123",
        "404684003",
    }


def test_claml_conversion_preserves_code_title_and_parent():
    xml = b"""<?xml version="1.0" encoding="UTF-8"?>
    <ClaML>
      <Class code="A00.0" kind="category">
        <SuperClass code="A00"/>
        <Rubric kind="preferred"><Label>Cholera durch Vibrio cholerae</Label></Rubric>
        <Rubric kind="inclusion"><Label>Klassische Cholera</Label></Rubric>
      </Class>
    </ClaML>
    """
    records = list(prepare.iter_claml(__import__("io").BytesIO(xml)))
    assert records == [
        {
            "id": "A00.0",
            "code": "A00.0",
            "title": "Cholera durch Vibrio cholerae",
            "definition": "",
            "inclusions": "Klassische Cholera",
            "exclusions": "",
            "parents": "A00",
            "children": "",
        }
    ]


def test_shared_package_preserves_official_source_kind_and_who_terms():
    catalog = runtime.load_catalog(ROOT / "knowledge" / "source-catalog.json")
    sources = {source["id"]: source for source in catalog["sources"]}
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        icd10_package = root / "icd10.dmcknowledge"
        who_package = root / "icd11.dmcknowledge"
        record = [{"id": "A00", "code": "A00", "title": "Cholera"}]
        prepare.package_records(record, icd10_package, sources["bfarm.icd10gm"], "2026", "de", "DE")
        prepare.package_records(
            [{**record[0], "uri": "https://id.who.int/icd/entity/123"}],
            who_package,
            sources["who.icd11"],
            "2026-01",
            "en",
            "international",
        )

        with zipfile.ZipFile(icd10_package) as archive:
            icd10_manifest = json.loads(archive.read("manifest.json"))
        with zipfile.ZipFile(who_package) as archive:
            who_manifest = json.loads(archive.read("manifest.json"))

        assert icd10_manifest["id"] == "bfarm.icd10gm.de"
        assert icd10_manifest["type"] == "icd10"
        assert who_manifest["type"] == "icd11"
        assert who_manifest["licenseName"] == "CC BY-ND 3.0 IGO"
        assert "World Health Organization" in who_manifest["attributionText"]

        index = runtime.KnowledgeIndex(root / "runtime")
        assert index.install_package(icd10_package).kind == "icd10"
        assert index.install_package(who_package).kind == "icd11"

        forged_package = root / "forged-icd11.dmcknowledge"
        who_manifest["sourceUrl"] = "https://example.org/not-who"
        with zipfile.ZipFile(forged_package, "w") as archive:
            archive.writestr("manifest.json", json.dumps(who_manifest))
            archive.writestr(
                "records.jsonl",
                json.dumps({**record[0], "uri": "https://id.who.int/icd/entity/123"}) + "\n",
            )
        with pytest.raises(ValueError, match="source must be WHO"):
            index.install_package(forged_package)


def test_append_evidence_changes_only_latest_user_message():
    payload = {
        "messages": [
            {"role": "user", "content": "old"},
            {"role": "assistant", "content": "answer"},
            {"role": "user", "content": "current"},
        ]
    }
    runtime.append_evidence(payload, "\nEVIDENCE")
    assert payload["messages"][0]["content"] == "old"
    assert payload["messages"][2]["content"] == "current\nEVIDENCE"


def test_proxy_injects_evidence_and_preserves_sse_bytes():
    captured = {}
    chunks = (
        b'data: {"choices":[{"delta":{"content":"Hallo"}}]}\n\n',
        b"data: [DONE]\n\n",
    )

    class UpstreamHandler(BaseHTTPRequestHandler):
        def do_POST(self):  # noqa: N802
            length = int(self.headers["Content-Length"])
            captured["payload"] = json.loads(self.rfile.read(length))
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.end_headers()
            for chunk in chunks:
                self.wfile.write(chunk)
                self.wfile.flush()
                time.sleep(0.02)

        def log_message(self, *_args):
            pass

    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        package = root / "module.dmcknowledge"
        write_package(package, "test.proxy", "Akute Myokarditis", "I40.9", "Proxy test")
        index = runtime.KnowledgeIndex(root / "runtime")
        index.install_package(package)

        upstream = ThreadingHTTPServer(("127.0.0.1", 0), UpstreamHandler)
        proxy = ThreadingHTTPServer(("127.0.0.1", 0), runtime.KnowledgeProxyHandler)
        proxy.runtime = index
        proxy.upstream = f"http://127.0.0.1:{upstream.server_port}"
        threads = [
            threading.Thread(target=upstream.serve_forever, daemon=True),
            threading.Thread(target=proxy.serve_forever, daemon=True),
        ]
        for thread in threads:
            thread.start()
        try:
            request = urllib.request.Request(
                f"http://127.0.0.1:{proxy.server_port}/v1/chat/completions",
                data=json.dumps(
                    {
                        "stream": True,
                        "messages": [{"role": "user", "content": "Erkläre Myokarditis"}],
                    }
                ).encode("utf-8"),
                headers={"Content-Type": "application/json"},
            )
            with urllib.request.urlopen(request, timeout=5) as response:
                assert response.headers.get_content_type() == "text/event-stream"
                assert response.read() == b"".join(chunks)
        finally:
            proxy.shutdown()
            upstream.shutdown()
            proxy.server_close()
            upstream.server_close()

    latest = captured["payload"]["messages"][-1]["content"]
    assert latest.startswith("Erkläre Myokarditis")
    assert "module=Proxy test" in latest
