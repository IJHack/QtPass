param(
    [string]$InputScript = 'qtpass.iss',
    [string]$OutputScript = 'qtpass.ci.iss',
    [string]$InnoDir = 'C:\Program Files (x86)\Inno Setup 6'
)

if ($env:INNO_DIR) {
    $InnoDir = $env:INNO_DIR
}

$inputLines = Get-Content -Path $InputScript -ErrorAction Stop
$outputLines = New-Object System.Collections.Generic.List[string]
$removed = New-Object System.Collections.Generic.List[string]

foreach ($line in $inputLines) {
    if ($line -match 'MessagesFile:\s*"compiler:(?<file>[^\"]+)"') {
        $relativePath = $matches['file']

        # Keep only language includes that actually exist in this Inno Setup installation.
        # This avoids build failures when optional language packs are not installed.
        $fullPath = Join-Path -Path $InnoDir -ChildPath $relativePath
        if (Test-Path -Path $fullPath -PathType Leaf) {
            $outputLines.Add($line)
        } else {
            $removed.Add($relativePath)
        }

        continue
    }

    $outputLines.Add($line)
}

Set-Content -Path $OutputScript -Value $outputLines

if ($removed.Count -gt 0) {
    Write-Host "Removed $($removed.Count) missing Inno Setup language file(s) from installer script:"
    $removed | ForEach-Object { Write-Host " - $_" }
}
