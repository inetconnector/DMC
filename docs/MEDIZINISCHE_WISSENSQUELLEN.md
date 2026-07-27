# Medizinische Wissensquellen / Medical Knowledge Sources

## Deutsch

### Ziel und Grundprinzip

InetMind/DMC erweitert ein lokales Sprachmodell um überprüfbares,
versioniertes Fachwissen. Die Quellen werden **nicht in das Modell
eintrainiert**. Stattdessen werden sie lokal indexiert. Bei jeder Frage sucht
die Laufzeit nur passende Datensätze, fügt einen kleinen Quellenblock in den
aktuellen DMC-Kontext ein und lässt das Modell daraus antworten.

Dieser Ansatz hat vier Vorteile:

- Aktualisierungen erfordern kein neues Modelltraining.
- Quelle, Ausgabe, Rechtsraum und URI bleiben nachvollziehbar.
- Daten können einzeln aktiviert, deaktiviert und ersetzt werden.
- Nach dem Import funktionieren Suche und Generierung vollständig offline.

Eine Auswahl im Quellenkatalog ist noch kein Download und keine
Lizenzannahme. Die Daten werden immer direkt beim offiziellen Herausgeber
bezogen. Quellen mit Konto, API-Schlüssel, individueller Dokumentlizenz oder
Nutzungsvereinbarung werden absichtlich nicht automatisiert umgangen.

### Empfohlene Wissensschichten

#### 1. Klinischer Kern

Diese Quellen bilden die kompakte Basis auf Telefon und Windows:

| Quelle | Zweck | Aktualisierung | Zugriff |
| --- | --- | --- | --- |
| ICD-10-GM | Deutsche Diagnosen und Kodierung | jährlich, einschließlich Korrekturen | BfArM-Downloadbedingungen |
| ICD-11 MMS | Internationale, mehrsprachige Diagnosen | ausgabenbezogen | WHO-API mit Zugangsdaten |
| OPS | Deutsche Operationen und Prozeduren | jährlich, einschließlich Korrekturen | BfArM-Downloadbedingungen |
| Alpha-ID-SE | Diagnosetexte und Verknüpfung seltener Erkrankungen | jährlich | BfArM-Downloadbedingungen |
| Orphanet | Seltene Erkrankungen und ORPHA-IDs | jährlich | offener Download, CC BY 4.0 |
| LOINC | Labor- und klinische Beobachtungen | halbjährlich | kostenloses Konto und LOINC-Lizenz |

#### 2. Medikamente, Leitlinien und Evidenz

Diese Quellen ergänzen den Kern themenbezogen:

| Quelle | Zweck | Wichtige Grenze |
| --- | --- | --- |
| EMA PMS | EU-Arzneimittel, Wirkstoffe, Formen und Stärken | öffentliche FHIR-API; Drittinhalte gesondert prüfen |
| AWMF-Leitlinien | Deutsche klinische Leitlinien | Rechte liegen bei den herausgebenden Fachgesellschaften |
| PubMed | Literaturangaben und Abstracts | Abstract- und Artikelrechte sind einzelfallabhängig |
| PMC Open Access | Volltexte | nur zulässigen OA-Teilbestand und Lizenz pro Artikel verwenden |
| ClinicalTrials.gov | Studienregistrierungen und Ergebnisse | Registerdaten sind keine Therapieempfehlung |
| DailyMed | US-Arzneimittelinformationen | Vollbestand ist für ein Telefon sehr groß |
| openFDA | Labels, Rückrufe und Verdachtsmeldungen | Meldungen beweisen weder Häufigkeit noch Kausalität |

#### 3. Erweiterte Terminologien und öffentliche Gesundheit

SNOMED CT, ICF und ausgewählte RKI-Datensätze sind wertvoll, aber nicht
pauschal installierbar. SNOMED CT benötigt eine Affiliate-Lizenz. Bei
RKI-Datensätzen gelten die Bedingungen und Aktualisierungszyklen des
jeweiligen Repositories. ICF wird wie andere offizielle BfArM-ClaML-Dateien
importiert.

### Auswahl und Aktualisierung auf Android

1. **Einstellungen > Import/Export > Offline-Referenzen verwalten** öffnen.
2. **Offizielle Quellen** wählen.
3. Die wichtigen Kernquellen sind beim ersten Start vorausgewählt. Die Auswahl
   kann jederzeit geändert und gespeichert werden.
4. **Herunterladen/Aktualisieren** öffnet die gewählte offizielle Quelle und
   deren Bedingungen.
5. Die dort rechtmäßig bezogene Originaldatei mit **Datei importieren**
   auswählen.

ICD-10-GM, OPS und ICF können direkt als offizielles ClaML-XML oder als
vollständiges BfArM-ZIP importiert werden. Die App findet die eigentliche
ClaML-Datei im ZIP automatisch und ignoriert XSD-, PDF- und Begleitdateien.

Andere Quellen werden als `.dmcknowledge` importiert. Diese Pakete werden auf
Windows aus einer rechtmäßig bezogenen Quelldatei erstellt. Ein erfolgreicher
Import einer neuen ICD-10-GM-, OPS- oder ICF-Ausgabe ersetzt die ältere Ausgabe
derselben Quelle. Bei einem Fehler bleibt der bisherige Index erhalten.

### Auswahl und Aktualisierung auf Windows

`manage-knowledge.bat` öffnet den gemeinsamen Quellenmanager:

- Die Quellenauswahl ist identisch zum Android-Katalog.
- **Auswahl speichern** merkt die gewünschten Quellen lokal.
- **Quelle öffnen / aktualisieren** öffnet die offizielle Download-, API- oder
  Lizenzseite.
- **Datei importieren** wandelt unterstützte Originaldateien in ein
  `.dmcknowledge`-Paket um und installiert es.
- Installierte Module lassen sich einzeln aktivieren oder deaktivieren.

Der Manager unterstützt derzeit direkt:

- BfArM-ClaML als XML oder ZIP,
- LOINC als Release-ZIP oder CSV,
- Orphadata-Nomenklatur als ZIP oder XML,
- SNOMED-CT-RF2-Description-Snapshot als ZIP,
- strukturierte CSV-, TXT- und JSONL-Exporte, zum Beispiel Alpha-ID-SE.

Für API-Quellen wie WHO ICD-11, EMA, PubMed, ClinicalTrials.gov, DailyMed oder
openFDA wird ein rechtmäßig erzeugter, begrenzter JSONL-Snapshot verwendet.
Die Anwendung lädt bewusst nicht den gesamten Datenbestand ungefragt auf ein
Mobilgerät.

Manuell lässt sich eine Quelldatei so vorbereiten:

```powershell
python scripts/knowledge/prepare_knowledge_file.py `
  --catalog knowledge/source-catalog.json `
  --source-id bfarm.icd10gm `
  --input C:\Daten\icd10gm2026syst-claml.zip `
  --output C:\Daten\icd10gm-2026.dmcknowledge `
  --version 2026 `
  --language de `
  --jurisdiction DE `
  --confirm-lawful-source
```

Danach kann dasselbe Paket auf Android und Windows importiert werden:

```powershell
python scripts/knowledge/knowledge_runtime.py install `
  --root runtime/knowledge `
  C:\Daten\icd10gm-2026.dmcknowledge
```

`run.bat` und `run-phone.bat` erkennen installierte Module automatisch. Sie
starten dann `llama.cpp` auf einem internen Port und davor den lokalen
Wissens-Proxy. Der öffentliche `/v1/chat/completions`-Pfad und das
Token-Streaming bleiben unverändert.

### Wie alle aktiven Module gemeinsam verwendet werden

Vor einer Modellanfrage durchsucht DMC **alle aktivierten Module**. Exakte
medizinische Codes werden bevorzugt. Freitext benötigt einen klaren
medizinischen Treffer; Begrüßungen und allgemeine Gesprächswörter werden
verworfen.

Treffer werden fair im Rundlauf aus den passenden Modulen zusammengeführt.
Dadurch kann ein sehr großer ICD- oder Literaturindex kleinere Quellen wie
Orphanet oder LOINC nicht vollständig verdrängen. Höchstens zwölf Treffer und
16.000 Zeichen gelangen in den Prompt. Jeder Block enthält Modul, Ausgabe,
Quelle und URI. Danach nutzt die Antwort denselben DMC- und Streaming-Pfad wie
ein normaler Chat.

### Größen- und Qualitätsstrategie

Auf einem Telefon ist nicht der größtmögliche Download, sondern der
bestmögliche relevante Index sinnvoll:

- Terminologien und Klassifikationen vollständig lokal halten.
- Leitlinien und Literatur als fach- oder krankheitsspezifische Pakete
  importieren.
- Sehr große Volltext- und Arzneimittelbestände auf Windows vorbereiten und
  nur benötigte Pakete auf das Telefon übertragen.
- Alte Ausgaben nicht mit neuen mischen.
- Sprache, Rechtsraum und Stichtag in jedem Modul erhalten.
- Quellen nie ohne Lizenz, Attribution und offizielle URI verpacken.

### Medizinische und rechtliche Grenzen

- Die Wissensmodule sind Nachschlagequellen, kein Medizinprodukt und kein
  Ersatz für Diagnose, Behandlung oder professionelle Beratung.
- Die App prüft Dateiformat und Metadaten, aber nicht automatisch sämtliche
  Nutzungsrechte eines Benutzers.
- AWMF-Dokumente, PubMed-Abstracts und PMC-Artikel haben keine pauschal
  einheitlichen Weiterverbreitungsrechte.
- SNOMED CT darf nur im Rahmen der passenden Affiliate-Lizenz genutzt werden.
- ICD-11 muss WHO-Identifikatoren, unveränderte Titel, Lizenz und Attribution
  behalten; Übersetzungen und Crosswalks können zusätzliche Rechte benötigen.
- openFDA-Daten sind weitgehend öffentlich, können aber ausdrücklich
  gekennzeichnete Drittinhalte enthalten.
- Diese technische Dokumentation ist keine Rechts- oder Medizinberatung.

### Offizielle Ausgangspunkte

- [BfArM-Downloadportal](https://www.bfarm.de/DE/Kodiersysteme/Services/Downloads/_verteilerseite.html)
- [WHO ICD API](https://icd.who.int/docs/icd-api/APIDoc-Version2/)
- [WHO ICD-11-Lizenz](https://icd.who.int/docs/icd-api/license/)
- [Orphadata-Nomenklatur und Klassifikationsabgleiche](https://sciences.orphadata.com/alignments/)
- [LOINC-Downloads](https://loinc.org/downloads/)
- [LOINC-Lizenz](https://loinc.org/license/)
- [EMA PMS API](https://api.pms.ema.europa.eu/public/v1/swagger)
- [AWMF-Leitlinienregister](https://register.awmf.org/de/leitlinien/aktuelle-leitlinien)
- [NCBI APIs](https://www.ncbi.nlm.nih.gov/home/develop/api/)
- [PMC Open Access](https://pmc.ncbi.nlm.nih.gov/tools/openftlist/)
- [ClinicalTrials.gov API](https://clinicaltrials.gov/data-about-studies/learn-about-api)
- [DailyMed Web Services](https://dailymed.nlm.nih.gov/dailymed/app-support-web-services.cfm)
- [openFDA APIs](https://open.fda.gov/apis/)
- [SNOMED-CT-Lizenz in Deutschland](https://www.bfarm.de/DE/Kodiersysteme/Terminologien/SNOMED-CT/Lizenz/_node.html)
- [RKI Open Data](https://github.com/robert-koch-institut)

## English

### Purpose and model

InetMind/DMC augments a local language model with verifiable, versioned
specialist knowledge. Source data is **not trained into the model**. It is
indexed locally. For each question, the runtime retrieves only relevant
records, appends a small attributed evidence block to the current DMC context,
and lets the model answer from that evidence.

This makes updates independent of model training, preserves source and edition
metadata, allows each module to be enabled separately, and remains fully
offline after import.

Selecting a catalog entry is not a download or acceptance of a licence. Data
must come directly from the official publisher. InetMind deliberately does not
bypass accounts, API credentials, document-specific rights, or terms of use.

### Recommended layers

The mobile clinical core consists of ICD-10-GM, ICD-11 MMS, OPS, Alpha-ID-SE,
Orphanet, and LOINC. Medicines, evidence, and guidance can be added through EMA
PMS, lawfully reusable AWMF guidelines, PubMed metadata, eligible PMC Open
Access articles, ClinicalTrials.gov, targeted DailyMed labels, and openFDA.
SNOMED CT, ICF, and selected RKI datasets are useful advanced modules but have
licence or dataset-specific conditions.

### Android workflow

1. Open **Settings > Import/Export > Manage offline references**.
2. Open **Official sources**.
3. Important core sources are preselected the first time. Save any changes.
4. **Download/update** opens the selected official source and its terms.
5. Import the lawfully obtained source file with **Import file**.

Android directly imports official BfArM ClaML XML/ZIP files for ICD-10-GM, OPS,
and ICF. Other sources use the shared `.dmcknowledge` format prepared on
Windows. A successful update replaces the previous edition of the same BfArM
classification; a failed import leaves the installed index untouched.

### Windows workflow

Run `manage-knowledge.bat` for the shared source catalog, default selection,
official-source links, file conversion, import, and module enable/disable
controls. The converter supports BfArM ClaML, LOINC ZIP/CSV, Orphadata
ZIP/XML, SNOMED CT RF2 description snapshots, and structured CSV/TXT/JSONL
exports.

API-backed sources use lawfully created, bounded JSONL snapshots. InetMind does
not silently mirror entire PubMed, DailyMed, or pharmacovigilance collections
onto a phone.

If modules are installed, `run.bat` and `run-phone.bat` automatically place the
local knowledge proxy in front of `llama.cpp`. The existing OpenAI-compatible
endpoint and server-sent-event streaming contract remain unchanged.

### Combined retrieval

Every enabled module participates in each retrieval. Exact codes receive the
highest priority; general greetings are rejected. Relevant records are merged
round-robin across modules so a large source cannot crowd out every smaller
source. No more than twelve records and 16,000 characters are appended, with
module, edition, source, and URI attribution.

### Safety and rights

Knowledge modules are reference material, not diagnosis or treatment advice.
Rights remain source-specific. In particular, AWMF documents, PubMed abstracts,
and PMC articles do not share one universal redistribution licence; SNOMED CT
requires the applicable affiliate licence; and ICD-11 must retain WHO
identifiers, unchanged titles, licence, and attribution. This document is
neither legal nor medical advice.

The official links in the German section are the canonical starting points for
downloads, APIs, licences, and current terms.
