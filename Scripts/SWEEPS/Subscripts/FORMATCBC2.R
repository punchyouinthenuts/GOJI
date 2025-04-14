# Define paths
file_path <- file.path(Sys.getenv("USERPROFILE"), "Desktop", "AUTOMATION", "RAC", "SWEEPS", "JOB", "OUTPUT", "SWEEPS.csv")
output_path <- file.path(Sys.getenv("USERPROFILE"), "Desktop", "AUTOMATION", "RAC", "SWEEPS", "JOB", "OUTPUT", "SWEEPSREFORMAT.csv")

# Process data
tryCatch({
  # Read data
  data <- read.csv(file_path)
  
  # Verify column exists
  if (!"CUSTOM_03" %in% names(data)) {
    stop("CUSTOM_03 column not found in the CSV file")
  }
  
  # Format CUSTOM_03 as currency without dollar sign
  data$CUSTOM_03 <- format(as.numeric(data$CUSTOM_03), nsmall=2, big.mark=",")
  
  # Save modified data
  write.csv(data, output_path, row.names=FALSE)
  
}, error = function(e) {
  cat(sprintf("Error: %s\n", e$message))
  quit(status=1)
})
