#!/usr/bin/env python3
"""
GOJI Rollback Helper Script
Reads rollback log files and executes rollback operations
"""

import os
import sys
import json
import shutil
from datetime import datetime

def rollback_from_log(log_file_path):
    """
    Read rollback log and execute rollback operations
    
    Args:
        log_file_path (str): Path to the rollback log file
        
    Returns:
        bool: True if rollback successful, False otherwise
    """
    if not os.path.exists(log_file_path):
        print(f"=== ROLLBACK_LOG_NOT_FOUND: {log_file_path} ===")
        return True  # No log means no operations to rollback
    
    try:
        # Read the rollback log
        with open(log_file_path, 'r') as f:
            operations = json.load(f)
        
        if not operations:
            print("=== ROLLBACK_LOG_EMPTY ===")
            return True
        
        print(f"=== ROLLBACK_OPERATIONS_FOUND: {len(operations)} ===")
        
        # Execute rollback operations in reverse order
        success_count = 0
        error_count = 0
        
        for operation in reversed(operations):
            try:
                op_type = operation.get("type")
                
                if op_type == "move":
                    # Restore moved file
                    source = operation.get("destination")
                    dest = operation.get("source")
                    
                    if source and dest and os.path.exists(source):
                        shutil.move(source, dest)
                        print(f"=== ROLLBACK_RESTORED: {os.path.basename(dest)} ===")
                        success_count += 1
                    else:
                        print(f"=== ROLLBACK_SKIP: File not found: {source} ===")
                        
                elif op_type == "create":
                    # Delete created file
                    created_file = operation.get("created_file")
                    
                    if created_file and os.path.exists(created_file):
                        os.remove(created_file)
                        print(f"=== ROLLBACK_DELETED: {os.path.basename(created_file)} ===")
                        success_count += 1
                    else:
                        print(f"=== ROLLBACK_SKIP: File not found: {created_file} ===")
                        
                elif op_type == "mkdir":
                    # Remove created directory if empty
                    created_dir = operation.get("created_file")
                    
                    if created_dir and os.path.exists(created_dir):
                        try:
                            os.rmdir(created_dir)
                            print(f"=== ROLLBACK_REMOVED_DIR: {os.path.basename(created_dir)} ===")
                            success_count += 1
                        except OSError:
                            # Directory not empty, leave it
                            print(f"=== ROLLBACK_SKIP: Directory not empty: {created_dir} ===")
                            
                elif op_type == "backup":
                    # Restore from backup (for script 02)
                    file_path = operation.get("file_path")
                    backup_path = operation.get("backup_path")
                    
                    if backup_path and file_path and os.path.exists(backup_path):
                        if os.path.exists(file_path):
                            # Replace modified file with backup
                            shutil.move(backup_path, file_path)
                            print(f"=== ROLLBACK_RESTORED_FROM_BACKUP: {os.path.basename(file_path)} ===")
                            success_count += 1
                        else:
                            print(f"=== ROLLBACK_ERROR: Original file missing: {file_path} ===")
                            error_count += 1
                    else:
                        print(f"=== ROLLBACK_ERROR: Backup file missing: {backup_path} ===")
                        error_count += 1
                        
                elif op_type == "rename":
                    # Restore original filename (for script 03)
                    original_path = operation.get("original_path")
                    new_path = operation.get("new_path")
                    
                    if new_path and original_path and os.path.exists(new_path):
                        os.rename(new_path, original_path)
                        print(f"=== ROLLBACK_RENAMED: {os.path.basename(new_path)} -> {os.path.basename(original_path)} ===")
                        success_count += 1
                    else:
                        print(f"=== ROLLBACK_ERROR: Renamed file not found: {new_path} ===")
                        error_count += 1
                        
                else:
                    print(f"=== ROLLBACK_UNKNOWN_OPERATION: {op_type} ===")
                    error_count += 1
                    
            except Exception as e:
                print(f"=== ROLLBACK_OPERATION_ERROR: {str(e)} ===")
                error_count += 1
        
        # Clean up any remaining backup files from script 02
        temp_dir = os.path.dirname(log_file_path)
        if os.path.exists(temp_dir):
            for file in os.listdir(temp_dir):
                if file.startswith("backup_02_"):
                    backup_file = os.path.join(temp_dir, file)
                    try:
                        os.remove(backup_file)
                        print(f"=== ROLLBACK_CLEANUP: {file} ===")
                    except:
                        pass
        
        print(f"=== ROLLBACK_SUMMARY: {success_count} successful, {error_count} errors ===")
        
        # Remove the rollback log file
        try:
            os.remove(log_file_path)
            print(f"=== ROLLBACK_LOG_REMOVED: {os.path.basename(log_file_path)} ===")
        except:
            print(f"=== ROLLBACK_LOG_CLEANUP_ERROR: {os.path.basename(log_file_path)} ===")
        
        return error_count == 0
        
    except Exception as e:
        print(f"=== ROLLBACK_CRITICAL_ERROR: {str(e)} ===")
        return False

def main():
    """Main function"""
    if len(sys.argv) != 2:
        print("=== ROLLBACK_USAGE_ERROR: Missing log file path ===")
        return 1
    
    log_file_path = sys.argv[1]
    
    print(f"=== ROLLBACK_STARTING: {os.path.basename(log_file_path)} ===")
    
    success = rollback_from_log(log_file_path)
    
    if success:
        print("=== ROLLBACK_SUCCESS ===")
        return 0
    else:
        print("=== ROLLBACK_FAILED ===")
        return 1

if __name__ == "__main__":
    sys.exit(main())