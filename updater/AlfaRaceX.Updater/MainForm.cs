using System.Diagnostics;
using System.Drawing;
using System.Reflection;
using System.Security.Cryptography;

namespace AlfaRaceX.Updater;

internal enum AppPage
{
    Dashboard,
    Update,
    Backup,
    Restore,
    Logs,
    About
}

internal sealed class MainForm : Form
{
    private readonly UpdateCoordinator _coordinator = new();
    private readonly BackupRestoreService _backupRestore = new();
    private readonly Dictionary<string, ModuleUi> _modules =
        new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<AppPage, SideNavButton> _nav = new();
    private readonly Dictionary<AppPage, Panel> _pages = new();

    private readonly Panel _contentHost = new();
    private readonly System.Windows.Forms.Timer _usbTimer = new() { Interval = 1200 };

    private readonly Label _headerConnection = new();
    private readonly Label _headerConnectionSub = new();
    private readonly StatusDot _headerConnectionDot = new();
    private readonly Label _systemState = new();
    private readonly Label _systemStateSub = new();
    private readonly StatusDot _systemStateDot = new();
    private readonly Label _updaterValue = new();
    private readonly Label _firmwareValue = new();
    private readonly Label _lastCheckValue = new();
    private readonly Label _channelValue = new();
    private readonly Label _downloadPathValue = new();

    private readonly ArxButton _check = new();
    private readonly ArxButton _download = new();
    private readonly ArxButton _update = new();
    private readonly StepStrip _dashboardSteps = new();
    private readonly StepStrip _updateSteps = new();
    private readonly DataGridView _dashboardLog = new();
    private readonly DataGridView _fullLog = new();
    private readonly Panel _moduleRows = new();

    private readonly Label _updatePageStatus = new();

    private readonly ComboBox _backupRole = new();
    private readonly TextBox _backupFolder = new();
    private readonly ProgressBar _backupProgress = new();
    private readonly Label _backupResult = new();
    private readonly ArxButton _backupStart = new();

    private readonly ComboBox _restoreRole = new();
    private readonly TextBox _restoreFile = new();
    private readonly Label _restoreHash = new();
    private readonly ProgressBar _restoreProgress = new();
    private readonly Label _restoreResult = new();
    private readonly ArxButton _restoreStart = new();

    private UpdateManifest? _manifest;
    private IReadOnlyList<PreparedFirmware>? _prepared;
    private CancellationTokenSource? _cts;
    private string _lastUsbState = "";

    public MainForm()
    {
        Text = $"AlfaRaceX Updater {AppConstants.UpdaterVersion}";
        Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);
        ClientSize = new Size(1536, 830);
        MinimumSize = new Size(1536, 830);
        MaximumSize = new Size(1536, 830);
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = ArxTheme.Background;
        ForeColor = ArxTheme.Text;
        Font = ArxTheme.Font(9.2f);
        AutoScaleMode = AutoScaleMode.Dpi;

        BuildShell();
        LoadExistingLogs();
        ShowPage(AppPage.Dashboard);

        _usbTimer.Tick += (_, _) => RefreshUsbState();
        _usbTimer.Start();

        FormClosing += (_, _) =>
        {
            _cts?.Cancel();
            _usbTimer.Stop();
        };

        Shown += (_, _) =>
        {
            Log($"AlfaRaceX Updater {AppConstants.UpdaterVersion} avviato.");
            RefreshUsbState();
        };
    }

    private void BuildShell()
    {
        var sidebar = new Panel
        {
            BackColor = ArxTheme.Sidebar,
            Dock = DockStyle.Left,
            Width = 276
        };
        Controls.Add(sidebar);

        var logo = new PictureBox
        {
            Image = LoadImage("AlfaRaceX.ARX.png"),
            SizeMode = PictureBoxSizeMode.Zoom,
            Bounds = new Rectangle(18, 12, 235, 72),
            BackColor = Color.Transparent
        };
        sidebar.Controls.Add(logo);

        sidebar.Controls.Add(MakeLabel(
            "AlfaRaceX Updater", 22, 78, 220, 25,
            11.5f, FontStyle.Bold, Color.White));
        sidebar.Controls.Add(MakeLabel(
            $"v{AppConstants.UpdaterVersion}", 214, 81, 48, 20,
            8.6f, FontStyle.Regular, ArxTheme.Muted));

        AddNav(sidebar, AppPage.Dashboard, "⌂", "Dashboard",
            "Panoramica e stato sistema", 112);
        AddNav(sidebar, AppPage.Update, "↓", "Aggiornamento",
            "Verifica e installa firmware", 184);
        AddNav(sidebar, AppPage.Backup, "●", "Backup",
            "Salva firmware completo", 256);
        AddNav(sidebar, AppPage.Restore, "↶", "Ripristino",
            "Ripristina firmware", 328);
        AddNav(sidebar, AppPage.Logs, "☷", "Log",
            "Visualizza operazioni", 400);

        var watermark = new PictureBox
        {
            Image = LoadImage("AlfaRaceX.Watermark.jpg"),
            SizeMode = PictureBoxSizeMode.Zoom,
            Bounds = new Rectangle(26, 492, 224, 198),
            BackColor = ArxTheme.Sidebar
        };
        sidebar.Controls.Add(watermark);

        var about = new ArxPanel
        {
            Bounds = new Rectangle(14, 704, 248, 108),
            BackColor = Color.FromArgb(6, 14, 20),
            Cursor = Cursors.Hand
        };
        about.Click += (_, _) => ShowPage(AppPage.About);
        about.Controls.Add(MakeLabel(
            "ⓘ  Informazioni", 16, 10, 200, 22,
            8.8f, FontStyle.Bold, ArxTheme.Text));
        about.Controls.Add(MakeLabel(
            $"Updater: {AppConstants.UpdaterVersion}", 42, 36, 180, 18,
            8f, FontStyle.Regular, ArxTheme.Muted));
        about.Controls.Add(MakeLabel(
            $"Build: {File.GetLastWriteTime(Application.ExecutablePath):dd/MM/yyyy}", 42, 56, 180, 18,
            8f, FontStyle.Regular, ArxTheme.Muted));
        about.Controls.Add(MakeLabel(
            "© AlfaRaceX", 42, 76, 180, 18,
            8f, FontStyle.Regular, ArxTheme.Muted));
        sidebar.Controls.Add(about);

        _contentHost.Dock = DockStyle.Fill;
        _contentHost.BackColor = ArxTheme.Background;
        Controls.Add(_contentHost);
        sidebar.BringToFront();

        BuildDashboardPage();
        BuildUpdatePage();
        BuildBackupPage();
        BuildRestorePage();
        BuildLogsPage();
        BuildAboutPage();

        foreach (Panel page in _pages.Values)
        {
            page.Dock = DockStyle.Fill;
            page.Visible = false;
            _contentHost.Controls.Add(page);
        }
    }

    private void AddNav(
        Control parent,
        AppPage page,
        string glyph,
        string title,
        string subtitle,
        int y)
    {
        var button = new SideNavButton
        {
            Glyph = glyph,
            Title = title,
            Subtitle = subtitle,
            Bounds = new Rectangle(0, y, 276, 72)
        };
        button.Click += (_, _) => ShowPage(page);
        parent.Controls.Add(button);
        _nav[page] = button;
    }

    private void ShowPage(AppPage page)
    {
        foreach (var pair in _pages)
            pair.Value.Visible = pair.Key == page;

        foreach (var pair in _nav)
            pair.Value.Active = pair.Key == page;

        if (_pages.TryGetValue(page, out Panel? selected))
            selected.BringToFront();

        if (page == AppPage.Logs)
            RefreshFullLog();
    }

    private void BuildDashboardPage()
    {
        var page = NewPage();
        _pages[AppPage.Dashboard] = page;

        BuildDashboardHeader(page);

        _check.Text = "↻   Verifica aggiornamenti";
        _check.SetBounds(14, 258, 370, 62);
        _check.Click += async (_, _) => await CheckAsync();
        page.Controls.Add(_check);

        _download.Text = "↓   Scarica firmware";
        _download.SetBounds(396, 258, 350, 62);
        _download.Enabled = false;
        _download.Click += async (_, _) => await DownloadAsync();
        page.Controls.Add(_download);

        _update.Text = "⚙   Installa aggiornamento";
        _update.SetBounds(758, 258, 488, 62);
        _update.Primary = true;
        _update.Enabled = false;
        _update.Click += async (_, _) => await UpdateAsync();
        page.Controls.Add(_update);

        var modules = new ArxPanel
        {
            Bounds = new Rectangle(14, 334, 1232, 198)
        };
        modules.Controls.Add(MakeLabel(
            "Moduli firmware", 14, 8, 240, 26,
            10.6f, FontStyle.Bold, ArxTheme.Text));
        page.Controls.Add(modules);

        _moduleRows.SetBounds(12, 39, 1208, 147);
        _moduleRows.BackColor = Color.Transparent;
        modules.Controls.Add(_moduleRows);

        CreateModuleRow("BH", "Body Hub", ArxTheme.Red, 0);
        CreateModuleRow("C2", "CAN 2", ArxTheme.Blue, 1);
        CreateModuleRow("C1", "CAN 1", ArxTheme.Green, 2);

        var progress = new ArxPanel
        {
            Bounds = new Rectangle(14, 542, 1232, 96)
        };
        progress.Controls.Add(MakeLabel(
            "Avanzamento operazione", 14, 6, 260, 22,
            9.3f, FontStyle.Bold, ArxTheme.Text));
        _dashboardSteps.SetBounds(8, 20, 1216, 72);
        progress.Controls.Add(_dashboardSteps);
        page.Controls.Add(progress);

        var logBox = new ArxPanel
        {
            Bounds = new Rectangle(14, 648, 1232, 168)
        };
        logBox.Controls.Add(MakeLabel(
            "Log operazioni", 14, 6, 220, 22,
            9.3f, FontStyle.Bold, ArxTheme.Text));

        var clear = MakeSmallButton("Pulisci log", 1112, 5, 104, 27);
        clear.Click += (_, _) => ClearLogs();
        logBox.Controls.Add(clear);

        ConfigureLogGrid(_dashboardLog);
        _dashboardLog.SetBounds(10, 35, 1212, 122);
        logBox.Controls.Add(_dashboardLog);
        page.Controls.Add(logBox);
    }

    private void BuildDashboardHeader(Panel page)
    {
        var quick = new Panel
        {
            Bounds = new Rectangle(14, 0, 532, 82),
            BackColor = Color.Transparent
        };

        _headerConnectionDot.Location = new Point(14, 22);
        quick.Controls.Add(_headerConnectionDot);

        _headerConnection.SetBounds(46, 15, 220, 28);
        _headerConnection.Text = "Nessun modulo DFU";
        _headerConnection.Font = ArxTheme.Font(11f, FontStyle.Bold);
        _headerConnection.ForeColor = ArxTheme.Muted;
        quick.Controls.Add(_headerConnection);

        _headerConnectionSub.SetBounds(46, 43, 220, 22);
        _headerConnectionSub.Text = "Collega un modulo in modalità DFU";
        _headerConnectionSub.Font = ArxTheme.Font(8.4f);
        _headerConnectionSub.ForeColor = ArxTheme.Muted;
        quick.Controls.Add(_headerConnectionSub);

        quick.Controls.Add(MakeLabel(
            "Versione updater", 286, 13, 120, 18,
            7.7f, FontStyle.Regular, ArxTheme.Muted));
        quick.Controls.Add(MakeLabel(
            AppConstants.UpdaterVersion, 286, 32, 120, 24,
            9.4f, FontStyle.Bold, ArxTheme.Text));

        quick.Controls.Add(MakeLabel(
            "Ultima verifica", 412, 13, 105, 18,
            7.7f, FontStyle.Regular, ArxTheme.Muted));
        _lastCheckValue.SetBounds(412, 32, 118, 24);
        _lastCheckValue.Text = "—";
        _lastCheckValue.Font = ArxTheme.Font(8.8f, FontStyle.Bold);
        _lastCheckValue.ForeColor = ArxTheme.Text;
        quick.Controls.Add(_lastCheckValue);

        page.Controls.Add(quick);

        var status = new ArxPanel
        {
            Bounds = new Rectangle(14, 86, 532, 158)
        };
        status.Controls.Add(MakeLabel(
            "Stato sistema", 14, 8, 190, 22,
            10f, FontStyle.Bold, ArxTheme.Text));

        _systemStateDot.Location = new Point(24, 65);
        status.Controls.Add(_systemStateDot);

        _systemState.SetBounds(62, 52, 180, 28);
        _systemState.Text = "In attesa";
        _systemState.Font = ArxTheme.Font(11f, FontStyle.Bold);
        _systemState.ForeColor = ArxTheme.Muted;
        status.Controls.Add(_systemState);

        _systemStateSub.SetBounds(62, 79, 170, 42);
        _systemStateSub.Text = "Nessun dispositivo DFU rilevato";
        _systemStateSub.Font = ArxTheme.Font(8.3f);
        _systemStateSub.ForeColor = ArxTheme.Muted;
        status.Controls.Add(_systemStateSub);

        status.Controls.Add(MakeLabel(
            "Versione updater:", 255, 40, 130, 18,
            8f, FontStyle.Regular, ArxTheme.Muted));
        _updaterValue.SetBounds(388, 38, 124, 20);
        _updaterValue.Text = AppConstants.UpdaterVersion;
        _updaterValue.Font = ArxTheme.Font(8.7f, FontStyle.Bold);
        _updaterValue.ForeColor = ArxTheme.Text;
        status.Controls.Add(_updaterValue);

        status.Controls.Add(MakeLabel(
            "Firmware disponibile:", 255, 63, 130, 18,
            8f, FontStyle.Regular, ArxTheme.Muted));
        _firmwareValue.SetBounds(388, 61, 124, 20);
        _firmwareValue.Text = "Non verificato";
        _firmwareValue.ForeColor = ArxTheme.Text;
        status.Controls.Add(_firmwareValue);

        status.Controls.Add(MakeLabel(
            "Canale:", 255, 86, 130, 18,
            8f, FontStyle.Regular, ArxTheme.Muted));
        _channelValue.SetBounds(388, 84, 124, 20);
        _channelValue.Text = "—";
        _channelValue.ForeColor = ArxTheme.Text;
        status.Controls.Add(_channelValue);

        status.Controls.Add(MakeLabel(
            "Percorso download:", 255, 109, 130, 18,
            8f, FontStyle.Regular, ArxTheme.Muted));
        _downloadPathValue.SetBounds(388, 107, 124, 37);
        _downloadPathValue.Text = "—";
        _downloadPathValue.ForeColor = ArxTheme.Text;
        _downloadPathValue.AutoEllipsis = true;
        status.Controls.Add(_downloadPathValue);

        page.Controls.Add(status);

        var hero = new PictureBox
        {
            Image = LoadImage("AlfaRaceX.Hero.jpg"),
            SizeMode = PictureBoxSizeMode.StretchImage,
            Bounds = new Rectangle(556, 0, 690, 244),
            BackColor = Color.Black
        };
        page.Controls.Add(hero);

        var help = MakeSmallButton("?", 1193, 14, 38, 38);
        help.Font = ArxTheme.Font(12f, FontStyle.Bold);
        help.Click += (_, _) => ShowPage(AppPage.About);
        page.Controls.Add(help);
        help.BringToFront();
    }

    private void CreateModuleRow(
        string id,
        string label,
        Color badge,
        int index)
    {
        int y = index * 47;
        var row = new Panel
        {
            Bounds = new Rectangle(0, y, 1208, 43),
            BackColor = index % 2 == 0
                ? Color.FromArgb(5, 12, 17)
                : Color.FromArgb(8, 16, 22)
        };

        var role = new Label
        {
            Bounds = new Rectangle(6, 4, 116, 35),
            Text = $"{id}\r\n{label}",
            TextAlign = ContentAlignment.MiddleCenter,
            BackColor = badge,
            ForeColor = Color.White,
            Font = ArxTheme.Font(9.5f, FontStyle.Bold)
        };
        row.Controls.Add(role);

        row.Controls.Add(MakeLabel(
            "Versione attuale", 170, 3, 140, 17,
            7.3f, FontStyle.Regular, ArxTheme.Muted));
        var current = MakeLabel(
            "—", 170, 19, 140, 20,
            8.8f, FontStyle.Bold, ArxTheme.Text);
        row.Controls.Add(current);

        row.Controls.Add(MakeLabel(
            "Versione disponibile", 330, 3, 150, 17,
            7.3f, FontStyle.Regular, ArxTheme.Muted));
        var available = MakeLabel(
            "—", 330, 19, 150, 20,
            8.8f, FontStyle.Bold, ArxTheme.Text);
        row.Controls.Add(available);

        row.Controls.Add(MakeLabel(
            "Checksum (SHA-256)", 500, 3, 170, 17,
            7.3f, FontStyle.Regular, ArxTheme.Muted));
        var hash = MakeLabel(
            "—", 500, 19, 180, 20,
            8.4f, FontStyle.Regular, ArxTheme.Text);
        row.Controls.Add(hash);

        row.Controls.Add(MakeLabel(
            "Stato", 730, 3, 90, 17,
            7.3f, FontStyle.Regular, ArxTheme.Muted));

        var dot = new StatusDot
        {
            Location = new Point(730, 19),
            DotColor = ArxTheme.Muted
        };
        row.Controls.Add(dot);

        var state = MakeLabel(
            "In attesa", 757, 18, 250, 21,
            8.6f, FontStyle.Bold, ArxTheme.Muted);
        row.Controls.Add(state);

        var details = MakeSmallButton("Dettagli", 1054, 7, 110, 30);
        details.Click += (_, _) => ShowModuleDetails(id);
        row.Controls.Add(details);

        _moduleRows.Controls.Add(row);
        _modules[id] = new ModuleUi(
            current,
            available,
            hash,
            state,
            dot);
    }

    private void BuildUpdatePage()
    {
        var page = NewPage();
        _pages[AppPage.Update] = page;

        page.Controls.Add(MakeLabel(
            "Aggiornamento firmware", 20, 18, 420, 34,
            16f, FontStyle.Bold, ArxTheme.Text));
        page.Controls.Add(MakeLabel(
            "Verifica il manifest, scarica le immagini firmate dagli hash e programma BH, C2 e C1 in sequenza.",
            20, 55, 910, 25,
            9.2f, FontStyle.Regular, ArxTheme.Muted));

        var actionBox = new ArxPanel
        {
            Bounds = new Rectangle(20, 96, 1218, 150)
        };

        var check = new ArxButton
        {
            Text = "1   Verifica aggiornamenti",
            Bounds = new Rectangle(18, 25, 365, 64)
        };
        check.Click += async (_, _) => await CheckAsync();

        var download = new ArxButton
        {
            Text = "2   Scarica e verifica",
            Bounds = new Rectangle(401, 25, 365, 64)
        };
        download.Click += async (_, _) => await DownloadAsync();

        var install = new ArxButton
        {
            Text = "3   Installa sui moduli",
            Bounds = new Rectangle(784, 25, 416, 64),
            Primary = true
        };
        install.Click += async (_, _) => await UpdateAsync();

        actionBox.Controls.Add(check);
        actionBox.Controls.Add(download);
        actionBox.Controls.Add(install);

        _updatePageStatus.SetBounds(20, 105, 1140, 26);
        _updatePageStatus.Text = "Pronto. Inizia dalla verifica della release disponibile.";
        _updatePageStatus.ForeColor = ArxTheme.Muted;
        _updatePageStatus.Font = ArxTheme.Font(9f, FontStyle.Bold);
        actionBox.Controls.Add(_updatePageStatus);
        page.Controls.Add(actionBox);

        var sequence = new ArxPanel
        {
            Bounds = new Rectangle(20, 264, 1218, 154)
        };
        sequence.Controls.Add(MakeLabel(
            "Sequenza moduli", 18, 12, 240, 24,
            10f, FontStyle.Bold, ArxTheme.Text));

        sequence.Controls.Add(MakeLabel(
            "BH  ·  USB-C sinistra", 32, 53, 340, 28,
            11f, FontStyle.Bold, ArxTheme.Red));
        sequence.Controls.Add(MakeLabel(
            "C2  ·  USB-C centrale", 438, 53, 340, 28,
            11f, FontStyle.Bold, ArxTheme.Blue));
        sequence.Controls.Add(MakeLabel(
            "C1  ·  USB-C destra", 844, 53, 340, 28,
            11f, FontStyle.Bold, ArxTheme.Green));
        sequence.Controls.Add(MakeLabel(
            "Collega un solo modulo alla volta in modalità DFU. L'Updater cancella esclusivamente le pagine previste dall'immagine di aggiornamento.",
            32, 93, 1120, 40,
            8.8f, FontStyle.Regular, ArxTheme.Muted));
        page.Controls.Add(sequence);

        var progress = new ArxPanel
        {
            Bounds = new Rectangle(20, 438, 1218, 112)
        };
        progress.Controls.Add(MakeLabel(
            "Avanzamento", 18, 8, 200, 22,
            9.5f, FontStyle.Bold, ArxTheme.Text));
        _updateSteps.SetBounds(8, 24, 1202, 80);
        progress.Controls.Add(_updateSteps);
        page.Controls.Add(progress);

        var note = new ArxPanel
        {
            Bounds = new Rectangle(20, 570, 1218, 180),
            BackColor = Color.FromArgb(7, 14, 19)
        };
        note.Controls.Add(MakeLabel(
            "Sicurezza programmazione", 18, 14, 300, 24,
            10f, FontStyle.Bold, ArxTheme.Text));
        note.Controls.Add(MakeLabel(
            "• Nessuna cancellazione totale del chip durante l'aggiornamento standard.\r\n" +
            "• Verifica SHA-256 prima della scrittura.\r\n" +
            "• Verifica byte-per-byte dopo la programmazione.\r\n" +
            "• Gli Option Bytes non vengono modificati.\r\n" +
            "• In caso di errore la sequenza si interrompe prima del modulo successivo.",
            28, 52, 1120, 105,
            9f, FontStyle.Regular, ArxTheme.Muted));
        page.Controls.Add(note);
    }

    private void BuildBackupPage()
    {
        var page = NewPage();
        _pages[AppPage.Backup] = page;

        page.Controls.Add(MakeLabel(
            "Backup firmware", 20, 18, 420, 34,
            16f, FontStyle.Bold, ArxTheme.Text));
        page.Controls.Add(MakeLabel(
            "Legge e salva l'intera Flash interna da 128 KiB del modulo selezionato, con SHA-256 e metadati del ruolo.",
            20, 55, 980, 25,
            9.2f, FontStyle.Regular, ArxTheme.Muted));

        var box = new ArxPanel
        {
            Bounds = new Rectangle(20, 100, 820, 370)
        };
        box.Controls.Add(MakeLabel(
            "Modulo", 24, 30, 180, 22,
            9f, FontStyle.Bold, ArxTheme.Text));

        _backupRole.SetBounds(220, 27, 250, 30);
        _backupRole.DropDownStyle = ComboBoxStyle.DropDownList;
        _backupRole.Items.AddRange(new object[] { "BH", "C2", "C1" });
        _backupRole.SelectedIndex = 0;
        box.Controls.Add(_backupRole);

        box.Controls.Add(MakeLabel(
            "Cartella destinazione", 24, 85, 180, 22,
            9f, FontStyle.Bold, ArxTheme.Text));

        _backupFolder.SetBounds(220, 82, 470, 30);
        _backupFolder.Text = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
            "AlfaRaceX Backups");
        box.Controls.Add(_backupFolder);

        var browse = MakeSmallButton("Sfoglia", 700, 81, 92, 31);
        browse.Click += (_, _) => BrowseBackupFolder();
        box.Controls.Add(browse);

        box.Controls.Add(MakeLabel(
            "Procedura", 24, 140, 180, 22,
            9f, FontStyle.Bold, ArxTheme.Text));
        box.Controls.Add(MakeLabel(
            "1. Seleziona BH, C2 o C1.\r\n" +
            "2. Tieni premuto PROGRAMMAZIONE e collega la USB-C del modulo selezionato.\r\n" +
            "3. Rilascia il pulsante quando Windows rileva il dispositivo DFU.\r\n" +
            "4. Lascia collegato un solo modulo e avvia il backup.",
            220, 137, 570, 88,
            8.8f, FontStyle.Regular, ArxTheme.Muted));

        _backupStart.Text = "CREA BACKUP COMPLETO";
        _backupStart.SetBounds(220, 245, 360, 56);
        _backupStart.Primary = true;
        _backupStart.Click += async (_, _) => await StartBackupAsync();
        box.Controls.Add(_backupStart);

        _backupProgress.SetBounds(220, 318, 570, 18);
        _backupProgress.Minimum = 0;
        _backupProgress.Maximum = 100;
        box.Controls.Add(_backupProgress);

        _backupResult.SetBounds(220, 342, 570, 22);
        _backupResult.ForeColor = ArxTheme.Muted;
        _backupResult.AutoEllipsis = true;
        box.Controls.Add(_backupResult);
        page.Controls.Add(box);

        var info = new ArxPanel
        {
            Bounds = new Rectangle(860, 100, 378, 370)
        };
        info.Controls.Add(MakeLabel(
            "Cosa viene salvato", 18, 18, 320, 26,
            11f, FontStyle.Bold, ArxTheme.Text));
        info.Controls.Add(MakeLabel(
            "• 0x08000000 → 0x0801FFFF\r\n" +
            "• 131.072 byte di Flash interna\r\n" +
            "• applicazione e pagine persistenti\r\n" +
            "• SHA-256 del file\r\n" +
            "• file JSON con ruolo e metadati\r\n\r\n" +
            "Gli Option Bytes non fanno parte del file .bin e non vengono modificati.",
            22, 65, 330, 230,
            9f, FontStyle.Regular, ArxTheme.Muted));
        page.Controls.Add(info);
    }

    private void BuildRestorePage()
    {
        var page = NewPage();
        _pages[AppPage.Restore] = page;

        page.Controls.Add(MakeLabel(
            "Ripristino firmware", 20, 18, 420, 34,
            16f, FontStyle.Bold, ArxTheme.Text));
        page.Controls.Add(MakeLabel(
            "Ripristina un'immagine completa da 128 KiB sul modulo corretto e verifica ogni byte dopo la scrittura.",
            20, 55, 980, 25,
            9.2f, FontStyle.Regular, ArxTheme.Muted));

        var box = new ArxPanel
        {
            Bounds = new Rectangle(20, 100, 850, 390)
        };

        box.Controls.Add(MakeLabel(
            "Modulo", 24, 30, 180, 22,
            9f, FontStyle.Bold, ArxTheme.Text));

        _restoreRole.SetBounds(220, 27, 250, 30);
        _restoreRole.DropDownStyle = ComboBoxStyle.DropDownList;
        _restoreRole.Items.AddRange(new object[] { "BH", "C2", "C1" });
        _restoreRole.SelectedIndex = 0;
        box.Controls.Add(_restoreRole);

        box.Controls.Add(MakeLabel(
            "File backup .bin", 24, 85, 180, 22,
            9f, FontStyle.Bold, ArxTheme.Text));

        _restoreFile.SetBounds(220, 82, 490, 30);
        _restoreFile.ReadOnly = true;
        box.Controls.Add(_restoreFile);

        var browse = MakeSmallButton("Sfoglia", 720, 81, 100, 31);
        browse.Click += (_, _) => BrowseRestoreFile();
        box.Controls.Add(browse);

        box.Controls.Add(MakeLabel(
            "SHA-256", 24, 136, 180, 22,
            9f, FontStyle.Bold, ArxTheme.Text));

        _restoreHash.SetBounds(220, 133, 600, 42);
        _restoreHash.Text = "—";
        _restoreHash.ForeColor = ArxTheme.Muted;
        _restoreHash.AutoEllipsis = true;
        box.Controls.Add(_restoreHash);

        box.Controls.Add(MakeLabel(
            "Il ripristino cancella e riscrive l'intera Flash interna del modulo selezionato. " +
            "Se il nome file o i metadati indicano un ruolo diverso, l'operazione viene bloccata.",
            220, 188, 600, 55,
            8.8f, FontStyle.Regular, ArxTheme.Amber));

        _restoreStart.Text = "RIPRISTINA E VERIFICA";
        _restoreStart.SetBounds(220, 260, 360, 56);
        _restoreStart.Primary = true;
        _restoreStart.Click += async (_, _) => await StartRestoreAsync();
        box.Controls.Add(_restoreStart);

        _restoreProgress.SetBounds(220, 338, 600, 18);
        _restoreProgress.Minimum = 0;
        _restoreProgress.Maximum = 100;
        box.Controls.Add(_restoreProgress);

        _restoreResult.SetBounds(220, 362, 600, 22);
        _restoreResult.ForeColor = ArxTheme.Muted;
        _restoreResult.AutoEllipsis = true;
        box.Controls.Add(_restoreResult);
        page.Controls.Add(box);

        var warning = new ArxPanel
        {
            Bounds = new Rectangle(890, 100, 348, 390),
            BorderColor = Color.FromArgb(102, 67, 14)
        };
        warning.Controls.Add(MakeLabel(
            "Controlli di sicurezza", 18, 18, 300, 26,
            11f, FontStyle.Bold, ArxTheme.Amber));
        warning.Controls.Add(MakeLabel(
            "• dimensione obbligatoria: 131.072 byte\r\n" +
            "• verifica ruolo da nome/metadati\r\n" +
            "• verifica SHA-256 se presente il JSON\r\n" +
            "• un solo dispositivo DFU collegato\r\n" +
            "• verifica byte-per-byte dopo scrittura\r\n" +
            "• nessuna modifica agli Option Bytes\r\n\r\n" +
            "Il ripristino viene avviato solo dopo una seconda conferma esplicita.",
            22, 65, 300, 260,
            9f, FontStyle.Regular, ArxTheme.Muted));
        page.Controls.Add(warning);
    }

    private void BuildLogsPage()
    {
        var page = NewPage();
        _pages[AppPage.Logs] = page;

        page.Controls.Add(MakeLabel(
            "Log operazioni", 20, 18, 420, 34,
            16f, FontStyle.Bold, ArxTheme.Text));
        page.Controls.Add(MakeLabel(
            $"Registro persistente: {AppLog.LogDirectory}",
            20, 55, 960, 25,
            9f, FontStyle.Regular, ArxTheme.Muted));

        var export = new ArxButton
        {
            Text = "Esporta log",
            Bounds = new Rectangle(848, 18, 120, 42)
        };
        export.Click += (_, _) => ExportLog();
        page.Controls.Add(export);

        var open = new ArxButton
        {
            Text = "Apri cartella",
            Bounds = new Rectangle(978, 18, 120, 42)
        };
        open.Click += (_, _) => OpenLogFolder();
        page.Controls.Add(open);

        var clear = new ArxButton
        {
            Text = "Pulisci",
            Bounds = new Rectangle(1108, 18, 120, 42)
        };
        clear.Click += (_, _) => ClearLogs();
        page.Controls.Add(clear);

        var box = new ArxPanel
        {
            Bounds = new Rectangle(20, 92, 1218, 680)
        };
        ConfigureLogGrid(_fullLog);
        _fullLog.SetBounds(10, 10, 1198, 660);
        box.Controls.Add(_fullLog);
        page.Controls.Add(box);
    }

    private void BuildAboutPage()
    {
        var page = NewPage();
        _pages[AppPage.About] = page;

        var logo = new PictureBox
        {
            Image = LoadImage("AlfaRaceX.ARX.png"),
            SizeMode = PictureBoxSizeMode.Zoom,
            Bounds = new Rectangle(70, 58, 310, 150),
            BackColor = Color.Transparent
        };
        page.Controls.Add(logo);

        page.Controls.Add(MakeLabel(
            "AlfaRaceX Updater", 70, 220, 460, 44,
            20f, FontStyle.Bold, ArxTheme.Text));
        page.Controls.Add(MakeLabel(
            $"Versione {AppConstants.UpdaterVersion}", 72, 270, 360, 30,
            11f, FontStyle.Bold, ArxTheme.Red));

        page.Controls.Add(MakeLabel(
            "Interfaccia Windows per aggiornamento, backup e ripristino dei tre controller STM32F072 della piattaforma AlfaRaceX.",
            72, 325, 760, 62,
            10f, FontStyle.Regular, ArxTheme.Muted));

        var project = new ArxButton
        {
            Text = "Apri progetto GitHub",
            Bounds = new Rectangle(72, 420, 250, 54)
        };
        project.Click += (_, _) => OpenUrl(
            "https://github.com/AriotaG/AlfaRaceX-Framework");
        page.Controls.Add(project);

        var releases = new ArxButton
        {
            Text = "Apri release",
            Bounds = new Rectangle(338, 420, 220, 54),
            Primary = true
        };
        releases.Click += (_, _) => OpenUrl(
            "https://github.com/AriotaG/AlfaRaceX-Framework/releases");
        page.Controls.Add(releases);

        var card = new ArxPanel
        {
            Bounds = new Rectangle(880, 76, 330, 390)
        };
        card.Controls.Add(MakeLabel(
            "Informazioni tecniche", 20, 20, 280, 28,
            11f, FontStyle.Bold, ArxTheme.Text));
        card.Controls.Add(MakeLabel(
            "Target: STM32F072\r\n" +
            "Moduli: BH · C2 · C1\r\n" +
            "Flash: 128 KiB per controller\r\n" +
            "Aggiornamento: DFU USB\r\n" +
            "Integrità: SHA-256 + verifica byte\r\n" +
            "Log: persistente locale\r\n\r\n" +
            "AlfaRaceX è un progetto indipendente.",
            24, 72, 270, 230,
            9.3f, FontStyle.Regular, ArxTheme.Muted));
        page.Controls.Add(card);
    }

    private Panel NewPage() =>
        new()
        {
            BackColor = ArxTheme.Background
        };

    private async Task CheckAsync()
    {
        SetUpdateBusy(true);
        _prepared = null;
        _dashboardSteps.Step = 1;
        _dashboardSteps.ProgressValue = 10;
        _updateSteps.Step = 1;
        _updateSteps.ProgressValue = 10;
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
                Path.GetTempPath(),
                "AlfaRaceX-Updater",
                _manifest.Version);

            ApplyManifest(_manifest);
            _dashboardSteps.ProgressValue = 20;
            _updateSteps.ProgressValue = 20;
            _updatePageStatus.Text =
                $"Release {_manifest.Version} disponibile. Puoi scaricare e verificare i firmware.";

            Log($"Release {_manifest.Version} disponibile.");
            _download.Enabled = true;
        }
        catch (Exception ex)
        {
            Log(ex.Message, "ERRORE");
            _updatePageStatus.Text = ex.Message;
            MessageBox.Show(
                this,
                ex.Message,
                "AlfaRaceX",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
        finally
        {
            SetUpdateBusy(false);
        }
    }

    private async Task DownloadAsync()
    {
        if (_manifest is null)
        {
            MessageBox.Show(
                this,
                "Esegui prima la verifica aggiornamenti.",
                "AlfaRaceX",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);
            return;
        }

        SetUpdateBusy(true);
        _dashboardSteps.Step = 2;
        _dashboardSteps.ProgressValue = 30;
        _updateSteps.Step = 2;
        _updateSteps.ProgressValue = 30;
        _cts = new CancellationTokenSource();

        try
        {
            Log("Download firmware e verifica SHA-256...");

            var progress = new Progress<(string Message, int Progress)>(x =>
            {
                int overall = 30 + (int)Math.Round(x.Progress * 0.25);
                SetProgressMessage(x.Message, overall);
            });

            _prepared = await _coordinator.PrepareAsync(
                _manifest,
                progress,
                _cts.Token);

            _dashboardSteps.Step = 3;
            _dashboardSteps.ProgressValue = 55;
            _updateSteps.Step = 3;
            _updateSteps.ProgressValue = 55;
            _updatePageStatus.Text =
                "Download completato. SHA-256 e intervalli Flash verificati.";

            ApplyPreparedState();
            Log("Tutti i firmware hanno superato SHA-256 e controllo indirizzi.");
            _update.Enabled = true;
        }
        catch (Exception ex)
        {
            Log(ex.Message, "ERRORE");
            _updatePageStatus.Text = ex.Message;
            MessageBox.Show(
                this,
                ex.Message,
                "AlfaRaceX",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
        finally
        {
            SetUpdateBusy(false);
        }
    }

    private async Task UpdateAsync()
    {
        if (_manifest is null || _prepared is null)
        {
            MessageBox.Show(
                this,
                "Prima verifica e scarica i firmware.",
                "AlfaRaceX",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);
            return;
        }

        SetUpdateBusy(true);
        _dashboardSteps.Step = 4;
        _dashboardSteps.ProgressValue = 60;
        _updateSteps.Step = 4;
        _updateSteps.ProgressValue = 60;
        _cts = new CancellationTokenSource();

        try
        {
            for (int index = 0; index < _prepared.Count; index++)
            {
                PreparedFirmware firmware = _prepared[index];
                SetModuleState(
                    firmware.Target.Id,
                    "In attesa modalità DFU",
                    ArxTheme.Amber);

                DialogResult answer = MessageBox.Show(
                    this,
                    $"Preparazione {firmware.Target.Label}\n\n" +
                    $"Collega {firmware.Target.PortHint} tenendo premuto PROGRAMMAZIONE. " +
                    "Rilascia il pulsante quando Windows rileva il dispositivo DFU.\n\n" +
                    "Lascia collegato un solo modulo e premi OK.",
                    "AlfaRaceX",
                    MessageBoxButtons.OKCancel,
                    MessageBoxIcon.Information);

                if (answer != DialogResult.OK)
                    throw new OperationCanceledException();

                SetModuleState(
                    firmware.Target.Id,
                    "Installazione...",
                    ArxTheme.Amber);
                Log($"{firmware.Target.Label}: avvio installazione.");

                int moduleIndex = index;
                var progress = new Progress<(string Message, int Progress)>(x =>
                {
                    double unit = 35d / _prepared.Count;
                    int overall = 60 + (int)Math.Round(
                        moduleIndex * unit + x.Progress / 100d * unit);
                    SetProgressMessage(x.Message, overall);
                });

                await _coordinator.FlashAsync(
                    firmware,
                    progress,
                    m => Log(m),
                    _cts.Token);

                SetModuleInstalled(
                    firmware.Target.Id,
                    _manifest.Version);
                Log($"{firmware.Target.Label}: completato.");

                MessageBox.Show(
                    this,
                    $"{firmware.Target.Label} completato e verificato.\n\n" +
                    "Scollega il cavo prima di passare al modulo successivo.",
                    "AlfaRaceX",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information);
            }

            _dashboardSteps.Step = 5;
            _dashboardSteps.ProgressValue = 100;
            _updateSteps.Step = 5;
            _updateSteps.ProgressValue = 100;
            _updatePageStatus.Text =
                $"AlfaRaceX {_manifest.Version} installato e verificato sui tre moduli.";

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
            Log("Operazione annullata.", "AVVISO");
            _updatePageStatus.Text = "Operazione annullata.";
        }
        catch (Exception ex)
        {
            Log(ex.Message, "ERRORE");
            _updatePageStatus.Text = ex.Message;
            MessageBox.Show(
                this,
                ex.Message +
                "\n\nLa sequenza è stata interrotta prima di passare al modulo successivo.",
                "AlfaRaceX",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
        finally
        {
            SetUpdateBusy(false);
        }
    }

    private async Task StartBackupAsync()
    {
        string role = SelectedRole(_backupRole);
        string folder = _backupFolder.Text.Trim();

        if (string.IsNullOrWhiteSpace(folder))
        {
            MessageBox.Show(
                this,
                "Seleziona una cartella di destinazione.",
                "AlfaRaceX",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);
            return;
        }

        DialogResult ready = MessageBox.Show(
            this,
            $"Backup modulo {role}\n\n" +
            $"{PortHint(role)}. Tieni premuto PROGRAMMAZIONE mentre colleghi la USB-C, " +
            "poi rilascia quando il dispositivo entra in DFU.\n\n" +
            "Lascia collegato un solo modulo. Premi OK per iniziare la lettura completa.",
            "AlfaRaceX Backup",
            MessageBoxButtons.OKCancel,
            MessageBoxIcon.Information);

        if (ready != DialogResult.OK)
            return;

        _backupStart.Enabled = false;
        _backupProgress.Value = 0;
        _backupResult.Text = "";
        _cts = new CancellationTokenSource();

        try
        {
            var progress = new Progress<(string Message, int Progress)>(x =>
            {
                _backupProgress.Value = Math.Clamp(x.Progress, 0, 100);
                _backupResult.Text = x.Message;
            });

            BackupResult result = await _backupRestore.BackupAsync(
                role,
                folder,
                progress,
                m => Log(m),
                _cts.Token);

            _backupResult.Text =
                $"Creato: {result.BinPath}  ·  SHA-256 {ShortHash(result.Sha256)}";
            Log($"Backup {role} salvato: {result.BinPath}.");

            MessageBox.Show(
                this,
                $"Backup {role} completato.\n\n{result.BinPath}\n\nSHA-256:\n{result.Sha256}",
                "AlfaRaceX Backup",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);
        }
        catch (Exception ex)
        {
            Log(ex.Message, "ERRORE");
            _backupResult.Text = ex.Message;
            MessageBox.Show(
                this,
                ex.Message,
                "AlfaRaceX Backup",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
        finally
        {
            _backupStart.Enabled = true;
        }
    }

    private async Task StartRestoreAsync()
    {
        string role = SelectedRole(_restoreRole);
        string path = _restoreFile.Text.Trim();

        if (!File.Exists(path))
        {
            MessageBox.Show(
                this,
                "Seleziona un file .bin valido.",
                "AlfaRaceX",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);
            return;
        }

        string hash = Convert.ToHexString(
            SHA256.HashData(File.ReadAllBytes(path))).ToLowerInvariant();

        DialogResult confirm = MessageBox.Show(
            this,
            $"ATTENZIONE\n\n" +
            $"Stai per ripristinare l'intera Flash del modulo {role}.\n\n" +
            $"File: {Path.GetFileName(path)}\n" +
            $"SHA-256: {hash}\n\n" +
            "L'operazione cancellerà e riscriverà tutti i 128 KiB di Flash interna. " +
            "Gli Option Bytes non verranno modificati.\n\nContinuare?",
            "Conferma ripristino completo",
            MessageBoxButtons.YesNo,
            MessageBoxIcon.Warning,
            MessageBoxDefaultButton.Button2);

        if (confirm != DialogResult.Yes)
            return;

        DialogResult ready = MessageBox.Show(
            this,
            $"{PortHint(role)}. Tieni premuto PROGRAMMAZIONE mentre colleghi la USB-C, " +
            "poi rilascia quando il dispositivo entra in DFU.\n\n" +
            "Lascia collegato un solo modulo. Premi OK per procedere.",
            "AlfaRaceX Ripristino",
            MessageBoxButtons.OKCancel,
            MessageBoxIcon.Warning);

        if (ready != DialogResult.OK)
            return;

        _restoreStart.Enabled = false;
        _restoreProgress.Value = 0;
        _restoreResult.Text = "";
        _cts = new CancellationTokenSource();

        try
        {
            var progress = new Progress<(string Message, int Progress)>(x =>
            {
                _restoreProgress.Value = Math.Clamp(x.Progress, 0, 100);
                _restoreResult.Text = x.Message;
            });

            await _backupRestore.RestoreAsync(
                role,
                path,
                progress,
                m => Log(m),
                _cts.Token);

            _restoreResult.Text = $"Ripristino {role} completato e verificato.";
            Log($"Ripristino {role} completato e verificato.");

            MessageBox.Show(
                this,
                $"Ripristino {role} completato e verificato byte-per-byte.",
                "AlfaRaceX Ripristino",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);
        }
        catch (Exception ex)
        {
            Log(ex.Message, "ERRORE");
            _restoreResult.Text = ex.Message;
            MessageBox.Show(
                this,
                ex.Message,
                "AlfaRaceX Ripristino",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
        finally
        {
            _restoreStart.Enabled = true;
        }
    }

    private async Task CheckUpdaterVersionAsync(CancellationToken ct)
    {
        try
        {
            UpdaterManifest available =
                await _coordinator.LoadUpdaterManifestAsync(ct);

            if (!IsNewerVersion(
                available.Version,
                AppConstants.UpdaterVersion))
                return;

            Log($"Updater {available.Version} disponibile.", "AVVISO");

            DialogResult answer = MessageBox.Show(
                this,
                $"È disponibile AlfaRaceX Updater {available.Version}.\n\n" +
                $"Questa versione è {AppConstants.UpdaterVersion}. " +
                "Il firmware può comunque essere gestito con l'Updater attuale.\n\n" +
                "Vuoi aprire la pagina della nuova versione?",
                "Aggiornamento Updater disponibile",
                MessageBoxButtons.YesNo,
                MessageBoxIcon.Information);

            if (answer == DialogResult.Yes)
                OpenUrl(available.ReleaseUrl);
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        catch (Exception ex)
        {
            Log(
                "Controllo versione Updater non disponibile: " +
                ex.Message,
                "AVVISO");
        }
    }

    private void ApplyManifest(UpdateManifest manifest)
    {
        foreach (FirmwareTarget target in manifest.Targets)
        {
            if (!_modules.TryGetValue(target.Id, out ModuleUi? ui))
                continue;

            ui.Available.Text = manifest.Version;
            ui.Hash.Text = ShortHash(target.Sha256);

            if (ui.Current.Text == manifest.Version)
            {
                ui.State.Text = "Aggiornato";
                ui.State.ForeColor = ArxTheme.Green;
                ui.Dot.DotColor = ArxTheme.Green;
            }
            else
            {
                ui.State.Text = "Aggiornamento disponibile";
                ui.State.ForeColor = ArxTheme.Amber;
                ui.Dot.DotColor = ArxTheme.Amber;
            }

            ui.Dot.Invalidate();
        }
    }

    private void ApplyPreparedState()
    {
        if (_prepared is null)
            return;

        foreach (PreparedFirmware firmware in _prepared)
        {
            if (!_modules.TryGetValue(
                firmware.Target.Id,
                out ModuleUi? ui))
                continue;

            ui.State.Text = "Firmware verificato";
            ui.State.ForeColor = ArxTheme.Green;
            ui.Dot.DotColor = ArxTheme.Green;
            ui.Dot.Invalidate();
        }
    }

    private void SetModuleInstalled(string id, string version)
    {
        if (!_modules.TryGetValue(id, out ModuleUi? ui))
            return;

        ui.Current.Text = version;
        ui.Available.Text = version;
        ui.State.Text = "Aggiornato";
        ui.State.ForeColor = ArxTheme.Green;
        ui.Dot.DotColor = ArxTheme.Green;
        ui.Dot.Invalidate();
    }

    private void SetModuleState(
        string id,
        string state,
        Color color)
    {
        if (!_modules.TryGetValue(id, out ModuleUi? ui))
            return;

        ui.State.Text = state;
        ui.State.ForeColor = color;
        ui.Dot.DotColor = color;
        ui.Dot.Invalidate();
    }

    private void ShowModuleDetails(string id)
    {
        FirmwareTarget? target = _manifest?.Targets.FirstOrDefault(
            x => string.Equals(
                x.Id,
                id,
                StringComparison.OrdinalIgnoreCase));

        if (target is null)
        {
            MessageBox.Show(
                this,
                $"{id}\n\nNessun manifest caricato. Premi Verifica aggiornamenti.",
                "Dettagli modulo",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);
            return;
        }

        string local = _prepared?
            .FirstOrDefault(x => string.Equals(
                x.Target.Id,
                id,
                StringComparison.OrdinalIgnoreCase))
            ?.LocalPath ?? "Non ancora scaricato";

        MessageBox.Show(
            this,
            $"{target.Label}\n\n" +
            $"Versione disponibile: {_manifest!.Version}\n" +
            $"SHA-256: {target.Sha256}\n" +
            $"Flash: 0x{target.ApplicationStart:X8} → 0x{target.ApplicationLimitExclusive - 1:X8}\n" +
            $"Porta: {target.PortHint}\n" +
            $"File locale: {local}",
            $"Dettagli {id}",
            MessageBoxButtons.OK,
            MessageBoxIcon.Information);
    }

    private void RefreshUsbState()
    {
        try
        {
            int count = UsbDeviceEnumerator.FindDfuPaths().Count;
            string state = count switch
            {
                0 => "none",
                1 => "one",
                _ => "many"
            };

            if (state != _lastUsbState)
            {
                if (state == "one")
                    Log("Dispositivo STM32 DFU rilevato.");
                else if (state == "many")
                    Log("Rilevati più dispositivi DFU.", "AVVISO");
                else if (!string.IsNullOrEmpty(_lastUsbState))
                    Log("Nessun dispositivo DFU collegato.");

                _lastUsbState = state;
            }

            if (count == 1)
                SetConnectionUi(
                    "Dispositivo connesso",
                    "Modalità DFU pronta",
                    "Connesso",
                    "Un modulo DFU rilevato e pronto",
                    ArxTheme.Green);
            else if (count > 1)
                SetConnectionUi(
                    "Più dispositivi DFU",
                    "Lascia collegato un solo modulo",
                    "Attenzione",
                    "Sono collegati più moduli DFU",
                    ArxTheme.Amber);
            else
                SetConnectionUi(
                    "Nessun modulo DFU",
                    "Collega un modulo in modalità DFU",
                    "In attesa",
                    "Nessun dispositivo DFU rilevato",
                    ArxTheme.Muted);
        }
        catch (Exception ex)
        {
            SetConnectionUi(
                "USB non disponibile",
                ex.Message,
                "Errore USB",
                ex.Message,
                ArxTheme.Red);
        }
    }

    private void SetConnectionUi(
        string header,
        string headerSub,
        string system,
        string systemSub,
        Color color)
    {
        _headerConnection.Text = header;
        _headerConnectionSub.Text = headerSub;
        _headerConnection.ForeColor = color;
        _headerConnectionDot.DotColor = color;
        _headerConnectionDot.Invalidate();

        _systemState.Text = system;
        _systemStateSub.Text = systemSub;
        _systemState.ForeColor = color;
        _systemStateDot.DotColor = color;
        _systemStateDot.Invalidate();
    }

    private void SetProgressMessage(string message, int progress)
    {
        progress = Math.Clamp(progress, 0, 100);
        _dashboardSteps.ProgressValue = progress;
        _updateSteps.ProgressValue = progress;
        _updatePageStatus.Text = message;
        Text = $"AlfaRaceX Updater {AppConstants.UpdaterVersion} — {progress}%";
    }

    private void SetUpdateBusy(bool busy)
    {
        _check.Enabled = !busy;
        if (busy)
        {
            _download.Enabled = false;
            _update.Enabled = false;
        }
        else
        {
            _download.Enabled = _manifest is not null;
            _update.Enabled = _prepared is not null;
            Text = $"AlfaRaceX Updater {AppConstants.UpdaterVersion}";
        }

        UseWaitCursor = busy;
    }

    private void BrowseBackupFolder()
    {
        using var dialog = new FolderBrowserDialog
        {
            Description = "Seleziona la cartella per i backup AlfaRaceX",
            SelectedPath = Directory.Exists(_backupFolder.Text)
                ? _backupFolder.Text
                : Environment.GetFolderPath(
                    Environment.SpecialFolder.MyDocuments)
        };

        if (dialog.ShowDialog(this) == DialogResult.OK)
            _backupFolder.Text = dialog.SelectedPath;
    }

    private void BrowseRestoreFile()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "Seleziona backup firmware",
            Filter = "Backup firmware (*.bin)|*.bin|Tutti i file (*.*)|*.*",
            CheckFileExists = true,
            Multiselect = false
        };

        if (dialog.ShowDialog(this) != DialogResult.OK)
            return;

        _restoreFile.Text = dialog.FileName;

        try
        {
            byte[] data = File.ReadAllBytes(dialog.FileName);
            string hash = Convert.ToHexString(
                SHA256.HashData(data)).ToLowerInvariant();
            _restoreHash.Text =
                $"{hash}  ·  {data.Length:N0} byte";
        }
        catch (Exception ex)
        {
            _restoreHash.Text = ex.Message;
        }
    }

    private void ExportLog()
    {
        using var dialog = new SaveFileDialog
        {
            Title = "Esporta log AlfaRaceX",
            Filter = "File log (*.log)|*.log|File testo (*.txt)|*.txt",
            FileName = $"AlfaRaceX-Updater-{DateTime.Now:yyyyMMdd-HHmmss}.log"
        };

        if (dialog.ShowDialog(this) != DialogResult.OK)
            return;

        AppLog.ExportCurrent(dialog.FileName);
        Log($"Log esportato: {dialog.FileName}.");
    }

    private void OpenLogFolder()
    {
        Directory.CreateDirectory(AppLog.LogDirectory);
        Process.Start(new ProcessStartInfo
        {
            FileName = AppLog.LogDirectory,
            UseShellExecute = true
        });
    }

    private void ClearLogs()
    {
        DialogResult answer = MessageBox.Show(
            this,
            "Vuoi cancellare il log della giornata corrente?",
            "AlfaRaceX",
            MessageBoxButtons.YesNo,
            MessageBoxIcon.Question);

        if (answer != DialogResult.Yes)
            return;

        AppLog.ClearCurrent();
        _dashboardLog.Rows.Clear();
        _fullLog.Rows.Clear();
        Log("Log ripulito.");
    }

    private void LoadExistingLogs()
    {
        foreach (AppLogEntry entry in AppLog.ReadRecent())
        {
            AddLogRow(_fullLog, entry);
            AddLogRow(_dashboardLog, entry);
        }

        TrimDashboardLog();
    }

    private void RefreshFullLog()
    {
        _fullLog.Rows.Clear();
        foreach (AppLogEntry entry in AppLog.ReadRecent())
            AddLogRow(_fullLog, entry);
    }

    private void Log(string message, string level = "INFO")
    {
        if (InvokeRequired)
        {
            BeginInvoke(new Action(() => Log(message, level)));
            return;
        }

        AppLog.Write(level, message);
        var entry = new AppLogEntry(
            DateTime.Now,
            level.ToUpperInvariant(),
            message);

        AddLogRow(_dashboardLog, entry);
        AddLogRow(_fullLog, entry);
        TrimDashboardLog();
    }

    private static void AddLogRow(
        DataGridView grid,
        AppLogEntry entry)
    {
        int row = grid.Rows.Add(
            entry.Timestamp.ToString("dd/MM/yyyy HH:mm:ss"),
            entry.Level,
            entry.Message);

        Color color = entry.Level switch
        {
            "ERRORE" => Color.FromArgb(255, 82, 98),
            "AVVISO" => ArxTheme.Amber,
            "DEBUG" => ArxTheme.Blue,
            _ => ArxTheme.Green
        };

        grid.Rows[row].Cells[1].Style.ForeColor = color;

        if (grid.Rows.Count > 0)
            grid.FirstDisplayedScrollingRowIndex =
                grid.Rows.Count - 1;
    }

    private void TrimDashboardLog()
    {
        while (_dashboardLog.Rows.Count > 8)
            _dashboardLog.Rows.RemoveAt(0);
    }

    private static void ConfigureLogGrid(DataGridView grid)
    {
        grid.BackgroundColor = Color.FromArgb(3, 8, 11);
        grid.BorderStyle = BorderStyle.None;
        grid.ReadOnly = true;
        grid.AllowUserToAddRows = false;
        grid.AllowUserToDeleteRows = false;
        grid.AllowUserToResizeRows = false;
        grid.RowHeadersVisible = false;
        grid.EnableHeadersVisualStyles = false;
        grid.ColumnHeadersDefaultCellStyle.BackColor =
            Color.FromArgb(12, 22, 29);
        grid.ColumnHeadersDefaultCellStyle.ForeColor =
            ArxTheme.Muted;
        grid.ColumnHeadersDefaultCellStyle.Font =
            ArxTheme.Font(7.8f, FontStyle.Bold);
        grid.DefaultCellStyle.BackColor =
            Color.FromArgb(3, 8, 11);
        grid.DefaultCellStyle.ForeColor =
            Color.FromArgb(201, 212, 221);
        grid.DefaultCellStyle.SelectionBackColor =
            Color.FromArgb(18, 33, 43);
        grid.DefaultCellStyle.SelectionForeColor =
            Color.White;
        grid.DefaultCellStyle.Font =
            new Font("Consolas", 7.8f);
        grid.GridColor = Color.FromArgb(15, 28, 36);
        grid.AutoSizeColumnsMode =
            DataGridViewAutoSizeColumnsMode.Fill;

        grid.Columns.Clear();
        grid.Columns.Add("time", "Data e ora");
        grid.Columns.Add("level", "Livello");
        grid.Columns.Add("message", "Messaggio");
        grid.Columns[0].FillWeight = 17;
        grid.Columns[1].FillWeight = 10;
        grid.Columns[2].FillWeight = 73;
    }

    private static Button MakeSmallButton(
        string text,
        int x,
        int y,
        int width,
        int height)
    {
        var button = new Button
        {
            Text = text,
            Bounds = new Rectangle(x, y, width, height),
            FlatStyle = FlatStyle.Flat,
            BackColor = Color.FromArgb(13, 24, 32),
            ForeColor = ArxTheme.Text,
            Font = ArxTheme.Font(8f, FontStyle.Bold),
            Cursor = Cursors.Hand
        };
        button.FlatAppearance.BorderColor = ArxTheme.Border;
        button.FlatAppearance.BorderSize = 1;
        return button;
    }

    private static Label MakeLabel(
        string text,
        int x,
        int y,
        int width,
        int height,
        float size,
        FontStyle style,
        Color color)
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
        Stream? stream = Assembly
            .GetExecutingAssembly()
            .GetManifestResourceStream(resourceName);

        if (stream is null)
            throw new InvalidOperationException(
                $"Risorsa grafica non disponibile: {resourceName}");

        using (stream)
            return Image.FromStream(stream).Clone() as Image
                ?? throw new InvalidOperationException(
                    $"Risorsa grafica non valida: {resourceName}");
    }

    private static void OpenUrl(string url)
    {
        Process.Start(new ProcessStartInfo
        {
            FileName = url,
            UseShellExecute = true
        });
    }

    private static bool IsNewerVersion(
        string available,
        string current) =>
        Version.TryParse(
            available.Trim(),
            out Version? remote) &&
        Version.TryParse(
            current.Trim(),
            out Version? local) &&
        remote > local;

    private static string SelectedRole(ComboBox combo) =>
        combo.SelectedItem?.ToString() ?? "BH";

    private static string PortHint(string role) =>
        role.ToUpperInvariant() switch
        {
            "BH" => "Usa la porta USB-C sinistra (BH)",
            "C2" => "Usa la porta USB-C centrale (C2)",
            "C1" => "Usa la porta USB-C destra (C1)",
            _ => "Usa la porta del modulo selezionato"
        };

    private static string ShortHash(string hash)
    {
        if (string.IsNullOrWhiteSpace(hash))
            return "—";

        return hash.Length >= 12
            ? $"{hash[..6]}…{hash[^6..]}"
            : hash;
    }

    private sealed record ModuleUi(
        Label Current,
        Label Available,
        Label Hash,
        Label State,
        StatusDot Dot);
}
