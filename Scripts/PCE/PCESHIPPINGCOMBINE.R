# Set working directory and paths
input_path <- file.path(Sys.getenv("USERPROFILE"), "Desktop", "AUTOMATION", "RAC", "PCE", "DATA")

# Define files and multiplication factors
file_multipliers <- list(
  "BI_75.csv" = 75,
  "BI_100.csv" = 100,
  "BI_150.csv" = 150,
  "BI_300.csv" = 300,
  "BI_400.csv" = 400,
  "English_75.csv" = 75,
  "English_100.csv" = 100,
  "English_300.csv" = 300
)

# Process each file
for (filename in names(file_multipliers)) {
  file_path <- file.path(input_path, filename)
  if (file.exists(file_path)) {
    df <- read.csv(file_path, header = FALSE, skip = 1)
    df <- df[, -c(1, 2, 7, 8, 9, 10)]
    df_multiplied <- df[rep(seq_len(nrow(df)), file_multipliers[[filename]]), ]
    write.csv(df_multiplied, file_path, row.names = FALSE, quote = FALSE)
    cat(sprintf("Successfully processed %s\n", filename))
  }
}

# Define file order and combine
file_order <- c("BI_75.csv", "BI_100.csv", "BI_150.csv", "BI_300.csv", 
                "BI_400.csv", "English_75.csv", "English_100.csv", "English_300.csv")

combined_df <- data.frame()
for (filename in file_order) {
  file_path <- file.path(input_path, filename)
  if (file.exists(file_path)) {
    temp_df <- read.csv(file_path, header = FALSE)
    combined_df <- rbind(combined_df, temp_df)
  }
}

# Remove empty rows and add break marks
combined_df <- combined_df[rowSums(is.na(combined_df)) != ncol(combined_df), ]
groups <- paste(combined_df[,3], combined_df[,4])
last_in_group <- c(groups[-1] != groups[-length(groups)], TRUE)
combined_df[last_in_group, 5] <- "BM"

# Add headers and save
colnames(combined_df) <- c("Address Line 1", "City", "State", "ZIP Code", "Break Mark")
write.csv(combined_df, file.path(input_path, "PCE_COMBINE.csv"), row.names = FALSE)

# Delete input files
for (filename in file_order) {
  file_path <- file.path(input_path, filename)
  if (file.exists(file_path)) {
    file.remove(file_path)
    cat(sprintf("Deleted input file: %s\n", filename))
  }
}

cat("\nSuccessfully created combined file: PCE_COMBINE.csv\n")
cat("All input CSV files have been deleted\n")
