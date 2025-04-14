# Define paths
$csvFile = "$env:USERPROFILE\Desktop\AUTOMATION\RAC\LOC\JOB\OUTPUT\LOC_MONTHLY_OUTPUT.csv"

# Import CSV data
$records = Import-Csv -Path $csvFile

# Get unique Store_Ids (first 15)
$uniqueRecords = $records | Sort-Object Store_Id -Unique | Select-Object -First 15

# Get user input for folder creation
$jobInfo = Read-Host "ENTER MONTHLY LOC JOB NUMBER AND MONTH"

# Export filtered data to CSV
$csvProofPath = "C:\Program Files\Goji\RAC\LOC\JOB\PROOF\LOC_MONTHLY_OUTPUT (PROOF DATA).csv"
$uniqueRecords | Export-Csv -Path $csvProofPath -NoTypeInformation
