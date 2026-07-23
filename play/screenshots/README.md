# Google Play phone screenshots

This directory contains the version-controlled, privacy-checked screenshots for
the InetMind Google Play listing. The upload layout follows the `gplay release
--screenshots-dir` convention:

```text
play/screenshots/
  de-DE/
    phoneScreenshots/
      01-local-chat-performance-1080x1920.png
      02-welcome-1080x1920.png
      03-camera-and-attachments-1080x1920.png
      04-model-context-and-modalities-1080x1920.png
      05-offline-icd10-module-1080x1920.png
  en-US/
    phoneScreenshots/
      01-local-chat-performance-1080x1920.png
      02-model-context-and-modalities-1080x1920.png
```

All seven files are direct captures from a Samsung SM-S931B running InetMind
with the DMC context engine. They
are 1080x1920 (9:16), 24-bit RGB PNG files without an alpha channel. The source
captures were reviewed to exclude private chats, notifications, credentials,
and unrelated applications.

## Ordered subjects and suggested German alt text

1. Local chat performance: "Lokale InetMind-Antwort mit Modellname, Tokenzahl,
   Antwortzeit und Geschwindigkeit."
2. Welcome screen: "InetMind-Startansicht mit lokaler Modellauswahl, Spracheingabe
   und Nachrichteneingabe."
3. Camera and attachments: "Anhangmenue fuer Bilder, Kamera, Audio, Text, PDF,
   MCP-Server und Systemnachrichten."
4. Model capabilities: "Modellinformationen mit 131.072 Token Kontext sowie
   Bild- und Audiounterstuetzung."
5. Offline knowledge: "Aktiviertes ICD-10-GM-2026-Offlinemodul mit 12.606
   lokalen Eintraegen."

The `en-US` set contains a local chat/performance view with English controls
and the English model-information dialog showing the 131072-token context plus
vision and audio capabilities.

Validate the directory before upload:

```powershell
gplay validate screenshots --dir .\play\screenshots --locale de-DE --pretty
gplay validate screenshots --dir .\play\screenshots --locale en-US --pretty
```

The ignored source and capture metadata remain under
`publish/playstore/current/screenshots/phone/`; only reviewed store assets are
tracked here.
