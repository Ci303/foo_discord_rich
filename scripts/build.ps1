param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('x64', 'Win32')]
    [string]$Platform = 'x64',
    [string]$Toolset = 'v145',
    [string]$VCToolsVersion = '14.51.36231',
    [string]$PythonExecutable,
    [switch]$NoPackage,
    [switch]$Deploy
)

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDir 'python_command.ps1')
Set-Location (Resolve-Path (Join-Path $scriptDir '..'))

if (-not (Get-Command msbuild -ErrorAction SilentlyContinue))
{
    throw 'msbuild was not found in PATH. Run this script from a Visual Studio developer prompt or invoke via Launch-VsDevShell first.'
}

$toolsetPath = Join-Path (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\18\Community\VC\Tools\MSVC') $VCToolsVersion
if (-not (Test-Path $toolsetPath))
{
    Write-Warning "MSVC toolset path for '$VCToolsVersion' was not found at '$toolsetPath'. Build may still succeed if an equivalent toolset is available via Visual Studio configuration." 
}

$msbuildArgs = @(
    'workspaces\\foo_discord_rich.sln',
    '/m',
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    "/p:PlatformToolset=$Toolset",
    "/p:VCToolsVersion=$VCToolsVersion"
)

Write-Host "Running: msbuild $($msbuildArgs -join ' ')"
& msbuild @msbuildArgs
if ($LASTEXITCODE -ne 0)
{
    throw "msbuild exited with code $LASTEXITCODE"
}

if (-not $NoPackage)
{
    $pythonCommand = Resolve-PythonCommand -RequestedExecutable $PythonExecutable
    $packArgs = @(
        'scripts\\pack_component.py',
        '--configuration', $Configuration,
        '--platform', $Platform
    )

    $pythonDisplay = $pythonCommand.Path
    if ($pythonCommand.PrefixArguments.Count -gt 0)
    {
        $pythonDisplay += ' ' + ($pythonCommand.PrefixArguments -join ' ')
    }
    Write-Host "Using Python $($pythonCommand.Version): $pythonDisplay"
    Write-Host "Running: $pythonDisplay $($packArgs -join ' ')"
    Invoke-PythonCommand -PythonCommand $pythonCommand -Arguments $packArgs
    if ($LASTEXITCODE -ne 0)
    {
        throw "pack_component.py exited with code $LASTEXITCODE"
    }
}

if ($Deploy)
{
    $targetRoot = Join-Path $env:APPDATA 'foobar2000'
    $platformSuffix = if ($Platform -eq 'x64') { '-x64' } else { '' }
    $targetDir = Join-Path $targetRoot ('user-components' + $platformSuffix)
    if (-not (Test-Path $targetDir))
    {
        New-Item -ItemType Directory -Path $targetDir | Out-Null
    }

    Copy-Item ".\\_result\\${Platform}_$Configuration\\foo_discord_rich.fb2k-component" -Destination (Join-Path $targetDir 'foo_discord_rich.fb2k-component') -Force
    Write-Host "Deployed component to $targetDir"
}
