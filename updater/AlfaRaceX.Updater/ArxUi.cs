using System.Drawing.Drawing2D;

namespace AlfaRaceX.Updater;

internal static class ArxTheme
{
    public static readonly Color Background = Color.FromArgb(5, 8, 11);
    public static readonly Color Sidebar = Color.FromArgb(7, 11, 15);
    public static readonly Color Panel = Color.FromArgb(10, 16, 22);
    public static readonly Color Panel2 = Color.FromArgb(13, 21, 28);
    public static readonly Color Border = Color.FromArgb(42, 54, 66);
    public static readonly Color Red = Color.FromArgb(226, 8, 34);
    public static readonly Color RedDark = Color.FromArgb(120, 0, 17);
    public static readonly Color Green = Color.FromArgb(34, 226, 119);
    public static readonly Color Amber = Color.FromArgb(255, 194, 26);
    public static readonly Color Blue = Color.FromArgb(22, 156, 228);
    public static readonly Color Text = Color.FromArgb(237, 241, 245);
    public static readonly Color Muted = Color.FromArgb(142, 157, 172);

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
        r.Width -= 1; r.Height -= 1;
        using var path = ArxTheme.Rounded(r, Radius);
        using var pen = new Pen(BorderColor);
        e.Graphics.DrawPath(pen, path);
    }
}

internal sealed class ArxButton : Button
{
    public Color Accent { get; set; } = ArxTheme.Panel2;
    public bool Primary { get; set; }

    public ArxButton()
    {
        FlatStyle = FlatStyle.Flat;
        FlatAppearance.BorderSize = 0;
        ForeColor = ArxTheme.Text;
        BackColor = Color.Transparent;
        Cursor = Cursors.Hand;
        Font = ArxTheme.Font(10.5f, FontStyle.Bold);
        DoubleBuffered = true;
    }

    protected override void OnPaint(PaintEventArgs pevent)
    {
        pevent.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        var r = ClientRectangle;
        r.Width -= 1; r.Height -= 1;
        using var path = ArxTheme.Rounded(r, 8);
        Color top = Enabled
            ? (Primary ? ArxTheme.Red : Accent)
            : Color.FromArgb(32, 42, 51);
        Color bottom = Enabled
            ? (Primary ? Color.FromArgb(166, 0, 24) : Color.FromArgb(15, 23, 30))
            : Color.FromArgb(24, 31, 38);
        using var brush = new LinearGradientBrush(r, top, bottom, 90f);
        pevent.Graphics.FillPath(brush, path);
        using var pen = new Pen(Enabled && Primary ? Color.FromArgb(255, 72, 90) : ArxTheme.Border);
        pevent.Graphics.DrawPath(pen, path);

        TextRenderer.DrawText(
            pevent.Graphics, Text, Font, r,
            Enabled ? ForeColor : Color.FromArgb(105, 119, 132),
            TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter |
            TextFormatFlags.EndEllipsis);
    }
}

internal sealed class SideNavButton : Button
{
    public bool Active { get; set; }

    public SideNavButton()
    {
        FlatStyle = FlatStyle.Flat;
        FlatAppearance.BorderSize = 0;
        TextAlign = ContentAlignment.MiddleLeft;
        Padding = new Padding(22, 0, 8, 0);
        Font = ArxTheme.Font(10.5f, FontStyle.Bold);
        Height = 64;
        ForeColor = ArxTheme.Text;
        BackColor = ArxTheme.Sidebar;
        Cursor = Cursors.Hand;
        DoubleBuffered = true;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        var r = ClientRectangle;
        using var bg = new LinearGradientBrush(
            r,
            Active ? Color.FromArgb(119, 5, 20) : ArxTheme.Sidebar,
            Active ? Color.FromArgb(27, 13, 18) : Color.FromArgb(9, 14, 18),
            0f);
        e.Graphics.FillRectangle(bg, r);
        if (Active)
        {
            using var accent = new SolidBrush(ArxTheme.Red);
            e.Graphics.FillRectangle(accent, 0, 0, 5, Height);
        }
        var textRect = new Rectangle(22, 0, Width - 26, Height);
        TextRenderer.DrawText(e.Graphics, Text, Font, textRect,
            Active ? Color.White : Color.FromArgb(210, 218, 226),
            TextFormatFlags.Left | TextFormatFlags.VerticalCenter);
    }
}

internal sealed class StepStrip : Control
{
    private readonly string[] _titles =
        { "Verifica", "Download", "Preparazione", "Installazione", "Verifica finale" };

    private int _step;
    public int Step
    {
        get => _step;
        set { _step = Math.Clamp(value, 0, 5); Invalidate(); }
    }

    public StepStrip()
    {
        DoubleBuffered = true;
        BackColor = ArxTheme.Panel;
        Height = 82;
        Font = ArxTheme.Font(9f);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        int margin = 42;
        int y = 33;
        int width = Math.Max(1, ClientSize.Width - margin * 2);
        int segment = width / 4;

        using var basePen = new Pen(Color.FromArgb(52, 64, 76), 3);
        using var activePen = new Pen(ArxTheme.Red, 3);
        e.Graphics.DrawLine(basePen, margin, y, margin + width, y);
        if (_step > 1)
            e.Graphics.DrawLine(activePen, margin, y,
                margin + Math.Min(4, _step - 1) * segment, y);

        for (int i = 0; i < 5; i++)
        {
            int x = margin + i * segment;
            bool active = i < _step;
            using var fill = new SolidBrush(active ? ArxTheme.Red : Color.FromArgb(31, 40, 49));
            using var pen = new Pen(active ? Color.FromArgb(255, 68, 87) : Color.FromArgb(118, 132, 145), 2);
            e.Graphics.FillEllipse(fill, x - 17, y - 17, 34, 34);
            e.Graphics.DrawEllipse(pen, x - 17, y - 17, 34, 34);
            var nr = new Rectangle(x - 17, y - 17, 34, 34);
            TextRenderer.DrawText(e.Graphics, (i + 1).ToString(),
                ArxTheme.Font(9.5f, FontStyle.Bold), nr, Color.White,
                TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);

            int tx = Math.Clamp(x - 54, 0, Math.Max(0, ClientSize.Width - 108));
            var tr = new Rectangle(tx, 55, 108, 22);
            TextRenderer.DrawText(e.Graphics, _titles[i], Font, tr,
                active ? ArxTheme.Text : ArxTheme.Muted,
                TextFormatFlags.HorizontalCenter | TextFormatFlags.Top |
                TextFormatFlags.EndEllipsis);
        }
    }
}

internal sealed class StatusDot : Control
{
    public Color DotColor { get; set; } = ArxTheme.Green;
    public StatusDot()
    {
        Size = new Size(18, 18);
        DoubleBuffered = true;
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        using var glow = new SolidBrush(Color.FromArgb(55, DotColor));
        using var fill = new SolidBrush(DotColor);
        e.Graphics.FillEllipse(glow, 0, 0, Width, Height);
        e.Graphics.FillEllipse(fill, 4, 4, Width - 8, Height - 8);
    }
}
