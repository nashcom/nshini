$ErrorActionPreference = "Stop"

$Version = (Get-Content version.txt).Trim()
$ZipFile = "bin/w64/nshini-$Version-win64.zip"

if (Test-Path $ZipFile)
{
    Write-Host
    Write-Host "Removing existing release file: $ZipFile"
    Write-Host

    Remove-Item $ZipFile -Force
}

Compress-Archive -Path bin\w64\nshini.exe,bin\w64\nshini.pdb -DestinationPath $ZipFile -Force

Write-Host
Write-Host "Release file created: $ZipFile"
Write-Host
