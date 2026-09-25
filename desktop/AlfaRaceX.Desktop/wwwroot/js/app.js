(() => {
  'use strict';
  const state = { page: 'dashboard', backupRole: 'BH', busy: false, appInfo: {}, backups: [], manifest: null, disclaimerAccepted: false };
  const $ = id => document.getElementById(id);
  const esc = v => String(v ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const host = (action, payload = {}) => window.chrome.webview.postMessage({ action, payload });

  function showPage(name) {
    state.page = name;
    document.querySelectorAll('.page').forEach(x => x.classList.toggle('active', x.id === `page-${name}`));
    document.querySelectorAll('.nav-item').forEach(x => x.classList.toggle('active', x.dataset.page === name));
    const titles = { dashboard:'Dashboard', update:'Aggiornamento', backup:'Backup', restore:'Ripristino', logs:'Log', info:'Informazioni' };
    $('pageTitle').textContent = titles[name] || 'AlfaRaceX';
    if (name === 'restore') host('listBackups');
    if (name === 'logs') host('getLogs');
  }

  function toast(text, title = 'AlfaRaceX') {
    $('toastTitle').textContent = title;
    $('toastText').textContent = text;
    $('toast').classList.add('show');
    clearTimeout(toast.t);
    toast.t = setTimeout(() => $('toast').classList.remove('show'), 4200);
  }

  function renderDashboard(d) {
    $('metricFirmware').textContent = d.firmwareVersion || '—';
    $('metricChannel').textContent = d.channel || '—';
    $('metricBackups').textContent = d.backupCount ?? 0;
    $('metricApp').textContent = `v${d.appVersion || '—'}`;
    $('dataRoot').textContent = d.dataRoot || '—';
    $('firmwareBadge').textContent = `Firmware disponibile ${d.firmwareVersion || '—'}`;
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
  }

  function renderLogs(items) {
    $('logList').innerHTML = (items || []).map(x => `<div class="log-row ${esc(x.level)}"><span class="ts">${esc(new Date(x.createdUtc).toLocaleString('it-IT'))}</span><span class="lvl">${esc(x.level)}</span><span class="cat">${esc(x.category)}</span><span>${esc(x.message)}</span></div>`).join('');
  }

  function setBusy(value) {
    state.busy = !!value;
    $('prepareBtn').disabled = state.busy;
    $('backupBtn').disabled = state.busy;
    $('importBtn').disabled = state.busy;
    $('cancelBtn').hidden = !state.busy;
    if (state.manifest) renderManifest(state.manifest);
    renderBackups(state.backups);
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
        break;
      case 'disclaimerAccepted':
        state.disclaimerAccepted = true;
        $('disclaimerGate').hidden = true;
        toast('Disclaimer registrato. / Disclaimer accepted.', 'AlfaRaceX');
        break;
      case 'dashboard': renderDashboard(d); break;
      case 'manifest': renderManifest(d); break;
      case 'manifestError': toast(`Manifest firmware non disponibile: ${d.message}`, 'Connessione'); break;
      case 'backups': renderBackups(d); break;
      case 'logs': renderLogs(d); break;
      case 'log': if (state.page === 'logs') host('getLogs'); break;
      case 'busy': setBusy(d.value); break;
      case 'operationProgress':
        $('operationText').textContent = d.message || 'Operazione in corso…';
        $('operationPercent').textContent = `${d.progress ?? 0}%`;
        $('operationProgress').style.width = `${Math.max(0, Math.min(100, d.progress ?? 0))}%`;
        break;
      case 'operationComplete':
        $('operationText').textContent = d.message || 'Operazione completata.';
        $('operationPercent').textContent = '100%';
        $('operationProgress').style.width = '100%';
        toast(d.message || 'Operazione completata.', 'Completato');
        host('refreshDashboard');
        break;
      case 'operationFailed': toast(d.message || 'Operazione fallita.', 'Errore'); break;
      case 'operationCancelled': toast('Operazione annullata.', 'AlfaRaceX'); break;
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
  $('clearLogsBtn').addEventListener('click', () => { if (confirm('Pulire la cronologia dei log?')) host('clearLogs'); });
  $('repoBtn').addEventListener('click', () => host('openExternal', { url: 'https://github.com/AriotaG/AlfaRaceX-Framework' }));
  $('disclaimerRepoBtn').addEventListener('click', () => host('openExternal', { url: 'https://github.com/AriotaG/AlfaRaceX-Framework/blob/main/DISCLAIMER.md' }));
  $('disclaimerAcceptBtn').addEventListener('click', () => host('acceptDisclaimer'));
  $('disclaimerExitBtn').addEventListener('click', () => host('exitApplication'));

  host('initialize');
})();
