# 개발용: 게임 창만 캡처하고(다른 창이나 화면 전체는 찍지 않는다), 필요하면 그 전에 키 메시지를 보낸다.
# 창이 가려져 있어도 된다(PrintWindow). 실제 마우스·키보드와 전경 창은 건드리지 않는다.
# 게임 창이 앞에 없으면 ImGui 는 마우스 위치를 받지 않는다(findings 오버레이 스파이크). 그래서 화면 조작은 할 수 없고,
# -Click 은 마우스 버튼 메시지가 창 프로시저를 지나가도 게임이 버티는지 볼 때만 쓴다(게임에도 그 자리의 클릭으로 전달된다).
#   -Out <png>      저장할 파일. 없으면 캡처하지 않는다
#   -Key <이름>     보낼 키: Insert, Home, End, F7, F8, F9
#   -Click <x>,<y>  창 안 좌표에 왼쪽 버튼을 눌렀다 떼는 메시지를 보낸다
#   -WaitMs <n>     메시지를 보낸 뒤 캡처하기 전에 기다릴 시간
param([string]$Out, [ValidateSet('Insert', 'Home', 'End', 'F7', 'F8', 'F9')][string]$Key, [int[]]$Click, [switch]$Move, [int]$WaitMs = 700)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
if (-not ('MLToyboxCapture' -as [type])) {
    Add-Type @'
using System; using System.Runtime.InteropServices;
public static class MLToyboxCapture {
  public delegate bool EnumWindowsProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
}
'@
}
$game = Get-Process -Name 'ManorLords-Win64-Shipping' -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $game) { throw 'game is not running' }
$gamePid = $game.Id
$script:found = [IntPtr]::Zero
$script:best = 0
[MLToyboxCapture]::EnumWindows({ param($h, $l)
    [uint32]$p = 0
    [void][MLToyboxCapture]::GetWindowThreadProcessId($h, [ref]$p)
    if ($p -eq $gamePid -and [MLToyboxCapture]::IsWindowVisible($h)) {
        $r = New-Object MLToyboxCapture+RECT
        [void][MLToyboxCapture]::GetWindowRect($h, [ref]$r)
        $area = ($r.R - $r.L) * ($r.B - $r.T)
        if ($area -gt $script:best) { $script:best = $area; $script:found = $h }
    }
    $true }, [IntPtr]::Zero) | Out-Null
$hwnd = $script:found
if ($hwnd -eq [IntPtr]::Zero) { throw 'game window not found' }
$rect = New-Object MLToyboxCapture+RECT
[void][MLToyboxCapture]::GetWindowRect($hwnd, [ref]$rect)
$w = $rect.R - $rect.L
$h = $rect.B - $rect.T

if ($Key) {
    $vk = @{ Insert = 0x2D; Home = 0x24; End = 0x23; F7 = 0x76; F8 = 0x77; F9 = 0x78 }[$Key]
    [void][MLToyboxCapture]::PostMessage($hwnd, 0x100, [IntPtr]$vk, [IntPtr]1)                       # WM_KEYDOWN
    Start-Sleep -Milliseconds 60
    [void][MLToyboxCapture]::PostMessage($hwnd, 0x101, [IntPtr]$vk, [IntPtr]([Int64]0xC0000001))    # WM_KEYUP
    Write-Host "posted $Key"
}
if ($Click) {
    if ($Click.Count -ne 2) { throw '-Click needs x,y' }
    $pos = [IntPtr](($Click[1] -shl 16) -bor ($Click[0] -band 0xFFFF))
    if ($Move) {
        [void][MLToyboxCapture]::PostMessage($hwnd, 0x200, [IntPtr]0, $pos)   # WM_MOUSEMOVE
        Start-Sleep -Milliseconds 250
    }
    [void][MLToyboxCapture]::PostMessage($hwnd, 0x201, [IntPtr]1, $pos)   # WM_LBUTTONDOWN (MK_LBUTTON)
    Start-Sleep -Milliseconds 60
    [void][MLToyboxCapture]::PostMessage($hwnd, 0x202, [IntPtr]0, $pos)   # WM_LBUTTONUP
    Write-Host "posted click $($Click[0]),$($Click[1])"
}
if (-not $Out) { return }
Start-Sleep -Milliseconds $WaitMs
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
$ok = [MLToyboxCapture]::PrintWindow($hwnd, $hdc, 2)   # PW_RENDERFULLCONTENT
$g.ReleaseHdc($hdc)
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
if (-not $ok) { throw 'PrintWindow failed' }
Write-Host "saved $Out (${w}x${h})"
