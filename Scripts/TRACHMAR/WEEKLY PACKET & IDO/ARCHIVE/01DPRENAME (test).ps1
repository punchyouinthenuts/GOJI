# Define the directory path
$folderPath = "$env:USERPROFILE\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\RAW FILES"

# Get all xlsx and csv files in the directory
$targetFiles = Get-ChildItem -Path $folderPath -File | Where-Object { $_.Extension -in '.xlsx','.csv' } | Sort-Object Name

# Rename files with numerical prefix
$index = 1
foreach ($file in $targetFiles) {
    $newName = "{0:D2} {1}" -f $index, $file.Name
    Rename-Item -Path $file.FullName -NewName $newName
    Write-Host "Renamed: $($file.Name) -> $newName"
    $index++
}
