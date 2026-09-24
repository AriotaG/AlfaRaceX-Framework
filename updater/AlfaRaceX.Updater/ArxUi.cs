using System.Drawing.Drawing2D;

namespace AlfaRaceX.Updater;

internal static class ArxTheme
{
    public static readonly Color Background = Color.FromArgb(4, 8, 11);
    public static readonly Color Sidebar = Color.FromArgb(5, 12, 17);
    public static readonly Color Panel = Color.FromArgb(8, 15, 21);
    public static readonly Color Panel2 = Color.FromArgb(12, 22, 29);
    public static readonly Color Border = Color.FromArgb(39, 55, 67);
    public static readonly Color BorderSoft = Color.FromArgb(25, 38, 48);
    public static readonly Color Red = Color.FromArgb(226, 0, 37);
    public static readonly Color RedDark = Color.FromArgb(105, 4, 19);
    public static readonly Color Green = Color.FromArgb(20, 225, 115);
    public static readonly Color Amber = Color.FromArgb(255, 185, 0);
    public static readonly Color Blue = Color.FromArgb(20, 147, 220);
    public static readonly Color Text = Color.FromArgb(238, 243, 247);
    public static readonly Color Muted = Color.FromArgb(137, 154, 168);

    public static Font Font(float size, FontStyle style = FontStyle.Regular) =>
        new("Segoe UI", size, style, GraphicsUnit.Point);

    public static GraphicsPath Rounded(Rectangle rect, int radius)
    {
        int d = radius * 2;
        var p = new GraphicsPath();
        p.AddArc(rect.X, rect.Y, d, d, 180, 90);
        p.AddArc(rect.Right - d, rect.Y, d, d, 270, 90);
        p.AddArc(rect.Right - d, rect.Bottom - d, d, d, 0, 90);
        p.AddArc(rect.X, rect.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }
}

internal sealed class ArxPanel : Panel
{
    public Color BorderColor { get; set; } = ArxTheme.Border;
    public int Radius { get; set; } = 9;

    public ArxPanel()
    {
        DoubleBuffered = true;
        BackColor = ArxTheme.Panel;
        Padding = new Padding(1);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        var r = ClientRectangle;
        r.Width -= 1;
        r.Height -= 1;
        using var path = ArxTheme.Rounded(r, Radius);
        using var pen = new Pen(BorderColor);
        e.Graphics.DrawPath(pen, path);
    }
}

internal sealed class ArxButton : Button
{
    public bool Primary { get; set; }
    public Color Accent { get; set; } = ArxTheme.Panel2;

    public ArxButton()
    {
        FlatStyle = FlatStyle.Flat;
        FlatAppearance.BorderSize = 0;
        ForeColor = ArxTheme.Text;
        BackColor = Color.Transparent;
        Cursor = Cursors.Hand;
        Font = ArxTheme.Font(10f, FontStyle.Bold);
        DoubleBuffered = true;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        var r = ClientRectangle;
        r.Width -= 1;
        r.Height -= 1;
        using var path = ArxTheme.Rounded(r, 8);

        Color top;
        Color bottom;

        if (!Enabled)
        {
            top = Color.FromArgb(27, 36, 43);
            bottom = Color.FromArgb(17, 24, 30);
        }
        else if (Primary)
        {
            top = ArxTheme.Red;
            bottom = Color.FromArgb(151, 0, 24);
        }
        else
        {
            top = Accent;
            bottom = Color.FromArgb(12, 21, 28);
        }

        using var brush = new LinearGradientBrush(r, top, bottom, 90f);
        e.Graphics.FillPath(brush, path);

        using var pen = new Pen(
            Enabled && Primary
                ? Color.FromArgb(255, 70, 89)
                : ArxTheme.Border);
        e.Graphics.DrawPath(pen, path);

        TextRenderer.DrawText(
            e.Graphics,
            Text,
            Font,
            r,
            Enabled ? ForeColor : Color.FromArgb(101, 116, 128),
            TextFormatFlags.HorizontalCenter |
            TextFormatFlags.VerticalCenter |
            TextFormatFlags.EndEllipsis);
    }
}

internal sealed class SideNavButton : Control
{
    private bool _active;

    public string Glyph { get; set; } = "";
    public string Title { get; set; } = "";
    public string Subtitle { get; set; } = "";

    public bool Active
    {
        get => _active;
        set
        {
            _active = value;
            Invalidate();
        }
    }

    public SideNavButton()
    {
        Height = 72;
        Cursor = Cursors.Hand;
        DoubleBuffered = true;
        BackColor = ArxTheme.Sidebar;
        TabStop = true;
        SetStyle(ControlStyles.Selectable, true);
    }

    protected override void OnClick(EventArgs e)
    {
        base.OnClick(e);
        Focus();
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.KeyCode is Keys.Enter or Keys.Space)
        {
            OnClick(EventArgs.Empty);
            e.Handled = true;
        }
        base.OnKeyDown(e);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        Rectangle r = ClientRectangle;

        using var bg = new LinearGradientBrush(
            r,
            Active ? Color.FromArgb(114, 4, 18) : ArxTheme.Sidebar,
            Active ? Color.FromArgb(25, 12, 17) : Color.FromArgb(7, 14, 19),
            0f);
        e.Graphics.FillRectangle(bg, r);

        if (Active)
        {
            using var accent = new SolidBrush(ArxTheme.Red);
            e.Graphics.FillRectangle(accent, 0, 0, 5, Height);
        }

        var glyphRect = new Rectangle(18, 13, 48, 48);
        TextRenderer.DrawText(
            e.Graphics,
            Glyph,
            ArxTheme.Font(21f, FontStyle.Bold),
            glyphRect,
            Active ? Color.White : Color.FromArgb(222, 230, 237),
            TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);

        var titleRect = new Rectangle(76, 12, Width - 86, 28);
        TextRenderer.DrawText(
            e.Graphics,
            Title,
            ArxTheme.Font(10.4f, FontStyle.Bold),
            titleRect,
            Color.White,
            TextFormatFlags.Left | TextFormatFlags.VerticalCenter |
            TextFormatFlags.EndEllipsis);

        var subRect = new Rectangle(76, 38, Width - 86, 22);
        TextRenderer.DrawText(
            e.Graphics,
            Subtitle,
            ArxTheme.Font(8.4f),
            subRect,
            ArxTheme.Muted,
            TextFormatFlags.Left | TextFormatFlags.VerticalCenter |
            TextFormatFlags.EndEllipsis);

        using var line = new Pen(Color.FromArgb(15, 28, 36));
        e.Graphics.DrawLine(line, 0, Height - 1, Width, Height - 1);
    }
}

internal sealed class StepStrip : Control
{
    private readonly string[] _titles =
        { "Verifica", "Download", "Preparazione", "Installazione", "Verifica finale" };

    private readonly string[] _subtitles =
        { "Controllo versioni", "Scarica firmware", "Prepara moduli", "Scrittura firmware", "Controllo integrità" };

    private int _step;
    private int _progress;

    public int Step
    {
        get => _step;
        set
        {
            _step = Math.Clamp(value, 0, 5);
            Invalidate();
        }
    }

    public int ProgressValue
    {
        get => _progress;
        set
        {
            _progress = Math.Clamp(value, 0, 100);
            Invalidate();
        }
    }

    public StepStrip()
    {
        DoubleBuffered = true;
        BackColor = ArxTheme.Panel;
        Height = 88;
        Font = ArxTheme.Font(8.8f);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;

        int margin = 48;
        int y = 32;
        int width = Math.Max(1, ClientSize.Width - margin * 2 - 78);
        int segment = width / 4;

        using var basePen = new Pen(Color.FromArgb(54, 69, 81), 2);
        using var activePen = new Pen(ArxTheme.Red, 2);

        e.Graphics.DrawLine(basePen, margin, y, margin + width, y);

        if (_step > 1)
        {
            int completedSegments = Math.Min(4, _step - 1);
            e.Graphics.DrawLine(
                activePen,
                margin,
                y,
                margin + completedSegments * segment,
                y);
        }

        for (int i = 0; i < 5; i++)
        {
            int x = margin + i * segment;
            bool active = i < _step;

            using var fill = new SolidBrush(
                active ? Color.FromArgb(27, 8, 14) : Color.FromArgb(18, 28, 36));
            using var pen = new Pen(
                active ? ArxTheme.Red : Color.FromArgb(112, 130, 144),
                2);

            e.Graphics.FillEllipse(fill, x - 17, y - 17, 34, 34);
            e.Graphics.DrawEllipse(pen, x - 17, y - 17, 34, 34);

            var nr = new Rectangle(x - 17, y - 17, 34, 34);
            TextRenderer.DrawText(
                e.Graphics,
                (i + 1).ToString(),
                ArxTheme.Font(9.5f, FontStyle.Bold),
                nr,
                Color.White,
                TextFormatFlags.HorizontalCenter |
                TextFormatFlags.VerticalCenter);

            var title = new Rectangle(x + 31, 7, 145, 22);
            var sub = new Rectangle(x + 31, 29, 160, 22);

            if (i == 4)
            {
                title.X = x + 30;
                title.Width = 130;
                sub.X = x + 30;
                sub.Width = 130;
            }

            TextRenderer.DrawText(
                e.Graphics,
                _titles[i],
                ArxTheme.Font(8.6f, FontStyle.Bold),
                title,
                active ? ArxTheme.Text : Color.FromArgb(213, 221, 228),
                TextFormatFlags.Left | TextFormatFlags.VerticalCenter);

            TextRenderer.DrawText(
                e.Graphics,
                _subtitles[i],
                ArxTheme.Font(7.5f),
                sub,
                ArxTheme.Muted,
                TextFormatFlags.Left | TextFormatFlags.VerticalCenter);
        }

        var bar = new Rectangle(18, Height - 18, ClientSize.Width - 84, 9);
        using var barBg = new SolidBrush(Color.FromArgb(25, 40, 52));
        e.Graphics.FillRectangle(barBg, bar);

        int progressFill = (int)Math.Round(bar.Width * (_progress / 100d));
        if (progressFill > 0)
        {
            using var barFill = new LinearGradientBrush(
                new Rectangle(bar.X, bar.Y, Math.Max(progressFill, 1), bar.Height),
                ArxTheme.Red,
                Color.FromArgb(255, 38, 60),
                0f);
            e.Graphics.FillRectangle(
                barFill,
                bar.X,
                bar.Y,
                progressFill,
                bar.Height);
        }

        var pct = new Rectangle(ClientSize.Width - 60, Height - 27, 54, 22);
        TextRenderer.DrawText(
            e.Graphics,
            $"{_progress}%",
            ArxTheme.Font(8.6f, FontStyle.Bold),
            pct,
            ArxTheme.Text,
            TextFormatFlags.Right | TextFormatFlags.VerticalCenter);
    }
}

internal sealed class StatusDot : Control
{
    public Color DotColor { get; set; } = ArxTheme.Green;

    public StatusDot()
    {
        Size = new Size(20, 20);
        DoubleBuffered = true;
        BackColor = Color.Transparent;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;

        using var glow = new SolidBrush(Color.FromArgb(65, DotColor));
        using var ring = new Pen(DotColor, 2);
        using var fill = new SolidBrush(DotColor);

        e.Graphics.FillEllipse(glow, 0, 0, Width, Height);
        e.Graphics.DrawEllipse(ring, 3, 3, Width - 7, Height - 7);
        e.Graphics.FillEllipse(fill, 7, 7, Width - 14, Height - 14);
    }
}
