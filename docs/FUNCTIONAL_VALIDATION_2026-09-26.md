# AlfaRaceX - Sviluppo e verifica funzionale, 26 settembre 2026

Stato: IN PROGRESS. Cinque commit funzionali locali sul branch `audit/functional-validation-2026-09-26`, derivato dal main verificato `c7e6a11b2d96b43259b0de0e96ba427d56c6c257`. Fetch eseguito: origin/main coincide con la baseline. Nessun push, merge o rilascio. Il repository e la patch precedenti restano preservati. Installer non ricostruito.

## Modifiche e motivazione

- HEX: rifiuto di file senza EOF, vuoti, dati dopo EOF, indirizzi duplicati/overflow, record malformati. Controllo delle pagine e diagnostica coerente. I dati validi, segmentati e sparsi restano supportati.
- ISO-TP: rifiuto esplicito dei messaggi oltre 255 byte, dei frame troncati e dei DLC non validi; flusso RX legato a bus, ID e formato CAN. I messaggi di altre sorgenti non alterano una ricezione attiva. Nessuna sequenza hardware inventata.
- Manifest/download: tre ruoli distinti, intervalli corrispondenti ai linker del repository, versione e URL coerenti con gli asset GitHub ufficiali, hash valido. Download separati, limite di dimensione/tempo e rimozione dei file incompleti. Questi controlli NON provano la capacita fisica della Flash o il ruolo del dispositivo collegato.
- Backup: salvataggi esclusivi con nomi univoci, flush su disco, ricalcolo dell'hash dei byte salvati prima del successo; metadati con ambito e ruolo dichiarato dall'utente. La funzione di salvataggio e separata dalla lettura USB per poterla verificare senza simulare un dispositivo reale. Nessun cambiamento del protocollo DFU o della dimensione di lettura.
- Desktop: invio dei messaggi dal worker tramite dispatcher WPF, verifica dell'origine del bridge e diagnostica ProcessFailed di WebView2. Test isolati con profilo dedicato; test UI e acquisizione delle sei viste predisposti in CI. HTML, CSS, immagini e layout non modificati.

## Verifiche realmente eseguite

- Core C, GCC, strict warnings: 22/22 test in Debug e 22/22 in Release. Test ISO-TP con 765 round trip (lunghezze 1..255 e tre dimensioni di blocco), confronto completo dei payload, sequenze errate e traffico intercalato.
- Backend .NET 8: 39/39 test. Inclusi HEX, manifest, download/hash/cancellazione, metadati backup, scrittura concorrente di 12 snapshot e rifiuto prima della scrittura di un backup incompleto. Le fixture HTTP sono esclusivamente test in memoria; i test backup esercitano il filesystem reale con byte sintetici.
- Controprova HEX sul sorgente main originale: 11/19 casi falliscono; tutti i 19 passano sul branch corretto.
- Desktop Release: compilazione riuscita, zero warning/errori. Eseguibile avviato con --smoke-test: exit 0, SQLite crea e riapre i dati, asset locali presenti, runtime WebView2 individuato.
- Firmware RC5 effettivamente scaricati dalle URL del manifest: tre hash MATCH. Il parser corretto accetta BH 43508 byte (22 pagine), C2 43960 byte (22 pagine), C1 71740 byte (36 pagine), entro i limiti del manifest. Nessuno dei firmware e stato eseguito o scritto su hardware.
- git diff --check senza errori; revisione del diff focalizzata sui sottosistemi modificati.

## Verifica UI fallita: evidenza e limiti

Due avvii reali --ui-smoke-test su profili nuovi terminano con exit 2. Il secondo registra GpuProcessExited / Crashed / exit -1073741790 (0xC0000022), RenderProcessExited / LaunchFailed / exit 49, poi BrowserProcessExited / Unexpected / exit -2147483645. Il browser si arresta prima della disponibilita del bridge. Nessuna delle sei viste e stata validata visivamente in questa sessione; nessuna fedelta al riferimento approvato viene dichiarata. Il test del dispatcher e predisposto ma non raggiunto a causa del crash.

La causa esatta del fallimento nativo resta da isolare. Restrizioni dell'ambiente di esecuzione sono un'ipotesi, non un fatto dimostrato. Non sono state disabilitate sandbox o protezioni. Le indicazioni Microsoft sui processi e sui permessi sono riferimenti diagnostici:
https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/process-related-events
https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/measures

Il controllo UI e aggiunto alla pipeline Windows, ma questa nuova revisione della CI non e stata eseguita in remoto. Una CI precedente non valida questi commit.

## Questioni ancora aperte

Identita fisica C1/C2/BH e capacita Flash; confronto completo sorgenti BACCAble/dump; ripristino originale senza metadati ARX; validazione DFU e protezione cross-flash; backup preventivo automatico prima di ogni scrittura; journaling e recupero delle operazioni interrotte; errore di enumerazione USB trattato come nessun dispositivo; stato firmware installato distinto dalla versione pubblicata; integrazione completa delle funzioni e verifica della UI approvata. Non sono state importate alla cieca tutte le correzioni del vecchio branch.

Il salvataggio locale dei backup e verificato con dati sintetici. La lettura e il ripristino sul dispositivo restano non verificati. Implementato ma non ancora verificato su hardware reale.

## Commit

4ed03fd Reject incomplete and ambiguous firmware HEX files with parser regression tests
dab80c4 Reject truncated ISO-TP messages and isolate active receive streams
4111d15 Validate firmware manifest targets and preserve verified downloads on failure
8e0025e Marshal backend messages to the UI thread and add isolated WebView2 validation
83b02a8 Preserve concurrent backup snapshots and verify persisted bytes before completion

## Riproduzione

Core: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DARX_STRICT_WARNINGS=ON`, build e `ctest --test-dir build --output-on-failure`; ripetere Release.
Backend: `dotnet run --project tests/AlfaRaceX.HostTests -c Release`.
Desktop: predisporre Bootstrap 5.3.8 locale come nel workflow build-desktop.yml; `dotnet build desktop/AlfaRaceX.Desktop -c Release`.
Smoke: `AlfaRaceX.exe --smoke-test --smoke-root=<cartella-nuova>`; UI: `--ui-smoke-test` con cartella nuova. Il profilo di test non deve esistere. I test non programmano dispositivi.

Evidenze in `evidenze/functional-*.log` e `evidenze/ui-process-failures.json`. Il bundle incrementale contiene i commit locali e richiede la baseline main indicata; non e un installer.
