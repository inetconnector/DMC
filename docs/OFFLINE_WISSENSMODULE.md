# Offline-Wissensmodule / Offline Knowledge Modules

## Deutsch

### Ziel

DMC kann große, strukturierte Nachschlagewerke lokal durchsuchen und passende
Auszüge als belegten Kontext an das bereits lokal laufende Modell übergeben.
Nach dem Import benötigt die Suche keine Internetverbindung. Die Daten werden
nicht in das Modell eintrainiert und nicht an einen Server gesendet.

Die APK und dieses Repository enthalten **keine** ICD-Klassifikationsdaten.
Nutzer beziehen die gewünschte Ausgabe selbst direkt vom jeweiligen
Rechteinhaber, prüfen dessen aktuelle Bedingungen und importieren die
unveränderte Datei. Damit werden veraltete, falsch übersetzte oder ohne
Erlaubnis weiterverteilte Kopien vermieden.

### Unterstützte Wege

#### ICD-10-GM

1. Die gewünschte Ausgabe auf der offiziellen
   [BfArM-Downloadseite](https://www.bfarm.de/DE/Kodiersysteme/Services/Downloads/_verteilerseite.html)
   auswählen.
2. Die dort angezeigten Downloadbedingungen lesen und akzeptieren.
3. Die offizielle ClaML-XML-Datei oder das unveränderte ZIP in DMC unter
   **Einstellungen > Import/Export > Offline-Referenzen** importieren.
4. DMC speichert Ausgabe, Prüfsumme, Quelle, Lizenz und die vorgeschriebene
   Quellenangabe zusammen mit dem lokalen Index.

Beim ZIP-Import wird das vollständige BfArM-Archiv ausgewählt, nicht eine Datei
darin. DMC durchsucht alle ZIP-Unterverzeichnisse, erkennt die eigentliche
ClaML-XML an ihrem Inhalt und ignoriert XSD, PDF, TXT und sonstige Begleitdateien
automatisch. Für `icd10gm2026syst-claml.zip` wird beispielsweise
`Klassifikationsdateien/icd10gm2026syst_claml_20250912.xml` gefunden, während
`Klassifikationsdateien/ClaML2.0.0.xsd` nicht als Klassifikationsinhalt gilt.

Ein installiertes Modul bleibt absichtlich auch für neue Chats verfügbar. Seine
Inhalte werden jedoch nicht pauschal als Chatverlauf übernommen: Vor jeder
Anfrage sucht DMC nur nach zur aktuellen Frage passenden Datensätzen. Häufige
Gesprächswörter und Begrüßungen werden entfernt, kurze Suchbegriffe müssen als
ganzes Wort passen, und Präfixsuchen sind erst ab sieben Zeichen zulässig. Ein
Treffer wird nur bei einem exakten Code, einem passenden Titelwort oder mehreren
passenden Inhaltsbegriffen an das Modell übergeben. Dadurch lädt ein neues
`Hallo` weder früheren Chatinhalt noch beispielsweise den ICD-10-Eintrag
`Hallopeau` in den Prompt.

Die BfArM-Bedingungen behandeln ICD-10-GM als amtliches Werk, verlangen aber
insbesondere Unverändertheit und Quellenangabe. Bei Vervielfältigung,
Verbreitung oder öffentlicher Bereitstellung gelten weitere Bedingungen. Der
Importdialog ersetzt nicht die Prüfung der jeweils aktuellen Bedingungen.

#### ICD-11

ICD-11 steht laut WHO unter **CC BY-ND 3.0 IGO**. Die Einbindung in Software ist
unter den WHO-Bedingungen möglich, wenn unter anderem Code, Titel und
offizielle URI erhalten bleiben und die vorgeschriebene Quellenangabe
mitgeführt wird. Übersetzungen und Crosswalks können eine gesonderte
Vereinbarung benötigen.

1. Daten rechtmäßig über die offizielle
   [WHO ICD API](https://icd.who.int/icdapi) beziehen.
2. Pro JSONL-Zeile die unveränderten Felder `id`, `code`, `title` und
   `uri` bereitstellen. `uri` muss die offizielle `id.who.int`-URI sein.
3. Ein lokales Importpaket erzeugen:

   ```powershell
   python scripts/knowledge/build_knowledge_module.py `
     --type icd11 `
     --input C:\Pfad\icd11-records.jsonl `
     --output C:\Pfad\icd11.dmcknowledge `
     --id who.icd11.2026.en `
     --name "ICD-11 2026 English" `
     --version 2026-01 `
     --language en `
     --confirm-unmodified-official-content
   ```

4. Das erzeugte `.dmcknowledge`-Paket in der App importieren.

Der Builder lädt keine WHO-Daten herunter, enthält keine Zugangsdaten und
erteilt keine Weiterverbreitungsrechte. Er verpackt ausschließlich eine lokal
vorliegende, rechtmäßig bezogene Datei und lehnt fehlende WHO-URIs,
abweichende Hosts, falsche Lizenzangaben und eine fehlende
Unverändertheitsbestätigung ab.

### Technischer Aufbau

Ein `.dmcknowledge`-Paket ist ein deterministisches ZIP:

- `manifest.json`: Modul-ID, Typ, Version, Sprache, Rechtsraum, offizielle
  Quelle, Lizenz, Quellenangabe, Datensatzanzahl und
  `officialContentUnmodified`.
- `records.jsonl`: genau ein JSON-Objekt je Datensatz.

Der Android-Importer prüft Dateigröße, ZIP-Pfade, Schema, offizielle Hosts,
Lizenzmetadaten, Datensatzanzahl und Pflichtfelder. Erst danach ersetzt eine
SQLite-Transaktion eine vorhandene Modulversion. Bei einem Fehler bleibt der
alte Index vollständig erhalten.

Aktivierte Module werden mit exakter Code-Suche und SQLite FTS4 durchsucht.
Höchstens acht Treffer und maximal 12.000 Zeichen werden als nicht
vertrauenswürdige, belegte Referenzdaten an die aktuelle Frage angefügt. Die
Quellenangabe, Ausgabe und URI bleiben im Kontext sichtbar. Dieser zusätzliche
Kontext läuft anschließend durch denselben DMC- und Streaming-Pfad wie normale
Nachrichten.

### Sicherheits- und Qualitätsgrenzen

- Ein Treffer ist Nachschlagewissen, keine Diagnose oder Therapieempfehlung.
- Ausgabe, Sprache und Rechtsraum müssen zur konkreten Verwendung passen.
- DMC verändert offizielle Titel nicht und erzeugt keine eigenen
  ICD-Übersetzungen oder Crosswalks.
- Module lassen sich einzeln aktivieren, deaktivieren und vollständig löschen.
- Daten und Suchindex liegen im privaten App-Speicher.
- Vor einer Veröffentlichung eines Datenpakets müssen die dafür geltenden
  Rechte unabhängig geprüft werden. Dies ist keine Rechtsberatung.

## English

### Purpose

DMC can search large structured reference works locally and append relevant,
attributed excerpts to the context of the on-device model. Search is fully
offline after import. The data is neither trained into the model nor sent to a
server.

The APK and repository contain **no** ICD classification data. Users obtain the
required edition directly from the rights holder, review the current terms, and
import the unchanged file. This avoids stale, mistranslated, or unlawfully
redistributed copies.

### Supported paths

#### ICD-10-GM

1. Select the edition on the official
   [BfArM download page](https://www.bfarm.de/DE/Kodiersysteme/Services/Downloads/_verteilerseite.html).
2. Read and accept the terms shown there.
3. Import the official ClaML XML file or unchanged ZIP through
   **Settings > Import/Export > Offline references**.
4. DMC stores the edition, checksum, source, licence, and required attribution
   with the private local index.

For ZIP imports, select the complete BfArM archive rather than a file inside it.
DMC searches every ZIP subdirectory, identifies the actual ClaML XML by its
content, and automatically ignores XSD, PDF, TXT, and other companion files. In
`icd10gm2026syst-claml.zip`, for example, it finds
`Klassifikationsdateien/icd10gm2026syst_claml_20250912.xml` and does not treat
`Klassifikationsdateien/ClaML2.0.0.xsd` as classification content.

An installed module intentionally remains available to new chats, but its
contents are not carried over as conversation history. Before each request, DMC
retrieves only records relevant to the current question. Greetings and common
conversational words are removed, short search terms require whole-word matches,
and prefix matching starts at seven characters. Evidence is passed to the model
only for an exact code, a matching title word, or multiple matching content
terms. A fresh `Hallo` therefore injects neither previous chat content nor an
ICD-10 record such as `Hallopeau` into the prompt.

The import dialog does not replace reviewing the current BfArM terms,
especially before reproducing, distributing, or making the work publicly
available.

#### ICD-11

WHO publishes ICD-11 under **CC BY-ND 3.0 IGO**. Under the WHO terms,
incorporation into software can be possible when the code, title, official URI,
licence, and attribution remain attached. Translations and crosswalks may
require a separate agreement.

1. Obtain data lawfully through the official
   [WHO ICD API](https://icd.who.int/icdapi).
2. Supply unchanged string fields `id`, `code`, `title`, and `uri` on
   each JSONL line. `uri` must be the official `id.who.int` URI.
3. Build a local package with `scripts/knowledge/build_knowledge_module.py`
   and `--confirm-unmodified-official-content`.
4. Import the resulting `.dmcknowledge` file in the app.

The builder downloads no WHO data, embeds no credentials, and grants no
redistribution rights. It rejects missing WHO URIs, unofficial hosts, wrong
licence metadata, and missing confirmation that official content is unchanged.

### Architecture

A `.dmcknowledge` file is a deterministic ZIP containing:

- `manifest.json`: module identity, type, edition, language, jurisdiction,
  official source, licence, attribution, record count, and the unchanged-content
  declaration.
- `records.jsonl`: one JSON object per record.

Android validates size, ZIP paths, schema, official hosts, licence metadata,
record count, and required fields. A SQLite transaction replaces an existing
module only after every record passes validation. A failed import leaves the
previous index intact.

Enabled modules use exact-code lookup and SQLite FTS4. At most eight hits and
12,000 characters are appended as attributed, untrusted reference evidence to
the current request. Source, edition, and URI remain visible. Generation then
uses the same DMC and streaming path as an ordinary message.

### Safety and quality boundaries

- A result is reference information, not a diagnosis or treatment decision.
- Edition, language, and jurisdiction must match the intended use.
- DMC does not modify official titles or create ICD translations or crosswalks.
- Modules can be enabled, disabled, and deleted independently.
- Data and indexes remain in private app storage.
- Redistribution rights must be checked independently before publishing a data
  package. This document is not legal advice.

## Official references

- [BfArM download portal](https://www.bfarm.de/DE/Kodiersysteme/Services/Downloads/_verteilerseite.html)
- [BfArM 2025 download terms](https://www.bfarm.de/SharedDocs/Downloads/DE/Kodiersysteme/downloadbedingungen-2025.pdf?__blob=publicationFile)
- [WHO ICD-11 API](https://icd.who.int/icdapi)
- [WHO ICD-11 licence](https://icd.who.int/en/docs/icd11-license.pdf)
