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
    private readonly Label _noticeLabel = new() { AutoSize = true, ForeColor = Color.Firebrick };
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
    private readonly CheckBox _noRegionLimit = new() { Text = "지역당 개수 제한 해제 (영주 저택 모듈·세금 징수소·장작/식량 수레)", AutoSize = true };

    private readonly CheckBox _upgradeEnabled = new() { Text = "업그레이드 조건·비용·해금 무시", AutoSize = true };

    private readonly CheckBox _milEnabled = new() { Text = "군사 기능 사용", AutoSize = true };
    private readonly CheckBox _ignoreEquipment = new() { Text = "민병대 장비 요구 무시", AutoSize = true };
    private readonly CheckBox _ignorePopulation = new() { Text = "징집 조건(집 레벨·훈련) 무시 — 주민 수보다 많은 병력은 아래 '병력 생성' 사용", AutoSize = true };
    private readonly CheckBox _zeroUpkeep = new() { Text = "민병대 모집비 0", AutoSize = true };
    private readonly CheckBox _unlimitedSquads = new() { Text = "부대 수 상한 해제", AutoSize = true };
    private readonly MercenaryTab _mercTab = new() { Dock = DockStyle.Fill };

    private readonly CheckBox _popEnabled = new() { Text = "인구 기능 사용", AutoSize = true };
    private readonly NumericUpDown _popMultiplier = new() { Minimum = 1, Maximum = 10, Value = 2, Width = 50 };
    private readonly NumericUpDown _popMonthly = new() { Minimum = 0, Maximum = 31, Value = 0, Width = 50 };
    private readonly NumericUpDown _popHouseCapacity = new() { Minimum = 1, Maximum = 10, Value = 1, Width = 50 };
    private readonly NumericUpDown _popTarget = new() { Minimum = 0, Maximum = 1000, Value = 0, Width = 70 };
    private readonly NumericUpDown _popAddCount = new() { Minimum = 1, Maximum = 20, Value = 3, Width = 50 };
    private readonly Label _popInfo = new() { AutoSize = true, Text = "현재: -" };
    // 인구 범위: null = 공통(영지마다 최소 가족 수·모든 영지 합계), 아니면 영지 키
    private readonly ComboBox _popRegion = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 230 };
    private readonly CheckBox _popOverride = new() { Text = "이 영지만 따로 지정", AutoSize = true };
    private readonly Label _popTargetLabel = new() { AutoSize = true, Padding = new Padding(0, 6, 0, 0) };
    private string? _popScope;
    private string _popRegionsKey = "";
    private bool _popUpdating;
    private PopulationStatus? _lastPop;

    private readonly ComboBox _spawnUnit = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 180, DisplayMember = nameof(UnitOption.Label) };
    private readonly NumericUpDown _spawnCount = new() { Minimum = 1, Maximum = 5, Value = 1, Width = 50 };
    // 병력 생성·재구성 위치(영지). 목록은 모드가 보고하는 내 영지(playerRegions)
    private readonly ComboBox _spawnRegion = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 200 };
    private readonly ComboBox _retinueSquad = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 260 };
    private readonly Button _retinueButton = new() { Text = "꾸미기 열기", AutoSize = true, Enabled = false };
    private string _retinueKey = "";
    private string _spawnRegionsKey = "";
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
        tabs.TabPages.Add(Page("건설", _buildEnabled, _ignorePlacement, _instantBuild, _instantRepair, _noMaterials, _noRegionLimit));
        tabs.TabPages.Add(Page("업그레이드", _upgradeEnabled));
        _spawnUnit.Items.AddRange(UnitCatalog.Units.Cast<object>().ToArray());
        _spawnUnit.SelectedIndex = 1;
        var spawnButton = new Button { Text = "분대 생성", AutoSize = true };
        spawnButton.Click += (_, _) => SpawnSquads();
        var spawnRow = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 16, 0, 0) };
        spawnRow.Controls.AddRange(new Control[]
        {
            new Label { Text = "병력 생성 (주민과 무관):", AutoSize = true, Padding = new Padding(0, 6, 0, 0) },
            _spawnUnit, _spawnCount, new Label { Text = "개 분대, 위치:", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, _spawnRegion, spawnButton,
        });
        // 생성 분대는 집이 없어 게임의 해제→집결이 안 된다(해제하면 0/N 빈 카드). 모드가 같은 병종으로 다시 생성한다
        _reformButton.Click += (_, _) => ReformSquads();
        var reformRow = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 8, 0, 0) };
        reformRow.Controls.AddRange(new Control[] { _reformLabel, _reformButton });
        // 모드가 만든 수행원 분대에 게임의 꾸미기 화면을 연다(위 "위치" 영지의 영주 저택 기준)
        _retinueButton.Click += (_, _) => CustomizeRetinue();
        var retinueRow = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 8, 0, 0) };
        retinueRow.Controls.AddRange(new Control[]
        {
            new Label { Text = "수행원 꾸미기 (생성·고용한 친위대 분대):", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, _retinueSquad, _retinueButton,
        });
        tabs.TabPages.Add(Page("군사", _milEnabled, _ignoreEquipment, _ignorePopulation, _zeroUpkeep, _unlimitedSquads, spawnRow, reformRow, retinueRow));
        var mercPage = new TabPage("용병");
        mercPage.Controls.Add(_mercTab);
        tabs.TabPages.Add(mercPage);
        _mercTab.ApplyRequested += (_, _) => Apply();
        var addFamilies = new Button { Text = "가족 추가", AutoSize = true };
        addFamilies.Click += (_, _) => SendCommand(ControlCommand.AddFamilies((int)_popAddCount.Value, DateTimeOffset.UtcNow, _popScope));
        _popRegion.Items.Add(new ScopeOption(null, "공통 (모든 내 영지)"));
        _popRegion.SelectedIndex = 0;
        _popRegion.SelectedIndexChanged += (_, _) => SwitchPopScope();
        _popOverride.CheckedChanged += (_, _) => _popTarget.Enabled = _popScope is null || _popOverride.Checked;
        tabs.TabPages.Add(Page("인구",
            _popEnabled,
            Row(new Label { Text = "영지:", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, _popRegion),
            Row(new Label { Text = "월 자연 이민 가족 수(0 = 게임 그대로):", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, _popMonthly,
                new Label { Text = "이민 속도 배율(배):", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, _popMultiplier),
            Row(new Label { Text = "집의 수용 가족 수 배율(배):", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, _popHouseCapacity),
            Row(_popTargetLabel, _popTarget, _popOverride),
            Row(new Label { Text = "지금 바로 들일 가족 수:", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, _popAddCount, addFamilies),
            new Label { Text = "월 가족 수와 배율은 내 영지의 자연 이민을 바꿉니다(영지 창의 인구 증가 숫자도 따라 바뀝니다). 월 가족 수는 지지율과 무관하게 매달 그만큼, 배율은 게임의 값에 곱합니다." + Environment.NewLine + "월 가족 수를 넣으면 배율은 쓰이지 않습니다. 자연 이민은 하루 한 가족까지 오고, 빈 주거 공간이 없거나 집 없는 가족이 있으면 오지 않습니다(게임의 규칙)." + Environment.NewLine + "수용 배율은 내 집이 받을 수 있는 가족 수(1·2레벨 1, 3레벨 2, 4레벨 3, 확장이 있으면 +1)에 곱합니다. 늘어난 자리는 자연 이민으로 찹니다(가족 추가와 최소 가족 수는 집당 2가족까지)." + Environment.NewLine + "배율을 낮추거나 꺼도 이미 들어온 가족은 그 집에 그대로 삽니다(세이브를 불러와도 같습니다). 새 가족만 더 들어오지 않습니다." + Environment.NewLine + "가족 추가는 선택한 영지에, 공통이면 빈 자리가 많은 영지부터 들입니다." + Environment.NewLine + "빈 집(가족 0 → 1 → 2 순)에 들어오며 미배치 가족으로 들어옵니다. 빈 자리가 없으면 들어오지 않습니다.", AutoSize = true },
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
        top.Controls.AddRange(new Control[] { _gameDirLabel, changeDir, _noticeLabel });
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
        var loaded = _bridge?.LoadControlChecked() ?? (new ControlDocument(), false);
        _control = loaded.Document;
        // 읽지 못한 파일은 기본값으로 뜬다. 조용히 뜨면 "적용"이 설정을 기본값으로 덮는 줄 모른다
        _noticeLabel.Text = loaded.Unreadable ? "control.json 을 읽지 못해 기본값으로 열었습니다. 적용하면 control.json.bak 에 사본을 남기고 덮습니다." : "";
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
        _noRegionLimit.Checked = f.Build.NoRegionLimit;
        _upgradeEnabled.Checked = f.Upgrade.Enabled;
        _milEnabled.Checked = f.Military.Enabled;
        _ignoreEquipment.Checked = f.Military.IgnoreEquipment;
        _ignorePopulation.Checked = f.Military.IgnorePopulation;
        _zeroUpkeep.Checked = f.Military.ZeroUpkeep;
        _unlimitedSquads.Checked = f.Military.UnlimitedSquads;
        _mercTab.LoadFrom(f.Mercenaries);
        _popEnabled.Checked = f.Population.Enabled;
        _popMultiplier.Value = Math.Clamp(f.Population.Multiplier, 1, 10);
        _popMonthly.Value = Math.Clamp(f.Population.MonthlyFamilies, 0, 31);
        _popHouseCapacity.Value = Math.Clamp(f.Population.HouseCapacity, 1, 10);
        _popScope = null;
        _popRegionsKey = "";
        if (_popRegion.Items.Count > 0) { _popUpdating = true; _popRegion.SelectedIndex = 0; _popUpdating = false; }
        LoadPopTarget();
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
        f.Build.NoRegionLimit = _noRegionLimit.Checked;
        f.Upgrade.Enabled = _upgradeEnabled.Checked;
        f.Military.Enabled = _milEnabled.Checked;
        f.Military.IgnoreEquipment = _ignoreEquipment.Checked;
        f.Military.IgnorePopulation = _ignorePopulation.Checked;
        f.Military.ZeroUpkeep = _zeroUpkeep.Checked;
        f.Military.UnlimitedSquads = _unlimitedSquads.Checked;
        f.Mercenaries = _mercTab.Read();
        f.Population.Enabled = _popEnabled.Checked;
        f.Population.Multiplier = (int)_popMultiplier.Value;
        f.Population.MonthlyFamilies = (int)_popMonthly.Value;
        f.Population.HouseCapacity = (int)_popHouseCapacity.Value;
        StorePopTarget();
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
        SendCommand(ControlCommand.SpawnSquads(unit.Id, (int)_spawnCount.Value, DateTimeOffset.UtcNow, SpawnRegionKey()));
    }

    private void SendCommand(ControlCommand command)
    {
        _control.Commands = new List<ControlCommand> { command };
        Apply();
        _control.Commands = new List<ControlCommand>();
    }

    // 공통: 영지마다 최소 가족 수. 영지: '따로 지정'을 켜면 그 영지 값, 끄면 공통 값을 따른다
    private void StorePopTarget()
    {
        var pop = _control.Features.Population;
        if (_popScope is null) pop.TargetFamilies = (int)_popTarget.Value;
        else if (_popOverride.Checked) pop.RegionTargets[_popScope] = (int)_popTarget.Value;
        else pop.RegionTargets.Remove(_popScope);
    }

    private void LoadPopTarget()
    {
        var pop = _control.Features.Population;
        if (_popScope is null)
        {
            _popTargetLabel.Text = "영지마다 최소 가족 수(0 = 끔, 부족분만 채움):";
            _popOverride.Visible = false;
            _popTarget.Enabled = true;
            _popTarget.Value = Math.Clamp(pop.TargetFamilies, 0, 1000);
            return;
        }
        var own = pop.RegionTargets.TryGetValue(_popScope, out var v);
        _popTargetLabel.Text = "이 영지 최소 가족 수(0 = 끔):";
        _popOverride.Visible = true;
        _popOverride.Checked = own;
        _popTarget.Enabled = own;
        _popTarget.Value = Math.Clamp(own ? v : pop.TargetFamilies, 0, 1000);
    }

    private void SwitchPopScope()
    {
        if (_popUpdating || _popRegion.SelectedItem is not ScopeOption option || option.Key == _popScope) return;
        StorePopTarget();
        _popScope = option.Key;
        LoadPopTarget();
        UpdatePopInfo();
    }

    private void RefreshPopRegions()
    {
        var regions = _lastPop?.Regions ?? new List<PopulationRegion>();
        var key = string.Join("|", regions.Select(r => $"{r.Key}={r.Name}"));
        if (key == _popRegionsKey) return;
        _popRegionsKey = key;
        var options = new List<ScopeOption> { new(null, "공통 (모든 내 영지)") };
        options.AddRange(regions.Select(r => new ScopeOption(r.Key, $"{r.Name} ({r.Key})")));
        _popUpdating = true;   // 목록을 비우는 동안 범위가 바뀌지 않게 한다
        _popRegion.BeginUpdate();
        _popRegion.Items.Clear();
        _popRegion.Items.AddRange(options.Cast<object>().ToArray());
        _popRegion.EndUpdate();
        var index = options.FindIndex(o => o.Key == _popScope);
        if (index < 0) { StorePopTarget(); _popScope = null; LoadPopTarget(); index = 0; }
        _popRegion.SelectedIndex = index;
        _popUpdating = false;
    }

    private void UpdatePopInfo()
    {
        var p = _lastPop;
        if (p is null) { _popInfo.Text = "현재: - (인구 기능이 꺼져 있거나 게임 밖)"; return; }
        var r = _popScope is null ? null : p.Regions?.FirstOrDefault(x => x.Key == _popScope);
        var (families, people, homeless, free, unassigned) = r is null
            ? (p.Families, p.Population, p.Homeless, p.FreeSlots, p.Unassigned)
            : (r.Families, r.Population, r.Homeless, r.FreeSlots, r.Unassigned);
        var scope = r is null ? "모든 내 영지 합계" : r.Name;
        _popInfo.Text = $"현재({scope}): 가족 {families} · 인구 {people} · 집 없는 가족 {homeless} · 빈 자리 {free} · 미배치 가족 {unassigned}"
            + $"{Environment.NewLine}이번 세션(전체): 자연 이민 {p.Natural}가족"
            + (p.Multiplied > 0 ? $" → 배율로 추가 {p.Multiplied}가족" : "");   // 네이티브가 배율을 맡으면 모드가 따로 들인 가족이 없다
    }

    private string? SpawnRegionKey() => (_spawnRegion.SelectedItem as ScopeOption)?.Key;

    private void CustomizeRetinue()
    {
        if (_retinueSquad.SelectedItem is not RetinueSquad squad) return;
        SendCommand(ControlCommand.CustomizeRetinue(squad.Id, DateTimeOffset.UtcNow, SpawnRegionKey()));
    }

    // 선택은 분대 ID 로 유지한다. 화면이 열려 있는 동안(editing)은 버튼을 막는다
    private void RefreshRetinue(StatusDocument? status)
    {
        var retinue = status is { InGame: true } ? status.Retinue : null;
        var squads = retinue?.Squads ?? new List<RetinueSquad>();
        var key = string.Join("|", squads.Select(RetinueSquad.Label));
        if (key != _retinueKey)
        {
            _retinueKey = key;
            var selected = (_retinueSquad.SelectedItem as RetinueSquad)?.Id;
            _retinueSquad.BeginUpdate();
            _retinueSquad.Items.Clear();
            _retinueSquad.Items.AddRange(squads.Cast<object>().ToArray());
            _retinueSquad.EndUpdate();
            var index = squads.FindIndex(s => s.Id == selected);
            _retinueSquad.SelectedIndex = squads.Count == 0 ? -1 : Math.Max(0, index);
        }
        _retinueButton.Enabled = squads.Count > 0 && retinue?.Editing is null;
        _retinueButton.Text = retinue?.Editing is int id ? $"꾸미기 열림 (#{id})" : "꾸미기 열기";
    }

    // 선택은 영지 키로 유지한다. 목록이 없으면(게임 밖·구버전 모드) "첫 영지" 하나만 둔다
    private void RefreshSpawnRegions(StatusDocument? status)
    {
        var regions = status is { InGame: true } ? status.PlayerRegions ?? new List<RegionInfo>() : new List<RegionInfo>();
        var key = string.Join("|", regions.Select(r => $"{r.Key}={r.Name}"));
        if (key == _spawnRegionsKey && _spawnRegion.Items.Count > 0) return;
        _spawnRegionsKey = key;
        var selected = SpawnRegionKey();
        var options = regions.Count == 0
            ? new List<ScopeOption> { new(null, "내 첫 영지") }
            : regions.Select(r => new ScopeOption(r.Key, $"{r.Name} ({r.Key})")).ToList();
        _spawnRegion.BeginUpdate();
        _spawnRegion.Items.Clear();
        _spawnRegion.Items.AddRange(options.Cast<object>().ToArray());
        _spawnRegion.EndUpdate();
        _spawnRegion.SelectedIndex = Math.Max(0, options.FindIndex(o => o.Key == selected));
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
        _control.Commands = new List<ControlCommand> { ControlCommand.ReformSquads(DateTimeOffset.UtcNow, SpawnRegionKey()) };
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
        RefreshSpawnRegions(status);
        RefreshRetinue(status);
        _mercTab.ShowStatus(status);
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
        _lastPop = status.Population;
        RefreshPopRegions();
        UpdatePopInfo();
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
