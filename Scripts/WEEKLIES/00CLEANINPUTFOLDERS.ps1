#!pwsh

# Define base path
$basePath = "C:\Users\JCox\Desktop\AUTOMATION\RAC"

# Define job types and their paths
$jobPaths = @{
    "CBC" = "$basePath\CBC\JOB"
    "EXC" = "$basePath\EXC\JOB"
    "INACTIVE" = "$basePath\INACTIVE_2310-DM07\FOLDERS"
    "NCWO" = "$basePath\NCWO_4TH\DM03"
    "PREPIF" = "$basePath\PREPIF\FOLDERS"
}

# Define folders to clean for each job
$foldersToClean = @("INPUT", "OUTPUT", "PROOF")

# Function to clean a folder
function Clean-Folder {
    param (
        [string]$FolderPath,
        [string]$JobType,
        [string]$SubFolder
    )
    
    Write-Host "Processing $JobType - $SubFolder folder" -ForegroundColor Yellow
    
    if (Test-Path $FolderPath) {
        try {
            $files = Get-ChildItem -Path $FolderPath -File
            if ($files.Count -gt 0) {
                $files | ForEach-Object {
                    Remove-Item $_.FullName -Force
                    Write-Host "Deleted: $($_.Name)" -ForegroundColor Green
                }
            } else {
                Write-Host "No files found in $FolderPath" -ForegroundColor Cyan
            }
        } catch {
            Write-Host "Error cleaning $FolderPath : $_" -ForegroundColor Red
        }
    } else {
        Write-Host "Folder not found: $FolderPath" -ForegroundColor Red
    }
}

# Main execution
Write-Host "Starting cleanup process..." -ForegroundColor Magenta

foreach ($job in $jobPaths.GetEnumerator()) {
    Write-Host "`nProcessing $($job.Key) job folders..." -ForegroundColor Blue
    
    foreach ($folder in $foldersToClean) {
        $fullPath = Join-Path $job.Value $folder
        Clean-Folder -FolderPath $fullPath -JobType $job.Key -SubFolder $folder
    }
}

Write-Host "`nCleanup process completed!" -ForegroundColor Green
