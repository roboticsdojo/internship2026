import os
import cv2 
from ultralytics import YOLO

model = YOLO("yolo11n.pt")

cap=cv2.VideoCapture(0) 
output_dir = "saved_detections_pt" 
os.makedirs(output_dir, exist_ok=True) 

img_counter = 0 
print("Starting pipeline in headless mode...") 
print("Saving frames to disk. Press CTRL+C in terminal to quit.") 

frame_count = 0 
try: 
    while cap.isOpened(): 
        ret, frame = cap.read() 
        if not ret: print("Failed to grab frame.") 
        break 

except KeyboardInterrupt: 
    print("\nPipeline stopped by user.")

finally: 
 cap.release()
