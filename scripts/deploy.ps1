# deploy.ps1 — Copy all needed DLLs into build dir so exes work from Explorer
param(
    [string]$MsysBin = "C:\msys64\mingw64\bin",
    [string]$BuildDir = "$PSScriptRoot\..\build"
)

$ErrorActionPreference = "Stop"
Push-Location $BuildDir

Write-Host "=== Deploy DLLs to $BuildDir ==="

# 1. MinGW runtime (all exes need these)
@("libgcc_s_seh-1.dll", "libwinpthread-1.dll", "libstdc++-6.dll") | ForEach-Object {
    Copy-Item "$MsysBin\$_" . -Force; Write-Host "  Copy: $_"
}

# 2. windeployqt for GUI (handles Qt DLLs + plugins)
if (Test-Path modbus_gui.exe) {
    $env:Path = "$MsysBin;$env:Path"
    windeployqt modbus_gui.exe --no-translations 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Host "  windeployqt warning (ignored)" }
    Write-Host "  windeployqt done"
}

# 3. Recursive scan: copy any remaining DLLs that Qt depends on
#    (libdouble-conversion, libicu*, libfreetype, libharfbuzz, etc.)
$queue = [System.Collections.Generic.Queue[string]]::new()
Get-ChildItem "*.dll" | ForEach-Object { $queue.Enqueue($_.Name) }
$seen = @{}

while ($queue.Count -gt 0) {
    $dll = $queue.Dequeue()
    if ($seen[$dll]) { continue }; $seen[$dll] = $true

    $depends = & "$MsysBin\objdump.exe" -p $dll 2>&1 | Select-String 'DLL Name: (.*)' |
        ForEach-Object { $_.Matches.Groups[1].Value }
    foreach ($dep in $depends) {
        if ($dep -match '^(KERNEL32|msvcrt|USER32|GDI32|SHELL32|ADVAPI32|ole32|OLEAUT32|IMM32|SETUPAPI|WINMM|IPHLPAPI|WS2_32|COMCTL32|SHLWAPI|VERSION|UXTHEME|RPCRT4|USP10|DNSAPI|Secur32|WINHTTP|AUTHZ|MPR|NETAPI32|ntdll|USERENV|d3d11|d3d12|DWrite|dxgi|dwmapi)') { continue }
        if ($dep -match '^api-ms-win|^ext-ms-') { continue }
        if (-not (Test-Path $dep)) {
            if (Test-Path "$MsysBin\$dep") {
                Copy-Item "$MsysBin\$dep" . -Force
                Write-Host "  Copy: $dep"
                $queue.Enqueue($dep)
            }
        }
    }
}

Write-Host "=== Done ==="
Get-ChildItem "*.dll" | Select Name
Pop-Location
