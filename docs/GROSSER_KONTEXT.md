# Wie der große DMC-Kontext funktioniert

Sprache: **Deutsch** | [English](LARGE_CONTEXT.md)

Dieses Dokument erklärt den großen Kontext der DMC-Android-App so, dass ein
Programmierer die Idee, den tatsächlichen Ablauf und die Grenzen verstehen
kann, ohne zuerst den gesamten C++-Code lesen zu müssen.

## Kurzfassung

Ein Sprachmodell verarbeitet Text nicht direkt als Wörter, sondern als
**Tokens**. Für jedes bereits verarbeitete Token hält llama.cpp berechnete
Zwischenergebnisse im **KV-Cache**. Ein großer KV-Cache benötigt viel
Arbeitsspeicher. Auf einem Smartphone wäre ein dichter KV-Cache für 131.072
Tokens je nach Modell sehr groß, langsam oder gar nicht zuverlässig
allozierbar.

DMC trennt deshalb zwei Dinge:

1. Die **logische Historie** enthält weiterhin die vollständige Folge der
   Token-IDs des Chats.
2. Der **physische Kontext** enthält nur die Tokens, für die llama.cpp gerade
   einen KV-Cache auf dem Gerät aufgebaut hat.

Solange alles in den physischen Kontext passt, arbeitet die App ganz normal und
ohne Auswahl. Erst wenn der Platz knapp wird, wählt DMC deterministisch aus:

- den Anfang mit Systemanweisung und Chat-Präfix,
- die neuesten Tokens,
- mehrere ältere Bereiche in immer größeren zeitlichen Abständen.

Die Auswahl wird chronologisch sortiert und erneut durch das echte Modell
geschickt. So entsteht ein neuer, kleinerer KV-Cache. Die vollständige logische
Tokenhistorie bleibt dabei erhalten und kann für spätere Neuaufbauten wieder
verwendet werden.

## Was bedeutet „Kontext“?

Der Kontext ist alles, was das Modell bei der nächsten Vorhersage
berücksichtigen kann: Systemanweisung, Benutzernachrichten, Antworten und
gegebenenfalls aufbereitete Anhänge.

Dabei sind drei Größen zu unterscheiden:

| Begriff | Bedeutung |
| --- | --- |
| Trainingskontext | Kontextgröße, die das GGUF-Modell in seinen Metadaten meldet. |
| Logischer Kontext | Von DMC geführte vollständige Tokenhistorie und die der Oberfläche gemeldete Kontextgröße. |
| Physischer Kontext | Tatsächlich von llama.cpp angelegtes KV-Fenster im Arbeitsspeicher. |

Beim aktuell verwendeten Gemma-Modell meldet das Modell 131.072 trainierte
Kontexttokens. Die Android-App meldet deshalb 131.072 als logisches Fenster,
legt auf dem Telefon aber normalerweise nur 16.384 physische Kontextplätze an.
Falls diese Speicherallokation scheitert, probiert die Runtime nacheinander
kleinere Fenster, beispielsweise 8.192 und schließlich 4.096 Tokens.

Wichtig: Die Anzeige `131072` bedeutet nicht, dass zu jedem Zeitpunkt ein
dichter 131K-KV-Cache im RAM liegt. Sie bedeutet, dass DMC eine entsprechend
lange logische Historie verwaltet und daraus einen kleineren Arbeitskontext
aufbaut. Wie sinnvoll 131K Tokens sind, hängt zusätzlich davon ab, wofür das
jeweilige Modell trainiert wurde.

## Warum der KV-Cache so viel Speicher braucht

Bei autoregressiven Transformer-Modellen wird für jedes vorherige Token und
für viele Modellschichten ein Key- und Value-Zustand gespeichert. Dadurch muss
das Modell beim nächsten Token nicht die gesamte Sequenz von Grund auf neu
berechnen.

Der Speicherbedarf wächst ungefähr linear mit:

```text
Anzahl Tokens × Anzahl Schichten × KV-Dimension × Datentypgröße
```

Die Gewichtsquantisierung eines GGUF-Modells, zum Beispiel Q4, verkleinert vor
allem die Modellgewichte. Sie bedeutet nicht automatisch, dass auch jeder
KV-Zustand genauso klein wird. Deshalb kann ein Modell auf dem Telefon
grundsätzlich laufen, während ein vollständig dichter 131K-Kontext trotzdem
zu viel RAM benötigen würde.

## Die zwei Speicherbereiche von DMC

### 1. Logische Tokenhistorie

`g_dmc_token_history` enthält die kanonische Folge der Token-IDs. Dazu gehören:

- die formatierte Systemnachricht,
- die formatierten Benutzernachrichten,
- die vom Modell erzeugten Antworttokens,
- Modell-Endtoken wie EOG.

Diese Historie speichert Token-IDs, nicht für jedes Token den großen KV-Zustand.
Sie ist deshalb wesentlich kleiner als ein gleich langer KV-Cache. DMC fasst
den Text nicht rekursiv zusammen und ersetzt ihn nicht durch eine automatisch
erzeugte Kurzfassung.

### 2. Physischer llama.cpp-Kontext

Der physische Kontext ist der normale llama.cpp-Kontext mit seinem KV-Cache.
Auf Android ist das Ziel 16.384 Tokens. Vier Plätze bleiben als technische
Überlaufreserve ungenutzt. Zusätzlich reserviert die Runtime Platz für die
nächste Antwort.

Ohne explizites Ausgabelimit werden standardmäßig 4.096 Plätze für die
Fortsetzung reserviert. Bei einem expliziten Limit werden mindestens 512
Plätze vorgesehen; die Reserve wird auf höchstens die Hälfte des physischen
Fensters begrenzt.

## Wann DMC aktiv auswählt

Ein kurzer Chat nimmt den normalen dichten Pfad:

```text
Neue Tokens -> llama_decode -> KV-Cache wächst
```

DMC baut den Kontext erst neu auf, wenn sowohl die logische Historie als auch
der aktuelle physische Zustand das verfügbare Prompt-Budget überschreiten.
Nach einem Neuaufbau kann der ausgewählte KV-Zustand für weitere kurze
Nachrichten wiederverwendet werden. Es wird also nicht bei jeder Nachricht die
gesamte Auswahl neu eingelesen.

Der Ablauf ist vereinfacht:

```text
vollständige Tokenhistorie
          |
          v
passt in das physische Prompt-Budget? ---- ja ---> normal weiterrechnen
          |
         nein
          |
          v
DMC wählt Anfang + jüngsten Bereich + ältere Rückblickbereiche
          |
          v
Auswahl deduplizieren und chronologisch sortieren
          |
          v
physischen KV-Cache leeren
          |
          v
ausgewählte Originaltokens mit llama_decode neu einlesen
          |
          v
Generierung normal und tokenweise fortsetzen
```

## Die Mehrskalen-Auswahl

Die Android-Konfiguration wird in `runtime_dmc_config()` erzeugt:

| Parameter | Android-Wert | Zweck |
| --- | ---: | --- |
| `block_size` | 64 | Grundgröße eines älteren Rückblickblocks. |
| `local_window` | 2.048 | Neueste Tokens, die vollständig erhalten bleiben. |
| `global_tokens` | mindestens 64, höchstens 1.024 | Anfang der Sequenz, insbesondere System- und Formatpräfix. |
| `replay_levels` | 7 | Anzahl möglicher älterer Zeitskalen. |

Die Zahl der globalen Tokens richtet sich nach der Größe des Systempräfixes:

```text
global_tokens = max(64, min(Anzahl Systemtokens, 1024))
```

Für jede Rückblickebene `L` gelten vereinfacht:

```text
Abstand(L)   = max(local_window, block_size) × 2^L
Blockgröße(L) = block_size × 2^L
```

Nahe Rückblicke sind also klein und detailreich. Weiter entfernte Rückblicke
sind größer, liegen aber weiter auseinander. Daher der Name
**Deterministic Multiresolution Context**: deterministischer Kontext mit
mehreren zeitlichen Auflösungen.

Alle ausgewählten Tokenpositionen werden zusammengeführt, doppelte Positionen
werden entfernt und das Ergebnis wird aufsteigend sortiert. Diese
chronologische Reihenfolge ist wichtig, weil ein Decoder die Tokens kausal von
alt nach neu einlesen muss.

Falls eine Auswahl doch größer als das physische Budget wäre, haben der globale
Anfang und das lokale Ende Vorrang. Freie Plätze werden anschließend mit
Rückblicktokens in deterministischer Reihenfolge gefüllt. Das neueste Token
muss immer in der Auswahl enthalten sein; andernfalls gilt der Plan als Fehler.

## Konkretes Beispiel: 32K logisch, 4K physisch

Der native Selbsttest plant 32.768 logische Tokens für ein Budget von 4.096
physischen Tokens. Mit einem globalen Bereich von 64 Tokens ergibt sich:

| Bereich | Tokenpositionen | Anzahl |
| --- | --- | ---: |
| Globaler Anfang | 0 bis 63 | 64 |
| Lokales Ende | 30.720 bis 32.767 | 2.048 |
| Rückblick 1 | 30.656 bis 30.719 | 64 |
| Rückblick 2 | 28.544 bis 28.671 | 128 |
| Rückblick 3 | 24.320 bis 24.575 | 256 |
| Rückblick 4 | 15.872 bis 16.383 | 512 |
| **Gesamt** | nach Deduplizierung | **3.072** |

Die Auswahl ist kleiner als das 4K-Budget, enthält den Anfang, den kompletten
jüngsten Bereich und mehrere ältere Zeitskalen. Bei identischer Historie und
identischer Konfiguration entsteht immer exakt dieselbe Auswahl.

## Was beim Neuaufbau genau passiert

`rebuild_dmc_context()` führt folgende Schritte aus:

1. `plan_runtime_context()` erhält die Länge der logischen Historie und das
   physische Tokenbudget.
2. Der Plan liefert Positionen in der vollständigen Historie.
3. Die Runtime liest an diesen Positionen die unveränderten Original-Token-IDs.
4. Der bisherige llama.cpp-KV-Cache wird geleert.
5. Die ausgewählten Tokens werden in Batches von bis zu 512 Tokens erneut mit
   `llama_decode()` verarbeitet.
6. llama.cpp berechnet daraus einen normalen neuen KV-Cache.
7. Die vollständige logische Historie bleibt separat erhalten.

„Exakte Attention über ausgewählte Bereiche“ bedeutet hier: Für die Tokens,
die ausgewählt wurden, wird kein angenäherter Attention-Wert erfunden. Die
Android-Runtime lässt das echte Modell diese Tokens erneut auswerten. Nicht
ausgewählte Tokens sind bei diesem Neuaufbau allerdings nicht direkt im
physischen KV-Cache sichtbar.

Die Python- und C++-Referenzen enthalten zusätzlich eine explizite
`exact_attention()`-Funktion. Tests belegen, dass sie mit dichter Attention
übereinstimmt, wenn alle Tokens ausgewählt werden. Android ersetzt den
llama.cpp-Attention-Kernel nicht durch diese Referenzfunktion; Android nutzt den
gemeinsamen Auswahlplan und rehydriert damit den normalen llama.cpp-Kontext.

## Lange Antworten werden nicht am Fenster abgeschnitten

Auch während einer Antwort wächst der physische KV-Cache. Jedes erzeugte Token
wird gleichzeitig an die logische Historie angehängt. Erreicht der physische
Kontext seine Grenze, führt die Runtime einen weiteren DMC-Neuaufbau durch und
reserviert erneut 4.096 Plätze für die Fortsetzung.

Danach wird weitergeneriert, bis:

- das Modell selbst ein Endtoken erzeugt oder
- ein vom Aufrufer ausdrücklich gesetztes Ausgabetokenlimit erreicht ist.

Eine volle physische Position wird damit nicht mehr fälschlich als normales
Antwortende behandelt. Schlägt ein notwendiger Neuaufbau fehl, meldet die
Runtime einen Fehler, statt eine sichtbar halbe Antwort als vollständig
auszugeben.

## Was DMC bewusst nicht macht

DMC ist in dieser Implementierung:

- **keine rekursive Zusammenfassung** alter Nachrichten,
- **keine semantische Vektorsuche**,
- **kein RAG-System**,
- **kein Embedding- oder ANN-Index**,
- **kein gelerntes Routing**,
- **keine inhaltsabhängige Top-k-Auswahl**,
- **keine vollständige dichte 131K-Attention auf dem Smartphone**.

Die Auswahl hängt nur von Positionen, festen Parametern und dem verfügbaren
Budget ab. Das macht das Verhalten reproduzierbar, testbar und vergleichsweise
einfach zu portieren.

## Vorteile und Grenzen

### Vorteile

- Deutlich kleinerer physischer KV-Speicher als bei einem dichten 131K-Fenster.
- Kurze Chats laufen unverändert über den dichten Standardpfad.
- Der Anfang und der jüngste Gesprächsteil bleiben bevorzugt erhalten.
- Ältere Zeitbereiche werden über mehrere Skalen repräsentiert.
- Gleiche Eingabe und gleiche Parameter liefern dieselbe Auswahl.
- Es gibt keine zusätzliche Zusammenfassungsanfrage und kein zweites Modell.
- Die vollständige Tokenfolge bleibt für spätere Neuaufbauten verfügbar.

### Grenzen

- Nicht ausgewählte alte Tokens können beim aktuellen Neuaufbau nicht direkt
  beachtet werden. Ein Detail in einer Lücke kann daher fehlen.
- Ein Neuaufbau kostet Rechenzeit, weil ausgewählte Tokens erneut durch das
  Modell laufen müssen.
- Nach der Auswahl werden die behaltenen Tokens dicht und chronologisch neu
  positioniert. Ursprüngliche große Abstände sind damit nicht identisch im
  physischen Kontext abgebildet.
- Die vom Modell gemeldete Trainingskontextgröße bleibt eine wichtige
  Qualitätsgrenze. DMC macht aus einem kurz trainierten Modell nicht automatisch
  ein gutes Langkontextmodell.
- Ein angezeigtes logisches Fenster ist kein Beweis für perfekte Erinnerung an
  jedes einzelne Token.
- Die Qualität muss mit echten Langkontextaufgaben und Regressionstests gemessen
  werden; Determinismus allein garantiert noch keine gute Antwort.

## Selbsttest, Build-Schutz und Diagnose

Beim Vorbereiten eines Modells läuft ein nativer Selbsttest. Er prüft unter
anderem:

- dass eine 32K-Historie tatsächlich verdichtet wird,
- dass zwei identische Planungen dieselben Tokenpositionen liefern,
- dass das Ergebnis das physische Budget nicht überschreitet,
- dass das neueste Token erhalten bleibt.

Der Android-Build durchsucht außerdem die gepackte ARM64-Bibliothek nach
DMC-Markern. Eine APK ohne aktive DMC-Runtime soll dadurch nicht versehentlich
veröffentlicht werden.

Wichtige Logcat-Meldungen sind:

```text
DMC_RUNTIME self-test passed
DMC_RUNTIME enabled=1 physical_context=... logical_context=...
DMC_RUNTIME incremental turn: mode=dense ...
DMC_RUNTIME incremental turn: mode=selected-reuse ...
DMC_RUNTIME rebuild=... logical=... selected=... spans=...
DMC_RUNTIME generation continuation: ...
```

`mode=dense` bedeutet, dass noch keine Auswahl nötig war.
`mode=selected-reuse` bedeutet, dass ein bereits verdichteter physischer
Kontext weiterverwendet wird. Eine `rebuild`-Meldung zeigt einen echten
DMC-Neuaufbau.

## Wo der Code liegt

| Datei | Aufgabe |
| --- | --- |
| `cpp/dmc_reference.hpp` | Gemeinsame C++-Datenstrukturen, Auswahlalgorithmus und Runtime-Plan. |
| `android/llama.android/lib/src/main/cpp/ai_chat.cpp` | Android-Integration, logische Historie, KV-Neuaufbau und Generierungsfortsetzung. |
| `dmc/core.py` | Lesbare Python-Referenz für Auswahl und ausgewählte Attention. |
| `tests/test_dmc.py` | Python-Tests für Determinismus, Kausalität, Skalen und Dense-Gleichheit. |
| `cpp/test.cpp` | Native C++-Regressionstests des gemeinsamen Plans. |
| `scripts/windows/verify-android-dmc.ps1` | Prüft DMC-Marker in der gebauten Android-Bibliothek beziehungsweise APK. |

## Regeln für sichere Änderungen

Wer DMC verändert, sollte mindestens diese Invarianten erhalten:

1. Kurze Historien bleiben vollständig und dicht.
2. Es werden niemals Tokens aus der Zukunft ausgewählt.
3. Das neueste Token ist immer enthalten.
4. Die Auswahl überschreitet das physische Budget nicht.
5. Die Reihenfolge beim Wiedereinlesen ist chronologisch.
6. Gleiche Eingaben ergeben dieselbe Auswahl.
7. Die kanonische Tokenhistorie geht bei einem KV-Neuaufbau nicht verloren.
8. Eine volle KV-Position darf eine Antwort nicht still abschneiden.
9. Fehler beim Neuaufbau müssen sichtbar gemeldet werden.
10. Python-Referenz, C++-Referenz und Android-Verhalten bleiben aufeinander
    abgestimmt.

Nach Änderungen sollten mindestens die Python-Tests, die C++-Tests, der
Android-DMC-Markercheck und ein echter Langkontexttest auf dem Gerät laufen.

## Ein Satz zum Mitnehmen

DMC hält den vollständigen Chat als kleine logische Tokenfolge fest, arbeitet
auf dem Smartphone aber mit einem begrenzten echten llama.cpp-KV-Cache, den es
bei Bedarf aus Anfang, jüngstem Text und deterministischen älteren
Rückblickbereichen neu aufbaut.
