using MLToybox.Panel.Core;

namespace MLToybox.Panel;

public sealed class MainForm : Form
{
    private readonly string _settingsPath = PanelSettings.DefaultPath;
    private readonly PanelSettings _settings;
    private BridgeClient? _bridge;
    private ControlDocument _control = new();
    private long _lastSentSeq;
    private string _resourceIdsKey = "";

    private readonly Label _gameDirLabel = new() { AutoSize = true };
    private readonly Label _stateLabel = new() { AutoSize = true, Font = new Font(SystemFonts.DefaultFont, FontStyle.Bold) };

    private readonly CheckBox _resEnabled = new() { Text = "자원 목표값 유지", AutoSize = true };
    private readonly NumericUpDown _resInterval = new() { Minimum = 1, Maximum = 60, Value = 2, Width = 60 };
    private readonly NumericUpDown _fillValue = new() { Minimum = 0, Maximum = 1_000_000, Value = 500, Width = 90 };
    private readonly DataGridView _resGrid = new()
    {
        Dock = DockStyle.Fill, AllowUserToAddRows = false, AllowUserToDeleteRows = false,
        RowHeadersVisible = false, AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill,
    };

    private readonly CheckBox _buildEnabled = new() { Text = "건설 기능 사용", AutoSize = true };
    private readonly CheckBox _ignorePlacement = new() { Text = "배치 제한 무시", AutoSize = true };
    private readonly CheckBox _instantBuild = new() { Text = "즉시 완공", AutoSize = true };
    private readonly CheckBox _instantRepair = new() { Text = "즉시 수리", AutoSize = true };

    private readonly CheckBox _upgradeEnabled = new() { Text = "업그레이드 조건·비용·해금 무시", AutoSize = true };

    private readonly CheckBox _milEnabled = new() { Text = "군사 기능 사용", AutoSize = true };
    private readonly CheckBox _ignoreEquipment = new() { Text = "민병대 장비 요구 무시", AutoSize = true };
    private readonly CheckBox _ignorePopulation = new() { Text = "징집 인구 제한 무시", AutoSize = true };
    private readonly CheckBox _zeroUpkeep = new() { Text = "친위대·용병 유지비 0", AutoSize = true };
    private readonly CheckBox _unlimitedSquads = new() { Text = "부대 수 상한 해제", AutoSize = true };

    private readonly TextBox _statusText = new()
    {
        Multiline = true, ReadOnly = true, Dock = DockStyle.Fill, ScrollBars = ScrollBars.Vertical,
        Font = new Font(FontFamily.GenericMonospace, 9),
    };
    private readonly System.Windows.Forms.Timer _timer = new() { Interval = 1000 };

    public MainForm()
    {
        Text = "MLToybox Panel";
        MinimumSize = new Size(560, 480);
        _settings = PanelSettings.Load(_settingsPath);

        _resGrid.Columns.Add(new DataGridViewTextBoxColumn { Name = "Id", HeaderText = "자원", ReadOnly = true });
        _resGrid.Columns.Add(new DataGridViewTextBoxColumn { Name = "Current", HeaderText = "현재", ReadOnly = true });
        _resGrid.Columns.Add(new DataGridViewTextBoxColumn { Name = "Target", HeaderText = "목표 (빈칸=관리 안 함)" });

        var tabs = new TabControl { Dock = DockStyle.Fill };
        tabs.TabPages.Add(BuildResourcesTab());
        tabs.TabPages.Add(Page("건설", _buildEnabled, _ignorePlacement, _instantBuild, _instantRepair));
        tabs.TabPages.Add(Page("업그레이드", _upgradeEnabled));
        tabs.TabPages.Add(Page("군사", _milEnabled, _ignoreEquipment, _ignorePopulation, _zeroUpkeep, _unlimitedSquads));
        var statusPage = new TabPage("상태");
        statusPage.Controls.Add(_statusText);
        tabs.TabPages.Add(statusPage);

        var changeDir = new Button { Text = "경로 변경", AutoSize = true };
        changeDir.Click += (_, _) => ChooseGameDir();
        var apply = new Button { Text = "적용", AutoSize = true };
        apply.Click += (_, _) => Apply();
        var reload = new Button { Text = "되돌리기", AutoSize = true };
        reload.Click += (_, _) => LoadControlIntoUi();

        var top = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, Padding = new Padding(6) };
        top.Controls.AddRange(new Control[] { _gameDirLabel, changeDir });
        var bottom = new FlowLayoutPanel { Dock = DockStyle.Bottom, AutoSize = true, Padding = new Padding(6), FlowDirection = FlowDirection.RightToLeft };
        bottom.Controls.AddRange(new Control[] { apply, reload, _stateLabel });

        Controls.Add(tabs);
        Controls.Add(top);
        Controls.Add(bottom);

        InitGameDir();
        _timer.Tick += (_, _) => RefreshStatus();
        _timer.Start();
    }

    private TabPage BuildResourcesTab()
    {
        var fill = new Button { Text = "모두 이 값으로", AutoSize = true };
        fill.Click += (_, _) =>
        {
            foreach (DataGridViewRow row in _resGrid.Rows) row.Cells["Target"].Value = (int)_fillValue.Value;
        };
        var header = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true };
        header.Controls.AddRange(new Control[]
        {
            _resEnabled, new Label { Text = "주기(초)", AutoSize = true, Padding = new Padding(8, 6, 0, 0) }, _resInterval,
            _fillValue, fill,
        });
        var page = new TabPage("자원");
        page.Controls.Add(_resGrid);
        page.Controls.Add(header);
        return page;
    }

    private static TabPage Page(string title, params Control[] controls)
    {
        var flow = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, Padding = new Padding(10) };
        flow.Controls.AddRange(controls);
        var page = new TabPage(title);
        page.Controls.Add(flow);
        return page;
    }

    private void InitGameDir()
    {
        var dir = _settings.GameDir is { } saved && GameLocator.LooksLikeGameDir(saved)
            ? saved
            : GameLocator.Detect(GameLocator.SteamPathFromRegistry());
        SetGameDir(dir);
    }

    private void ChooseGameDir()
    {
        using var dlg = new FolderBrowserDialog { Description = "Manor Lords 설치 폴더 선택" };
        if (dlg.ShowDialog(this) != DialogResult.OK) return;
        if (!GameLocator.LooksLikeGameDir(dlg.SelectedPath))
        {
            MessageBox.Show(this, "ManorLords-Win64-Shipping.exe 를 찾을 수 없습니다.", "MLToybox");
            return;
        }
        _settings.GameDir = dlg.SelectedPath;
        _settings.Save(_settingsPath);
        SetGameDir(dlg.SelectedPath);
    }

    private void SetGameDir(string? dir)
    {
        _bridge = dir is null ? null : new BridgeClient(GameLocator.BridgeDir(dir));
        _gameDirLabel.Text = dir is null ? "게임 경로: (찾지 못함 — 경로 변경을 누르세요)" : $"게임 경로: {dir}";
        LoadControlIntoUi();
    }

    private void LoadControlIntoUi()
    {
        _control = _bridge?.LoadControl() ?? new ControlDocument();
        _lastSentSeq = _control.Seq;
        var f = _control.Features;
        _resEnabled.Checked = f.Resources.Enabled;
        _resInterval.Value = Math.Clamp(f.Resources.IntervalSec, 1, 60);
        _buildEnabled.Checked = f.Build.Enabled;
        _ignorePlacement.Checked = f.Build.IgnorePlacement;
        _instantBuild.Checked = f.Build.InstantBuild;
        _instantRepair.Checked = f.Build.InstantRepair;
        _upgradeEnabled.Checked = f.Upgrade.Enabled;
        _milEnabled.Checked = f.Military.Enabled;
        _ignoreEquipment.Checked = f.Military.IgnoreEquipment;
        _ignorePopulation.Checked = f.Military.IgnorePopulation;
        _zeroUpkeep.Checked = f.Military.ZeroUpkeep;
        _unlimitedSquads.Checked = f.Military.UnlimitedSquads;
        _resourceIdsKey = "";
        RebuildResourceRows(null, null);
    }

    private void RebuildResourceRows(List<string>? ids, Dictionary<string, double>? current)
    {
        _resGrid.Rows.Clear();
        foreach (var row in ResourceRows.Build(ids, current, _control.Features.Resources.Targets))
            _resGrid.Rows.Add(row.Id, row.Current?.ToString("0"), row.Target);
    }

    private void Apply()
    {
        if (_bridge is null)
        {
            MessageBox.Show(this, "게임 경로를 먼저 지정하세요.", "MLToybox");
            return;
        }
        var f = _control.Features;
        f.Resources.Enabled = _resEnabled.Checked;
        f.Resources.IntervalSec = (int)_resInterval.Value;
        f.Resources.Targets = ReadTargets();
        f.Build.Enabled = _buildEnabled.Checked;
        f.Build.IgnorePlacement = _ignorePlacement.Checked;
        f.Build.InstantBuild = _instantBuild.Checked;
        f.Build.InstantRepair = _instantRepair.Checked;
        f.Upgrade.Enabled = _upgradeEnabled.Checked;
        f.Military.Enabled = _milEnabled.Checked;
        f.Military.IgnoreEquipment = _ignoreEquipment.Checked;
        f.Military.IgnorePopulation = _ignorePopulation.Checked;
        f.Military.ZeroUpkeep = _zeroUpkeep.Checked;
        f.Military.UnlimitedSquads = _unlimitedSquads.Checked;
        try
        {
            _lastSentSeq = _bridge.SaveControl(_control);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            MessageBox.Show(this, $"control.json 저장 실패: {ex.Message}", "MLToybox");
        }
        RefreshStatus();
    }

    private Dictionary<string, int> ReadTargets()
    {
        var targets = new Dictionary<string, int>();
        foreach (DataGridViewRow row in _resGrid.Rows)
        {
            var id = row.Cells["Id"].Value as string;
            var raw = row.Cells["Target"].Value?.ToString();
            if (id is not null && int.TryParse(raw, out var v) && v >= 0) targets[id] = v;
        }
        return targets;
    }

    private void RefreshStatus()
    {
        var status = _bridge?.ReadStatus();
        var state = _bridge?.Evaluate(status, _lastSentSeq) ?? BridgeState.Disconnected;
        _stateLabel.Text = state switch
        {
            BridgeState.Disconnected => "● 연결 안 됨 (게임 미실행 또는 모드 미로드)",
            BridgeState.MainMenu => "● 메인 메뉴",
            BridgeState.Pending => "● 적용 대기 중",
            _ => "● 적용됨",
        };
        _stateLabel.ForeColor = state switch
        {
            BridgeState.Applied => Color.SeaGreen,
            BridgeState.Pending => Color.DarkOrange,
            BridgeState.MainMenu => Color.SteelBlue,
            _ => Color.Firebrick,
        };
        if (status is null)
        {
            _statusText.Text = "status.json 없음";
            return;
        }

        var key = string.Join("|", status.ResourceIds ?? new List<string>());
        if (key != _resourceIdsKey && !_resGrid.IsCurrentCellInEditMode)
        {
            _resourceIdsKey = key;
            var edited = ReadTargets();   // 재구성 전에 사용자가 입력 중이던 목표값을 보존
            if (edited.Count > 0) _control.Features.Resources.Targets = edited;
            RebuildResourceRows(status.ResourceIds, status.Resources);
        }
        else if (status.Resources is not null)
        {
            foreach (DataGridViewRow row in _resGrid.Rows)
                if (row.Cells["Id"].Value is string id && status.Resources.TryGetValue(id, out var cur))
                    row.Cells["Current"].Value = cur.ToString("0");
        }

        var lines = new List<string>
        {
            $"상태: {state}",
            $"heartbeat: {DateTimeOffset.FromUnixTimeSeconds(status.Heartbeat).ToLocalTime():HH:mm:ss}",
            $"inGame: {status.InGame}",
            $"appliedSeq: {status.AppliedSeq?.ToString() ?? "-"} / sent: {_lastSentSeq}",
            $"bridgeError: {status.BridgeError ?? "-"}",
            "",
            "[기능]",
        };
        if (status.Features is null) lines.Add("(모드에 등록된 기능 없음)");
        else
            foreach (var (name, fs) in status.Features.OrderBy(p => p.Key))
                lines.Add($"{name,-10} active={fs.Active,-5} error={fs.LastError ?? "-"}");
        _statusText.Text = string.Join(Environment.NewLine, lines);
    }
}
