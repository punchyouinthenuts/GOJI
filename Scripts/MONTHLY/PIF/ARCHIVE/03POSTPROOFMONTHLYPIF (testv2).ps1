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
    # Get user input
    $userInput = Read-Host "ENTER PIF JOB NUMBER FOLLOWED BY MONTH"
    $jobNumber = $userInput.Substring(0,5)
    $month = $userInput.Substring($userInput.Length - 3)

    # Define paths
    $outputDir = "$env:USERPROFILE\Desktop\AUTOMATION\RAC\MONTHLY_PIF\FOLDER\OUTPUT"
    $ppwkTempDir = "$env:USERPROFILE\Desktop\PPWK Temp"
    $proofDir = "$env:USERPROFILE\Desktop\AUTOMATION\RAC\MONTHLY_PIF\FOLDER\PROOF"

    # Move and rename PDFs from OUTPUT
    Get-ChildItem -Path $outputDir -Filter "*.pdf" | ForEach-Object {
        $newName = "$jobNumber $month PIF $($_.Name)"
        $destination = Join-Path $ppwkTempDir $newName
        Move-Item -Path $_.FullName -Destination $destination -Force
        Log-Change -Operation "Move" -SourcePath $_.FullName -DestinationPath $destination
    }
    Log-Success "Files renamed in OUTPUT directory"

    # Rename PROOF folder files
    Get-ChildItem -Path $proofDir | ForEach-Object {
        $originalName = $_.Name
        $newName = "$jobNumber $month PIF $originalName"
        $newPath = Join-Path $proofDir $newName
        Rename-Item -Path $_.FullName -NewName $newName -Force
        Log-Change -Operation "Rename" -SourcePath $_.FullName -DestinationPath $newPath
    }
    Log-Success "Files renamed in PROOF directory"

    # Create ZIP of PROOF files
    $zipPath = "$env:USERPROFILE\Desktop\AUTOMATION\RAC\MONTHLY_PIF\FOLDER\$jobNumber $month PIF MONTHLY_PIF PROOFS.zip"
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
