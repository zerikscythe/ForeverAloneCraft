param(
    [string]$ProcessName = "worldserver",
    [int]$SampleSeconds = 15,
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

if ($SampleSeconds -lt 1) {
    throw "SampleSeconds must be >= 1."
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputPath = Join-Path $PSScriptRoot ("worldserver-metrics-{0}.csv" -f $stamp)
}

$outputDir = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($outputDir) -and -not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

Write-Host "Waiting for process '$ProcessName'..."
$proc = $null
while (-not $proc) {
    $proc = Get-Process $ProcessName -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $proc) {
        Start-Sleep -Seconds 2
    }
}

$processId = $proc.Id
$logicalCpuCount = [Environment]::ProcessorCount
$lastWall = Get-Date
$lastCpuSeconds = [double]$proc.CPU

"timestamp,pid,uptime_sec,proc_cpu_pct,proc_cpu_total_sec,working_set_mb,private_mem_mb,paged_mem_mb,threads,handles,system_cpu_pct,avail_mem_mb" |
    Set-Content -Path $OutputPath -Encoding ASCII

Write-Host ("Monitoring PID {0}. Writing to {1}" -f $processId, $OutputPath)

while ($true) {
    Start-Sleep -Seconds $SampleSeconds

    $proc = Get-Process -Id $processId -ErrorAction SilentlyContinue
    if (-not $proc) {
        Write-Host "Process exited. Monitoring complete."
        break
    }

    $now = Get-Date
    $wallSeconds = [math]::Max(0.001, ($now - $lastWall).TotalSeconds)
    $cpuTotalSeconds = [double]$proc.CPU
    $cpuDeltaSeconds = [math]::Max(0.0, $cpuTotalSeconds - $lastCpuSeconds)
    $procCpuPct = [math]::Round(($cpuDeltaSeconds / ($wallSeconds * $logicalCpuCount)) * 100.0, 2)
    $lastWall = $now
    $lastCpuSeconds = $cpuTotalSeconds

    $systemCpuCounter = Get-Counter '\Processor(_Total)\% Processor Time'
    $systemCpuPct = [math]::Round($systemCpuCounter.CounterSamples[0].CookedValue, 2)

    $os = Get-CimInstance Win32_OperatingSystem
    $availMemMb = [math]::Round(([double]$os.FreePhysicalMemory / 1024.0), 2)

    $uptimeSec = [math]::Round(($now - $proc.StartTime).TotalSeconds, 0)
    $workingSetMb = [math]::Round(($proc.WorkingSet64 / 1MB), 2)
    $privateMemMb = [math]::Round(($proc.PrivateMemorySize64 / 1MB), 2)
    $pagedMemMb = [math]::Round(($proc.PagedMemorySize64 / 1MB), 2)

    $line = "{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11}" -f `
        $now.ToString("o"), `
        $processId, `
        $uptimeSec, `
        $procCpuPct, `
        ([math]::Round($cpuTotalSeconds, 2)), `
        $workingSetMb, `
        $privateMemMb, `
        $pagedMemMb, `
        $proc.Threads.Count, `
        $proc.HandleCount, `
        $systemCpuPct, `
        $availMemMb

    Add-Content -Path $OutputPath -Value $line -Encoding ASCII
    Write-Host ("[{0}] CPU {1}% WS {2} MB Private {3} MB SystemCPU {4}% AvailMem {5} MB" -f `
        $now.ToString("HH:mm:ss"), $procCpuPct, $workingSetMb, $privateMemMb, $systemCpuPct, $availMemMb)
}
