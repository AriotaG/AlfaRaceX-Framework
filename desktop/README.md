# AlfaRaceX Desktop

Applicazione Windows installabile che sostituisce la precedente distribuzione portable come frontend ufficiale per le operazioni host-side AlfaRaceX.

## Architettura

- .NET 8 / WPF: shell nativa e ciclo di vita Windows.
- WebView2: UI HTML/CSS/JavaScript locale.
- Bootstrap locale: copiato in `wwwroot/vendor` durante la build; nessuna dipendenza CDN a runtime.
- Servizi DFU: riuso diretto del core già validato in `updater/AlfaRaceX.Updater`.
- SQLite: catalogo backup e cronologia persistente.
- Velopack: installazione e base per gli aggiornamenti dell'app.

I dati persistenti sono in `%LOCALAPPDATA%\AlfaRaceX\Desktop` e restano separati dalla directory di installazione.

## Sidebar

Dashboard, Aggiornamento, Backup, Ripristino, Log, Informazioni. Non esiste una pagina Impostazioni.

## Build

La pipeline `.github/workflows/build-desktop.yml` compila su Windows, esegue uno smoke test dell'eseguibile pubblicato, crea `AlfaRaceX-Setup.exe` con Velopack, installa il Setup in una directory temporanea e riesegue lo smoke test sull'eseguibile installato.