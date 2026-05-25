param([int]$Pid,[string]$Csv)
'utc,elapsed_s,proc_cpu_s,proc_cpu_pct_1core,proc_cpu_pct_total,working_set_mb,private_mb,system_cpu_pct' | Out-File -FilePath $Csv -Encoding ascii
$start = Get-Date
$cpuCount = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors
while ($true) {
  $p = Get-Process -Id $Pid -ErrorAction SilentlyContinue
  if (-not $p) { break }
  $now = Get-Date
  $elapsed = [math]::Max((New-TimeSpan -Start $start -End $now).TotalSeconds, 1)
  $procCpu = $p.CPU
  $procPct1 = ($procCpu / $elapsed) * 100.0
  $procPctTotal = $procPct1 / [math]::Max($cpuCount,1)
  $sys = (Get-Counter '\Processor(_Total)\% Processor Time').CounterSamples[0].CookedValue
  '{0},{1:N0},{2:N2},{3:N2},{4:N2},{5:N2},{6:N2},{7:N2}' -f ($now.ToUniversalTime().ToString('o')),$elapsed,$procCpu,$procPct1,$procPctTotal,($p.WorkingSet64/1MB),($p.PrivateMemorySize64/1MB),$sys | Add-Content -Path $Csv
  Start-Sleep -Seconds 10
}
