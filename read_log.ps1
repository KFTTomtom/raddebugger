$logPath = "$env:APPDATA\raddbg\logs\ui_thread.raddbg_log"
$lines = Get-Content $logPath -Tail 5
foreach($l in $lines) {
    Write-Output $l.Substring(0, [Math]::Min(500, $l.Length))
    Write-Output "---END LINE---"
}
