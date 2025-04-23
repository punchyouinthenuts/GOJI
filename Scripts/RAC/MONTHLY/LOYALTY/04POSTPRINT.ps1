# Function to validate input
function Test-UserInput {
    param (
        [string]$jobNumber,
        [string]$month
    )
    return ($jobNumber -match '^\d{5}$') -and ($month -match '^[A-Za-z]{3}$')
}

# Function to rollback changes
function Undo-Changes {
    param (
        [string]$archiveFolder,
        [string]$networkFolder,
        [array]$renamedFiles
    )
    
    if (Test-Path $archiveFolder) {
        Remove-Item $archiveFolder -Recurse -Force
    }
    
    if (Test-Path $networkFolder) {
        $printFolder = "C:\Program Files\Goji\RAC\LOYALTY\JOB\PRINT"
        Get-ChildItem $networkFolder -Filter "*.pdf" | Move-Item -Destination $printFolder
        Remove-Item $networkFolder -Recurse -Force
    }
    
    foreach ($file in $renamedFiles) {
        if ($file.NewName -and (Test-Path $file.NewPath)) {
            Rename-Item -Path $file.NewPath -NewName $file.OriginalName
        }
    }
}

try {
    # Store original state for potential rollback
    $renamedFiles = @()
    
    # Prompt for input
    do {
        $jobNumber = Read-Host "Enter 5-digit job number"
        $month = Read-Host "Enter 3-letter month code"
        $month = $month.ToUpper()
    } while (-not (Test-UserInput $jobNumber $month))
    
    # Define paths
    $baseFolder = "C:\Program Files\Goji\RAC\LOYALTY\JOB"
    $archivePath = "C:\Program Files\Goji\RAC\LOYALTY\ARCHIVE\$jobNumber $month LOYALTY"
    $networkPath = "\\NAS1069D9\AMPrintData\2025_SrcFiles\I\Innerworkings"
    $networkFolder = Join-Path $networkPath "$jobNumber $month LOYALTY"
    
    # Create archive folder
    New-Item -Path $archivePath -ItemType Directory -Force
    
    # Move PROOFS zip file
    Move-Item -Path "$baseFolder\*PROOFS*.zip" -Destination "$baseFolder\PROOFS"
    
    # Copy folders to archive
    foreach ($folder in @('INPUT', 'OUTPUT', 'PRINT', 'PROOF')) {
        Copy-Item -Path "$baseFolder\$folder" -Destination $archivePath -Recurse
    }
    
    # Rename PDFs in PRINT folder
    $printFiles = Get-ChildItem "$baseFolder\PRINT\*.pdf"
    foreach ($file in $printFiles) {
        $newName = "$jobNumber $month $($file.Name)"
        $renamedFiles += @{
            OriginalName = $file.Name
            NewName = $newName
            NewPath = Join-Path $file.Directory.FullName $newName
        }
        Rename-Item -Path $file.FullName -NewName $newName
    }
    
    # Create network folder and move PDFs
    New-Item -Path $networkFolder -ItemType Directory -Force
    Move-Item -Path "$baseFolder\PRINT\*.pdf" -Destination $networkFolder
    
    # Clear folder contents after successful archive
    foreach ($folder in @('INPUT', 'OUTPUT', 'PRINT', 'PROOF')) {
        Get-ChildItem "$baseFolder\$folder" | Remove-Item -Recurse -Force
    }
    
    Write-Host "TASK COMPLETE! Press any key to terminate..."
    cmd /c pause > $null
    exit
    
} catch {
    Write-Host "Error: $($_.Exception.Message)"
    Write-Host "Press any key to terminate..."
    cmd /c pause > $null
    
    # Rollback changes
    Undo-Changes -archiveFolder $archivePath -networkFolder $networkFolder -renamedFiles $renamedFiles
    exit
}
