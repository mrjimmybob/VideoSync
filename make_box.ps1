# ===========================================================================
#  make_box.ps1 - package the deployed VideoSync into ONE standalone .exe
#
#  Uses Enigma Virtual Box to embed the Qt runtime (DLLs + plugin folders)
#  inside a single VideoSync_boxed.exe. The config and log files are
#  deliberately NOT boxed - VideoSync.ini and VideoSync.log are read/written
#  next to the boxed exe as normal external files.
#
#  Run build.bat first (it produces build\release via windeployqt).
#
#  Usage:  pwsh -File make_box.ps1     (or run from Qt's PowerShell)
# ===========================================================================

$ErrorActionPreference = 'Stop'

$projRoot   = $PSScriptRoot
$releaseDir = Join-Path $projRoot 'build\release'
$publishDir = Join-Path $projRoot 'build\Publish'
$inputExe   = Join-Path $releaseDir 'VideoSync.exe'
$outputExe  = Join-Path $publishDir 'VideoSync_boxed.exe'
$evbPath    = Join-Path $projRoot 'VideoSync.evb'
$enigma     = 'C:\Program Files (x86)\Enigma Virtual Box\enigmavbconsole.exe'

# Only these runtime items get boxed. Everything else in build\release
# (build artifacts, and crucially VideoSync.ini / VideoSync.log) is excluded.
$includeFolders = @('generic','iconengines','imageformats','networkinformation','platforms','styles','tls')

if (-not (Test-Path $inputExe)) { throw "Not found: $inputExe  -- run build.bat first." }
if (-not (Test-Path $enigma))   { throw "Enigma Virtual Box console not found: $enigma" }
New-Item -ItemType Directory -Force -Path $publishDir | Out-Null

# --- XML emitters --------------------------------------------------------
function New-FileEntry([System.IO.FileInfo]$f) {
@"
          <File>
            <Type>2</Type>
            <Name>$($f.Name)</Name>
            <File>$($f.FullName)</File>
            <ActiveX>False</ActiveX>
            <ActiveXInstall>False</ActiveXInstall>
            <Action>0</Action>
            <OverwriteDateTime>False</OverwriteDateTime>
            <OverwriteAttributes>False</OverwriteAttributes>
            <PassCommandLine>False</PassCommandLine>
            <HideFromDialogs>0</HideFromDialogs>
          </File>
"@
}

function New-FolderEntry([System.IO.DirectoryInfo]$d) {
    $children = ""
    foreach ($sub in (Get-ChildItem $d.FullName -Directory)) { $children += (New-FolderEntry $sub) }
    foreach ($file in (Get-ChildItem $d.FullName -File))      { $children += (New-FileEntry $file) }
@"
          <File>
            <Type>3</Type>
            <Name>$($d.Name)</Name>
            <Action>0</Action>
            <OverwriteDateTime>False</OverwriteDateTime>
            <OverwriteAttributes>False</OverwriteAttributes>
            <HideFromDialogs>0</HideFromDialogs>
            <Files>
$children
            </Files>
          </File>
"@
}

# --- Build the file tree (DLLs + plugin folders only) --------------------
$entries = ""
foreach ($dll in (Get-ChildItem $releaseDir -Filter *.dll -File)) { $entries += (New-FileEntry $dll) }
foreach ($name in $includeFolders) {
    $dir = Join-Path $releaseDir $name
    if (Test-Path $dir) { $entries += (New-FolderEntry (Get-Item $dir)) }
    else { Write-Warning "Plugin folder missing, skipping: $name" }
}

# --- Assemble the .evb project -------------------------------------------
$evb = @"
<?xml version="1.0" encoding="windows-1252"?>
<>
  <InputFile>$inputExe</InputFile>
  <OutputFile>$outputExe</OutputFile>
  <Files>
    <Enabled>True</Enabled>
    <DeleteExtractedOnExit>False</DeleteExtractedOnExit>
    <CompressFiles>True</CompressFiles>
    <Files>
      <File>
        <Type>3</Type>
        <Name>%DEFAULT FOLDER%</Name>
        <Action>0</Action>
        <OverwriteDateTime>False</OverwriteDateTime>
        <OverwriteAttributes>False</OverwriteAttributes>
        <HideFromDialogs>0</HideFromDialogs>
        <Files>
$entries
        </Files>
      </File>
    </Files>
  </Files>
  <Registries>
    <Enabled>False</Enabled>
    <Registries/>
  </Registries>
  <Packaging>
    <Enabled>False</Enabled>
  </Packaging>
  <Options>
    <ShareVirtualSystem>False</ShareVirtualSystem>
    <MapExecutableWithTemporaryFile>True</MapExecutableWithTemporaryFile>
    <TemporaryFileMask/>
    <AllowRunningOfVirtualExeFiles>True</AllowRunningOfVirtualExeFiles>
    <ProcessesOfAnyPlatforms>False</ProcessesOfAnyPlatforms>
  </Options>
  <Storage>
    <Files>
      <Enabled>False</Enabled>
      <Folder>%DEFAULT FOLDER%\</Folder>
      <RandomFileNames>False</RandomFileNames>
      <EncryptContent>False</EncryptContent>
    </Files>
  </Storage>
</>
"@

Set-Content -Path $evbPath -Value $evb -Encoding utf8
Write-Host "Wrote project: $evbPath"

# --- Box it --------------------------------------------------------------
Write-Host "Boxing with Enigma Virtual Box..."
& $enigma $evbPath
if ($LASTEXITCODE -ne 0) { throw "enigmavbconsole failed (exit $LASTEXITCODE)" }

if (Test-Path $outputExe) {
    $mb = [math]::Round((Get-Item $outputExe).Length / 1MB, 2)
    Write-Host "Done -> $outputExe ($mb MB)"
    Write-Host "Place VideoSync.ini (and optionally VideoSync.log) next to it; they stay external."
} else {
    throw "Boxing reported success but output not found: $outputExe"
}
