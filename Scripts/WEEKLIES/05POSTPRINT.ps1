# Constants
$JOB_TYPES = @('NCWO', 'INACTIVE', 'PREPIF', 'CBC', 'EXC')
$BASE_PATH = Join-Path $env:USERPROFILE "Desktop\AUTOMATION\RAC"

# Global tracking variables
$global:changeLog = @()
$global:successLog = @()

function Log-Change {
    param([string]$Operation, [string]$SourcePath, [string]$DestinationPath)
    $global:changeLog += @{
        Operation = $Operation
        Source = $SourcePath
        Destination = $DestinationPath
        Timestamp = Get-Date
    }
}

function Log-Success {
    param([string]$Operation)
    $global:successLog += $Operation
}

function Rollback-Changes {
    Write-Progress -Activity "Rolling Back Changes" -Status "Initializing rollback..." -PercentComplete 0
    Write-Host "Rolling back changes..." -ForegroundColor Yellow
    
    $totalChanges = $global:changeLog.Count
    $currentChange = 0
    
    for ($i = $global:changeLog.Count - 1; $i -ge 0; $i--) {
        $currentChange++
        $change = $global:changeLog[$i]
        $percentComplete = ($currentChange / $totalChanges) * 100
        
        Write-Progress -Activity "Rolling Back Changes" -Status "Rolling back $($change.Operation) operation" -PercentComplete $percentComplete
        
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
            Write-Host "Rollback error for $($change.Operation): $_" -ForegroundColor Red
        }
    }
    Write-Progress -Activity "Rolling Back Changes" -Status "Complete" -Completed
    Write-Host "Rollback completed" -ForegroundColor Green
}

function Test-JobNumber {
    param([string]$JobNumber)
    return $JobNumber -match '^\d{5}$'
}

function Test-WeekNumber {
    param([string]$WeekNumber)
    if ($WeekNumber -match '^\d{1,2}\.\d{1,2}$') {
        $month, $week = $WeekNumber.Split('.')
        return ([int]$month -ge 1 -and [int]$month -le 12 -and [int]$week -ge 1 -and [int]$week -le 53)
    }
    return $false
}

function Get-UserInputs {
    Write-Progress -Activity "User Input Collection" -Status "Collecting job numbers..." -PercentComplete 0
    
    $jobNumbers = @{}
    $totalJobs = $JOB_TYPES.Count
    $currentJob = 0
    
    foreach ($jobType in $JOB_TYPES) {
        $currentJob++
        $percentComplete = ($currentJob / $totalJobs) * 50  # First 50% for job numbers
        Write-Progress -Activity "User Input Collection" -Status "Enter $jobType job number" -PercentComplete $percentComplete
        
        do {
            $jobNum = Read-Host "ENTER $jobType JOB NUMBER"
            if (Test-JobNumber $jobNum) {
                $jobNumbers[$jobType] = $jobNum
                break
            }
            Write-Host "Job number must be 5 digits. Try again." -ForegroundColor Yellow
        } while ($true)
    }
    
    Write-Progress -Activity "User Input Collection" -Status "Enter week number" -PercentComplete 75
    
    do {
        $weekNum = Read-Host "`nENTER WEEK NUMBER (format: xx.xx)"
        if (Test-WeekNumber $weekNum) {
            break
        }
        Write-Host "Week number must be in format xx.xx with valid month/week. Try again." -ForegroundColor Yellow
    } while ($true)
    
    Write-Progress -Activity "User Input Collection" -Status "Complete" -Completed
    Log-Success "User inputs collected successfully"
    return $jobNumbers, $weekNum
}

function Move-ZipFiles {
    param([string]$WeekNum)
    
    Write-Progress -Activity "ZIP File Processing" -Status "Initializing..." -PercentComplete 0
    
    $sourceDir = Join-Path $BASE_PATH "WEEKLY\WEEKLY_ZIP"
    $destDir = Join-Path $sourceDir "OLD"
    
    Write-Progress -Activity "ZIP File Processing" -Status "Checking directories..." -PercentComplete 25
    
    if (-not (Test-Path $destDir)) {
        New-Item -Path $destDir -ItemType Directory -Force
        Log-Change -Operation "Create" -SourcePath $null -DestinationPath $destDir
    }
    
    Write-Progress -Activity "ZIP File Processing" -Status "Searching for matching files..." -PercentComplete 50
    
    $matchingFiles = Get-ChildItem -Path $sourceDir -Filter "*.zip" | 
                    Where-Object { $_.Name -like "*$WeekNum*" }
    
    if (-not $matchingFiles) {
        Write-Progress -Activity "ZIP File Processing" -Status "No matching files found" -PercentComplete 75
        $response = Read-Host "NO PROOF ZIPS FOR THAT WEEK ARE FOUND, DO YOU WANT TO CONTINUE? Y/N"
        if ($response -ne 'Y') {
            Write-Progress -Activity "ZIP File Processing" -Status "Cancelled" -Completed
            return $false
        }
        Write-Host "PLEASE REMEMBER TO CHECK FOR PROOF ZIPS" -ForegroundColor Yellow
    }
    else {
        $total = $matchingFiles.Count
        $current = 0
        foreach ($file in $matchingFiles) {
            $current++
            $percentComplete = 75 + (($current / $total) * 25)  # Last 25% for file moving
            Write-Progress -Activity "ZIP File Processing" -Status "Moving $($file.Name)" -PercentComplete $percentComplete
            
            $destination = Join-Path $destDir $file.Name
            Move-Item -Path $file.FullName -Destination $destination -Force
            Log-Change -Operation "Move" -SourcePath $file.FullName -DestinationPath $destination
        }
    }
    
    Write-Progress -Activity "ZIP File Processing" -Status "Complete" -Completed
    Log-Success "ZIP files processed"
    return $true
}

function Process-PdfFiles {
    param(
        [string]$JobNumber,
        [string]$WeekNumber,
        [string]$JobType
    )
    
    Write-Progress -Activity "Processing $JobType PDF Files" -Status "Initializing..." -PercentComplete 0
    
    $paths = @{
        'CBC' = @((Join-Path $BASE_PATH "CBC\JOB\PRINT"), (Join-Path $BASE_PATH "CBC"))
        'EXC' = @((Join-Path $BASE_PATH "EXC\JOB\PRINT"), (Join-Path $BASE_PATH "EXC"))
        'NCWO' = @((Join-Path $BASE_PATH "NCWO_4TH\DM03\PRINT"), (Join-Path $BASE_PATH "NCWO_4TH"))
        'PREPIF' = @((Join-Path $BASE_PATH "PREPIF\FOLDERS\PRINT"), (Join-Path $BASE_PATH "PREPIF"))
    }
    
    if (-not $paths.ContainsKey($JobType)) { return }
    
    Write-Progress -Activity "Processing $JobType PDF Files" -Status "Setting up directories..." -PercentComplete 20
    
    $pdfSource, $baseFolder = $paths[$JobType]
    $networkParentPath = "\\NAS1069D9\AMPrintData\2025_SrcFiles\I\Innerworkings\$JobNumber $JobType"
    
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
        $percentComplete = 40 + (($current / $total) * 60)  # Remaining 60% for file processing
        Write-Progress -Activity "Processing $JobType PDF Files" -Status "Processing $($pdf.Name)" -PercentComplete $percentComplete
        
        $originalPath = $pdf.FullName
        $newName = "$JobNumber $WeekNumber $($pdf.Name)"
        $newPath = Join-Path $pdfSource $newName
        
        Rename-Item -Path $originalPath -NewName $newName
        Log-Change -Operation "Rename" -SourcePath $originalPath -DestinationPath $newPath
        
        $weekFolder = Join-Path $baseFolder $WeekNumber
        $printFolder = Join-Path $weekFolder "PRINT"
        
        Copy-Item -Path $newPath -Destination $printFolder -Force
        Log-Change -Operation "Copy" -SourcePath $newPath -DestinationPath (Join-Path $printFolder $newName)
        
        Move-Item -Path $newPath -Destination $weekSubfolder -Force
        Log-Change -Operation "Move" -SourcePath $newPath -DestinationPath (Join-Path $weekSubfolder $newName)
    }
    
    Write-Progress -Activity "Processing $JobType PDF Files" -Status "Complete" -Completed
    Log-Success "$JobType PDF files processed"
}

function Process-InactiveFiles {
    param([string]$WeekNumber)
    
    Write-Progress -Activity "Processing Inactive Files" -Status "Initializing..." -PercentComplete 0
    
    $basePath = Join-Path $BASE_PATH "INACTIVE_2310-DM07"
    $targetPath = "W:\"
    $localBackupPath = "C:\Users\JCox\Desktop\MOVE TO BUSKRO"
    $weekFolder = Join-Path $basePath $WeekNumber
    $outputFolder = Join-Path $weekFolder "OUTPUT"
    
    Write-Progress -Activity "Processing Inactive Files" -Status "Checking directories..." -PercentComplete 25
    
    if (-not (Test-Path $weekFolder)) {
        throw "INACTIVE folder does not exist!"
    }
    
    if (-not (Test-Path $outputFolder)) {
        throw "OUTPUT folder not found!"
    }
    
    # Create local backup directory if it doesn't exist
    if (-not (Test-Path $localBackupPath)) {
        New-Item -Path $localBackupPath -ItemType Directory -Force
        Log-Change -Operation "Create" -SourcePath $null -DestinationPath $localBackupPath
    }
    
    Write-Progress -Activity "Processing Inactive Files" -Status "Copying CSV files..." -PercentComplete 50
    
    $csvFiles = Get-ChildItem -Path $outputFolder -Filter "*.csv"
    $total = $csvFiles.Count
    $current = 0
    
    # Test network path accessibility
    $networkAvailable = Test-Path $targetPath
    
    if (-not $networkAvailable) {
        Write-Host "Network drive access is unavailable. Moving files to LOCAL FOLDER instead." -ForegroundColor Yellow
        $targetPath = $localBackupPath
    }
    
    foreach ($csv in $csvFiles) {
        $current++
        $percentComplete = 50 + (($current / $total) * 50)
        Write-Progress -Activity "Processing Inactive Files" -Status "Copying $($csv.Name)" -PercentComplete $percentComplete
        
        $destination = Join-Path $targetPath $csv.Name
        Copy-Item -Path $csv.FullName -Destination $destination -Force
        Log-Change -Operation "Copy" -SourcePath $csv.FullName -DestinationPath $destination
    }
    
    Write-Progress -Activity "Processing Inactive Files" -Status "Complete" -Completed
    Log-Success "Inactive files processed"
}

# Main execution block
try {
    Write-Host "Starting Weekly File Processing..." -ForegroundColor Cyan
    $jobNumbers, $weekNumber = Get-UserInputs
    
    if (-not (Move-ZipFiles -WeekNum $weekNumber)) {
        throw "ZIP file processing cancelled by user"
    }
    
    foreach ($jobType in @('CBC', 'EXC', 'NCWO', 'PREPIF')) {
        Write-Host "`nProcessing $jobType files..." -ForegroundColor Cyan
        Process-PdfFiles -JobNumber $jobNumbers[$jobType] -WeekNumber $weekNumber -JobType $jobType
    }
    
    Write-Host "`nProcessing Inactive files..." -ForegroundColor Cyan
    Process-InactiveFiles -WeekNumber $weekNumber
    
    Write-Host "`nAll processing completed successfully!" -ForegroundColor Green
}
catch {
    Write-Host "`nAn error occurred: $_" -ForegroundColor Red
    Write-Host "Beginning rollback process..." -ForegroundColor Yellow
    Rollback-Changes
    Write-Host "`nCompleted processes before error:" -ForegroundColor Yellow
    $global:successLog | ForEach-Object { Write-Host "- $_" }
}
finally {
    Write-Host "`nAll processes have completed." -ForegroundColor Green
    Write-Host "Press Enter key to close this window..." -ForegroundColor Cyan
    Read-Host
}
