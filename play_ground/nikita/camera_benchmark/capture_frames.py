import cv2
import time
import math

gst_pipeline = (
    "libcamerasrc !",
    "video/x-raw, width=640, height=480, framerate=30/1 !"
    "videoconvert !"
    "appsink drop=true"
)

cam=cv2.VideoCapture(gst_pipeline, cv2.CAP_GSTREAMER)

start=time.perf_counter()

while True:
    ret, frame= cam.read()

    if not ret:
        print("Error: Failed to grab frame from pi camera")
        break

end_time=time.perf_counter()
loop_duration=end_time - start_time
start_time=end_time

if loop_duration > 0:
    fps=math.ceil(1/loop_duration)
else:
    fps=0

cv2.putText(frame, f'FPS: {fps}', (10,30), cv2.FONT_HERSHEY_SIMPLEX, 1, (255,0,0), 2)
cv2.imshow("Pi Camera V2 Feed", frame)

if cv2.waitKey(1) & 0xFF='q':
    break

cam.release()
cv2.destroyAllWindows()


#cam=cv2.VideoCapture(0)
#while True:
#    start=time.time()
#    ret, frame=cam.read()
#    end=time.time()
#    fps = math.ceil(1/(end-start))

#    cv2.putText(frame, f'FPS: {fps}', (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
#    cv2.imshow("Camera", frame)
#    if cv2.waitKey(1) & 0xFF == ord('q'):
#        break
        