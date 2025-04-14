# Load required libraries
library(logging)
library(dplyr)

# Configure logging
basicConfig(level='INFO')

# Define paths
input_dir <- file.path(Sys.getenv("USERPROFILE"), "Desktop", "AUTOMATION", "RAC", "SWEEPS", "JOB", "OUTPUT")
output_dir <- file.path(Sys.getenv("USERPROFILE"), "Desktop", "AUTOMATION", "RAC", "SWEEPS", "JOB", "PROOF")
file_name <- "SWEEPSREFORMAT.csv"

# Process and save function
process_and_save <- function(input_path, output_path) {
  tryCatch({
    # Read data
    data <- read.csv(input_path)
    
    # Process each version
    result <- data %>%
      group_by(Creative_Version_Cd) %>%
      group_modify(~{
        licensed <- filter(.x, !is.na(Store_License)) %>% head(1)
        unlicensed <- filter(.x, is.na(Store_License))
        bind_rows(licensed, unlicensed) %>% head(15)
      }) %>%
      ungroup()
    
    # Create directory if needed and save
    dir.create(dirname(output_path), showWarnings = FALSE, recursive = TRUE)
    write.csv(result, output_path, row.names = FALSE)
    
    loginfo(sprintf("Successfully processed data and saved to %s", output_path))
    TRUE
    
  }, error = function(e) {
    logerror(sprintf("An error occurred: %s", e$message))
    FALSE
  })
}

# Execute processing
input_path <- file.path(input_dir, file_name)
output_path <- file.path(output_dir, sub("\\.csv$", "-PD.csv", file_name))
process_and_save(input_path, output_path)
