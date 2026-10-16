
$ErrorActionPreference = "Stop"

# Git-Repository
Set-Location "C:\Dokumente\Schuldateien\SPL\Handy Garage"

# Dateien zum Commit hinzufuegen
git add -A
if ($LASTEXITCODE -ne 0) { throw "git add fehlgeschlagen" }

# Pruefen, ob Aenderungen vorhanden sind
git diff --cached --quiet

if ($LASTEXITCODE -eq 1) {
    $date = Get-Date -Format "yyyy-MM-dd"

    git commit -m "Weekly backup $date"
    if ($LASTEXITCODE -ne 0) { throw "Commit fehlgeschlagen" }

    git push
    if ($LASTEXITCODE -ne 0) { throw "Push fehlgeschlagen" }
}
elseif ($LASTEXITCODE -eq 0) {
    Write-Output "No changes found."
}
else {
    throw "Git diff fehlgeschlagen"
}
