library(dplyr)
library(tools)

# Define paths
input_dir <- file.path(Sys.getenv("USERPROFILE"), "Downloads", "Change Case")
output_dir <- input_dir
skip_columns <- c("ST", "STATE", "Country")

# Process all CSV files
csv_files <- list.files(input_dir, pattern = "\\.csv$", full.names = TRUE)

for (file_path in csv_files) {
  # Read CSV
  df <- read.csv(file_path)
  
  # Convert columns to proper case
  char_cols <- sapply(df, is.character)
  for (col in names(df)[char_cols]) {
    if (!col %in% skip_columns) {
      df[[col]] <- tools::toTitleCase(df[[col]])
    }
  }
  
  # Create output filename
  base_name <- tools::file_path_sans_ext(basename(file_path))
  output_filename <- paste0(base_name, "_proper.csv")
  output_path <- file.path(output_dir, output_filename)
  
  # Save converted file
  write.csv(df, output_path, row.names = FALSE)
  cat(sprintf("Processed: %s -> %s\n", basename(file_path), output_filename))
}
