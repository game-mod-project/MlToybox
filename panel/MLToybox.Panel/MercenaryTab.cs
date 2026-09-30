using MLToybox.Panel.Core;

namespace MLToybox.Panel;

// [용병] 탭: 고용 창 관리 옵션과 커스텀 용병단 등록 (spec 2026-09-30-mercenary-companies-design §5.1)
// 옵션 체크박스는 하단 "적용"으로 반영하고, 등록·삭제·사용 체크는 ApplyRequested 로 바로 적용한다.
public sealed class MercenaryTab : UserControl
{
    public event EventHandler? ApplyRequested;

    // 편집 영역의 구성 목록 한 줄(병종 하나와 분대 수)
    private sealed record UnitGroup(string Id, int Count)
    {
        public override string ToString() => MercCompanyRules.Summary(Enumerable.Repeat(Id, Count));
    }

    private const string FirstRegionLabel = MercCompanyRules.FirstRegionLabel;

    private readonly CheckBox _enabled = new() { Text = "용병 기능 사용 (고용 창 자동 보충)", AutoSize = true };
    private readonly CheckBox _refund = new() { Text = "내 용병단 고용비 환급, 유지비 0", AutoSize = true };
    private readonly CheckBox _lock = new() { Text = "커스텀 용병단 AI 잠금 (고용 창을 열 때만 설정한 고용비)", AutoSize = true };
    private readonly DataGridView _grid = new()
    {
        Size = new Size(640, 120), AllowUserToAddRows = false, AllowUserToDeleteRows = false, AllowUserToResizeRows = false,
        RowHeadersVisible = false, MultiSelect = false, SelectionMode = DataGridViewSelectionMode.FullRowSelect,
        AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill,
    };
    private readonly TextBox _name = new() { Width = 260, MaxLength = MercCompanyRules.NameMax };
    private readonly ComboBox _unit = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 180, DisplayMember = nameof(UnitOption.Label) };
    private readonly NumericUpDown _count = new() { Minimum = 1, Maximum = MercCompanyRules.MaxSquads, Value = 1, Width = 50 };
    private readonly ListBox _composition = new() { Width = 260, Height = 70 };
    private readonly Label _total = new() { AutoSize = true, Padding = new Padding(0, 6, 0, 0) };
    private readonly NumericUpDown _cost = new() { Minimum = 0, Maximum = 10_000_000, Value = 1000, Width = 110 };
    private readonly ComboBox _region = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 220 };
    private readonly Label _error = new() { AutoSize = true, ForeColor = Color.Firebrick };
    private readonly Label _status = new() { AutoSize = true };

    private List<MercCompany> _companies = new();
    private MercCompany? _editing;                 // null = 새 용병단
    private List<string> _editUnits = new();
    private string _regionsKey = "";
    private List<RegionInfo> _regions = new();     // 마지막으로 받은 내 영지 목록(게임 밖이면 비어 있다)
    private bool _loading;

    public MercenaryTab()
    {
        _grid.Columns.Add(new DataGridViewCheckBoxColumn { Name = "Use", HeaderText = "사용", FillWeight = 12 });
        _grid.Columns.Add(new DataGridViewTextBoxColumn { Name = "Name", HeaderText = "이름", ReadOnly = true, FillWeight = 30 });
        _grid.Columns.Add(new DataGridViewTextBoxColumn { Name = "Units", HeaderText = "구성", ReadOnly = true, FillWeight = 44 });
        _grid.Columns.Add(new DataGridViewTextBoxColumn { Name = "Cost", HeaderText = "고용비", ReadOnly = true, FillWeight = 16 });
        _grid.Columns.Add(new DataGridViewTextBoxColumn { Name = "Region", HeaderText = "도착 영지", ReadOnly = true, FillWeight = 24 });
        // 체크박스는 셀을 떠나기 전에는 값이 확정되지 않는다. 누르는 즉시 확정시킨다
        _grid.CurrentCellDirtyStateChanged += (_, _) =>
        {
            if (_grid.IsCurrentCellDirty) _grid.CommitEdit(DataGridViewDataErrorContexts.Commit);
        };
        _grid.CellValueChanged += (_, e) => OnUseChanged(e.RowIndex, e.ColumnIndex);
        _grid.SelectionChanged += (_, _) => OnRowSelected();

        _unit.Items.AddRange(MercCompanyRules.Units.Cast<object>().ToArray());
        var infantry = MercCompanyRules.Units.ToList().FindIndex(u => u.Id == "mercenary_infantry");
        if (_unit.Items.Count > 0) _unit.SelectedIndex = Math.Max(0, infantry);
        _region.Items.Add(new ScopeOption(null, FirstRegionLabel));
        _region.SelectedIndex = 0;

        var newButton = new Button { Text = "새 용병단", AutoSize = true };
        newButton.Click += (_, _) => StartNew();
        var deleteButton = new Button { Text = "삭제", AutoSize = true };
        deleteButton.Click += (_, _) => DeleteSelected();
        var addUnit = new Button { Text = "추가", AutoSize = true };
        addUnit.Click += (_, _) => AddUnits();
        var removeUnit = new Button { Text = "선택 병종 빼기", AutoSize = true };
        removeUnit.Click += (_, _) => RemoveUnits();
        var register = new Button { Text = "등록", AutoSize = true };
        register.Click += (_, _) => Register();

        var root = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoScroll = true, Padding = new Padding(10),
        };
        root.Controls.AddRange(new Control[]
        {
            _enabled, _refund, _lock,
            Caption($"등록한 용병단 (사용 최대 {MercCompanyRules.MaxEnabled}개)"),
            _grid,
            Row(newButton, deleteButton),
            Caption("용병단 편집"),
            Row(Field("이름"), _name),
            Row(Field("분대 추가"), _unit, _count, Field("개 분대"), addUnit),
            Row(Field("구성"), _composition, removeUnit, _total),
            Row(Field("고용비"), _cost),
            Row(Field("도착 영지"), _region),
            Row(register, _error),
            _status,
        });
        Controls.Add(root);
        ClearEditor();
    }

    public void LoadFrom(MercenariesControl control)
    {
        _enabled.Checked = control.Enabled;
        _refund.Checked = control.Refund;
        _lock.Checked = control.LockFromAi;
        _companies = control.Companies.Select(Clone).ToList();
        RebuildRegionOptions();
        ClearEditor();
        RebuildGrid(null);
    }

    public MercenariesControl Read() => new()
    {
        Enabled = _enabled.Checked,
        Refund = _refund.Checked,
        LockFromAi = _lock.Checked,
        Companies = _companies.Select(Clone).ToList(),
    };

    public void ShowStatus(StatusDocument? status)
    {
        var regions = status is { InGame: true } ? status.PlayerRegions ?? new List<RegionInfo>() : new List<RegionInfo>();
        var key = string.Join("|", regions.Select(r => $"{r.Key}={r.Name}"));
        if (key != _regionsKey)
        {
            _regionsKey = key;
            _regions = regions;
            RebuildRegionOptions();
            RebuildGrid(_editing);   // 영지 이름 표시를 새 목록으로 갱신
        }
        _status.Text = StatusText(status);
    }

    // 도착 영지 선택지를 다시 만든다. 등록된 용병단의 도착 영지와 지금 고른 값은 영지 목록에 없어도 남긴다
    // (게임이 꺼져 있을 때 용병단을 고쳐 등록해도 저장된 도착 영지가 "내 첫 영지"로 바뀌지 않게)
    private void RebuildRegionOptions()
    {
        var selected = (_region.SelectedItem as ScopeOption)?.Key;
        var options = MercCompanyRules.RegionOptions(_regions, _companies.Select(c => c.Region).Append(selected)).ToList();
        _region.BeginUpdate();
        _region.Items.Clear();
        _region.Items.AddRange(options.Cast<object>().ToArray());
        _region.EndUpdate();
        _region.SelectedIndex = Math.Max(0, options.FindIndex(o => o.Key == selected));
    }

    private static string StatusText(StatusDocument? status)
    {
        var m = status is { InGame: true } ? status.Mercenaries : null;
        if (m is null) return "고용 창: - (용병 기능이 꺼져 있거나 게임 밖)";
        var slots = m.Slots is null || m.Slots.Count == 0
            ? "(비어 있음)"
            : string.Join(", ", m.Slots.Select(s => $"{s.Name}{(s.Custom ? "(커스텀)" : "")} {s.Cost:N0}"));
        var lines = new List<string>
        {
            $"고용 창: {slots}",
            $"고용 중: 내 용병단 {m.HiredMine}개, AI {m.HiredAi}개 · 맵을 불러온 뒤 환급 {m.Refunded:N0}",
        };
        foreach (var s in m.Skipped ?? new List<MercSkipped>()) lines.Add($"띄우지 못함: {s.Name} — {s.Reason}");
        if (!string.IsNullOrEmpty(m.Note)) lines.Add($"참고: {m.Note}");
        return string.Join(Environment.NewLine, lines);
    }

    private static MercCompany Clone(MercCompany c) => new()
    {
        Name = c.Name, Units = new List<string>(c.Units), Cost = c.Cost, Region = c.Region, Enabled = c.Enabled,
    };

    private static Label Caption(string text) => new()
    {
        Text = text, AutoSize = true, Font = new Font(SystemFonts.DefaultFont, FontStyle.Bold), Margin = new Padding(3, 12, 3, 3),
    };

    private static Label Field(string text) => new() { Text = text, AutoSize = true, Padding = new Padding(0, 6, 0, 0) };

    private static FlowLayoutPanel Row(params Control[] controls)
    {
        var row = new FlowLayoutPanel { AutoSize = true, WrapContents = false };
        row.Controls.AddRange(controls);
        return row;
    }

    private string RegionLabel(string? key)
    {
        if (key is null) return FirstRegionLabel;
        var option = _region.Items.OfType<ScopeOption>().FirstOrDefault(o => o.Key == key);
        return option?.Label ?? key;
    }

    private void SelectRegion(string? key)
    {
        var index = _region.Items.OfType<ScopeOption>().ToList().FindIndex(o => o.Key == key);
        _region.SelectedIndex = Math.Max(0, index);
    }

    private void RebuildGrid(MercCompany? select)
    {
        _loading = true;
        _grid.Rows.Clear();
        foreach (var c in _companies)
        {
            var i = _grid.Rows.Add(c.Enabled, c.Name, MercCompanyRules.Summary(c.Units), c.Cost.ToString("N0"), RegionLabel(c.Region));
            _grid.Rows[i].Tag = c;
        }
        _grid.ClearSelection();
        foreach (DataGridViewRow row in _grid.Rows)
            if (ReferenceEquals(row.Tag, select)) row.Selected = true;
        _loading = false;
    }

    private void OnRowSelected()
    {
        if (_loading || _grid.SelectedRows.Count == 0) return;
        if (_grid.SelectedRows[0].Tag is MercCompany c) LoadEditor(c);
    }

    private void OnUseChanged(int rowIndex, int columnIndex)
    {
        if (_loading || rowIndex < 0 || columnIndex != 0) return;
        var row = _grid.Rows[rowIndex];
        if (row.Tag is not MercCompany c) return;
        var want = row.Cells[0].Value is true;
        if (want && !MercCompanyRules.CanEnable(_companies, c))
        {
            _loading = true;
            row.Cells[0].Value = false;
            _grid.RefreshEdit();
            _loading = false;
            _error.Text = $"사용은 최대 {MercCompanyRules.MaxEnabled}개입니다.";
            return;
        }
        c.Enabled = want;
        _error.Text = "";
        ApplyRequested?.Invoke(this, EventArgs.Empty);
    }

    private void LoadEditor(MercCompany c)
    {
        _editing = c;
        _name.Text = c.Name;
        _editUnits = new List<string>(c.Units);
        _cost.Value = Math.Clamp(c.Cost, 0, (int)_cost.Maximum);
        SelectRegion(c.Region);
        _error.Text = "";
        RefreshComposition();
    }

    private void ClearEditor()
    {
        _editing = null;
        _name.Text = "";
        _editUnits = new List<string>();
        _cost.Value = 1000;
        SelectRegion(null);
        _error.Text = "";
        RefreshComposition();
    }

    private void RefreshComposition()
    {
        _composition.Items.Clear();
        foreach (var id in _editUnits.Distinct())
            _composition.Items.Add(new UnitGroup(id, _editUnits.Count(u => u == id)));
        _total.Text = $"합계 {_editUnits.Count} / {MercCompanyRules.MaxSquads}";
    }

    private void StartNew()
    {
        ClearEditor();
        _loading = true;
        _grid.ClearSelection();
        _loading = false;
        _name.Focus();
    }

    private void AddUnits()
    {
        if (_unit.SelectedItem is not UnitOption unit) return;
        var count = (int)_count.Value;
        if (_editUnits.Count + count > MercCompanyRules.MaxSquads)
        {
            _error.Text = $"분대는 {MercCompanyRules.MaxSquads}개까지입니다.";
            return;
        }
        _editUnits.AddRange(Enumerable.Repeat(unit.Id, count));
        _error.Text = "";
        RefreshComposition();
    }

    private void RemoveUnits()
    {
        if (_composition.SelectedItem is not UnitGroup group) return;
        _editUnits.RemoveAll(u => u == group.Id);
        RefreshComposition();
    }

    private void Register()
    {
        var isNew = _editing is null;
        var candidate = new MercCompany
        {
            Name = _name.Text.Trim(),
            Units = new List<string>(_editUnits),
            Cost = (int)_cost.Value,
            Region = (_region.SelectedItem as ScopeOption)?.Key,
            Enabled = _editing?.Enabled ?? MercCompanyRules.CanEnable(_companies, null),
        };
        var error = MercCompanyRules.Validate(candidate, _companies.Where(c => !ReferenceEquals(c, _editing)));
        if (error is not null)
        {
            _error.Text = error;
            return;
        }
        if (_editing is null)
        {
            _companies.Add(candidate);
            _editing = candidate;
        }
        else
        {
            _editing.Name = candidate.Name;
            _editing.Units = candidate.Units;
            _editing.Cost = candidate.Cost;
            _editing.Region = candidate.Region;
        }
        _error.Text = isNew && !candidate.Enabled ? $"사용 중인 용병단이 {MercCompanyRules.MaxEnabled}개라 '사용'을 끈 채로 등록했습니다." : "";
        RebuildGrid(_editing);
        ApplyRequested?.Invoke(this, EventArgs.Empty);
    }

    private void DeleteSelected()
    {
        if (_editing is null) return;
        _companies.Remove(_editing);
        ClearEditor();
        RebuildGrid(null);
        ApplyRequested?.Invoke(this, EventArgs.Empty);
    }
}
