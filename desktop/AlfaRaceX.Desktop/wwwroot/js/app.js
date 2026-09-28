(() => {
  'use strict';
  const state = { page: 'dashboard', backupRole: 'BH', busy: false, dfuCount: null, appInfo: {}, backups: [], manifest: null, disclaimerAccepted: false, pedal: null };
  const $ = id => document.getElementById(id);
  const esc = v => String(v ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const host = (action, payload = {}) => window.chrome.webview.postMessage({ action, payload });
  const obdRows = new Map();
  function renderObd() {
    $('obdReadings').innerHTML = '<table class="table"><thead><tr><th>Parametro</th><th>Campione</th><th>Ora</th><th>Esito</th></tr></thead><tbody>' +
      [...obdRows.values()].map(r => `<tr><td>${esc(r.name)}</td><td>${r.value == null ? '—' : esc(r.value.toLocaleString('it-IT', { maximumFractionDigits: 2 }) + ' ' + r.unit)}</td><td>${r.observedUtc ? esc(new Date(r.observedUtc).toLocaleTimeString('it-IT')) : '—'}</td><td>${esc(r.message || `${r.provider} · ${Math.round(r.latencyMs)} ms`)}</td></tr>`).join('') + '</tbody></table>';
  }
  $('obdReadBtn').addEventListener('click', () => {
    const port = Number($('obdPort').value);
    if (!$('obdHost').value.trim() || !Number.isInteger(port) || port < 1 || port > 65535) {
      toast('Indica indirizzo e porta TCP dell’adattatore.', 'OBD'); return;
    }
    host('obdRead', { host: $('obdHost').value.trim(), port });
  });
  $('obdCancelBtn').addEventListener('click', () => host('cancelOperation'));

  // Reuse the same real controls in every page where an operation can start.
  const operationParts = { operationText: 'data-operation-text', operationPercent: 'data-operation-percent', operationProgress: 'data-operation-progress', cancelBtn: 'data-cancel-operation' };
  ['backup', 'restore'].forEach(page => {
    const card = $('operationCard').cloneNode(true);
    card.removeAttribute('id');
    Object.entries(operationParts).forEach(([id, attribute]) => {
      const element = card.querySelector(`#${id}`);
      element.removeAttribute('id');
      element.setAttribute(attribute, '');
    });
    $(`page-${page}`).appendChild(card);
  });

  function renderOperation(message, progress) {
    document.querySelectorAll('#operationText,[data-operation-text]').forEach(x => { x.textContent = message; });
    if (progress == null) return;
    const value = Math.max(0, Math.min(100, Number(progress) || 0));
    document.querySelectorAll('#operationPercent,[data-operation-percent]').forEach(x => { x.textContent = `${value}%`; });
    document.querySelectorAll('#operationProgress,[data-operation-progress]').forEach(x => { x.style.width = `${value}%`; });
  }

  function showPage(name) {
    state.page = name;
    document.querySelectorAll('.page').forEach(x => x.classList.toggle('active', x.id === `page-${name}`));
    document.querySelectorAll('.nav-item').forEach(x => x.classList.toggle('active', x.dataset.page === name));
    const titles = { dashboard:'Dashboard', update:'Aggiornamento', backup:'Backup', restore:'Ripristino', logs:'Log', info:'Informazioni', pedal:'PedalRaceX' };
    $('pageTitle').textContent = titles[name] || 'AlfaRaceX';
    if (name === 'restore') host('listBackups');
    if (name === 'logs') host('getLogs');
    if (name === 'pedal' && state.disclaimerAccepted) host('pedalPorts');
  }

  function toast(text, title = 'AlfaRaceX') {
    $('toastTitle').textContent = title;
    $('toastText').textContent = text;
    $('toast').classList.add('show');
    clearTimeout(toast.t);
    toast.t = setTimeout(() => $('toast').classList.remove('show'), 4200);
  }

  function renderDashboard(d) {
    state.dfuCount = d.deviceError || d.dfuCount == null ? null : Number(d.dfuCount);
    updateControls();
    $('metricFirmware').textContent = d.firmwareVersion || '—';
    $('metricChannel').textContent = d.channel || '—';
    $('metricBackups').textContent = d.backupCount ?? 0;
    $('metricApp').textContent = `v${d.appVersion || '—'}`;
    $('dataRoot').textContent = d.dataRoot || '—';
    $('firmwareBadge').textContent = `Firmware disponibile ${d.firmwareVersion || '—'}`;
    if (d.deviceError || d.dfuCount == null) {
      $('metricDevice').textContent = 'Stato non disponibile';
      $('sideStatus').textContent = 'Errore USB';
      $('sideStatusDot').classList.remove('ok');
      $('metricDeviceHint').textContent = d.deviceError || 'Enumerazione non completata';
      return;
    }
    $('metricDeviceHint').textContent = 'Collega un solo modulo';
    const connected = Number(d.dfuCount) === 1;
    $('metricDevice').textContent = connected ? 'Rilevato' : (Number(d.dfuCount) > 1 ? `${d.dfuCount} rilevati` : 'Non rilevato');
    $('sideStatus').textContent = connected ? 'Modulo rilevato' : (Number(d.dfuCount) > 1 ? 'Più moduli' : 'Non rilevato');
    $('sideStatusDot').classList.toggle('ok', connected);
  }

  function renderManifest(m) {
    state.manifest = m;
    $('firmwareBadge').textContent = `Firmware disponibile ${m.version}`;
    $('targets').innerHTML = (m.targets || []).map(t => `
      <article class="target-card ${t.prepared ? 'prepared' : ''}">
        <span class="state-pill ${t.prepared ? 'ok' : ''}">${t.prepared ? 'SHA-256 OK' : 'DA PREPARARE'}</span>
        <h3>${esc(t.id)}</h3>
        <div class="port">${esc(t.label)} · ${esc(t.portHint)}</div>
        <div class="hash" title="${esc(t.sha256)}">SHA-256 ${esc(t.sha256)}</div>
        <button class="btn-arx flash-btn" data-role="${esc(t.id)}" ${t.prepared && !state.busy ? '' : 'disabled'}>Programma ${esc(t.id)}</button>
      </article>`).join('');
    document.querySelectorAll('.flash-btn').forEach(b => b.addEventListener('click', () => {
      if (confirm(`Programmare il modulo ${b.dataset.role}?\n\nCollega un solo modulo in DFU e verifica la porta fisica. Il ruolo MCU non è riconosciuto automaticamente. Verrà creato un backup prima della scrittura.`))
        host('flashRole', { role: b.dataset.role });
    }));
    updateControls();
  }

  function renderBackups(items) {
    state.backups = items || [];
    $('backupEmpty').style.display = state.backups.length ? 'none' : 'block';
    $('backupTable').innerHTML = state.backups.map(b => {
      const dt = new Date(b.createdUtc).toLocaleString('it-IT');
      const sha = (b.sha256 || '').slice(0, 14) + '…';
      return `<tr>
        <td>${esc(dt)}</td><td><strong>${esc(b.role)}</strong></td>
        <td>${esc(b.fileName)}${b.exists ? '' : ' <small style="color:#ff657d">(mancante)</small>'}</td>
        <td><code title="${esc(b.sha256)}">${esc(sha)}</code></td><td>${esc(b.source)}</td>
        <td><div class="table-actions"><button class="mini-btn locate" data-id="${b.id}">Apri</button><button class="mini-btn danger restore" data-id="${b.id}" ${b.exists && !state.busy ? '' : 'disabled'}>Ripristina</button></div></td>
      </tr>`;
    }).join('');
    document.querySelectorAll('.locate').forEach(b => b.addEventListener('click', () => host('openBackupLocation', { id: Number(b.dataset.id) })));
    document.querySelectorAll('.restore').forEach(b => b.addEventListener('click', () => {
      const item = state.backups.find(x => x.id === Number(b.dataset.id));
      if (item && confirm(`Ripristinare il modulo ${item.role} dal backup ${item.fileName}?\n\nL'operazione riscrive la Flash del modulo collegato.`))
        host('restoreBackup', { id: Number(b.dataset.id) });
    }));
    updateControls();
  }

  function logRow(x) {
    return `<div class="log-row ${esc(x.level)}"><span class="ts">${esc(new Date(x.createdUtc).toLocaleString('it-IT'))}</span><span class="lvl">${esc(x.level)}</span><span class="cat">${esc(x.category)}</span><span>${esc(x.message)}</span></div>`;
  }

  function renderLogs(items) {
    $('logList').innerHTML = (items || []).slice(0, 500).map(logRow).join('');
  }

  function appendLog(item) {
    // A rejected history request itself emits a log. Never request history from this event.
    const list = $('logList');
    list.insertAdjacentHTML('afterbegin', logRow(item));
    while (list.children.length > 500) list.lastElementChild.remove();
  }

  const pedalMaps = ['Non confermata', 'Bypass', 'All Weather', 'Natural', 'Dynamic', 'Race'];
  const pedalComms = ['Non verificata', 'In attesa', 'Mappa confermata', 'Timeout', 'Errore invio', 'Risposta non valida', 'Mappa diversa'];
  function clearPedal() {
    state.pedal = null;
    $('pedalComm').textContent = 'Non verificata';
    ['pedalRequested','pedalApplied','pedalVehicle'].forEach(id => $(id).textContent = '—');
    $('pedalObserved').textContent = 'Collegamento non verificato. Seleziona C1 e leggi lo stato.';
    $('pedalReplyAge').textContent = 'Nessuna risposta osservata';
    $('pedalDiagnostics').textContent = 'Nessun dato corrente.';
    $('pedalPermission').textContent = 'Lettura necessaria';
    updateControls();
  }
  function pedalHelp() {
    const mode = Number($('pedalMode').value);
    $('pedalModeHelp').textContent = mode === 0 ? 'Disabilitato: richiede Bypass prima di interrompere i comandi.' :
      mode === 1 ? 'Automatic Map segue il selettore DNA: A, N, D o Race.' :
      mode === 7 ? 'Hybrid Align usa Natural in A/N/D e Race in Race.' :
      mode === 8 ? 'Kids Limiter usa All Weather a potenza minima. Richiede acceleratore 0% oltre 100 km/h oppure 3000 rpm diesel / 4000 rpm benzina. Non sostituisce il controllo del conducente.' :
      'Mappa manuale: non segue i cambi del selettore DNA.';
  }
  function renderPedal(d) {
    if (d.port !== $('pedalPort').value) return;
    const p = d.status;
    state.pedal = { ...p, port: d.port };
    $('pedalComm').textContent = pedalComms[p.communication];
    $('pedalRequested').textContent = pedalMaps[p.requestedMap];
    $('pedalApplied').textContent = pedalMaps[p.appliedMap];
    $('pedalVehicle').textContent = !p.engineKnown ? 'Non disponibile' : p.engineRunning ? 'Motore acceso' : 'Motore spento';
    $('pedalPermission').textContent = p.canConfigure ? 'Configurazione consentita da C1' : 'Configurazione bloccata: stato assente, scaduto o vettura in movimento';
    $('pedalObserved').textContent = `C1 · ${d.port} · lettura ${new Date(d.observedUtc).toLocaleString('it-IT')}${p.disablePending ? ' · Bypass ancora da confermare' : ''}`;
    $('pedalReplyAge').textContent = p.replyAgeMs === 4294967295 ? 'Nessuna risposta osservata' : `Ultimo byte ricevuto ${Math.floor(p.replyAgeMs / 1000)} s prima della lettura`;
    $('pedalDiagnostics').textContent = `Invii: ${p.transmissions} · Risposte: ${p.replies} · Errori: ${p.errors} · timeout risposta 100 ms · tentativi distanziati oltre 400 ms`;
    $('pedalMode').value = String(p.mode); $('pedalPower').value = p.power;
    pedalHelp(); updateControls();
  }

  function updateControls() {
    const allowed = state.disclaimerAccepted && !state.busy;
    $('obdReadBtn').disabled = !allowed;
    $('obdHost').disabled = !allowed;
    $('obdPort').disabled = !allowed;
    const deviceReady = allowed && state.dfuCount === 1;
    $('pedalReadBtn').disabled = !allowed || !$('pedalPort').value;
    $('pedalPortsBtn').disabled = !allowed;
    $('pedalPort').disabled = !allowed;
    $('pedalMode').disabled = !allowed;
    $('pedalPower').disabled = !allowed;
    $('pedalApplyBtn').disabled = !allowed || !state.pedal?.canConfigure || state.pedal.port !== $('pedalPort').value;
    $('prepareBtn').disabled = !allowed;
    $('backupBtn').disabled = !deviceReady;
    $('importBtn').disabled = !allowed;
    $('cancelBtn').hidden = !state.busy;
    document.querySelectorAll('[data-cancel-operation]').forEach(b => { b.hidden = !state.busy; });
    document.querySelectorAll('.flash-btn').forEach(b => {
      b.disabled = !deviceReady || !state.manifest?.targets?.some(t => t.id === b.dataset.role && t.prepared);
    });
    document.querySelectorAll('.restore').forEach(b => {
      b.disabled = !deviceReady || !state.backups.some(x => x.id === Number(b.dataset.id) && x.exists);
    });
  }

  function setBusy(value) {
    state.busy = !!value;
    updateControls();
  }

  window.chrome.webview.addEventListener('message', ev => {
    const msg = ev.data || {};
    const d = msg.data;
    switch (msg.type) {
      case 'appInfo':
        state.appInfo = d;
        state.disclaimerAccepted = !!d.disclaimerAccepted;
        $('infoVersion').textContent = `v${d.appVersion}`;
        $('infoData').textContent = d.dataRoot;
        $('infoBackup').textContent = d.backupRoot;
        $('infoDb').textContent = d.database;
        $('disclaimerGate').hidden = state.disclaimerAccepted;
        updateControls();
        $('recoveryNotice').hidden = !(d.interruptedOperations > 0);
        $('recoveryNotice').textContent = `Il registro contiene ${d.interruptedOperations || 0} operazioni interrotte senza esito. Consulta i log e verifica backup e dispositivo prima di nuove scritture. Nessuna operazione viene ripresa automaticamente.`;
        break;
      case 'disclaimerAccepted':
        state.disclaimerAccepted = true;
        updateControls();
        $('disclaimerGate').hidden = true;
        toast('Disclaimer registrato. / Disclaimer accepted.', 'AlfaRaceX');
        break;
      case 'dashboard': renderDashboard(d); break;
      case 'obdReset': obdRows.clear(); renderObd(); $('obdStatus').textContent = d.message; $('obdFaults').textContent = 'DTC motore non letti.'; break;
      case 'obdFaults': $('obdFaults').textContent = d.message; break;
      case 'obdReading':
      case 'obdSignalError': obdRows.set(d.id, d); renderObd(); break;
      case 'obdClosed': $('obdStatus').textContent = d.message; break;
      case 'manifest': renderManifest(d); break;
      case 'manifestError': toast(`Manifest firmware non disponibile: ${d.message}`, 'Connessione'); break;
      case 'backups': renderBackups(d); break;
      case 'logs': renderLogs(d); break;
      case 'log': appendLog(d); break;
      case 'pedalPorts': {
        const selected = $('pedalPort').value;
        $('pedalPort').replaceChildren(new Option('Seleziona una porta', ''), ...(d || []).map(p => new Option(p, p)));
        if ((d || []).includes(selected)) $('pedalPort').value = selected;
        else clearPedal();
        updateControls(); break;
      }
      case 'pedalStatus': renderPedal(d); break;
      case 'pedalUnavailable': clearPedal(); break;
      case 'busy':
        setBusy(d.value);
        $('obdReadBtn').disabled = d.value || !state.disclaimerAccepted;
        $('obdCancelBtn').disabled = !(d.value && d.category === 'OBD');
        break;
      case 'operationProgress':
        renderOperation(d.message || 'Operazione in corso…', d.progress ?? 0);
        break;
      case 'operationComplete':
        renderOperation(d.message || 'Operazione completata.', 100);
        toast(d.message || 'Operazione completata.', 'Completato');
        host('refreshDashboard');
        break;
      case 'operationFailed': renderOperation(`Operazione fallita: ${d.message || 'consulta i log.'}`); toast(d.message || 'Operazione fallita.', 'Errore'); break;
      case 'operationCancelled': renderOperation('Operazione annullata.'); toast('Operazione annullata.', 'AlfaRaceX'); break;
      case 'error': toast(d.message || 'Errore imprevisto.', 'Errore'); break;
    }
  });

  document.querySelectorAll('.nav-item').forEach(b => b.addEventListener('click', () => showPage(b.dataset.page)));
  document.querySelectorAll('[data-go]').forEach(b => b.addEventListener('click', () => showPage(b.dataset.go)));
  document.querySelectorAll('#backupRoles .role').forEach(b => b.addEventListener('click', () => {
    state.backupRole = b.dataset.role;
    document.querySelectorAll('#backupRoles .role').forEach(x => x.classList.toggle('active', x === b));
  }));
  $('refreshBtn').addEventListener('click', () => { host('refreshDashboard'); host('loadManifest'); });
  $('prepareBtn').addEventListener('click', () => host('prepareUpdate'));
  $('backupBtn').addEventListener('click', () => {
    if (confirm(`Creare un backup della Flash interna del modulo ${state.backupRole}?\n\nCollega un solo modulo in DFU.`))
      host('createBackup', { role: state.backupRole });
  });
  $('openDataBtn').addEventListener('click', () => host('openDataFolder'));
  $('openBackupBtn').addEventListener('click', () => host('openBackupFolder'));
  $('importBtn').addEventListener('click', () => host('importBackup', { role: $('importRole').value }));
  $('cancelBtn').addEventListener('click', () => host('cancelOperation'));
  document.querySelectorAll('[data-cancel-operation]').forEach(b => b.addEventListener('click', () => host('cancelOperation')));
  $('clearLogsBtn').addEventListener('click', () => { if (confirm('Pulire la cronologia dei log?')) host('clearLogs'); });
  $('repoBtn').addEventListener('click', () => host('openExternal', { url: 'https://github.com/AriotaG/AlfaRaceX-Framework' }));
  $('disclaimerRepoBtn').addEventListener('click', () => host('openExternal', { url: 'https://github.com/AriotaG/AlfaRaceX-Framework/blob/main/DISCLAIMER.md' }));
  $('disclaimerAcceptBtn').addEventListener('click', () => host('acceptDisclaimer'));
  $('disclaimerExitBtn').addEventListener('click', () => host('exitApplication'));

  $('pedalPortsBtn').addEventListener('click', () => host('pedalPorts'));
  $('pedalPort').addEventListener('change', clearPedal);
  $('pedalMode').addEventListener('change', pedalHelp);
  $('pedalReadBtn').addEventListener('click', () => host('pedalRead', { port: $('pedalPort').value }));
  $('pedalApplyBtn').addEventListener('click', () => {
    const mode = Number($('pedalMode').value), power = Number($('pedalPower').value);
    if (!Number.isInteger(power) || power < -10 || power > 10) { toast('Potenza ammessa: intero da −10 a +10.', 'PedalRaceX'); return; }
    if (confirm('Applicare questa modalità PedalRaceX alla vettura ferma? La risposta del pedale può cambiare. La conferma della mappa va verificata con una nuova lettura.'))
      host('pedalApply', { port: $('pedalPort').value, mode, power });
  });
  updateControls();
  host('initialize');
})();
