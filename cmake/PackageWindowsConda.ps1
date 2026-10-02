param(
    [Parameter(Mandatory=$true)][string]$DependencyPrefix,
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$prefix = (Resolve-Path -LiteralPath $DependencyPrefix).Path
$stage = Join-Path (Split-Path (Resolve-Path $BuildDirectory).Path -Parent) 'windows-dependencies'
if (Test-Path $stage) { throw 'Dependency staging directory already exists' }
New-Item -ItemType Directory -Path "$stage/bin","$stage/Qt6/plugins/platforms","$stage/share" | Out-Null
Get-ChildItem "$prefix/Library/bin" -Filter '*.dll' | Copy-Item -Destination "$stage/bin"
$platform = @(Get-ChildItem "$prefix/Library" -Recurse -Filter qwindows.dll)
if ($platform.Count -ne 1) { throw 'Cannot identify the Qt Windows platform plugin' }
Copy-Item $platform[0].FullName "$stage/Qt6/plugins/platforms"
# Conda retains the upstream notices in its extracted package cache.
$cache = Join-Path (Split-Path (Split-Path $prefix -Parent) -Parent) 'pkgs'
foreach ($record in Get-ChildItem "$prefix/conda-meta" -Filter '*.json') {
    $metadata = Get-Content $record.FullName -Raw | ConvertFrom-Json
    $package = Join-Path $cache "$($metadata.name)-$($metadata.version)-$($metadata.build)"
    $licenses = Join-Path $package 'info/licenses'
    if (Test-Path $licenses) {
        foreach ($notice in Get-ChildItem $licenses -Recurse -File) {
            $relative = $notice.FullName.Substring($licenses.Length).TrimStart('\','/')
            $folder = Join-Path "$stage/share" "$($metadata.name)/$relative"
            New-Item -ItemType Directory -Path $folder -Force | Out-Null
            Copy-Item $notice.FullName (Join-Path $folder 'copyright')
        }
    } elseif ($metadata.name -in @('qt6-main','openssl')) {
        throw "Missing upstream license notices for $($metadata.name)"
    }
}
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs = & $vswhere -latest -products '*' -property installationPath
$version = @(Get-ChildItem "$vs/VC/Redist/MSVC" -Directory | Sort-Object Name -Descending)[0].FullName
$crt = Join-Path $version 'x64/Microsoft.VC143.CRT'
if (!(Test-Path $crt)) { throw 'Visual C++ redistributable directory not found' }
& "$PSScriptRoot/PackagePortable.ps1" -BuildDirectory $BuildDirectory -VcpkgTripletDirectory $stage -VcRuntimeDirectory $crt -OutputDirectory $OutputDirectory -ServerHost orangepi3b.local
# Verify the distributed executables start with only their bundled DLLs.
foreach ($name in @('client','admin')) {
    $folder = Join-Path $OutputDirectory "encoder-$name-windows-x64"
    $oldPath = $env:PATH
    try {
        $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
        $process = Start-Process (Join-Path $folder "encoder-$name.exe") -WorkingDirectory $folder -PassThru
        Start-Sleep -Seconds 5
        if ($process.HasExited) { throw "$name failed to launch (exit $($process.ExitCode))" }
        Stop-Process -Id $process.Id
    } finally { $env:PATH = $oldPath }
}
