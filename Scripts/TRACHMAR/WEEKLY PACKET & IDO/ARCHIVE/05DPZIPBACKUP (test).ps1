# Define paths
$sourceDir = "$env:USERPROFILE\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING"
$backupDir = "$env:USERPROFILE\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\BACKUP"

# Ensure backup directory exists
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null

# Look for and move zip files
$zipFiles = Get-ChildItem -Path $sourceDir -Filter "*.zip" | 
    Where-Object { $_.Name -match '(PREFLIGHT|PROCESSED)' }

if ($zipFiles) {
    foreach ($zip in $zipFiles) {
        Move-Item $zip.FullName -Destination $backupDir
        Write-Host "Moved $($zip.Name) to backup directory"
    }
} else {
    Write-Host "No PREFLIGHT or PROCESSED ZIP files found!"
    Write-Host "Press any key to continue..."
    $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')
}
