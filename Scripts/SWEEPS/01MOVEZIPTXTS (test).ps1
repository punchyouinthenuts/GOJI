# Define paths
$basePath = "$env:USERPROFILE\Desktop\AUTOMATION\RAC\SWEEPS"
$inputZip = Join-Path $basePath "INPUTZIP"
$fileBox = Join-Path $basePath "FILEBOX"
$jobInput = Join-Path $basePath "JOB\INPUT"
$macOSX = Join-Path $fileBox "__MACOSX"

# Create directories if they don't exist
New-Item -ItemType Directory -Force -Path $fileBox | Out-Null
New-Item -ItemType Directory -Force -Path $jobInput | Out-Null

# Find and extract ZIP file
$zipFile = Get-ChildItem -Path $inputZip -Filter "*.zip" | Select-Object -First 1
if ($zipFile) {
    Expand-Archive -Path $zipFile.FullName -DestinationPath $fileBox -Force
    Write-Host "Extracted zip file to $fileBox"
}

# Remove __MACOSX folder if it exists
if (Test-Path $macOSX) {
    Remove-Item -Path $macOSX -Recurse -Force
    Write-Host "Removed __MACOSX folder"
}

# Find and move TXT files
$txtFiles = Get-ChildItem -Path $fileBox -Recurse -Filter "*.txt" | 
    Where-Object { $_.FullName -notlike "*__MACOSX*" } |
    Sort-Object Name

$index = 1
foreach ($file in $txtFiles) {
    $newName = "SWEEPS {0:D2}.txt" -f $index
    Move-Item -Path $file.FullName -Destination (Join-Path $jobInput $newName) -Force
    Write-Host "Moved and renamed $($file.Name) to $newName"
    $index++
}

if (-not $txtFiles) {
    Write-Host "No TXT files found to move"
}

# Clean up directories
if (Test-Path $fileBox) {
    Remove-Item -Path $fileBox -Recurse -Force
    New-Item -ItemType Directory -Force -Path $fileBox | Out-Null
}

Get-ChildItem -Path $inputZip -File | Remove-Item -Force

Write-Host "File processing completed successfully!"
