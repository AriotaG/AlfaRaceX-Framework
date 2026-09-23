using System.Drawing;
using System.Reflection;

namespace AlfaRaceX.Updater;

internal sealed class SplashForm : Form
{
    public SplashForm()
    {
        FormBorderStyle = FormBorderStyle.None;
        StartPosition = FormStartPosition.CenterScreen;
        ShowInTaskbar = false;
        BackColor = Color.Black;
        ClientSize = new Size(960, 540);
        TopMost = true;

        var image = LoadSplash();
        BackgroundImage = image;
        BackgroundImageLayout = ImageLayout.Stretch;
    }

    private static Image LoadSplash()
    {
        Stream? stream = Assembly.GetExecutingAssembly()
            .GetManifestResourceStream("AlfaRaceX.Splash.jpg");

        if (stream is null)
            throw new InvalidOperationException("Risorsa grafica AlfaRaceX non disponibile.");

        using (stream)
            return Image.FromStream(stream).Clone() as Image
                ?? throw new InvalidOperationException("Immagine AlfaRaceX non valida.");
    }
}
