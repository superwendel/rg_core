param(
    [Parameter(Mandatory=$true)][string]$BaselineRoot,
    [string]$CandidateRoot = '',
    [string]$BaselineBuildPath = '.bench-build\before',
    [string]$CandidateBuildPath = '.bench-build\after',
    [ValidateRange(3,99)][int]$Runs = 7,
    [ValidateSet('math','sprintf','storage','input','prof')][string[]]$Suites = @('math','sprintf','storage','input','prof'),
    [string]$Filter = '',
    [string[]]$Cases = @(),
    [switch]$SkipBuild,
    [string]$OutputPath = '.bench-build\comparison'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (![IO.Path]::IsPathRooted($BaselineRoot)) { $BaselineRoot = Join-Path $repo $BaselineRoot }
$BaselineRoot = (Resolve-Path -LiteralPath $BaselineRoot).Path
if ($CandidateRoot) {
    if (![IO.Path]::IsPathRooted($CandidateRoot)) { $CandidateRoot = Join-Path $repo $CandidateRoot }
    $CandidateRoot = (Resolve-Path -LiteralPath $CandidateRoot).Path
} else { $CandidateRoot = $repo }
if (![IO.Path]::IsPathRooted($OutputPath)) { $OutputPath = Join-Path $repo $OutputPath }
New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
$builds = @{
    baseline= $(if ([IO.Path]::IsPathRooted($BaselineBuildPath)) { $BaselineBuildPath } else { Join-Path $repo $BaselineBuildPath })
}
$culture = [Globalization.CultureInfo]::InvariantCulture
$rawRuns = @{}
$expectedChecksums = @{}

function Median([double[]]$Values) {
    $sorted = @($Values | Sort-Object)
    $mid = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2) { return $sorted[$mid] }
    return ($sorted[$mid-1] + $sorted[$mid]) / 2
}

function Run-Process([string]$Executable, [string]$Label, [string]$Case) {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = Split-Path -Parent $Executable
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.Arguments = '"=' + $Case.Replace('"', '') + '"'
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    [void]$process.Start()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $stdout = $process.StandardOutput.ReadToEnd()
    $process.WaitForExit()
    $stderr = $stderrTask.Result
    $exitCode = $process.ExitCode
    $process.Dispose()
    [IO.File]::WriteAllText((Join-Path $OutputPath "$Label.txt"), $stdout + $stderr)
    if ($exitCode -ne 0) { throw "$Label failed ($exitCode): $stderr" }
    $samples = @{}
    $checksums = @{}
    foreach ($line in ($stdout -split '\r?\n')) {
        if ($line -match '^BENCH (\S+) (\d+) ([0-9.eE+-]+) (\d+)$') {
            $name = $Matches[1]
            $value = [double]::Parse($Matches[3], $culture)
            if ($value -le 0 -or [double]::IsNaN($value) -or [double]::IsInfinity($value)) {
                throw "Invalid duration in $Label`: $line"
            }
            if (!$samples.ContainsKey($name)) { $samples[$name] = @(); $checksums[$name] = @() }
            $samples[$name] += $value
            $checksums[$name] += $Matches[4]
        }
    }
    $medians = @{}
    foreach ($name in $samples.Keys) {
        if ($samples[$name].Count -ne 7) { throw "$Label/$name did not emit seven samples." }
        if ($name -ne $Case) { throw "Unexpected case in $Label`: $name" }
        $medians[$name] = Median $samples[$name]
        if ($name.StartsWith('storage.') -or $name.StartsWith('sprintf.')) {
            if (!$script:expectedChecksums.ContainsKey($name)) { $script:expectedChecksums[$name] = $checksums[$name][0] }
            foreach ($checksum in $checksums[$name]) {
                if ($checksum -ne $script:expectedChecksums[$name]) { throw "Output checksum mismatch: $Label/$name" }
            }
        }
    }
    if ($medians.Count -ne 1) { throw "$Label did not emit exactly one case." }
    $script:rawRuns[$Label] = @{ samples=$samples; checksums=$checksums }
    return $medians
}

function Get-Cases([string]$Executable) {
    $listing = @(& $Executable '--list')
    if ($LASTEXITCODE -ne 0) { throw "Could not enumerate $Executable" }
    foreach ($line in $listing) {
        if ($line -match '^CASE (\S+)$') {
            $name = $Matches[1]
            # This collapsible control is not an operation or a regression gate.
            if ($name -eq 'prof.sum_control') { continue }
            if ($Cases.Count -and $name -notin $Cases) { continue }
            if (!$Filter -or $name.Contains($Filter)) { $name }
        }
    }
}

$saved = @{}
foreach ($name in @('RG_BENCH_SOURCE_ROOT','RG_BENCH_OUTPUT_DIR','RG_BENCH_BUILD_ONLY')) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
Push-Location $repo
try {
    if (!$SkipBuild) {
        $env:RG_BENCH_BUILD_ONLY = '1'
        foreach ($variant in @('baseline','candidate')) {
            $env:RG_BENCH_SOURCE_ROOT = if ($variant -eq 'baseline') { $BaselineRoot } else { $CandidateRoot }
            $env:RG_BENCH_OUTPUT_DIR = $builds[$variant]
            foreach ($suite in $Suites) {
                & (Join-Path $repo 'build.bat') "bench_$suite"
                if ($LASTEXITCODE -ne 0) { throw "Could not build $variant/$suite." }
            }
        }
    }
    foreach ($variant in @('baseline','candidate')) {
        foreach ($suite in $Suites) {
            $manifestPath = Join-Path $builds[$variant] "build-$suite.json"
            if (!(Test-Path -LiteralPath $manifestPath)) { throw "Missing build manifest: $manifestPath" }
            $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            $expectedSource = if ($variant -eq 'baseline') { $BaselineRoot } else { $CandidateRoot }
            if ($manifest.source -ne $expectedSource) { throw "Wrong source tree for $variant/$suite; rebuild." }
            foreach ($manifestInput in $manifest.inputs) {
                if (!(Test-Path -LiteralPath $manifestInput.path) -or
                    (Get-FileHash -LiteralPath $manifestInput.path -Algorithm SHA256).Hash -ne $manifestInput.sha256) {
                    throw "Stale $variant/$suite build; rebuild after changes to $($manifestInput.path)."
                }
            }
            $exe = Join-Path $builds[$variant] "bench_$suite.exe"
            if (!(Test-Path -LiteralPath $exe) -or
                (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne $manifest.executable_hash) {
                throw "Missing or modified $variant/$suite executable."
            }
            Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $OutputPath "$variant-build-$suite.json") -Force
        }
    }
    $results = @{}
    foreach ($suite in $Suites) {
        $beforeCases = @(Get-Cases (Join-Path $builds.baseline "bench_$suite.exe"))
        $afterCases = @(Get-Cases (Join-Path $builds.candidate "bench_$suite.exe"))
        if (($beforeCases -join '|') -ne ($afterCases -join '|')) { throw "Mismatched case list for $suite" }
        foreach ($case in $beforeCases) {
            Write-Host "Comparing $case ($Runs adjacent process pairs)"
            for ($run = 0; $run -lt $Runs; ++$run) {
                $order = if ($run % 2) { @('candidate','baseline') } else { @('baseline','candidate') }
                foreach ($variant in $order) {
                    $executable = Join-Path $builds[$variant] "bench_$suite.exe"
                    $medians = Run-Process $executable "$variant-$case-$run" $case
                    foreach ($name in $medians.Keys) {
                        if (!$results.ContainsKey($name)) { $results[$name] = @{ baseline=@(); candidate=@() } }
                        $results[$name][$variant] += $medians[$name]
                    }
                }
            }
        }
    }
    if ($results.Count -eq 0) { throw 'No benchmark cases matched.' }
    $rows = @()
    foreach ($name in ($results.Keys | Sort-Object)) {
        $before = $results[$name].baseline
        $after = $results[$name].candidate
        if ($before.Count -ne $Runs -or $after.Count -ne $Runs) { throw "Mismatched case set: $name" }
        $ratios = @()
        for ($i = 0; $i -lt $Runs; ++$i) { $ratios += $after[$i] / $before[$i] }
        $delta = ((Median $ratios) - 1) * 100
        $slower = @($ratios | Where-Object { $_ -gt 1 }).Count
        # Flag consistent losses and material median losses for a separate confirmation.
        $review = $slower -ge [Math]::Ceiling($Runs * 0.85) -or $delta -gt 2
        $rows += [PSCustomObject]@{
            Case=$name; BaselineNs=(Median $before); CandidateNs=(Median $after)
            ChangePercent=$delta; SlowerPairs=$slower; Pairs=$Runs; Recheck=$review
            Ratios=$ratios; BaselineMedians=$before; CandidateMedians=$after
        }
    }
    try { $cpu = (Get-ItemProperty 'HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0' -Name ProcessorNameString).ProcessorNameString.Trim() }
    catch { $cpu = $env:PROCESSOR_IDENTIFIER }
    @{ timestamp_utc=[DateTime]::UtcNow.ToString('o'); baseline=$BaselineRoot; candidate=$CandidateRoot;
        candidate_revision=(& git rev-parse HEAD); cpu=$cpu; os=[Environment]::OSVersion.VersionString;
        suites=$Suites; filter=$Filter; cases=$Cases; runs=$Runs; pairing='adjacent processes per case'; results=$rows; raw=$rawRuns
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputPath 'results.json') -Encoding UTF8
    $report = @('# Paired Windows/x64 benchmark results', '',
        "CPU: $cpu. Each cell uses $Runs process medians, each from seven samples after warmup.",
        'Baseline/candidate processes are adjacent for each case; their order alternates between pairs. The collapsible profiler sum control is excluded.',
        'Positive change means slower. RECHECK identifies candidates for a separate confirmation, not an automatic acceptance decision.', '',
        '| Case | Baseline ns/op | Candidate ns/op | Paired change | Slower pairs | Status |',
        '| --- | ---: | ---: | ---: | ---: | --- |')
    foreach ($row in $rows) {
        $status = if ($row.Recheck) { 'RECHECK' } else { 'Measured' }
        $report += [string]::Format($culture, '| {0} | {1:F3} | {2:F3} | {3:+0.00;-0.00;0.00}% | {4}/{5} | {6} |',
            $row.Case, $row.BaselineNs, $row.CandidateNs, $row.ChangePercent, $row.SlowerPairs, $Runs, $status)
    }
    $report | Set-Content -LiteralPath (Join-Path $OutputPath 'report.md') -Encoding UTF8
    Write-Output ($report -join [Environment]::NewLine)
} finally {
    foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
    Pop-Location
}
