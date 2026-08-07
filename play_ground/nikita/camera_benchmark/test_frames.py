import cv2

pipeline = (
    "v4l2src device=/dev/video0 ! "
    "video/x-raw,format=BGR,width=640,height=480 ! "
    "appsink"
)

cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)

ret, frame = cap.read()

print(ret)
if ret:
    print(frame.min(), frame.max())
    cv2.imwrite("gst_frame.jpg", frame)

cap.release()