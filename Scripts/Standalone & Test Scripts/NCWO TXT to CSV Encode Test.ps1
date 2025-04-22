#!pwsh
Clear-Host
Write-Host "Starting NCWO file processing..." -ForegroundColor Cyan

# Animation function
function Show-ProcessingAnimation {
    param($Message)
    $spinner = @('|', '/', '-', '\')
    $spinnerPos = 0
    Write-Host "`r$Message $($spinner[$spinnerPos])" -NoNewline
    $script:spinnerPos = ($spinnerPos + 1) % 4
}

try {
    # Define input directory
    $inputDir = "C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH\DM03\INPUT"
    $outputFile = Join-Path $inputDir "ALLINPUT.csv"

    # Define column names
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

    # Display column names from each file
    Get-ChildItem -Path $inputDir -Filter "*.txt" | ForEach-Object {
        Write-Host "`nColumns in $($_.Name):"
        $firstLine = Get-Content $_.FullName -TotalCount 1
        Write-Host $firstLine
    }

    # Process TXT files with explicit UTF-8 encoding
    $txtFiles = Get-ChildItem -Path $inputDir -Filter "*.txt"
    Write-Host "Found $($txtFiles.Count) TXT files to process" -ForegroundColor Cyan

    $txtFiles | ForEach-Object {
        Write-Host "`nProcessing: $($_.Name)" -ForegroundColor Yellow
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
        Write-Host "Completed processing: $($_.Name)" -ForegroundColor Green
    }

    # Show unique version codes found
    Write-Host "`nUnique version codes found:"
    $allData | Select-Object -ExpandProperty Creative_Version_Cd -Unique | ForEach-Object {
        Write-Host $_
    }

    Write-Host "`nProcessing data and creating files..." -ForegroundColor Yellow

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
        Show-ProcessingAnimation "Creating version file: $($versionCodes[$version])"
        $versionData = $allData | Where-Object { $_.'Creative_Version_Cd' -eq $version }
        if ($versionData) {
            $versionData | ForEach-Object { 
                $_.'VERSION' = $versionCodes[$version] -replace '\.csv$', '' 
            }
            $outputPath = Join-Path $inputDir $versionCodes[$version]
            [System.IO.File]::WriteAllLines($outputPath, 
                (@($columnNames -join ',') + ($versionData | ConvertTo-Csv -NoTypeInformation | Select-Object -Skip 1)), 
                [System.Text.Encoding]::UTF8)
            Write-Host "`nCreated: $($versionCodes[$version])" -ForegroundColor Green
        }
    }

    Write-Host "`nProcessing complete! All files created successfully." -ForegroundColor Green

    # Create detailed log file with timestamp
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $logFile = Join-Path $PSScriptRoot "ncwo_processing_log_${timestamp}.txt"

    # Write processing details to log
    "Processing completed at $(Get-Date)" | Out-File $logFile
    "Input Files Processed:" | Add-Content $logFile
    $txtFiles | ForEach-Object { $_.Name } | Add-Content $logFile
    "`nUnique Version Codes Found:" | Add-Content $logFile
    $allData | Select-Object Creative_Version_Cd | Sort-Object Creative_Version_Cd -Unique | Add-Content $logFile
    "`nOutput Files Created:" | Add-Content $logFile
    Get-ChildItem -Path $inputDir -Filter "*.csv" | ForEach-Object { $_.Name } | Add-Content $logFile

    Write-Host "`nLog file created at: $logFile" -ForegroundColor Green
    Write-Host "`nPlease copy any needed information from the screen now."
    Write-Host "Press any key when ready to exit..." -ForegroundColor Yellow
    $host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

} catch {
    Write-Host "`nError occurred during processing:" -ForegroundColor Red
    Write-Host "Details: $_" -ForegroundColor Red
    Write-Host "Location: $($_.InvocationInfo.PositionMessage)" -ForegroundColor Red
    Write-Host "`nPress any key to exit..."
    $host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
}
