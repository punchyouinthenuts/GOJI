# Parameters to accept inputs from the Qt application
param(
    [string]$cbcJobNumber,
    [string]$excJobNumber,
    [string]$ncwoJobNumber,
    [string]$prepifJobNumber,
    [string]$inactiveJobNumber,
    [string]$weekNumber,
    [string]$basePath,
    [string]$year
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
        [string]$JobType,
        [string]$Year
    )
    
    Write-Progress -Activity "Processing $JobType PDF Files" -Status "Initializing..." -PercentComplete 0
    
    $paths = @{
        'CBC' = @((Join-Path $basePath "CBC\JOB\PRINT"), (Join-Path $basePath "CBC"))
        'EXC' = @((Join-Path $basePath "EXC\JOB\PRINT"), (Join-Path $basePath "EXC"))
        'NCWO' = @((Join-Path $basePath "NCWO\JOB\PRINT"), (Join-Path $basePath "NCWO"))
        'PREPIF' = @((Join-Path $basePath "PREPIF\JOB\PRINT"), (Join-Path $basePath "PREPIF"))
    }
    
    if (-not $paths.ContainsKey($JobType)) { return }
    
    Write-Progress -Activity "Processing $JobType PDF Files" -Status "Setting up directories..." -PercentComplete 20
    
    $pdfSource, $baseFolder = $paths[$JobType]
    $networkParentPath = "\\NAS1069D9\AMPrintData\${Year}_SrcFiles\I\Innerworkings\$JobNumber $JobType"
    
    New-Item -Path $networkParentPath -ItemType Directory -Force
    Log-Change -Operation "Create" -SourcePath $null -DestinationPath $networkParentPath
    
    $weekSubfolder = Join-Path $networkParentPath $WeekNumber
    New-Item -Path $weekSubfolder -ItemType Directory -Force
    Log-Change -Operation "Create" -SourcePath $null -DestinationPath $weekSubfolder
    
    Write-Progress -Activity "Processing $JobType PDF Files" -Status "Scanning for PDF files..." -PercentComplete 40
    
    $pdfFiles = Get-ChildItem -Path $pdfSource -Filter "*.pdf"
    $total = $pdfFiles.Count
    $current = 0
    
    foreach ($pdf in $pdfFiles) {
        $current++
        $percentComplete = 40 + (($current / $total) * 40)  # 40-80% for copying
        Write-Progress -Activity "Processing $JobType PDF Files" -Status "Processing $($pdf.Name)" -PercentComplete $percentComplete
        
        $originalPath = $pdf.FullName
        $newName = "$JobNumber $WeekNumber $($pdf.Name)"
        $newPath = Join-Path $pdfSource $newName
        
        Rename-Item -Path $originalPath -NewName $newName
        Log-Change -Operation "Rename" -SourcePath $originalPath -DestinationPath $newPath
        
        Copy-Item -Path $newPath -Destination $weekSubfolder -Force
        Log-Change -Operation "Copy" -SourcePath $newPath -DestinationPath (Join-Path $weekSubfolder $newName)
    }
    
    # Compress PDFs into a .7z archive
    if ($pdfFiles.Count -gt 0) {
        Write-Progress -Activity "Processing $JobType PDF Files" -Status "Compressing PDFs..." -PercentComplete 80
        
        $archiveName = "$JobNumber $WeekNumber $JobType PRINT FILES.7z"
        $archivePath = Join-Path $pdfSource $archiveName
        $sevenZipPath = "$env:ProgramFiles\7-Zip\7z.exe"
        
        if (-not (Test-Path $sevenZipPath)) {
            throw "7-Zip executable not found at $sevenZipPath"
        }
        
        $command = "& '$sevenZipPath' a -t7z -mx=9 -m0=lzma2 -ms=on -mmt=2 '$archivePath' '$pdfSource\*.pdf'"
        Invoke-Expression $command
        
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to compress PDFs for $JobType to $archivePath"
        }
        
        Log-Change -Operation "Create" -SourcePath $null -DestinationPath $archivePath
        Write-Output "Compressed PDFs to $archivePath"
    }
    
    Write-Progress -Activity "Processing $JobType PDF Files" -Status "Complete" -PercentComplete 100
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
        Write-Output "Processing $JobType files..."
        Process-PdfFiles -JobNumber $jobNumbers[$jobType] -WeekNumber $weekNumber -JobType $jobType -Year $year
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