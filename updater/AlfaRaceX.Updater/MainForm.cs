using System.Diagnostics;
using System.Drawing;
using System.Reflection;

namespace AlfaRaceX.Updater;

internal sealed class MainForm : Form
{
    private readonly UpdateCoordinator _coordinator = new();
    private readonly Dictionary<string, ModuleUi> _modules = new(StringComparer.OrdinalIgnoreCase);

    private readonly Label _headline = new();
    private readonly Label _statusSub = new();
    private readonly Label _updaterValue = new();
    private readonly Label _firmwareValue = new();
    private readonly Label _lastCheckValue = new();
    private readonly Label _channelValue = new();
    private readonly Label _downloadPathValue = new();

    private readonly ArxButton _check = new();
    private readonly ArxButton _download = new();
    private readonly ArxButton _update = new();
    private readonly StepStrip _steps = new();
    private readonly DataGridView _log = new();
    private readonly Panel _moduleRows = new();
    private readonly StatusDot _readyDot = new();

    private UpdateManifest? _manifest;
    private IReadOnlyList<PreparedFirmware>? _prepared;
    private CancellationTokenSource? _cts;

    public MainForm()
    {
        Text = $"AlfaRaceX Updater {AppConstants.UpdaterVersion}";
        Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);
        ClientSize = new Size(1540, 860);
        MinimumSize = new Size(1280, 760);
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = ArxTheme.Background;
        ForeColor = ArxTheme.Text;
        Font = ArxTheme.Font(9.5f);
        AutoScaleMode = AutoScaleMode.Dpi;

        BuildShell();
        FormClosing += (_, _) => _cts?.Cancel();
        Shown += (_, _) => Log($"AlfaRaceX Updater {AppConstants.UpdaterVersion} avviato.");
    }

    private void BuildShell()
    {
        var sidebar = new Panel
        {
            BackColor = ArxTheme.Sidebar,
            Dock = DockStyle.Left,
            Width = 282
        };
        Controls.Add(sidebar);

        var logo = new PictureBox
        {
            Image = LoadImage("AlfaRaceX.ARX.png"),
            SizeMode = PictureBoxSizeMode.Zoom,
            Bounds = new Rectangle(24, 22, 196, 88),
            BackColor = Color.Transparent
        };
        sidebar.Controls.Add(logo);

        sidebar.Controls.Add(MakeLabel(
            "AlfaRaceX Updater", 25, 106, 220, 26, 12.2f, FontStyle.Bold, Color.White));
        sidebar.Controls.Add(MakeLabel(
            $"v{AppConstants.UpdaterVersion}", 25, 133, 220, 24, 9.3f, FontStyle.Regular, ArxTheme.Muted));

        int navY = 178;
        AddNav(sidebar, "⌂   Dashboard", navY, true, () => _check.Focus());
        AddNav(sidebar, "↓   Aggiornamento", navY + 66, false, () => _check.Focus());
        AddNav(sidebar, "≡   Log operazioni", navY + 132, false, () => _log.Focus());
        AddNav(sidebar, "⚙   Impostazioni", navY + 198, false, ShowSettings);
        AddNav(sidebar, "ⓘ   Informazioni", navY + 264, false, ShowAbout);

        var info = new ArxPanel
        {
            Bounds = new Rectangle(18, ClientSize.Height - 144, 246, 112),
            Anchor = AnchorStyles.Left | AnchorStyles.Bottom,
            BackColor = Color.FromArgb(8, 13, 18)
        };
        info.Controls.Add(MakeLabel("STATO", 16, 12, 190, 20, 8.4f, FontStyle.Bold, ArxTheme.Muted));
        info.Controls.Add(MakeLabel("●  Pronto", 16, 36, 190, 22, 10f, FontStyle.Bold, ArxTheme.Green));
        info.Controls.Add(MakeLabel(
            $"Updater {AppConstants.UpdaterVersion}", 16, 64, 200, 20, 8.8f, FontStyle.Regular, ArxTheme.Text));
        info.Controls.Add(MakeLabel(
            "Canale firmware: Release Candidate", 16, 84, 215, 18, 8f, FontStyle.Regular, ArxTheme.Muted));
        sidebar.Controls.Add(info);

        var body = new Panel
        {
            BackColor = ArxTheme.Background,
            Dock = DockStyle.Fill,
            Padding = new Padding(18)
        };
        Controls.Add(body);
        body.BringToFront();

        BuildHeader(body);
        BuildActions(body);
        BuildModules(body);
        BuildProgress(body);
        BuildLog(body);
    }

    private void BuildHeader(Panel body)
    {
        var status = new ArxPanel
        {
            Bounds = new Rectangle(18, 18, 560, 246),
            Anchor = AnchorStyles.Top | AnchorStyles.Left,
            BackColor = ArxTheme.Panel
        };
        body.Controls.Add(status);

        _readyDot.Location = new Point(20, 22);
        status.Controls.Add(_readyDot);
        status.Controls.Add(MakeLabel("Updater pronto", 50, 16, 300, 28, 13f, FontStyle.Bold, ArxTheme.Green));
        status.Controls.Add(MakeLabel(
            "Verifica la release disponibile e prepara i tre moduli AlfaRaceX.",
            50, 46, 475, 38, 9.3f, FontStyle.Regular, ArxTheme.Muted));

        status.Controls.Add(MakeLabel("Versione updater", 22, 99, 170, 20, 8.5f, FontStyle.Regular, ArxTheme.Muted));
        _updaterValue.SetBounds(202, 97, 310, 22);
        _updaterValue.Text = AppConstants.UpdaterVersion;
        _updaterValue.Font = ArxTheme.Font(9.6f, FontStyle.Bold);
        _updaterValue.ForeColor = ArxTheme.Text;
        status.Controls.Add(_updaterValue);

        status.Controls.Add(MakeLabel("Firmware disponibile", 22, 124, 170, 20, 8.5f, FontStyle.Regular, ArxTheme.Muted));
        _firmwareValue.SetBounds(202, 122, 310, 22);
        _firmwareValue.Text = "Non verificato";
        _firmwareValue.Font = ArxTheme.Font(9.6f, FontStyle.Bold);
        _firmwareValue.ForeColor = ArxTheme.Text;
        status.Controls.Add(_firmwareValue);

        status.Controls.Add(MakeLabel("Ultima verifica", 22, 149, 170, 20, 8.5f, FontStyle.Regular, ArxTheme.Muted));
        _lastCheckValue.SetBounds(202, 147, 310, 22);
        _lastCheckValue.Text = "—";
        _lastCheckValue.ForeColor = ArxTheme.Text;
        status.Controls.Add(_lastCheckValue);

        status.Controls.Add(MakeLabel("Canale", 22, 174, 170, 20, 8.5f, FontStyle.Regular, ArxTheme.Muted));
        _channelValue.SetBounds(202, 172, 310, 22);
        _channelValue.Text = "—";
        _channelValue.ForeColor = ArxTheme.Text;
        status.Controls.Add(_channelValue);

        status.Controls.Add(MakeLabel("Percorso download", 22, 199, 170, 20, 8.5f, FontStyle.Regular, ArxTheme.Muted));
        _downloadPathValue.SetBounds(202, 197, 330, 34);
        _downloadPathValue.Text = Path.Combine(Path.GetTempPath(), "AlfaRaceX-Updater");
        _downloadPathValue.ForeColor = ArxTheme.Text;
        _downloadPathValue.AutoEllipsis = true;
        status.Controls.Add(_downloadPathValue);

        var hero = new PictureBox
        {
            Image = LoadImage("AlfaRaceX.Hero.jpg"),
            SizeMode = PictureBoxSizeMode.Zoom,
            BackColor = Color.Black,
            Bounds = new Rectangle(594, 18, 626, 246),
            Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
        };
        body.Controls.Add(hero);

        var heroShade = new Label
        {
            BackColor = Color.FromArgb(125, 0, 0, 0),
            ForeColor = Color.White,
            Text = "ALFARACEX\r\nPERFORMANCE · DIAGNOSTICS · CONTROL",
            Font = ArxTheme.Font(12f, FontStyle.Bold),
            TextAlign = ContentAlignment.BottomLeft,
            Padding = new Padding(20, 0, 0, 20),
            Dock = DockStyle.Fill
        };
        hero.Controls.Add(heroShade);
    }

    private void BuildActions(Panel body)
    {
        _check.Text = "↻   VERIFICA AGGIORNAMENTI";
        _check.SetBounds(18, 280, 370, 64);
        _check.Click += async (_, _) => await CheckAsync();
        body.Controls.Add(_check);

        _download.Text = "↓   SCARICA E VERIFICA FIRMWARE";
        _download.SetBounds(402, 280, 402, 64);
        _download.Enabled = false;
        _download.Click += async (_, _) => await DownloadAsync();
        body.Controls.Add(_download);

        _update.Text = "⚙   INSTALLA AGGIORNAMENTO";
        _update.SetBounds(818, 280, 402, 64);
        _update.Primary = true;
        _update.Enabled = false;
        _update.Click += async (_, _) => await UpdateAsync();
        body.Controls.Add(_update);
    }

    private void BuildModules(Panel body)
    {
        var box = new ArxPanel
        {
            Bounds = new Rectangle(18, 360, 1202, 220),
            Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
        };
        body.Controls.Add(box);
        box.Controls.Add(MakeLabel("Moduli firmware", 18, 12, 260, 26, 11f, FontStyle.Bold, ArxTheme.Text));
        box.Controls.Add(MakeLabel(
            "Versione disponibile, checksum e stato operativo dei controller.",
            18, 38, 610, 22, 8.6f, FontStyle.Regular, ArxTheme.Muted));

        _moduleRows.SetBounds(14, 66, 1174, 142);
        _moduleRows.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        _moduleRows.BackColor = Color.Transparent;
        box.Controls.Add(_moduleRows);

        CreateModuleRow("BH", "Modulo BH", ArxTheme.Red, 0);
        CreateModuleRow("C2", "Modulo C2", ArxTheme.Blue, 1);
        CreateModuleRow("C1", "Modulo C1", ArxTheme.Green, 2);
    }

    private void CreateModuleRow(string id, string label, Color badge, int index)
    {
        int y = index * 46;
        var row = new Panel
        {
            Bounds = new Rectangle(0, y, _moduleRows.Width, 42),
            Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right,
            BackColor = index % 2 == 0 ? Color.FromArgb(8, 13, 18) : Color.FromArgb(11, 17, 23)
        };

        var role = new Label
        {
            Bounds = new Rectangle(8, 5, 110, 32),
            Text = id,
            TextAlign = ContentAlignment.MiddleCenter,
            BackColor = badge,
            ForeColor = Color.White,
            Font = ArxTheme.Font(11f, FontStyle.Bold)
        };
        row.Controls.Add(role);

        var name = MakeLabel(label, 134, 4, 175, 32, 9.5f, FontStyle.Bold, ArxTheme.Text);
        name.TextAlign = ContentAlignment.MiddleLeft;
        row.Controls.Add(name);

        var version = MakeLabel("—", 330, 4, 175, 32, 9f, FontStyle.Regular, ArxTheme.Text);
        version.TextAlign = ContentAlignment.MiddleLeft;
        row.Controls.Add(version);

        var hash = MakeLabel("Checksum: —", 520, 4, 300, 32, 8.7f, FontStyle.Regular, ArxTheme.Muted);
        hash.TextAlign = ContentAlignment.MiddleLeft;
        row.Controls.Add(hash);

        var dot = new StatusDot { Location = new Point(850, 12), DotColor = ArxTheme.Muted };
        row.Controls.Add(dot);

        var state = MakeLabel("In attesa", 875, 4, 250, 32, 9f, FontStyle.Bold, ArxTheme.Muted);
        state.TextAlign = ContentAlignment.MiddleLeft;
        row.Controls.Add(state);

        _moduleRows.Controls.Add(row);
        _modules[id] = new ModuleUi(version, hash, state, dot);
    }

    private void BuildProgress(Panel body)
    {
        var box = new ArxPanel
        {
            Bounds = new Rectangle(18, 594, 1202, 104),
            Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
        };
        body.Controls.Add(box);
        box.Controls.Add(MakeLabel("Avanzamento operazione", 18, 8, 280, 22, 10f, FontStyle.Bold, ArxTheme.Text));
        _steps.SetBounds(14, 24, 1174, 72);
        _steps.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        box.Controls.Add(_steps);
    }

    private void BuildLog(Panel body)
    {
        var box = new ArxPanel
        {
            Bounds = new Rectangle(18, 712, 1202, 124),
            Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right
        };
        body.Controls.Add(box);
        box.Controls.Add(MakeLabel("Log operazioni", 18, 8, 220, 22, 10f, FontStyle.Bold, ArxTheme.Text));

        var clear = new Button
        {
            Text = "PULISCI LOG",
            Bounds = new Rectangle(1065, 6, 110, 26),
            Anchor = AnchorStyles.Top | AnchorStyles.Right,
            FlatStyle = FlatStyle.Flat,
            ForeColor = ArxTheme.Muted,
            BackColor = Color.FromArgb(12, 18, 24),
            Font = ArxTheme.Font(8f),
            Cursor = Cursors.Hand
        };
        clear.FlatAppearance.BorderColor = ArxTheme.Border;
        clear.Click += (_, _) => _log.Rows.Clear();
        box.Controls.Add(clear);

        _log.SetBounds(14, 38, 1174, 74);
        _log.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
        _log.BackgroundColor = Color.FromArgb(4, 8, 11);
        _log.BorderStyle = BorderStyle.None;
        _log.ReadOnly = true;
        _log.AllowUserToAddRows = false;
        _log.AllowUserToDeleteRows = false;
        _log.AllowUserToResizeRows = false;
        _log.RowHeadersVisible = false;
        _log.ColumnHeadersVisible = true;
        _log.EnableHeadersVisualStyles = false;
        _log.ColumnHeadersDefaultCellStyle.BackColor = Color.FromArgb(14, 22, 29);
        _log.ColumnHeadersDefaultCellStyle.ForeColor = ArxTheme.Muted;
        _log.ColumnHeadersDefaultCellStyle.Font = ArxTheme.Font(8f, FontStyle.Bold);
        _log.DefaultCellStyle.BackColor = Color.FromArgb(4, 8, 11);
        _log.DefaultCellStyle.ForeColor = Color.FromArgb(205, 214, 222);
        _log.DefaultCellStyle.SelectionBackColor = Color.FromArgb(22, 34, 43);
        _log.DefaultCellStyle.SelectionForeColor = Color.White;
        _log.DefaultCellStyle.Font = new Font("Consolas", 8.2f);
        _log.GridColor = Color.FromArgb(20, 29, 37);
        _log.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
        _log.Columns.Add("time", "Ora");
        _log.Columns.Add("level", "Livello");
        _log.Columns.Add("message", "Messaggio");
        _log.Columns[0].FillWeight = 14;
        _log.Columns[1].FillWeight = 12;
        _log.Columns[2].FillWeight = 74;
        box.Controls.Add(_log);
    }

    private async Task CheckAsync()
    {
        SetBusy(true);
        _download.Enabled = false;
        _update.Enabled = false;
        _prepared = null;
        _steps.Step = 1;
        _cts = new CancellationTokenSource();

        try
        {
            await CheckUpdaterVersionAsync(_cts.Token);
            Log("Controllo manifest firmware...");
            _manifest = await _coordinator.LoadManifestAsync(_cts.Token);

            _firmwareValue.Text = _manifest.Version;
            _channelValue.Text = _manifest.Channel;
            _lastCheckValue.Text = DateTime.Now.ToString("dd/MM/yyyy HH:mm");
            _downloadPathValue.Text = Path.Combine(
                Path.GetTempPath(), "AlfaRaceX-Updater", _manifest.Version);

            ApplyManifest(_manifest, "Aggiornamento disponibile", ArxTheme.Amber);
            SetHeadline(
                "Aggiornamento disponibile",
                $"Release {_manifest.Version} rilevata. Pronta per download e verifica.",
                ArxTheme.Green);

            Log($"Release {_manifest.Version} disponibile.");
            _download.Enabled = true;
        }
        catch (Exception ex)
        {
            SetHeadline("Verifica non riuscita", ex.Message, ArxTheme.Red);
            Log(ex.Message, "ERRORE");
            MessageBox.Show(this, ex.Message, "AlfaRaceX",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        finally
        {
            SetBusy(false);
        }
    }

    private async Task DownloadAsync()
    {
        if (_manifest is null) return;
        SetBusy(true);
        _download.Enabled = false;
        _update.Enabled = false;
        _cts = new CancellationTokenSource();
        _steps.Step = 2;

        try
        {
            Log("Download firmware e verifica SHA-256...");
            _prepared = await _coordinator.PrepareAsync(
                _manifest, UiProgress(), _cts.Token);

            _steps.Step = 3;
            ApplyManifest(_manifest, "Pronto", ArxTheme.Green);
            SetHeadline(
                "Firmware pronto per l'installazione",
                "Download completato, checksum verificati e indirizzi firmware validati.",
                ArxTheme.Green);
            Log("Tutti i firmware hanno superato SHA-256 e controllo indirizzi.");
            _update.Enabled = true;
        }
        catch (Exception ex)
        {
            SetHeadline("Download non riuscito", ex.Message, ArxTheme.Red);
            Log(ex.Message, "ERRORE");
            MessageBox.Show(this, ex.Message, "AlfaRaceX",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
            _download.Enabled = true;
        }
        finally
        {
            SetBusy(false);
        }
    }

    private async Task UpdateAsync()
    {
        if (_manifest is null || _prepared is null) return;

        SetBusy(true);
        _update.Enabled = false;
        _cts = new CancellationTokenSource();
        _steps.Step = 4;

        try
        {
            foreach (PreparedFirmware firmware in _prepared)
            {
                SetModuleState(firmware.Target.Id, "In attesa modalità DFU", ArxTheme.Amber);

                var answer = MessageBox.Show(
                    this,
                    $"Preparazione {firmware.Target.Label}\n\n" +
                    $"Collega {firmware.Target.PortHint} tenendo premuto il pulsante di programmazione. " +
                    "Rilascia il pulsante quando il dispositivo entra in modalità DFU.\n\n" +
                    "Lascia collegato un solo modulo. Premi OK per procedere.",
                    "AlfaRaceX",
                    MessageBoxButtons.OKCancel,
                    MessageBoxIcon.Information);

                if (answer != DialogResult.OK)
                    throw new OperationCanceledException();

                SetModuleState(firmware.Target.Id, "Installazione...", ArxTheme.Amber);
                Log($"{firmware.Target.Label}: avvio installazione.");

                await _coordinator.FlashAsync(
                    firmware, UiProgress(),
                    m => Log(m), _cts.Token);

                SetModuleState(firmware.Target.Id, "Installato", ArxTheme.Green);
                Log($"{firmware.Target.Label}: completato.");

                MessageBox.Show(
                    this,
                    $"{firmware.Target.Label} completato.\n\n" +
                    "Scollega il cavo prima di passare al modulo successivo.",
                    "AlfaRaceX",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information);
            }

            _steps.Step = 5;
            SetHeadline(
                "Aggiornamento completato",
                $"AlfaRaceX {_manifest.Version} installato e verificato sui tre moduli.",
                ArxTheme.Green);
            Log($"AlfaRaceX {_manifest.Version}: aggiornamento completo.");

            MessageBox.Show(
                this,
                $"AlfaRaceX {_manifest.Version} installato e verificato sui tre moduli.",
                "AlfaRaceX",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);
        }
        catch (OperationCanceledException)
        {
            SetHeadline("Operazione annullata", "Nessuna ulteriore operazione verrà eseguita.", ArxTheme.Amber);
            Log("Operazione annullata.", "AVVISO");
        }
        catch (Exception ex)
        {
            SetHeadline("Aggiornamento interrotto", ex.Message, ArxTheme.Red);
            Log(ex.Message, "ERRORE");
            MessageBox.Show(
                this,
                ex.Message +
                "\n\nNessuna operazione di sblocco o cancellazione totale è stata eseguita.",
                "AlfaRaceX",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
        finally
        {
            SetBusy(false);
            _update.Enabled = _prepared is not null;
        }
    }

    private async Task CheckUpdaterVersionAsync(CancellationToken ct)
    {
        try
        {
            UpdaterManifest available =
                await _coordinator.LoadUpdaterManifestAsync(ct);

            if (!IsNewerVersion(available.Version, AppConstants.UpdaterVersion))
                return;

            Log($"Updater {available.Version} disponibile.", "AVVISO");
            var answer = MessageBox.Show(
                this,
                $"È disponibile AlfaRaceX Updater {available.Version}.\n\n" +
                $"Questa versione è {AppConstants.UpdaterVersion}. " +
                "Il firmware può comunque essere gestito con l'Updater attuale.\n\n" +
                "Vuoi aprire la pagina della nuova versione?",
                "Aggiornamento Updater disponibile",
                MessageBoxButtons.YesNo,
                MessageBoxIcon.Information);

            if (answer == DialogResult.Yes)
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = available.ReleaseUrl,
                    UseShellExecute = true
                });
            }
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        catch (Exception ex)
        {
            Log("Controllo versione Updater non disponibile: " + ex.Message, "AVVISO");
        }
    }

    private IProgress<(string Message, int Progress)> UiProgress() =>
        new Progress<(string Message, int Progress)>(x =>
        {
            _statusSub.Text = x.Message;
            Text = $"AlfaRaceX Updater {AppConstants.UpdaterVersion} — {x.Progress}%";
        });

    private void ApplyManifest(UpdateManifest manifest, string state, Color color)
    {
        foreach (FirmwareTarget target in manifest.Targets)
        {
            if (!_modules.TryGetValue(target.Id, out ModuleUi? ui)) continue;
            ui.Version.Text = $"Disponibile: {target.Id} {manifest.Version}";
            string shortHash = target.Sha256.Length >= 12
                ? $"{target.Sha256[..6]}…{target.Sha256[^6..]}"
                : target.Sha256;
            ui.Hash.Text = $"SHA-256: {shortHash}";
            ui.State.Text = state;
            ui.State.ForeColor = color;
            ui.Dot.DotColor = color;
            ui.Dot.Invalidate();
        }
    }

    private void SetModuleState(string id, string state, Color color)
    {
        if (!_modules.TryGetValue(id, out ModuleUi? ui)) return;
        ui.State.Text = state;
        ui.State.ForeColor = color;
        ui.Dot.DotColor = color;
        ui.Dot.Invalidate();
    }

    private void SetHeadline(string title, string sub, Color color)
    {
        _headline.Text = title;
        _headline.ForeColor = color;
        _statusSub.Text = sub;
        _readyDot.DotColor = color;
        _readyDot.Invalidate();
    }

    private void SetBusy(bool busy)
    {
        _check.Enabled = !busy;
        UseWaitCursor = busy;
        if (!busy)
            Text = $"AlfaRaceX Updater {AppConstants.UpdaterVersion}";
    }

    private void Log(string message, string level = "INFO")
    {
        if (InvokeRequired)
        {
            BeginInvoke(new Action(() => Log(message, level)));
            return;
        }

        int row = _log.Rows.Add(
            DateTime.Now.ToString("HH:mm:ss"),
            level,
            message);
        Color color = level switch
        {
            "ERRORE" => Color.FromArgb(255, 92, 105),
            "AVVISO" => ArxTheme.Amber,
            _ => ArxTheme.Green
        };
        _log.Rows[row].Cells[1].Style.ForeColor = color;
        if (_log.Rows.Count > 0)
            _log.FirstDisplayedScrollingRowIndex = _log.Rows.Count - 1;
    }

    private void AddNav(Control parent, string text, int y, bool active, Action action)
    {
        var b = new SideNavButton
        {
            Text = text,
            Active = active,
            Bounds = new Rectangle(0, y, 282, 64)
        };
        b.Click += (_, _) => action();
        parent.Controls.Add(b);
    }

    private void ShowSettings()
    {
        string path = _manifest is null
            ? Path.Combine(Path.GetTempPath(), "AlfaRaceX-Updater")
            : Path.Combine(Path.GetTempPath(), "AlfaRaceX-Updater", _manifest.Version);
        MessageBox.Show(
            this,
            $"Percorso di lavoro:\n{path}\n\n" +
            "L'Updater usa pagine Flash mirate, verifica SHA-256 e verifica post-scrittura.",
            "Impostazioni AlfaRaceX",
            MessageBoxButtons.OK,
            MessageBoxIcon.Information);
    }

    private void ShowAbout()
    {
        MessageBox.Show(
            this,
            $"AlfaRaceX Updater {AppConstants.UpdaterVersion}\n\n" +
            "Updater firmware per piattaforma AlfaRaceX STM32F072.\n" +
            "Tre moduli: BH, C2, C1.\n\n" +
            "AlfaRaceX è un progetto indipendente.",
            "Informazioni",
            MessageBoxButtons.OK,
            MessageBoxIcon.Information);
    }

    private static bool IsNewerVersion(string available, string current) =>
        Version.TryParse(available.Trim(), out Version? remote) &&
        Version.TryParse(current.Trim(), out Version? local) &&
        remote > local;

    private static Label MakeLabel(
        string text, int x, int y, int width, int height,
        float size, FontStyle style, Color color)
    {
        return new Label
        {
            Text = text,
            Bounds = new Rectangle(x, y, width, height),
            ForeColor = color,
            BackColor = Color.Transparent,
            Font = ArxTheme.Font(size, style),
            AutoEllipsis = true
        };
    }

    private static Image LoadImage(string resourceName)
    {
        Stream? stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(resourceName);
        if (stream is null)
            throw new InvalidOperationException($"Risorsa grafica non disponibile: {resourceName}");
        using (stream)
            return Image.FromStream(stream).Clone() as Image
                ?? throw new InvalidOperationException($"Risorsa grafica non valida: {resourceName}");
    }

    private sealed record ModuleUi(
        Label Version,
        Label Hash,
        Label State,
        StatusDot Dot);
}
