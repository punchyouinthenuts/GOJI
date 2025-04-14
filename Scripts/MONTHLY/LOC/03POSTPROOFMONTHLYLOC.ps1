# Global tracking
$global:successLog = @()
$global:changeLog = @()

function Log-Success {
    param([string]$Operation)
    $global:successLog += $Operation
}

function Log-Change {
    param([string]$Operation, [string]$SourcePath, [string]$DestinationPath)
    $global:changeLog += @{
        Operation = $Operation
        Source = $SourcePath
        Destination = $DestinationPath
        Timestamp = Get-Date
    }
}

function Test-JobNumber {
    param([string]$JobNum)
    if ($JobNum -match '^\d{5}$') {
        return $true
    }
    return $false
}

function Test-Month {
    param([string]$Month)
    $validMonths = @('JAN','FEB','MAR','APR','MAY','JUN','JUL','AUG','SEP','OCT','NOV','DEC')
    if ($Month.ToUpper() -in $validMonths) {
        return $true
    }
    return $false
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

try {
    # Get user input with separate validation
    do {
        $jobNumber = Read-Host "Enter the 5-digit job number"
        if (-not (Test-JobNumber $jobNumber)) {
            Write-Host "Job number must be exactly 5 digits. Try again." -ForegroundColor Yellow
            continue
        }

        $month = Read-Host "Enter the 3-letter month abbreviation"
        if (-not (Test-Month $month)) {
            Write-Host "Month must be a valid 3-letter abbreviation (e.g., JAN, FEB). Try again." -ForegroundColor Yellow
            continue
        }
        break
    } while ($true)

    # Clean up the input
    $jobNumber = $jobNumber.Trim()
    $month = $month.ToUpper().Trim()

    # Define paths
    $outputDir = "$env:USERPROFILE\Desktop\AUTOMATION\RAC\LOC\JOB\OUTPUT"
    $ppwkTempDir = "$env:USERPROFILE\Desktop\PPWK Temp"
    $proofDir = "$env:USERPROFILE\Desktop\AUTOMATION\RAC\LOC\JOB\PROOF"

    # Move and rename PDFs from OUTPUT with LOC check
    Get-ChildItem -Path $outputDir -Filter "*.pdf" | ForEach-Object {
        $originalName = $_.Name
        $newName = if ($originalName -notlike "*LOC*") {
            "$jobNumber $month LOC $originalName"
        } else {
            "$jobNumber $month $originalName"
        }
        $destination = Join-Path $ppwkTempDir $newName
        Move-Item -Path $_.FullName -Destination $destination -Force
        Log-Change -Operation "Move" -SourcePath $_.FullName -DestinationPath $destination
    }
    Log-Success "Files renamed in OUTPUT directory"

    # Rename PROOF folder files with LOC check
    Get-ChildItem -Path $proofDir | ForEach-Object {
        $originalName = $_.Name
        $newName = if ($originalName -notlike "*LOC*") {
            "$jobNumber $month LOC $originalName"
        } else {
            "$jobNumber $month $originalName"
        }
        $newPath = Join-Path $proofDir $newName
        Rename-Item -Path $_.FullName -NewName $newName -Force
        Log-Change -Operation "Rename" -SourcePath $_.FullName -DestinationPath $newPath
    }
    Log-Success "Files renamed in PROOF directory"

    # Create ZIP of PROOF files
    $zipPath = "$env:USERPROFILE\Desktop\AUTOMATION\RAC\LOC\JOB\$jobNumber $month LOC MONTHLY_LOC PROOFS.zip"
    Compress-Archive -Path "$proofDir\*" -DestinationPath $zipPath -Force
    Log-Change -Operation "Create" -SourcePath $null -DestinationPath $zipPath
    Log-Success "ZIP archive created successfully"

    Write-Host "`nAll processing completed successfully!" -ForegroundColor Green
    Write-Host "`nCompleted processes:" -ForegroundColor Yellow
    $global:successLog | ForEach-Object { Write-Host "- $_" }
}
catch {
    Write-Host "`nAn error occurred: $_" -ForegroundColor Red
    Write-Host "Beginning rollback process..." -ForegroundColor Yellow
    Rollback-Changes
    Write-Host "`nCompleted processes before error:" -ForegroundColor Yellow
    $global:successLog | ForEach-Object { Write-Host "- $_" }
}
finally {
    Write-Host "`nPress Enter key to close this window..." -ForegroundColor Cyan
    Read-Host
}
