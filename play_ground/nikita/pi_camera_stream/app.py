from flask import Flask, render_template, Response
import cv2

app = Flask(__name__)

def generate_frames():
    # Initialize the camera (0 is usually the default Pi Camera)
    camera = cv2.VideoCapture(0, cv2.CAP_V4L2)
    
    # Optional: Set camera resolution to improve framerate
    camera.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    while True:
        success, frame = camera.read()
        if not success:
            break
        else:
            # Encode the frame in JPEG format
            ret, buffer = cv2.imencode('.jpg', frame)
            frame_bytes = buffer.tobytes()
            
            # Yield the frame in the byte format expected by MJPEG
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')

@app.route('/')
def index():
    # Serve the HTML webpage
    return render_template('index.html')

@app.route('/video_feed')
def video_feed():
    # Return the multipart response containing the video stream
    return Response(generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == "__main__":
    # Run the Flask app on all available IP addresses on port 5000
    app.run(host='0.0.0.0', port=5000, debug=False)