# Define paths
input_folder <- file.path(Sys.getenv("USERPROFILE"), "Desktop", "AUTOMATION", "RAC", "LOYALTY", "JOB", "OUTPUT")
output_folder <- file.path(Sys.getenv("USERPROFILE"), "Desktop", "AUTOMATION", "RAC", "LOYALTY", "JOB", "PROOF")

# List CSV files in input folder
csv_files <- list.files(path = input_folder, pattern = "*.csv", full.names = TRUE)

if (length(csv_files) > 0) {
  # Read the first CSV file with na.strings = "" to treat empty values as empty strings
  data <- read.csv(csv_files[1], check.names = FALSE, na.strings = "")
  
  # Sort by Cell_Cd
  sorted_data <- data[order(data$Cell_Cd), ]
  
  # Create output filename
  input_filename <- basename(csv_files[1])
  output_filename <- paste0(tools::file_path_sans_ext(input_filename), "-PD.csv")
  output_path <- file.path(output_folder, output_filename)
  
  # Save sorted data with na = "" to write empty strings instead of NA
  write.csv(sorted_data, output_path, row.names = FALSE, na = "")
  cat(sprintf("Sorted file saved as: %s\n", output_path))
} else {
  cat("No CSV files found in the input folder\n")
}
