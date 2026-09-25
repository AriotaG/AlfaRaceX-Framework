# Hardware e Windows Validation Checklist

Stato iniziale di ogni prova: **NOT RUN**. Non compilare PASS senza allegare prove.
Le prove distruttive sono descritte per un banco controllato, non eseguite da questa
sessione. Prima di H05/H06 servono identità, capacità e backup verificati di H01-H04.
Per funzioni ADAS/freni/trazione serve personale competente e ambiente di prova
isolato; questa checklist non autorizza prove su strada o modifiche PROXY.

## Registrazione obbligatoria

Per ogni esecuzione registrare ID prova, data UTC, operatore, commit e SHA-256 degli
artefatti, OS/driver/WebView2, ruolo/porta/seriale, modello MCU letto, revisione scheda,
alimentazione, configurazione veicolo/ECU, azioni, log raw, expected/actual e PASS/FAIL.
Conservare originali read-only e copie di lavoro; non modificare il master BACCAble.
Una baseline documentale senza capture non basta per VERIFIED AGAINST BACCAble.

## Prove fisiche

| ID | Procedura | Expected e PASS | FAIL / arresto | Evidenza e rollback |
|---|---|---|---|---|
| H01 | Dispositivo disconnesso dalla vettura; fotografare porte e marcature; enumerare separatamente BH/C2/C1 normali e PROG seguendo §1.11 | Una porta alla volta; device/driver/VID/PID/seriale/descrittori associati senza ambiguità | Più dispositivi, identità non chiara o driver incompatibile: nessuna scrittura | Foto, export descrittori, log Windows/ARX; ritorno al collegamento iniziale |
| H02 | Leggere identificativi, capacità e protezioni con strumento ufficiale in sola lettura; confrontare mappe e dump | Capacità e pagina verificate per ogni MCU; regioni applicazione/persistenza entro limiti | Discrepanza C8/128 KiB o protezione attiva: STOP, non sbloccare con mass erase | Output strumento, hash dump; nessuna modifica e nessun rollback necessario |
| H03 | Avviare Desktop senza DFU, poi un dispositivo, poi due; provare driver non WinUSB su banco | Zero/due rifiutati; uno riconosciuto senza falsa associazione del ruolo; errori diagnostici | Successo fittizio o scelta arbitraria del dispositivo | Log SetupAPI e driver; ripristinare binding originale con procedura documentata |
| H04 | Due letture indipendenti della stessa Flash, salvate in nomi univoci; confrontare SHA e byte con master del ruolo | Dimensione reale corretta, due letture identiche, metadati accurati e originale intatto | Divergenza/area illeggibile: STOP prima di programmazione | Due binari, JSON, hash e diff; conservare copie offline |
| H05 | Solo su banco recuperabile: immagine corretta, erase pagine previste, write, lettura indipendente; verificare aree riservate invariate | Byte applicazione uguali all'immagine, pagine riservate invariate, boot osservato separatamente | Qualsiasi mismatch o errore non diagnosticato; nessun messaggio di successo | Log fasi/indirizzi, readback, hash, boot; recovery via bootloader documentato dal backup H04 |
| H06 | Su stesso banco: tentare file alterato, ruolo metadata errato, metadata assenti; poi restore valido e readback | Casi invalidi rifiutati prima di erase; valido identico al backup e boot osservato | Scrittura con ruolo fisico incerto o mismatch: STOP | Snapshot preventivo, log e diff completo; ripristino originale solo con identità certa |
| H07 | Logic analyzer su UART originale: boot/reset, traffico normale e diagnostico, disconnessione di un nodo; confronto ARX | Baud/polarità/framing/19 byte/timing e recovery misurati uguali o differenze motivate | Collisioni, perdita sync, blocco bus o assunzioni non confermate | Capture raw + decodifica e config strumento; rientro firmware originale |
| H08 | CAN isolato: replay catture originali, frame malformati, bus errato, ISO-TP >255 byte, CF mancanti, overflow, bus-off | Nessuna concatenazione tra ECU/bus; errore esplicito, timeout e recupero, traffico OEM preservato | Overflow silenzioso, polling incontrollato, invii fuori profilo | Trace RX/TX con timestamp/ID/bus/DLC, contatori errori; stop TX e ripristino baseline |
| H09 | Originale poi ARX: ELM C1 da setup, diagnostica read-only su ciascun bus con software/versione fissati; USB off >10 s e riconnessione documentata | Stessi risultati e routing; timeout/uscita/riattivazione corrispondenti; nessuna interferenza immobilizer | Semplice risposta AT senza risposta ECU non è PASS | Transcript seriale, USB e CAN simultanei; disabilitare ELM, ritorno normale |
| H10 | Replay IPC e poi banco quadro: menu, RES corto/lungo, pagine 18 caratteri, messaggi OEM e spegnimento | Stesse transizioni/rendering, nessun testo stale, uscite e timeout corretti | Sovrascrittura avvisi OEM o confusione widget infotainment/TFT | Video sincronizzato e capture CAN; menu off e baseline originale |
| H11 | Confronto Start/Stop, DPF, immobilizer con prerequisiti verificati e feature una alla volta | Solo effetti richiesti, default puliti senza interventi involontari, errore/timeout safe | Mancato avviamento, dati incoerenti o azioni non richieste | Log/capture prima-durante-dopo; disabilitare funzione e ripristinare configurazione |
| H12 | Misurare LED/USB PA11, sospensione/ripresa, sleep/wakeup e consumi | Nessun pilotaggio simultaneo del pin condiviso, UART/CAN recuperano, consumo misurato accettabile | Contesa pin, consumo persistente, mancato wake | Oscilloscopio, misura corrente, trace; rientro firmware originale |
| H13 | Piano specifico separato per ACC/HAS/Q4/DYNO/freni/ESC-TC su banco qualificato, comprendendo rilascio e guasti | Ogni requisito safety soddisfatto e revisione delle ECU coincidente | Nessuna prova veicolo automatica; qualsiasi effetto inatteso arresta il test | Trace UDS/CAN, condizioni e criteri firmati; ripristino controllo OEM documentato |
| H14 | Solo target sacrificabile e recuperabile: perdita USB/alimentazione durante read, erase, write e verify | Nessun falso successo; copia originale conservata; recovery ripetibile | Irrecuperabilità, dati sovrascritti o boot non verificato | Momento esatto guasto, log persistenti, hash; tool recovery e originale disponibili prima della prova |

H05/H06 non risolvono da sole la protezione cross-flash: occorre prima implementare
e testare l'associazione fisica del ruolo. Il file JSON può dichiarare un ruolo
senza dimostrarlo. Non effettuare il caso negativo scrivendo deliberatamente un
firmware sbagliato su un dispositivo utile.

## Windows e installer

Usare VM Windows pulita con snapshot e utente standard. Nessuna installazione
preesistente AlfaRaceX nel profilo. Dopo la disinstallazione attendere la scomparsa della cartella installata prima
di reinstallare nello stesso percorso (cleanup Velopack differito). Annotare versione Windows e assenza/presenza
WebView2. Non utilizzare il catalogo backup reale per test distruttivi.

| ID | Procedura | PASS | FAIL / evidenza / recupero |
|---|---|---|---|
| W01 | Installare Setup da cartella senza sorgenti/SDK/.NET; WebView2 presente | Avvio, sei viste, asset locali e SQLite; nessun riferimento a directory sviluppatore | Log installer + app + screenshot; revert snapshot |
| W02 | Prima esecuzione con WebView2 assente | Prerequisito installato oppure errore preciso senza falso completamento | Provare anche rete assente; log bootstrap; revert snapshot |
| W03 | Primo avvio offline; rifiutare disclaimer, poi accettare IT/EN; riavviare | Operazioni bloccate prima, accettazione UTC/versione persistita dopo; UI offline fruibile | Log bridge/DB, screenshot; reset solo profilo VM |
| W04 | Creare dati di prova, backup+metadati noti e hash; upgrade da desktop-v0.1.0 | Dati e hash invariati, schema leggibile, avvio reale riuscito | Export DB e hash prima/dopo; rollback snapshot |
| W05 | Reinstallare; disinstallare; reinstallare | Backup/configurazioni previste conservati, binari/collegamenti coerenti, nessun dato reale rimosso | Inventario file/registry e log; restore snapshot |
| W06 | Negare scrittura cartella dati/DB e rendere SQLite non accessibile | Errore comprensibile e nessuna operazione hardware iniziata | Stack/log di startup e bridge; ripristinare ACL della VM |
| W07 | Manifest irraggiungibile/malformato, hash errato, download interrotto | Nessun firmware preparato né bottone flash valido; niente file parziale accettato | Log e directory temporanea; ripristinare rete test |
| W08 | Eseguire smoke di base e UI reale sull'app pubblicata e installata | Exit 0, disclaimer registrato, tutte le viste e Bootstrap caricati | Conservare `%TEMP%/AlfaRaceX-Smoke-*`, eventi WebView2; no bypass sandbox |
| W09 | Chiudere/cancellare operazione lunga e riavviare dopo crash controllato | UI reattiva, stato finale coerente e recovery esplicito | Journal di rilevamento aggiunto; rollback e recovery fisico restano da provare |

La qualifica finale richiede evidenze H e W pertinenti, matrice aggiornata e controllo
dei binari esatti distribuiti. Un risultato su eseguibile differente non qualifica
l'installer consegnato.
