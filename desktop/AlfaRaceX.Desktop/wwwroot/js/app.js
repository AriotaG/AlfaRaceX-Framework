(() => {
  'use strict';
  const state = { page: 'dashboard', backupRole: 'BH', busy: false, appInfo: {}, backups: [], disclaimerAccepted: false };
  const $ = id => document.getElementById(id);
  const esc = v => String(v ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const bridge = window.chrome?.webview;
  const host = (action, payload = {}) => {
    if (!bridge) { $('legalText').textContent = 'Il collegamento con l’app Windows non è disponibile. Riavvia AlfaRaceX.'; return; }
    bridge.postMessage({ action, payload });
  };

  function showPage(name) {
    state.page = name;
    if (name === 'dashboard') document.querySelector('.dashboard-log').before($('operationCard'));
    else $('page-' + name).append($('operationCard'));
    document.querySelectorAll('.page').forEach(x => x.classList.toggle('active', x.id === `page-${name}`));
    document.querySelectorAll('.nav-item').forEach(x => x.classList.toggle('active', x.dataset.page === name));
    const titles = { dashboard:'Dashboard', update:'Aggiornamento', backup:'Backup', restore:'Ripristino', logs:'Log', info:'Informazioni' };
    $('pageTitle').textContent = titles[name] || 'AlfaRaceX';
    if (state.disclaimerAccepted && name === 'restore') host('listBackups');
    if (state.disclaimerAccepted && name === 'logs') host('getLogs');
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
    $('firmwareBadge').textContent = `Firmware ${d.firmwareVersion || '—'}`;
    const connected = Number(d.dfuCount) === 1;
    $('metricDevice').textContent = connected ? 'Rilevato' : (Number(d.dfuCount) > 1 ? `${d.dfuCount} rilevati` : 'Non rilevato');
    $('sideStatus').textContent = connected ? 'Modulo rilevato' : (Number(d.dfuCount) > 1 ? 'Più moduli' : 'Non rilevato');
    $('sideStatusDot').classList.toggle('ok', connected);
    $('connectionDot').classList.toggle('ok', connected);
    $('connectionTitle').textContent = connected ? 'Dispositivo DFU rilevato' : 'Dispositivo non rilevato';
    $('lastCheck').textContent = d.lastCheckUtc ? new Date(d.lastCheckUtc).toLocaleString('it-IT') : '—';
  }

  function renderManifest(m) {
    state.manifest = m;
    $('releaseCatalog').textContent = (m.releases || []).join(' · ') || 'Catalogo non disponibile';
    $('dashboardTargets').innerHTML = (m.targets || []).map(t => `<div class="module-row"><div class="module-badge ${esc(t.id)}">${esc(t.id)}<small>${esc(t.id === 'BH' ? 'Body Hub' : t.id === 'C1' ? 'CAN 1' : 'CAN 2')}</small></div><div><small>Versione sul dispositivo</small><span>Non interrogabile in DFU</span></div><div><small>Versione disponibile</small><span>${esc(m.version)}</span></div><div class="hash-cell"><small>Checksum (SHA-256)</small><span title="${esc(t.sha256)}">${esc(t.sha256.slice(0,8))}…${esc(t.sha256.slice(-4))}</span></div><div class="module-status ${t.prepared ? 'ready' : ''}"><small>Stato pacchetto</small>${t.prepared ? '● Verificato' : '● Da scaricare'}</div><button class="mini-btn" data-target="${esc(t.id)}">Dettagli</button></div>`).join('');
    document.querySelectorAll('[data-target]').forEach(b => b.onclick = () => showPage('update'));
    $('firmwareBadge').textContent = `Firmware ${m.version}`;
    $('targets').innerHTML = (m.targets || []).map(t => `
      <article class="target-card ${t.prepared ? 'prepared' : ''}">
        <span class="state-pill ${t.prepared ? 'ok' : ''}">${t.prepared ? 'VERIFICATO' : 'DA PREPARARE'}</span>
        <h3>${esc(t.id)}</h3>
        <div class="port">${esc(t.label)} · ${esc(t.portHint)}</div>
        <div class="hash" title="${esc(t.sha256)}">SHA-256 ${esc(t.sha256)}</div>
        <button class="btn-arx flash-btn" data-role="${esc(t.id)}" ${t.prepared && !state.busy ? '' : 'disabled'}>Programma ${esc(t.id)}</button>
      </article>`).join('');
    document.querySelectorAll('.flash-btn').forEach(b => b.addEventListener('click', () => {
      if (confirm(`Programmare il modulo ${b.dataset.role}?\n\nCollega un solo modulo in DFU e verifica di aver creato il backup.`))
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
        <td><code title="${esc(b.sha256)}">${esc(sha)}</code></td><td>${esc(b.source)}<small class="backup-note">${esc(b.notes || '')}</small></td>
        <td><div class="table-actions"><button class="mini-btn notes" data-id="${b.id}">Note</button><button class="mini-btn verify" data-id="${b.id}" ${b.exists ? '' : 'disabled'}>Verifica</button><button class="mini-btn locate" data-id="${b.id}">Apri</button><button class="mini-btn danger restore" data-id="${b.id}" ${b.exists && !state.busy ? '' : 'disabled'}>Ripristina</button></div></td>
      </tr>`;
    }).join('');
    document.querySelectorAll('.notes').forEach(b => b.onclick = () => {
      const item = state.backups.find(x => x.id === Number(b.dataset.id));
      const notes = prompt('Note del backup (massimo 2000 caratteri)', item.notes || '');
      if (notes !== null) host('saveBackupNotes', { id: item.id, notes });
    });
    document.querySelectorAll('.verify').forEach(b => b.onclick = () => host('verifyBackup', { id: Number(b.dataset.id) }));
    document.querySelectorAll('.locate').forEach(b => b.addEventListener('click', () => host('openBackupLocation', { id: Number(b.dataset.id) })));
    document.querySelectorAll('.restore').forEach(b => b.addEventListener('click', () => {
      const item = state.backups.find(x => x.id === Number(b.dataset.id));
      if (item && confirm(`Ripristinare il modulo ${item.role} dal backup ${item.fileName}?\n\nL'operazione riscrive la Flash del modulo collegato.`))
        host('restoreBackup', { id: Number(b.dataset.id) });
    }));
  }

  function renderLogs(items) {
    state.logs = items || [];
    $('logList').innerHTML = (items || []).map(x => `<div class="log-row ${esc(x.level)}"><span class="ts">${esc(new Date(x.createdUtc).toLocaleString('it-IT'))}</span><span class="lvl">${esc(x.level)}</span><span class="cat">${esc(x.category)}</span><span>${esc($('logMode').value === 'technical' ? x.message : String(x.message).split('\n')[0])}</span></div>`).join('');
  }

  function setBusy(value) {
    state.busy = !!value;
    $('prepareBtn').disabled = state.busy;
    $('downloadDashboard').disabled = state.busy;
    $('checkDashboard').disabled = state.busy;
    $('refreshBtn').disabled = state.busy;
    $('backupBtn').disabled = state.busy;
    $('importBtn').disabled = state.busy;
    $('cancelBtn').hidden = !state.busy;
    if (state.manifest) renderManifest(state.manifest);
    renderBackups(state.backups);
  }

  bridge?.addEventListener('message', ev => {
    const msg = ev.data || {};
    const d = msg.data;
    switch (msg.type) {
      case 'appInfo':
        state.appInfo = d;
        state.disclaimerAccepted = !!d.disclaimerAccepted;
        $('sideVersion').textContent = `v${d.appVersion}`;
        $('legalText').textContent = d.disclaimerText || 'Condizioni non disponibili';
        $('disclaimerAcceptBtn').disabled = !d.disclaimerText;
        $('shell').inert = !state.disclaimerAccepted;
        $('infoVersion').textContent = `v${d.appVersion}`;
        $('infoData').textContent = d.dataRoot;
        $('infoBackup').textContent = d.backupRoot;
        $('infoDb').textContent = d.database;
        $('disclaimerGate').hidden = state.disclaimerAccepted;
        break;
      case 'disclaimerAccepted':
        state.disclaimerAccepted = true;
        $('shell').inert = false;
        $('disclaimerGate').hidden = true;
        toast('Disclaimer registrato. / Disclaimer accepted.', 'AlfaRaceX');
        break;
      case 'dashboard': renderDashboard(d); break;
      case 'manifest': renderManifest(d); break;
      case 'manifestError': toast(`Manifest firmware non disponibile: ${d.message}`, 'Connessione'); break;
      case 'backups': renderBackups(d); break;
      case 'operations':
        const names = { running: 'In corso', completed: 'Completata', cancelled: 'Annullata', failed: 'Fallita' };
        $('operationHistory').innerHTML = d.map(x => `<tr><td>${esc(new Date(x.startedUtc).toLocaleString('it-IT'))}</td><td>${esc(x.kind)}</td><td>${esc(x.version || '—')}</td><td>${esc(names[x.status] || x.status)}</td></tr>`).join('');
        break;
      case 'logs': renderLogs(d); $('dashboardLogs').innerHTML = $('logList').innerHTML; break;
      case 'log': renderLogs([d, ...(state.logs || [])].slice(0,500)); $('dashboardLogs').innerHTML = $('logList').innerHTML; break;
      case 'busy': setBusy(d.value); break;
      case 'operationProgress':
        const stage = /backup/i.test(d.message || '') ? 3 : d.kind === 'prepare' ? 2 : /verifica finale/i.test(d.message || '') ? 5 : 4;
        document.querySelectorAll('[data-stage]').forEach(el => el.classList.toggle('active', Number(el.dataset.stage) === stage));
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
      case 'operationFailed': $('operationText').textContent = 'Operazione fallita: ' + d.message; toast(d.message || 'Operazione fallita.', 'Errore'); break;
      case 'operationCancelled': $('operationText').textContent = 'Operazione annullata; verifica lo stato del modulo prima di scollegarlo.'; toast('Operazione annullata.', 'AlfaRaceX'); break;
      case 'notice': toast(d.message); break;
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
    if (confirm(`Creare un backup completo del modulo ${state.backupRole}?\n\nCollega un solo modulo in DFU.`))
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

  $('checkDashboard').onclick = () => { host('refreshDashboard'); host('loadManifest'); };
  $('downloadDashboard').onclick = () => host('prepareUpdate');
  $('logMode').onchange = () => renderLogs(state.logs);
  $('copyLogsBtn').onclick = () => host('copyLogs');
  $('exportLogsBtn').onclick = () => host('exportLogs');
  $('openLogsBtn').onclick = () => host('openLogFolder');
  $('shell').inert = true;
  showPage('dashboard');
  host('initialize');
})();
