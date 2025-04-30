# Define paths
$sourceDirs = @{
    'PROCESSED' = "$env:USERPROFILE\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\PROCESSED"
    'PREFLIGHT' = "$env:USERPROFILE\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\PREFLIGHT"
}
$backupDir = "$env:USERPROFILE\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\BACKUP"
$parentDir = "$env:USERPROFILE\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING"

# Create backup directory if needed
New-Item -ItemType Directory -Force -Path $backupDir

# Get timestamp
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"

# Process each directory
foreach ($dirType in $sourceDirs.Keys) {
    $sourceDir = $sourceDirs[$dirType]
    $zipPath = Join-Path $parentDir "${dirType}_TM_FILES_${timestamp}.zip"
    
    # Create temporary directory for ZIP contents
    $tempDir = New-Item -ItemType Directory -Path (Join-Path $env:TEMP "ZIP_$timestamp")
    
    # Process CSV files
    Get-ChildItem -Path $sourceDir -Filter "*.csv" | ForEach-Object {
        $mergedName = "$($_.BaseName)_MERGED.csv"
        Copy-Item $_.FullName -Destination (Join-Path $tempDir $mergedName)
        Move-Item $_.FullName -Destination (Join-Path $backupDir $_.Name)
    }
    
    # Create ZIP file
    Compress-Archive -Path "$tempDir\*" -DestinationPath $zipPath
    Remove-Item -Path $tempDir -Recurse
    
    Write-Host "Process completed. ZIP file created: $zipPath"
}
