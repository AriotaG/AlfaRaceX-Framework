using System.Drawing;

namespace AlfaRaceX.Updater;

internal sealed class MainForm : Form
{
    private readonly Label _version = new();
    private readonly Label _status = new();
    private readonly ProgressBar _progress = new();
    private readonly Button _check = new();
    private readonly Button _update = new();
    private readonly TextBox _log = new();
    private readonly UpdateCoordinator _coordinator = new();

    private UpdateManifest? _manifest;
    private IReadOnlyList<PreparedFirmware>? _prepared;
    private CancellationTokenSource? _cts;

    public MainForm()
    {
        Text = "AlfaRaceX Updater";
        ClientSize = new Size(720, 560);
        MinimumSize = new Size(720, 560);
        MaximumSize = new Size(720, 560);
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Color.FromArgb(17, 17, 19);
        ForeColor = Color.White;
        Font = new Font("Segoe UI", 10f);
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;

        var logo = new Label
        {
            Text = "ALFARACEX",
            ForeColor = Color.FromArgb(220, 35, 55),
            Font = new Font("Segoe UI Semibold", 28f, FontStyle.Bold),
            AutoSize = true,
            Location = new Point(28, 22)
        };
        Controls.Add(logo);

        var subtitle = new Label
        {
            Text = "FIRMWARE UPDATER",
            ForeColor = Color.Gainsboro,
            Font = new Font("Segoe UI", 11f, FontStyle.Bold),
            AutoSize = true,
            Location = new Point(32, 72)
        };
        Controls.Add(subtitle);

        _version.SetBounds(32, 112, 650, 26);
        _version.Text = "Release: non verificata";
        Controls.Add(_version);

        _status.SetBounds(32, 144, 650, 46);
        _status.Text = "Verifica la release disponibile prima di iniziare.";
        Controls.Add(_status);

        _progress.SetBounds(32, 198, 656, 22);
        Controls.Add(_progress);

        _check.Text = "VERIFICA AGGIORNAMENTI";
        _check.SetBounds(32, 240, 260, 42);
        _check.FlatStyle = FlatStyle.Flat;
        _check.FlatAppearance.BorderColor = Color.FromArgb(100, 100, 105);
        _check.Click += async (_, _) => await CheckAsync();
        Controls.Add(_check);

        _update.Text = "AGGIORNA ALFARACEX";
        _update.SetBounds(308, 240, 380, 42);
        _update.Enabled = false;
        _update.BackColor = Color.FromArgb(180, 28, 45);
        _update.ForeColor = Color.White;
        _update.FlatStyle = FlatStyle.Flat;
        _update.FlatAppearance.BorderSize = 0;
        _update.Click += async (_, _) => await UpdateAsync();
        Controls.Add(_update);

        _log.SetBounds(32, 306, 656, 220);
        _log.Multiline = true;
        _log.ReadOnly = true;
        _log.ScrollBars = ScrollBars.Vertical;
        _log.BackColor = Color.FromArgb(8, 8, 10);
        _log.ForeColor = Color.Gainsboro;
        _log.BorderStyle = BorderStyle.FixedSingle;
        _log.Font = new Font("Consolas", 9f);
        Controls.Add(_log);

        FormClosing += (_, _) => _cts?.Cancel();
    }

    private IProgress<(string Message, int Progress)> UiProgress() =>
        new Progress<(string Message, int Progress)>(x =>
        {
            _status.Text = x.Message;
            _progress.Value = Math.Clamp(x.Progress, 0, 100);
        });

    private async Task CheckAsync()
    {
        _check.Enabled = false;
        _update.Enabled = false;
        _cts = new CancellationTokenSource();

        try
        {
            Log("Controllo manifest AlfaRaceX...");
            _manifest = await _coordinator.LoadManifestAsync(_cts.Token);
            _version.Text = $"Release disponibile: {_manifest.Version}  |  Canale: {_manifest.Channel}";
            Log($"Release {_manifest.Version} trovata.");

            _prepared = await _coordinator.PrepareAsync(
                _manifest,
                UiProgress(),
                _cts.Token);

            Log("Tutti i firmware hanno superato SHA-256 e controllo indirizzi.");
            _status.Text = "Pronto per l'aggiornamento.";
            _update.Enabled = true;
        }
        catch (Exception ex)
        {
            _status.Text = "Verifica non riuscita.";
            Log("ERRORE: " + ex.Message);
            MessageBox.Show(
                this, ex.Message, "AlfaRaceX",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        finally
        {
            _check.Enabled = true;
        }
    }

    private async Task UpdateAsync()
    {
        if (_manifest is null || _prepared is null) return;

        _check.Enabled = false;
        _update.Enabled = false;
        _cts = new CancellationTokenSource();

        try
        {
            foreach (PreparedFirmware firmware in _prepared)
            {
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

                await _coordinator.FlashAsync(
                    firmware,
                    UiProgress(),
                    Log,
                    _cts.Token);

                MessageBox.Show(
                    this,
                    $"{firmware.Target.Label} completato.\n\n" +
                    "Scollega il cavo prima di passare al modulo successivo.",
                    "AlfaRaceX",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information);
            }

            _progress.Value = 100;
            _status.Text = $"AlfaRaceX {_manifest.Version} installato.";
            Log("Aggiornamento completo.");

            MessageBox.Show(
                this,
                $"AlfaRaceX {_manifest.Version} installato e verificato sui tre moduli.",
                "AlfaRaceX",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);
        }
        catch (OperationCanceledException)
        {
            _status.Text = "Aggiornamento annullato.";
            Log("Operazione annullata.");
        }
        catch (Exception ex)
        {
            _status.Text = "Aggiornamento interrotto.";
            Log("ERRORE: " + ex.Message);
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
            _check.Enabled = true;
            _update.Enabled = _prepared is not null;
        }
    }

    private void Log(string message)
    {
        if (InvokeRequired)
        {
            BeginInvoke(new Action(() => Log(message)));
            return;
        }

        _log.AppendText(
            $"[{DateTime.Now:HH:mm:ss}] {message}{Environment.NewLine}");
    }
}
