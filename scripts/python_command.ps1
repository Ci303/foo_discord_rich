function Test-PythonCandidate
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Executable,
        [string[]]$PrefixArguments = @()
    )

    try
    {
        $command = Get-Command -Name $Executable -CommandType Application -ErrorAction Stop |
            Select-Object -First 1
        $commandPath = $command.Path
        $probeArguments = @($PrefixArguments) + @(
            '-c',
            'import sys; print("{}.{}.{}|{}".format(*sys.version_info[:3], sys.executable)); raise SystemExit(0 if sys.version_info >= (3, 10) else 1)'
        )
        $probeOutput = & $commandPath @probeArguments 2>&1
        if ($LASTEXITCODE -ne 0)
        {
            return $null
        }

        $probeLine = @($probeOutput | ForEach-Object { $_.ToString() }) |
            Where-Object { $_ -match '^\d+\.\d+\.\d+\|.+' } |
            Select-Object -Last 1
        if (-not $probeLine)
        {
            return $null
        }

        $version, $reportedPath = $probeLine -split '\|', 2
        return [pscustomobject]@{
            Path = $commandPath
            PrefixArguments = @($PrefixArguments)
            Version = $version
            ReportedPath = $reportedPath
        }
    }
    catch
    {
        return $null
    }
}

function Resolve-PythonCommand
{
    param(
        [string]$RequestedExecutable
    )

    if ($RequestedExecutable)
    {
        $resolved = Test-PythonCandidate -Executable $RequestedExecutable
        if (-not $resolved)
        {
            throw "Python executable '$RequestedExecutable' was not found or is not Python 3.10 or newer."
        }
        return $resolved
    }

    $candidates = @(
        [pscustomobject]@{ Executable = 'python'; PrefixArguments = @() },
        [pscustomobject]@{ Executable = 'py'; PrefixArguments = @('-3') },
        [pscustomobject]@{ Executable = 'python3'; PrefixArguments = @() }
    )
    foreach ($candidate in $candidates)
    {
        $resolved = Test-PythonCandidate `
            -Executable $candidate.Executable `
            -PrefixArguments $candidate.PrefixArguments
        if ($resolved)
        {
            return $resolved
        }
    }

    throw 'Python 3.10 or newer was not found. Install Python or pass -PythonExecutable with its full path.'
}

function Invoke-PythonCommand
{
    param(
        [Parameter(Mandatory = $true)]
        [psobject]$PythonCommand,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $allArguments = @($PythonCommand.PrefixArguments) + @($Arguments)
    & $PythonCommand.Path @allArguments
}
