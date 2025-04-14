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
    Write-Host "Rolling back changes..." -ForegroundColor Yellow
    for ($i = $global:changeLog.Count - 1; $i -ge 0; $i--) {
        $change = $global:changeLog[$i]
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
    $jobNumbers = @{}
    foreach ($jobType in $JOB_TYPES) {
        do {
            $jobNum = Read-Host "ENTER $jobType JOB NUMBER"
            if (Test-JobNumber $jobNum) {
                $jobNumbers[$jobType] = $jobNum
                break
            }
            Write-Host "Job number must be 5 digits. Try again." -ForegroundColor Yellow
        } while ($true)
    }
    
    do {
        $weekNum = Read-Host "`nENTER WEEK NUMBER (format: xx.xx)"
        if (Test-WeekNumber $weekNum) {
            break
        }
        Write-Host "Week number must be in format xx.xx with valid month/week. Try again." -ForegroundColor Yellow
    } while ($true)
    
    Log-Success "User inputs collected successfully"
    return $jobNumbers, $weekNum
}

function Move-ZipFiles {
    param([string]$WeekNum)
    
    $sourceDir = Join-Path $BASE_PATH "WEEKLY\WEEKLY_ZIP"
    $destDir = Join-Path $sourceDir "OLD"
    
    if (-not (Test-Path $destDir)) {
        New-Item -Path $destDir -ItemType Directory -Force
        Log-Change -Operation "Create" -SourcePath $null -DestinationPath $destDir
    }
    
    $matchingFiles = Get-ChildItem -Path $sourceDir -Filter "*.zip" | 
                    Where-Object { $_.Name -like "*$WeekNum*" }
    
    if (-not $matchingFiles) {
        $response = Read-Host "NO PROOF ZIPS FOR THAT WEEK ARE FOUND, DO YOU WANT TO CONTINUE? Y/N"
        if ($response -ne 'Y') {
            return $false
        }
        Write-Host "PLEASE REMEMBER TO CHECK FOR PROOF ZIPS" -ForegroundColor Yellow
    }
    else {
        foreach ($file in $matchingFiles) {
            $destination = Join-Path $destDir $file.Name
            Move-Item -Path $file.FullName -Destination $destination -Force
            Log-Change -Operation "Move" -SourcePath $file.FullName -DestinationPath $destination
        }
    }
    Log-Success "ZIP files processed"
    return $true
}

function Process-PdfFiles {
    param(
        [string]$JobNumber,
        [string]$WeekNumber,
        [string]$JobType
    )
    
    $paths = @{
        'CBC' = @((Join-Path $BASE_PATH "CBC\JOB\PRINT"), (Join-Path $BASE_PATH "CBC"))
        'EXC' = @((Join-Path $BASE_PATH "EXC\JOB\PRINT"), (Join-Path $BASE_PATH "EXC"))
        'NCWO' = @((Join-Path $BASE_PATH "NCWO_4TH\DM03\PRINT"), (Join-Path $BASE_PATH "NCWO_4TH"))
        'PREPIF' = @((Join-Path $BASE_PATH "PREPIF\FOLDERS\PRINT"), (Join-Path $BASE_PATH "PREPIF"))
    }
    
    if (-not $paths.ContainsKey($JobType)) { return }
    
    $pdfSource, $baseFolder = $paths[$JobType]
    $networkParentPath = "\\NAS1069D9\AMPrintData\2025_SrcFiles\I\Innerworkings\$JobNumber $JobType"
    
    New-Item -Path $networkParentPath -ItemType Directory -Force
    Log-Change -Operation "Create" -SourcePath $null -DestinationPath $networkParentPath
    
    $weekSubfolder = Join-Path $networkParentPath $WeekNumber
    New-Item -Path $weekSubfolder -ItemType Directory -Force
    Log-Change -Operation "Create" -SourcePath $null -DestinationPath $weekSubfolder
    
    Get-ChildItem -Path $pdfSource -Filter "*.pdf" | ForEach-Object {
        $originalPath = $_.FullName
        $newName = "$JobNumber $WeekNumber $($_.Name)"
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
    Log-Success "$JobType PDF files processed"
}

function Process-InactiveFiles {
    param([string]$WeekNumber)
    
    $basePath = Join-Path $BASE_PATH "INACTIVE_2310-DM07"
    $targetPath = "W:\"
    $weekFolder = Join-Path $basePath $WeekNumber
    $outputFolder = Join-Path $weekFolder "OUTPUT"
    
    if (-not (Test-Path $weekFolder)) {
        throw "INACTIVE folder does not exist!"
    }
    
    if (-not (Test-Path $outputFolder)) {
        throw "OUTPUT folder not found!"
    }
    
    Get-ChildItem -Path $outputFolder -Filter "*.csv" | ForEach-Object {
        $destination = Join-Path $targetPath $_.Name
        Copy-Item -Path $_.FullName -Destination $destination -Force
        Log-Change -Operation "Copy" -SourcePath $_.FullName -DestinationPath $destination
    }
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
