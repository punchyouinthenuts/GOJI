import pyautogui
import time
import os

# Define the screenshot save directory
SCREENSHOT_DIR = r"C:\Users\JCox\Desktop\AUTOMATION\AUTOMATION SCREENSHOTS"

def take_screenshot():
    print("\nPrepare to position your mouse!")
    print("Countdown starting:")
    for i in range(5,0,-1):
        print(f"{i}...")
        time.sleep(1)
    
    # Get mouse position
    x, y = pyautogui.position()
    
    # Take screenshot of region around mouse (increased size)
    screenshot = pyautogui.screenshot(region=(x-50, y-50, 100, 100))
    
    # Save the screenshot to specified directory
    element_name = input("Enter name for this screenshot (e.g., 'bcc_button'): ")
    screenshot.save(os.path.join(SCREENSHOT_DIR, f'{element_name}.png'))
    print(f"Screenshot saved as {element_name}.png in {SCREENSHOT_DIR}")

print("Let's capture a larger area for the BCC button")

# Create the screenshot directory if it doesn't exist
os.makedirs(SCREENSHOT_DIR, exist_ok=True)

input("\nPress Enter when ready to start capturing screenshot...")

take_screenshot()
