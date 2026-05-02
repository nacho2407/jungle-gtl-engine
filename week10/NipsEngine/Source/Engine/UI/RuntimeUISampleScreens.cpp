#include "UI/RuntimeUISampleScreens.h"

#include "UI/RuntimeUISystem.h"

namespace
{
    FRuntimeUIStyle MakePanelStyle()
    {
        FRuntimeUIStyle Style;
        Style.BackgroundColor = FRuntimeUIColor(0.02f, 0.025f, 0.035f, 0.78f);
        Style.TextColor = FRuntimeUIColor(0.85f, 0.95f, 1.0f, 1.0f);
        Style.Tint = FRuntimeUIColor(0.1f, 0.85f, 1.0f, 1.0f);
        Style.Rounding = 8.0f;
        return Style;
    }

    FRuntimeUIStyle MakeButtonStyle()
    {
        FRuntimeUIStyle Style;
        Style.BackgroundColor = FRuntimeUIColor(0.05f, 0.16f, 0.22f, 0.92f);
        Style.TextColor = FRuntimeUIColor(0.88f, 1.0f, 1.0f, 1.0f);
        Style.Tint = FRuntimeUIColor(0.4f, 0.95f, 1.0f, 1.0f);
        Style.DisabledTint = FRuntimeUIColor(0.2f, 0.24f, 0.26f, 0.6f);
        Style.Rounding = 6.0f;
        return Style;
    }
}

void RuntimeUISamples::BuildMinimalGameJamScreens(FRuntimeUISystem& UI)
{
    UI.CreateScreen("Title");
    UI.CreateScreen("HUD");
    UI.CreateScreen("Pause");
    UI.CreateScreen("Result");

    FRuntimeUIWidgetDesc TitlePanel;
    TitlePanel.Id = "Title.Root";
    TitlePanel.Type = ERuntimeUIWidgetType::Panel;
    TitlePanel.AnchorMin = FRuntimeUIVector2(0.5f, 0.5f);
    TitlePanel.AnchorMax = FRuntimeUIVector2(0.5f, 0.5f);
    TitlePanel.Pivot = FRuntimeUIVector2(0.5f, 0.5f);
    TitlePanel.Size = FRuntimeUIVector2(560.0f, 360.0f);
    TitlePanel.Style = MakePanelStyle();
    UI.CreateWidget("Title", TitlePanel);

    FRuntimeUIWidgetDesc TitleText;
    TitleText.Id = "Title.TitleText";
    TitleText.ParentId = "Title.Root";
    TitleText.Type = ERuntimeUIWidgetType::Text;
    TitleText.Position = FRuntimeUIVector2(64.0f, 54.0f);
    TitleText.Size = FRuntimeUIVector2(430.0f, 60.0f);
    TitleText.Text = "SSAMURAI";
    TitleText.Style = MakePanelStyle();
    TitleText.Style.FontScale = 2.2f;
    UI.CreateWidget("Title", TitleText);

    FRuntimeUIWidgetDesc StartButton;
    StartButton.Id = "Title.StartButton";
    StartButton.ParentId = "Title.Root";
    StartButton.Type = ERuntimeUIWidgetType::Button;
    StartButton.Position = FRuntimeUIVector2(160.0f, 220.0f);
    StartButton.Size = FRuntimeUIVector2(240.0f, 54.0f);
    StartButton.Text = "START";
    StartButton.ActionEvent = "RunRequested";
    StartButton.Style = MakeButtonStyle();
    UI.CreateWidget("Title", StartButton);

    FRuntimeUIWidgetDesc ScoreText;
    ScoreText.Id = "HUD.ScoreText";
    ScoreText.Type = ERuntimeUIWidgetType::Text;
    ScoreText.Position = FRuntimeUIVector2(32.0f, 28.0f);
    ScoreText.Size = FRuntimeUIVector2(300.0f, 36.0f);
    ScoreText.Text = "Score 0";
    ScoreText.Style = MakePanelStyle();
    ScoreText.Style.FontScale = 1.2f;
    UI.CreateWidget("HUD", ScoreText);

    FRuntimeUIWidgetDesc SlowGauge;
    SlowGauge.Id = "HUD.SlowGauge";
    SlowGauge.Type = ERuntimeUIWidgetType::ProgressBar;
    SlowGauge.AnchorMin = FRuntimeUIVector2(0.5f, 1.0f);
    SlowGauge.AnchorMax = FRuntimeUIVector2(0.5f, 1.0f);
    SlowGauge.Pivot = FRuntimeUIVector2(0.5f, 1.0f);
    SlowGauge.Position = FRuntimeUIVector2(0.0f, -42.0f);
    SlowGauge.Size = FRuntimeUIVector2(360.0f, 18.0f);
    SlowGauge.Progress = 1.0f;
    SlowGauge.Style = MakeButtonStyle();
    UI.CreateWidget("HUD", SlowGauge);

    FRuntimeUIWidgetDesc PausePanel;
    PausePanel.Id = "Pause.Root";
    PausePanel.Type = ERuntimeUIWidgetType::Panel;
    PausePanel.AnchorMin = FRuntimeUIVector2(0.5f, 0.5f);
    PausePanel.AnchorMax = FRuntimeUIVector2(0.5f, 0.5f);
    PausePanel.Pivot = FRuntimeUIVector2(0.5f, 0.5f);
    PausePanel.Size = FRuntimeUIVector2(420.0f, 260.0f);
    PausePanel.Style = MakePanelStyle();
    UI.CreateWidget("Pause", PausePanel);

    FRuntimeUIWidgetDesc ResumeButton;
    ResumeButton.Id = "Pause.ResumeButton";
    ResumeButton.ParentId = "Pause.Root";
    ResumeButton.Type = ERuntimeUIWidgetType::Button;
    ResumeButton.Position = FRuntimeUIVector2(90.0f, 104.0f);
    ResumeButton.Size = FRuntimeUIVector2(240.0f, 52.0f);
    ResumeButton.Text = "RESUME";
    ResumeButton.ActionEvent = "ResumeRequested";
    ResumeButton.Style = MakeButtonStyle();
    UI.CreateWidget("Pause", ResumeButton);

    FRuntimeUIWidgetDesc ResultPanel;
    ResultPanel.Id = "Result.Root";
    ResultPanel.Type = ERuntimeUIWidgetType::Panel;
    ResultPanel.AnchorMin = FRuntimeUIVector2(0.5f, 0.5f);
    ResultPanel.AnchorMax = FRuntimeUIVector2(0.5f, 0.5f);
    ResultPanel.Pivot = FRuntimeUIVector2(0.5f, 0.5f);
    ResultPanel.Size = FRuntimeUIVector2(520.0f, 320.0f);
    ResultPanel.Style = MakePanelStyle();
    UI.CreateWidget("Result", ResultPanel);

    FRuntimeUIWidgetDesc ResultText;
    ResultText.Id = "Result.FinalScoreText";
    ResultText.ParentId = "Result.Root";
    ResultText.Type = ERuntimeUIWidgetType::Text;
    ResultText.Position = FRuntimeUIVector2(72.0f, 76.0f);
    ResultText.Size = FRuntimeUIVector2(380.0f, 46.0f);
    ResultText.Text = "Final Score 0";
    ResultText.Style = MakePanelStyle();
    ResultText.Style.FontScale = 1.45f;
    UI.CreateWidget("Result", ResultText);

    UI.SetActiveScreen(FRuntimeUISystem::DefaultCanvasId, "Title");
}
