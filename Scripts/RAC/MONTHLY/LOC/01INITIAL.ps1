# Define paths
$zipFolder = "$env:USERPROFILE\Desktop\AUTOMATION\RAC\LOC\INPUTZIP"
$extractFolder = "$env:USERPROFILE\Desktop\AUTOMATION\RAC\LOC\JOB\INPUT"

# Find and extract ZIP file
$zipFile = Get-ChildItem -Path $zipFolder -Filter "*.zip" | Select-Object -First 1
if ($zipFile) {
    Expand-Archive -Path $zipFile.FullName -DestinationPath $extractFolder -Force
}
# Find "File Box" folder and move TXT files
$fileBoxFolder = Get-ChildItem -Path $extractFolder -Recurse -Directory | 
    Where-Object { $_.Name -like "*File Box*" } | 
    Select-Object -First 1

if ($fileBoxFolder) {
    Get-ChildItem -Path $fileBoxFolder.FullName -Filter "*.txt" | 
        ForEach-Object {
            Move-Item -Path $_.FullName -Destination $extractFolder -Force
        }
}
# Delete non-TXT files and empty directories
Get-ChildItem -Path $extractFolder -Recurse | ForEach-Object {
    if ($_.PSIsContainer) {
        Remove-Item $_.FullName -Recurse -Force
    }
    elseif ($_.Extension -ne '.txt') {
        Remove-Item $_.FullName -Force
    }
}
# Process and rename files
Get-ChildItem -Path $extractFolder -Filter "*.txt" | ForEach-Object {
    # Remove quotes from file content
    $content = Get-Content $_.FullName -Raw
    $content = $content.Replace('"', '')
    Set-Content -Path $_.FullName -Value $content -NoNewline
    
    # Rename files
    if ($_.Name -like "*GIN*") {
        Rename-Item -Path $_.FullName -NewName "GIN.txt" -Force
    }
    elseif ($_.Name -like "*HOM*") {
        Rename-Item -Path $_.FullName -NewName "HOM.txt" -Force
    }
}
# Delete the original ZIP file
if ($zipFile) {
    Remove-Item -Path $zipFile.FullName -Force
}
