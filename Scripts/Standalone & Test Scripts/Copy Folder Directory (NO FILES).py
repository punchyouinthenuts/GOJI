import os

def copy_folder_structure(source_path, target_path):
    # Walk through the source directory
    for root, dirs, files in os.walk(source_path):
        # Get the relative path from the source
        relative_path = os.path.relpath(root, source_path)
        
        # Create the same path in the target
        if relative_path == '.':
            continue
            
        new_path = os.path.join(target_path, relative_path)
        os.makedirs(new_path, exist_ok=True)
        print(f"Created: {new_path}")

def main():
    # Get user input for source and target directories
    print("Enter the source folder path (folder to copy structure from):")
    source_folder = input().strip()
    
    print("Enter the target folder path (folder to create structure in):")
    target_folder = input().strip()
    
    # Verify the source path exists
    if not os.path.exists(source_folder):
        print("Source folder does not exist!")
        return
        
    # Create target folder if it doesn't exist
    os.makedirs(target_folder, exist_ok=True)
    
    # Copy the structure
    print("\nCreating folder structure...")
    copy_folder_structure(source_folder, target_folder)
    print("\nFolder structure creation completed!")

if __name__ == "__main__":
    main()
