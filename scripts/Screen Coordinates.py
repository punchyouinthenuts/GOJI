import pyautogui
import time

print("Move your mouse to the desired position...")
time.sleep(5)
x, y = pyautogui.position()
print(f"Coordinates are: x={x}, y={y}")
