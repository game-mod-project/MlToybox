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
    private string _regionsKey = "";
    private string? _resScope;   // null = 공통, 아니면 영지 키(regionUniqueTag)
    private bool _updatingRegions;
    private StatusDocument? _lastStatus;
    private readonly ComboBox _resRegion = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 230 };

    // 영주 전체 값: 체크된 항목만 목표 이상으로 유지한다
    private readonly CheckBox _lordEnabled = new() { Text = "영주 자원 목표값 유지 (영지와 무관한 전체 값)", AutoSize = true };
    private readonly LordRow _lordTreasury = new("국고", "treasury");
    private readonly LordRow _lordInfluence = new("영향력", "influence");
    private readonly LordRow _lordFavour = new("왕의 총애", "kingsFavour");

    private sealed class LordRow
    {
        public readonly CheckBox Use;
        public readonly NumericUpDown Value = new() { Minimum = 0, Maximum = 10_000_000, Value = 0, Width = 110 };
        public readonly Label Current = new() { AutoSize = true, Text = "현재: -", Padding = new Padding(8, 6, 0, 0) };
        public readonly Button SetNow = new() { Text = "지금 설정", AutoSize = true };
        public readonly string Key;
        public LordRow(string label, string key) { Use = new CheckBox { Text = label, AutoSize = false, Width = 100 }; Key = key; }
        public Control Row()
        {
            var row = new FlowLayoutPanel { AutoSize = true };
            row.Controls.AddRange(new Control[] { Use, new Label { Text = "목표", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, Value, SetNow, Current });
            return row;
        }
        public void Load(int? target) { Use.Checked = target is not null; Value.Value = Math.Clamp(target ?? 0, 0, 10_000_000); }
        public int? Read() => Use.Checked ? (int)Value.Value : null;
        public void Show(double? current) => Current.Text = current is null ? "현재: -" : $"현재: {current:0}";
    }

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
    private readonly CheckBox _ignorePlacement = new() { Text = "배치 제한 무시 (영지 경계 안, 네이티브 DLL)", AutoSize = true };
    private readonly CheckBox _instantBuild = new() { Text = "즉시 완공 (네이티브 DLL)", AutoSize = true };
    private readonly CheckBox _instantRepair = new() { Text = "즉시 수리", AutoSize = true };
    private readonly CheckBox _noMaterials = new() { Text = "자재 불필요 (건설 자재 없이 공사)", AutoSize = true };

    private readonly CheckBox _upgradeEnabled = new() { Text = "업그레이드 조건·비용·해금 무시", AutoSize = true };

    private readonly CheckBox _milEnabled = new() { Text = "군사 기능 사용", AutoSize = true };
    private readonly CheckBox _ignoreEquipment = new() { Text = "민병대 장비 요구 무시", AutoSize = true };
    private readonly CheckBox _ignorePopulation = new() { Text = "징집 조건(집 레벨·훈련) 무시 — 주민 수보다 많은 병력은 아래 '병력 생성' 사용", AutoSize = true };
    private readonly CheckBox _zeroUpkeep = new() { Text = "용병 비용·모집비 0 (친위대 유지비는 미지원 — 자원 탭 금고 유지로 보정)", AutoSize = true };
    private readonly CheckBox _unlimitedSquads = new() { Text = "부대 수 상한 해제", AutoSize = true };

    private readonly CheckBox _popEnabled = new() { Text = "인구 기능 사용", AutoSize = true };
    private readonly NumericUpDown _popMultiplier = new() { Minimum = 1, Maximum = 10, Value = 2, Width = 50 };
    private readonly NumericUpDown _popTarget = new() { Minimum = 0, Maximum = 1000, Value = 0, Width = 70 };
    private readonly NumericUpDown _popAddCount = new() { Minimum = 1, Maximum = 20, Value = 3, Width = 50 };
    private readonly Label _popInfo = new() { AutoSize = true, Text = "현재: -" };

    private readonly ComboBox _spawnUnit = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 180, DisplayMember = nameof(UnitOption.Label) };
    private readonly NumericUpDown _spawnCount = new() { Minimum = 1, Maximum = 5, Value = 1, Width = 50 };
    private readonly Label _reformLabel = new() { Text = "해제된 생성 분대: -", AutoSize = true, Padding = new Padding(0, 6, 0, 0) };
    private readonly Button _reformButton = new() { Text = "재구성", AutoSize = true, Enabled = false };

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
        // '지금 설정'은 목표 유지와 별개로 그 값으로 한 번 맞춘다(올리기·내리기 모두)
        foreach (var r in new[] { _lordTreasury, _lordInfluence, _lordFavour })
        {
            var row = r;
            row.SetNow.Click += (_, _) => SendCommand(ControlCommand.SetLord(row.Key, (int)row.Value.Value, DateTimeOffset.UtcNow));
        }
        tabs.TabPages.Add(Page("영주", _lordEnabled, _lordTreasury.Row(), _lordInfluence.Row(), _lordFavour.Row(),
            new Label { Text = "체크한 항목은 목표값 아래로 내려가면 목표까지 채웁니다(더 많으면 그대로). 체크 해제 = 관리 안 함." + Environment.NewLine + "'지금 설정'은 체크와 상관없이 입력한 값으로 한 번 정확히 맞춥니다(내리기도 가능).", AutoSize = true }));
        tabs.TabPages.Add(Page("건설", _buildEnabled, _ignorePlacement, _instantBuild, _instantRepair, _noMaterials));
        tabs.TabPages.Add(Page("업그레이드", _upgradeEnabled));
        _spawnUnit.Items.AddRange(UnitCatalog.Units.Cast<object>().ToArray());
        _spawnUnit.SelectedIndex = 1;
        var spawnButton = new Button { Text = "분대 생성", AutoSize = true };
        spawnButton.Click += (_, _) => SpawnSquads();
        var spawnRow = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 16, 0, 0) };
        spawnRow.Controls.AddRange(new Control[]
        {
            new Label { Text = "병력 생성 (주민과 무관):", AutoSize = true, Padding = new Padding(0, 6, 0, 0) },
            _spawnUnit, _spawnCount, new Label { Text = "개 분대", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, spawnButton,
        });
        // 생성 분대는 집이 없어 게임의 해제→집결이 안 된다(해제하면 0/N 빈 카드). 모드가 같은 병종으로 다시 생성한다
        _reformButton.Click += (_, _) => ReformSquads();
        var reformRow = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 8, 0, 0) };
        reformRow.Controls.AddRange(new Control[] { _reformLabel, _reformButton });
        tabs.TabPages.Add(Page("군사", _milEnabled, _ignoreEquipment, _ignorePopulation, _zeroUpkeep, _unlimitedSquads, spawnRow, reformRow));
        var addFamilies = new Button { Text = "가족 추가", AutoSize = true };
        addFamilies.Click += (_, _) => SendCommand(ControlCommand.AddFamilies((int)_popAddCount.Value, DateTimeOffset.UtcNow));
        tabs.TabPages.Add(Page("인구",
            _popEnabled,
            Row(new Label { Text = "자연 이민 배율(배):", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, _popMultiplier),
            Row(new Label { Text = "목표 가족 수(0 = 끔, 부족분만 채움):", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, _popTarget),
            Row(new Label { Text = "지금 바로 들일 가족 수:", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, _popAddCount, addFamilies),
            new Label { Text = "빈 집(가족 0 → 1 → 2 순)에 정상 규모 가족이 들어옵니다. 빈 자리가 없으면 들어오지 않습니다.", AutoSize = true },
            _popInfo));
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
        AttachNumberBoxes(this);

        InitGameDir();
        _timer.Tick += (_, _) => RefreshStatus();
        _timer.Start();
    }

    // 한글 입력 상태에서 숫자를 치면 전각 숫자(９００００)가 들어가 NumericUpDown 이 값을 버린다.
    // IME 를 끄고, 그래도 들어온 전각 숫자·공백은 즉시 일반 숫자로 바꾼다.
    private static void AttachNumberBoxes(Control root)
    {
        foreach (Control c in root.Controls)
        {
            if (c is NumericUpDown n)
            {
                n.ImeMode = ImeMode.Disable;
                n.TextChanged += (_, _) =>
                {
                    var raw = n.Text;
                    if (!raw.Any(ch => ch is >= '０' and <= '９' || char.IsWhiteSpace(ch))) return;
                    if (NumberInput.Parse(raw) is not int v) return;
                    n.Text = v.ToString();
                    if (n.Controls.OfType<TextBox>().FirstOrDefault() is { } edit) edit.SelectionStart = edit.TextLength;
                };
            }
            AttachNumberBoxes(c);
        }
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
            new Label { Text = "영지", AutoSize = true, Padding = new Padding(8, 6, 0, 0) }, _resRegion,
        });
        _resRegion.Items.AddRange(ResourceScope.Options(null).Cast<object>().ToArray());
        _resRegion.SelectedIndex = 0;
        _resRegion.SelectedIndexChanged += (_, _) => SwitchScope();
        var page = new TabPage("자원");
        page.Controls.Add(_resGrid);
        page.Controls.Add(header);
        return page;
    }

    private static FlowLayoutPanel Row(params Control[] controls)
    {
        var row = new FlowLayoutPanel { AutoSize = true };
        row.Controls.AddRange(controls);
        return row;
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
        _lordEnabled.Checked = f.Lord.Enabled;
        _lordTreasury.Load(f.Lord.Treasury);
        _lordInfluence.Load(f.Lord.Influence);
        _lordFavour.Load(f.Lord.KingsFavour);
        _buildEnabled.Checked = f.Build.Enabled;
        _ignorePlacement.Checked = f.Build.IgnorePlacement;
        _instantBuild.Checked = f.Build.InstantBuild;
        _instantRepair.Checked = f.Build.InstantRepair;
        _noMaterials.Checked = f.Build.NoMaterials;
        _upgradeEnabled.Checked = f.Upgrade.Enabled;
        _milEnabled.Checked = f.Military.Enabled;
        _ignoreEquipment.Checked = f.Military.IgnoreEquipment;
        _ignorePopulation.Checked = f.Military.IgnorePopulation;
        _zeroUpkeep.Checked = f.Military.ZeroUpkeep;
        _unlimitedSquads.Checked = f.Military.UnlimitedSquads;
        _popEnabled.Checked = f.Population.Enabled;
        _popMultiplier.Value = Math.Clamp(f.Population.Multiplier, 1, 10);
        _popTarget.Value = Math.Clamp(f.Population.TargetFamilies, 0, 1000);
        _resourceIdsKey = "";
        _regionsKey = "";
        _resScope = null;
        if (_resRegion.Items.Count > 0) _resRegion.SelectedIndex = 0;
        RebuildResourceRows(null, null);
    }

    private void RebuildResourceRows(List<string>? ids, Dictionary<string, double>? current)
    {
        _resGrid.Rows.Clear();
        _resGrid.Columns["Target"]!.HeaderText = _resScope is null ? "목표 (빈칸=관리 안 함)" : "영지 목표 (빈칸=공통 목표 따름)";
        var targets = ResourceScope.Targets(_control.Features.Resources, _resScope);
        var exclude = _resScope is null ? null : ResourceScope.LordWide;
        foreach (var row in ResourceRows.Build(ids, current, targets, exclude))
            _resGrid.Rows.Add(row.Id, row.Current?.ToString("0"), row.Target);
    }

    // 범위를 바꾸기 전에 지금 표의 목표를 이전 범위에 저장한다
    private void SwitchScope()
    {
        if (_updatingRegions || _resRegion.SelectedItem is not ScopeOption option) return;
        var next = option.Key;
        if (next == _resScope) return;
        if (_resGrid.IsCurrentCellInEditMode) _resGrid.EndEdit();
        ResourceScope.Store(_control.Features.Resources, _resScope, ReadTargets());
        _resScope = next;
        RebuildResourceRows(ResourceScope.Ids(_lastStatus, _resScope), ResourceScope.Current(_lastStatus, _resScope));
    }

    private void RefreshRegionOptions(StatusDocument status)
    {
        var key = string.Join("|", (status.Regions ?? new List<RegionResources>()).Select(r => $"{r.Key}={r.Name}"));
        if (key == _regionsKey) return;
        _regionsKey = key;
        var options = ResourceScope.Options(status);
        _updatingRegions = true;   // 목록을 비우는 동안 SelectedIndexChanged 가 범위를 바꾸지 않게 한다
        _resRegion.BeginUpdate();
        _resRegion.Items.Clear();
        _resRegion.Items.AddRange(options.Cast<object>().ToArray());
        var index = options.FindIndex(o => o.Key == _resScope);
        _resRegion.EndUpdate();
        if (index < 0) { ResourceScope.Store(_control.Features.Resources, _resScope, ReadTargets()); _resScope = null; index = 0; _resourceIdsKey = ""; }
        _resRegion.SelectedIndex = index;
        _updatingRegions = false;
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
        ResourceScope.Store(f.Resources, _resScope, ReadTargets());
        f.Lord.Enabled = _lordEnabled.Checked;
        f.Lord.Treasury = _lordTreasury.Read();
        f.Lord.Influence = _lordInfluence.Read();
        f.Lord.KingsFavour = _lordFavour.Read();
        f.Build.Enabled = _buildEnabled.Checked;
        f.Build.IgnorePlacement = _ignorePlacement.Checked;
        f.Build.InstantBuild = _instantBuild.Checked;
        f.Build.InstantRepair = _instantRepair.Checked;
        f.Build.NoMaterials = _noMaterials.Checked;
        f.Upgrade.Enabled = _upgradeEnabled.Checked;
        f.Military.Enabled = _milEnabled.Checked;
        f.Military.IgnoreEquipment = _ignoreEquipment.Checked;
        f.Military.IgnorePopulation = _ignorePopulation.Checked;
        f.Military.ZeroUpkeep = _zeroUpkeep.Checked;
        f.Military.UnlimitedSquads = _unlimitedSquads.Checked;
        f.Population.Enabled = _popEnabled.Checked;
        f.Population.Multiplier = (int)_popMultiplier.Value;
        f.Population.TargetFamilies = (int)_popTarget.Value;
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

    // 현재 설정과 함께 일회성 생성 명령을 보낸다. 명령은 저장 직후 비워 다음 적용 때 다시 보내지 않는다.
    private void SpawnSquads()
    {
        if (_spawnUnit.SelectedItem is not UnitOption unit) return;
        SendCommand(ControlCommand.SpawnSquads(unit.Id, (int)_spawnCount.Value, DateTimeOffset.UtcNow));
    }

    private void SendCommand(ControlCommand command)
    {
        _control.Commands = new List<ControlCommand> { command };
        Apply();
        _control.Commands = new List<ControlCommand>();
    }

    private void UpdateReform(StatusDocument? status)
    {
        var spawn = status is { InGame: true } ? status.Spawn : null;
        if (spawn is null)
        {
            _reformLabel.Text = "해제된 생성 분대: -";
            _reformButton.Enabled = false;
            return;
        }
        var labels = UnitCatalog.Units.ToDictionary(u => u.Id, u => u.Label);
        var detail = spawn.ByUnit is null || spawn.ByUnit.Count == 0 ? ""
            : " (" + string.Join(", ", spawn.ByUnit.Select(p => $"{labels.GetValueOrDefault(p.Key, p.Key)} {p.Value}")) + ")";
        var pending = spawn.Pending > 0 ? $" — 빈 카드 정리 중 {spawn.Pending}" : "";
        _reformLabel.Text = $"해제된 생성 분대: {spawn.Disbanded}개{detail}{pending}";
        _reformButton.Enabled = spawn.Disbanded > 0 && spawn.Pending == 0;
    }

    private void ReformSquads()
    {
        _control.Commands = new List<ControlCommand> { ControlCommand.ReformSquads(DateTimeOffset.UtcNow) };
        Apply();
        _control.Commands = new List<ControlCommand>();
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
        UpdateReform(status);
        if (status is null)
        {
            _statusText.Text = "status.json 없음";
            return;
        }

        _lastStatus = status;
        _lordTreasury.Show(status.Lord?.Treasury);
        _lordInfluence.Show(status.Lord?.Influence);
        _lordFavour.Show(status.Lord?.KingsFavour);
        if (!_resGrid.IsCurrentCellInEditMode) RefreshRegionOptions(status);
        var ids = ResourceScope.Ids(status, _resScope);
        var current = ResourceScope.Current(status, _resScope);
        var key = (_resScope ?? "") + ":" + string.Join("|", ids ?? new List<string>());
        if (key != _resourceIdsKey && !_resGrid.IsCurrentCellInEditMode)
        {
            _resourceIdsKey = key;
            var edited = ReadTargets();   // 재구성 전에 사용자가 입력 중이던 목표값을 보존
            if (edited.Count > 0) ResourceScope.Store(_control.Features.Resources, _resScope, edited);
            RebuildResourceRows(ids, current);
        }
        else if (current is not null)
        {
            foreach (DataGridViewRow row in _resGrid.Rows)
                if (row.Cells["Id"].Value is string id && current.TryGetValue(id, out var cur))
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
        var p = status.Population;
        _popInfo.Text = p is null ? "현재: - (인구 기능이 꺼져 있거나 게임 밖)" : $"현재: 가족 {p.Families} · 인구 {p.Population} · 집 없는 가족 {p.Homeless} · 빈 자리 {p.FreeSlots}{Environment.NewLine}이번 세션: 자연 이민 {p.Natural}가족 → 배율로 추가 {p.Multiplied}가족";
        if (status.Commands is not null)
        {
            lines.Add("");
            lines.Add("[명령 결과]");
            foreach (var (id, r) in status.Commands)
                lines.Add($"{id[..Math.Min(8, id.Length)]} {(r.Ok ? "성공" : "실패")} {(r.Squads is null ? "" : "분대 " + string.Join(",", r.Squads))} {(r.Reformed is null ? "" : $"재구성 {r.Reformed}개")}{(r.Added is null ? "" : $"가족 {r.Added}/{r.Requested}")} {r.Error ?? ""}");
        }
        lines.Add("");
        lines.Add("[네이티브]");
        var n = status.Native;
        if (n is null) lines.Add("(정보 없음)");
        else
        {
            lines.Add(!n.Loaded ? $"미로드: {n.Error ?? "-"}" : n.Stale ? "응답 없음 (heartbeat 끊김)" : "동작 중");
            if (n.Features is not null)
                foreach (var (name, f) in n.Features.OrderBy(p => p.Key))
                    lines.Add($"{name,-16} installed={f.Installed,-5} active={f.Active,-5} error={f.LastError ?? "-"}");
        }
        _statusText.Text = string.Join(Environment.NewLine, lines);
    }
}
