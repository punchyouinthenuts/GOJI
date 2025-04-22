# Parameters to accept inputs from the Qt application
param(
    [string]$cbcJobNumber,
    [string]$excJobNumber,
    [string]$ncwoJobNumber,
    [string]$prepifJobNumber,
    [string]$inactiveJobNumber,
    [string]$weekNumber,
    [string]$basePath
)

# Global tracking variables for rollback and success logging
$global:changeLog = @()
$global:successLog = @()

# Function to log changes for rollback purposes
function Log-Change {
    param(
        [string]$Operation,
        [string]$SourcePath,
        [string]$DestinationPath
    )
    $global:changeLog += @{
        Operation    = $Operation
        Source       = $SourcePath
        Destination  = $DestinationPath
        Timestamp    = Get-Date
    }
}

# Function to log successful operations
function Log-Success {
    param([string]$Operation)
    $global:successLog += $Operation
}

# Function to rollback changes in case of errors
function Rollback-Changes {
    Write-Output "Rolling back changes..."
    foreach ($change in $global:changeLog | Sort-Object { $_.Timestamp } -Descending) {
        Write-Output "Rolling back $($change.Operation) operation..."
        try {
            switch ($change.Operation) {
                "Move" {
                    if (Test-Path $change.Destination) {
                        Move-Item -Path $change.Destination -Destination $change.Source -Force
                    }
                }
                "Copy" {
                    if (Test-Path $change.Destination) {
                        Remove-Item -Path $change.Destination -Force
                    }
                }
                "Rename" {
                    if (Test-Path $change.Destination) {
                        Rename-Item -Path $change.Destination -NewName (Split-Path $change.Source -Leaf)
                    }
                }
                "Create" {
                    if (Test-Path $change.Destination) {
                        Remove-Item -Path $change.Destination -Force -Recurse
                    }
                }
            }
        }
        catch {
            Write-Output "Rollback error for $($change.Operation): $_"
        }
    }
    Write-Output "Rollback completed"
}

# Function to move ZIP files
function Move-ZipFiles {
    param([string]$WeekNum)
    
    $sourceDir = Join-Path $basePath "WEEKLY\WEEKLY_ZIP"
    $destDir = Join-Path $sourceDir "OLD"
    
    Write-Output "Checking ZIP directories..."
    if (-not (Test-Path $destDir)) {
        New-Item -Path $destDir -ItemType Directory -Force
        Log-Change -Operation "Create" -SourcePath $null -DestinationPath $destDir
    }
    
    Write-Output "Searching for matching ZIP files..."
    $matchingFiles = Get-ChildItem -Path $sourceDir -Filter "*.zip" | Where-Object { $_.Name -like "*$WeekNum*" }
    
    if ($matchingFiles) {
        foreach ($file in $matchingFiles) {
            Write-Output "Moving $($file.Name) to $destDir"
            $destination = Join-Path $destDir $file.Name
            Move-Item -Path $file.FullName -Destination $destination -Force
            Log-Change -Operation "Move" -SourcePath $file.FullName -DestinationPath $destination
        }
    } else {
        Write-Output "No matching ZIP files found for week $WeekNum"
    }
    
    Log-Success "ZIP files processed"
    return $true
}

# Function to process PDF files for CBC, EXC, NCWO, and PREPIF job types
function Process-PdfFiles {
    param(
        [string]$JobNumber,
        [string]$WeekNumber,
        [string]$JobType
    )
    
    Write-Output "Processing $JobType PDF Files..."
    
    # Use the shared working folder for all job types
    $pdfSource = Join-Path $basePath "JOB\PRINT"
    $baseFolder = Join-Path $basePath $JobType
    $networkParentPath = "\\NAS1069D9\AMPrintData\2025_SrcFiles\I\Innerworkings\$JobNumber $JobType"
    
    Write-Output "Setting up directories..."
    New-Item -Path $networkParentPath -ItemType Directory -Force
    Log-Change -Operation "Create" -SourcePath $null -DestinationPath $networkParentPath
    
    $weekSubfolder = Join-Path $networkParentPath $WeekNumber
    New-Item -Path $weekSubfolder -ItemType Directory -Force
    Log-Change -Operation "Create" -SourcePath $null -DestinationPath $weekSubfolder
    
    Write-Output "Scanning for PDF files in $pdfSource..."
    $pdfFiles = Get-ChildItem -Path $pdfSource -Filter "*.pdf"
    
    foreach ($pdf in $pdfFiles) {
        Write-Output "Processing $($pdf.Name)..."
        $originalPath = $pdf.FullName
        $newName = "$JobNumber $WeekNumber $($pdf.Name)"
        $newPath = Join-Path $pdfSource $newName
        
        Rename-Item -Path $originalPath -NewName $newName
        Log-Change -Operation "Rename" -SourcePath $originalPath -DestinationPath $newPath
        
        # Copy to the home folder for this job type and week
        $weekFolder = Join-Path $baseFolder $WeekNumber
        $printFolder = Join-Path $weekFolder "PRINT"
        
        if (-not (Test-Path $printFolder)) {
            New-Item -Path $printFolder -ItemType Directory -Force
            Log-Change -Operation "Create" -SourcePath $null -DestinationPath $printFolder
        }
        
        Copy-Item -Path $newPath -Destination $printFolder -Force
        Log-Change -Operation "Copy" -SourcePath $newPath -DestinationPath (Join-Path $printFolder $newName)
        
        # Move to the network path
        Move-Item -Path $newPath -Destination $weekSubfolder -Force
        Log-Change -Operation "Move" -SourcePath $newPath -DestinationPath (Join-Path $weekSubfolder $newName)
    }
    
    Log-Success "$JobType PDF files processed"
}

# Function to process INACTIVE CSV files
function Process-InactiveFiles {
    param([string]$WeekNumber)
    
    Write-Output "Processing Inactive Files..."
    
    # Use the shared working folder for INACTIVE files
    $inactiveSource = Join-Path $basePath "JOB\OUTPUT"
    $targetPath = "W:\"
    $localBackupPath = "C:\Users\JCox\Desktop\MOVE TO BUSKRO"
    
    Write-Output "Checking directories..."
    if (-not (Test-Path $inactiveSource)) {
        throw "INACTIVE source folder does not exist: $inactiveSource"
    }
    
    if (-not (Test-Path $localBackupPath)) {
        New-Item -Path $localBackupPath -ItemType Directory -Force
        Log-Change -Operation "Create" -SourcePath $null -DestinationPath $localBackupPath
    }
    
    $networkAvailable = Test-Path $targetPath
    if (-not $networkAvailable) {
        Write-Output "Network drive W: unavailable. Using local backup: $localBackupPath"
        $targetPath = $localBackupPath
    }
    
    $csvFiles = Get-ChildItem -Path $inactiveSource -Filter "*.csv"
    
    foreach ($csv in $csvFiles) {
        Write-Output "Copying $($csv.Name) to $targetPath"
        $destination = Join-Path $targetPath $csv.Name
        Copy-Item -Path $csv.FullName -Destination $destination -Force
        Log-Change -Operation "Copy" -SourcePath $csv.FullName -DestinationPath $destination
    }
    
    Log-Success "Inactive files processed"
}

# Main execution block
try {
    Write-Output "Starting Weekly File Processing..."
    $jobNumbers = @{
        'CBC'      = $cbcJobNumber
        'EXC'      = $excJobNumber
        'NCWO'     = $ncwoJobNumber
        'PREPIF'   = $prepifJobNumber
        'INACTIVE' = $inactiveJobNumber
    }
    
    # Process ZIP files
    Move-ZipFiles -WeekNum $weekNumber
    
    # Process PDF files for specified job types
    foreach ($jobType in @('CBC', 'EXC', 'NCWO', 'PREPIF')) {
        Write-Output "Processing $jobType files..."
        Process-PdfFiles -JobNumber $jobNumbers[$jobType] -WeekNumber $weekNumber -JobType $jobType
    }
    
    # Process INACTIVE files
    Write-Output "Processing Inactive files..."
    Process-InactiveFiles -WeekNumber $weekNumber
    
    Write-Output "All processing completed successfully!"
}
catch {
    Write-Output "An error occurred: $_"
    Write-Output "Beginning rollback process..."
    Rollback-Changes
    Write-Output "Completed processes before error:"
    $global:successLog | ForEach-Object { Write-Output "- $_" }
}