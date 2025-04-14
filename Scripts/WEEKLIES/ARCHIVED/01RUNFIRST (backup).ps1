#!pwsh

# Define base paths as global variables
$script:BasePath = "C:\Users\JCox\Desktop\AUTOMATION\RAC"
$script:LogDir = Join-Path $BasePath "transaction_logs"
$script:BackupDir = Join-Path $LogDir "backups"

# Transaction Management Class
class TransactionManager {
    [string]$LogFile
    [System.Collections.ArrayList]$Operations
    [string]$LogDir
    [string]$BackupDir

    TransactionManager() {
        $this.LogDir = $script:LogDir
        $this.BackupDir = $script:BackupDir
        $this.Operations = [System.Collections.ArrayList]::new()
        
        if (-not (Test-Path $this.LogDir)) {
            New-Item -Path $this.LogDir -ItemType Directory
        }
        if (-not (Test-Path $this.BackupDir)) {
            New-Item -Path $this.BackupDir -ItemType Directory
        }
        
        $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
        $this.LogFile = Join-Path $this.LogDir "transaction_log_${timestamp}.txt"
    }

    [string] CreateBackup([string]$FilePath) {
        if (Test-Path $FilePath) {
            $backupPath = Join-Path $this.BackupDir "$([System.IO.Path]::GetFileName($FilePath)).bak"
            Copy-Item -Path $FilePath -Destination $backupPath -Force
            return $backupPath
        }
        return $null
    }

    [void] LogOperation([string]$OpType, [string]$OriginalState, [string]$NewState) {
        $backupPath = $null
        if ($OpType -in @('modify', 'delete')) {
            $backupPath = $this.CreateBackup($OriginalState)
        }

        $operation = @{
            Type = $OpType
            Original = $OriginalState
            New = $NewState
            Backup = $backupPath
            Timestamp = Get-Date
        }

        $this.Operations.Add($operation)
        Add-Content -Path $this.LogFile -Value "$($operation.Timestamp): $($operation.Type) - $($operation.Original) -> $($operation.New)"
    }

    [void] Rollback() {
        Write-Host "`nInitiating rollback of all operations..." -ForegroundColor Yellow
        
        for ($i = $this.Operations.Count - 1; $i -ge 0; $i--) {
            $operation = $this.Operations[$i]
            try {
                switch ($operation.Type) {
                    'rename' {
                        if (Test-Path $operation.New) {
                            Move-Item -Path $operation.New -Destination $operation.Original -Force
                        }
                    }
                    'move' {
                        if (Test-Path $operation.New) {
                            New-Item -Path (Split-Path $operation.Original) -ItemType Directory -Force
                            Move-Item -Path $operation.New -Destination $operation.Original -Force
                        }
                    }
                    'create' {
                        if (Test-Path $operation.Original) {
                            Remove-Item -Path $operation.Original -Force
                        }
                    }
                    'create_dir' {
                        if (Test-Path $operation.Original) {
                            Remove-Item -Path $operation.Original -Recurse -Force
                        }
                    }
                    'modify' {
                        if ($operation.Backup -and (Test-Path $operation.Backup)) {
                            Copy-Item -Path $operation.Backup -Destination $operation.Original -Force
                        }
                    }
                    'delete' {
                        if ($operation.Backup -and (Test-Path $operation.Backup)) {
                            New-Item -Path (Split-Path $operation.Original) -ItemType Directory -Force
                            Copy-Item -Path $operation.Backup -Destination $operation.Original -Force
                        }
                    }
                }
                Write-Host "Reverted: $($operation.Type) operation on $([System.IO.Path]::GetFileName($operation.Original))" -ForegroundColor Green
            }
            catch {
                Write-Host "Warning: Rollback operation failed - $_" -ForegroundColor Red
            }
        }
        
        Write-Host "Rollback completed" -ForegroundColor Green
        if (Test-Path $this.BackupDir) {
            Remove-Item -Path $this.BackupDir -Recurse -Force
        }
    }
}

# File Processing Functions
function Process-ZipFiles {
    param([TransactionManager]$Transaction)

    $zipSourcePath = Join-Path $script:BasePath "WEEKLY\INPUTZIP"
    $baseWeeklyPath = Join-Path $script:BasePath "WEEKLY"
    $fileboxPath = Join-Path $baseWeeklyPath "FILEBOX"
    $macosxPath = Join-Path $fileboxPath "__MACOSX"

    $destinations = @{
        '202404' = Join-Path $script:BasePath "CBC\JOB\INPUT"
        '201209' = Join-Path $script:BasePath "CBC\JOB\INPUT"
        '202406' = Join-Path $script:BasePath "EXC\JOB\INPUT"
        '201504' = Join-Path $script:BasePath "INACTIVE_2310-DM07\FOLDERS\INPUT"
        '201903' = Join-Path $script:BasePath "NCWO_4TH\DM03\INPUT"
        '202303' = Join-Path $script:BasePath "PREPIF\FOLDERS\INPUT"
    }

    New-Item -Path $fileboxPath -ItemType Directory -Force
    $Transaction.LogOperation('create_dir', $fileboxPath, $null)

    $zipFiles = Get-ChildItem -Path $zipSourcePath -Filter "*.zip"
    if (-not $zipFiles) {
        throw "No ZIP files found in source directory"
    }

    foreach ($zipFile in $zipFiles) {
        Expand-Archive -Path $zipFile.FullName -DestinationPath $fileboxPath -Force
        $Transaction.LogOperation('extract', $zipFile.FullName, $fileboxPath)
    }

    if (Test-Path $macosxPath) {
        Remove-Item -Path $macosxPath -Recurse -Force
    }

    # Continue with file processing and moving to destinations
    Get-ChildItem -Path $fileboxPath -Recurse -File | ForEach-Object {
        $file = $_
        foreach ($pattern in $destinations.Keys) {
            if ($file.Name -match $pattern) {
                $destPath = Join-Path $destinations[$pattern] $file.Name
                Move-Item -Path $file.FullName -Destination $destPath -Force
                $Transaction.LogOperation('move', $file.FullName, $destPath)
                break
            }
        }
    }

    Remove-Item -Path $fileboxPath -Recurse -Force
    $Transaction.LogOperation('delete_dir', $fileboxPath, $null)

    foreach ($zipFile in $zipFiles) {
        Remove-Item -Path $zipFile.FullName -Force
        $Transaction.LogOperation('delete', $zipFile.FullName, $null)
    }
}

function Process-CBCInput {
    param([TransactionManager]$Transaction)
    
    $inputDirectory = Join-Path $script:BasePath "CBC\JOB\INPUT"
    $versions = @(
        'RAC2401-DM03-A', 'RAC2401-DM03-CANC', 'RAC2401-DM03-PR',
        'RAC2404-DM07-CBC2-A', 'RAC2404-DM07-CBC2-PR', 'RAC2404-DM07-CBC2-CANC'
    )

    Get-ChildItem -Path $inputDirectory -Filter "*.txt" | ForEach-Object {
        $Transaction.LogOperation('modify', $_.FullName, $null)
        
        # Read content with tab delimiter and explicit encoding
        $content = Get-Content $_.FullName | ConvertFrom-Csv -Delimiter "`t"
        
        foreach ($version in $versions) {
            # Filter content based on the Creative_Version_Cd column
            $filteredContent = $content | Where-Object { $_.'Creative_Version_Cd' -eq $version }
            
            if ($filteredContent) {
                $outputFile = Join-Path $inputDirectory "$version.csv"
                
                if (-not (Test-Path $outputFile)) {
                    $Transaction.LogOperation('create', $outputFile, $null)
                    $filteredContent | Export-Csv -Path $outputFile -NoTypeInformation
                } else {
                    $Transaction.LogOperation('modify', $outputFile, $null)
                    $filteredContent | Export-Csv -Path $outputFile -NoTypeInformation -Append
                }
                
                Write-Host "Created/Updated: $version.csv" -ForegroundColor Green
            }
        }
    }
}

function Process-EXCInput {
    param([TransactionManager]$Transaction)
    
    $inputPath = Join-Path $script:BasePath "EXC\JOB\INPUT"
    $outputFile = Join-Path $inputPath "EXC.txt"
    
    $headers = $null
    $allData = New-Object System.Collections.ArrayList

    Get-ChildItem -Path $inputPath -Filter "*.txt" | ForEach-Object {
        $Transaction.LogOperation('modify', $_.FullName, $null)
        
        $lines = Get-Content $_.FullName
        if ($lines) {
            if (-not $headers) {
                $headers = $lines[0]
                $allData.AddRange($lines[1..($lines.Length-1)])
            } else {
                $allData.AddRange($lines[1..($lines.Length-1)])
            }
        }
    }

    $Transaction.LogOperation('create', $outputFile, $null)
    Set-Content -Path $outputFile -Value $headers
    Add-Content -Path $outputFile -Value $allData

    Get-ChildItem -Path $inputPath -Filter "*.txt" | Where-Object { $_.Name -ne "EXC.txt" } | ForEach-Object {
        $Transaction.LogOperation('delete', $_.FullName, $null)
        Remove-Item $_.FullName -Force
    }
}

function Process-InactiveInput {
    param([TransactionManager]$Transaction)
    
    $inputDir = Join-Path $script:BasePath "INACTIVE_2310-DM07\FOLDERS\INPUT"
    $renameRules = @{
        '^.*DM001_HHG.*$' = 'FZAPU.txt'
        '^.*DM001(?!_HHG).*$' = 'APU.txt'
        '^.*DM002_HHG.*$' = 'FZAPO.txt'
        '^.*DM002(?!_HHG).*$' = 'APO.txt'
    }

    Get-ChildItem -Path $inputDir -Filter "*.txt" | ForEach-Object {
        $file = $_
        foreach ($pattern in $renameRules.Keys) {
            if ($file.BaseName -match $pattern) {
                $newPath = Join-Path $inputDir $renameRules[$pattern]
                if (Test-Path $newPath) {
                    Remove-Item $newPath -Force
                }
                $Transaction.LogOperation('rename', $file.FullName, $newPath)
                Rename-Item -Path $file.FullName -NewName $renameRules[$pattern] -Force
                Write-Host "Renamed: $($file.Name) -> $($renameRules[$pattern])" -ForegroundColor Green
                break
            }
        }
    }
}

function Process-NCWOInput {
    param([TransactionManager]$Transaction)
    
    $inputDir = Join-Path $script:BasePath "NCWO_4TH\DM03\INPUT"
    $outputFile = Join-Path $inputDir "ALLINPUT.csv"
    
    $columnNames = @(
        "Campaign_Cd", "Campaign_Name", "Campaign_Type_Cd", "Cell_Cd", "Cell_Name",
        "Channel_Cd", "Creative_Version_Cd", "Campaign_Deployment_Dt", "Individual_Id",
        "First_Name", "Last_Name", "OCR", "AddressLine_1", "AddressLIne_2", "City",
        "State_Cd", "Postal_Cd", "Zip4", "Store_Id", "Store_AddressLine_1", "Store_AddressLine_2",
        "Store_City", "Store_State_Cd", "Store_Postal_Cd", "Store_Phone_Number", "Store_License",
        "DMA_Name", "CUSTOM_01", "CUSTOM_02", "CUSTOM_03", "CUSTOM_04", "CUSTOM_05",
        "CUSTOM_06", "CUSTOM_07", "CUSTOM_08", "CUSTOM_09", "CUSTOM_10"
    )

    $allData = New-Object System.Collections.ArrayList

    # Process TXT files with explicit UTF-8 encoding
    $txtFiles = Get-ChildItem -Path $inputDir -Filter "*.txt"
    
    if ($txtFiles.Count -eq 0) {
        throw "No input files found for NCWO processing"
    }

    $txtFiles | ForEach-Object {
        $Transaction.LogOperation('modify', $_.FullName, $null)
        $rawContent = [System.IO.File]::ReadAllText($_.FullName, [System.Text.Encoding]::UTF8)
        $reader = New-Object System.IO.StringReader($rawContent)
        $csv = New-Object System.Collections.ArrayList
        
        while (($line = $reader.ReadLine()) -ne $null) {
            $null = $csv.Add($line.Split("`t"))
        }
        
        $content = $csv | Select-Object -Skip 1 | ForEach-Object {
            $obj = New-Object PSObject
            for ($i = 0; $i -lt $columnNames.Count; $i++) {
                # Strip quotes from values during processing
                $value = $_[$i] -replace '^"|"$', ''
                $obj | Add-Member -NotePropertyName $columnNames[$i] -NotePropertyValue $value
            }
            $obj
        }
        
        $null = $allData.AddRange($content)
    }

    # Process CUSTOM_04 dates and add VERSION column
    $allData | ForEach-Object {
        $_ | Add-Member -NotePropertyName 'START_DATE' -NotePropertyValue '' -Force
        $_ | Add-Member -NotePropertyName 'END_DATE' -NotePropertyValue '' -Force
        $_ | Add-Member -NotePropertyName 'VERSION' -NotePropertyValue '' -Force
        
        if ($_.'CUSTOM_04' -match '(.*?)\s*-\s*(.*)') {
            $_.'START_DATE' = $matches[1].Trim()
            $_.'END_DATE' = $matches[2].Trim()
        }
    }

    # Export combined ALLINPUT.csv with UTF-8 encoding
    $Transaction.LogOperation('create', $outputFile, $null)
    [System.IO.File]::WriteAllLines($outputFile, 
        (@($columnNames -join ',') + ($allData | ConvertTo-Csv -NoTypeInformation | Select-Object -Skip 1)), 
        [System.Text.Encoding]::UTF8)

    # Define version codes and create individual version files
    $versionCodes = @{
        "RAC2501-DM05-NCWO2-APPR" = "2-APPR.csv"
        "RAC2501-DM05-NCWO2-PR" = "2-PR.csv"
        "RAC2501-DM05-NCWO2-AP" = "2-AP.csv"
        "RAC2501-DM05-NCWO2-A" = "2-A.csv"
        "RAC2501-DM05-NCWO1-PR" = "1-PR.csv"
        "RAC2501-DM05-NCWO1-A" = "1-A.csv"
        "RAC2501-DM05-NCWO1-APPR" = "1-APPR.csv"
        "RAC2501-DM05-NCWO1-AP" = "1-AP.csv"
    }

    foreach ($version in $versionCodes.Keys) {
        $versionData = $allData | Where-Object { $_.'Creative_Version_Cd' -eq $version }
        if ($versionData) {
            $versionData | ForEach-Object { 
                $_.'VERSION' = $versionCodes[$version] -replace '\.csv$', '' 
            }
            $outputPath = Join-Path $inputDir $versionCodes[$version]
            $Transaction.LogOperation('create', $outputPath, $null)
            [System.IO.File]::WriteAllLines($outputPath, 
                (@($columnNames -join ',') + ($versionData | ConvertTo-Csv -NoTypeInformation | Select-Object -Skip 1)), 
                [System.Text.Encoding]::UTF8)
        }
    }
}

function Process-PrepifInput {
    param([TransactionManager]$Transaction)
    
    $inputDir = Join-Path $script:BasePath "PREPIF\FOLDERS\INPUT"
    $renameRules = @{
        '^.*Pre-PIF_Prod_ALL_Files_HHG.*$' = 'PREPIF.txt'
        '^.*DM001(?!.*Pre-PIF).*$' = 'PIF.txt'
    }

    Get-ChildItem -Path $inputDir -Filter "*.txt" | ForEach-Object {
        $file = $_
        foreach ($pattern in $renameRules.Keys) {
            if ($file.BaseName -match $pattern) {
                $newPath = Join-Path $inputDir $renameRules[$pattern]
                if (Test-Path $newPath) {
                    Remove-Item $newPath -Force
                }
                $Transaction.LogOperation('rename', $file.FullName, $newPath)
                Rename-Item -Path $file.FullName -NewName $renameRules[$pattern] -Force
                Write-Host "Renamed: $($file.Name) -> $($renameRules[$pattern])" -ForegroundColor Green
                break
            }
        }
    }
}

# Main execution
try {
    $transaction = [TransactionManager]::new()
    
    Write-Host "Starting input processing sequence..." -ForegroundColor Cyan
    
    Process-ZipFiles -Transaction $transaction
    Process-CBCInput -Transaction $transaction
    Process-EXCInput -Transaction $transaction
    Process-InactiveInput -Transaction $transaction
    Process-NCWOInput -Transaction $transaction
    Process-PrepifInput -Transaction $transaction
    
    Write-Host "`nAll processing completed successfully!" -ForegroundColor Green
    
    if (Test-Path $transaction.LogFile) {
        $newLogName = $transaction.LogFile -replace '\.txt$', '_completed.txt'
        Rename-Item -Path $transaction.LogFile -NewName $newLogName
    }

    Write-Host "TASK COMPLETE! Press any key to terminate..."
    cmd /c pause > $null
    exit
}
catch {
    Write-Host "`nScript terminated due to error: $_" -ForegroundColor Red
    $transaction.Rollback()
    Write-Host "Press any key to terminate..."
    cmd /c pause > $null
    exit 1
}
