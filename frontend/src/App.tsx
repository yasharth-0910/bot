import { useState, useEffect, useRef } from 'react';
import axios from 'axios';
import { DetectionResponse } from './types/detection';

const BACKEND_URL = 'http://localhost:8000';
const POLLING_INTERVAL = 500; // ms

function App() {
  const [isDetecting, setIsDetecting] = useState(true);
  const [detectionData, setDetectionData] = useState<DetectionResponse | null>(null);
  const [error, setError] = useState<string | null>(null);
  const videoRef = useRef<HTMLCanvasElement>(null);
  const overlayRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    let intervalId: number;

    const fetchFrame = async () => {
      try {
        // Create a canvas element to capture the webcam frame
        const canvas = videoRef.current;
        if (!canvas) return;

        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        // Clear previous frame
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        // Draw test image (in production, this would be the ESP32-CAM frame)
        ctx.fillStyle = '#333';
        ctx.fillRect(0, 0, canvas.width, canvas.height);
        ctx.fillStyle = '#666';
        ctx.fillText('Simulated Camera Feed', 10, 20);

        // Convert canvas to blob
        const blob = await new Promise<Blob>((resolve) => {
          canvas.toBlob((blob) => {
            if (blob) resolve(blob);
          }, 'image/jpeg');
        });

        if (!isDetecting) return;

        // Create form data
        const formData = new FormData();
        formData.append('file', blob, 'frame.jpg');

        // Send to backend
        const response = await axios.post<DetectionResponse>(
          `${BACKEND_URL}/detect/`,
          formData,
          {
            headers: {
              'Content-Type': 'multipart/form-data',
            },
          }
        );

        setDetectionData(response.data);
        setError(null);
        drawDetections(response.data);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'An error occurred');
      }
    };

    if (isDetecting) {
      fetchFrame();
      intervalId = window.setInterval(fetchFrame, POLLING_INTERVAL);
    }

    return () => {
      if (intervalId) {
        clearInterval(intervalId);
      }
    };
  }, [isDetecting]);

  const drawDetections = (data: DetectionResponse) => {
    const overlay = overlayRef.current;
    if (!overlay) return;

    const ctx = overlay.getContext('2d');
    if (!ctx) return;

    // Clear previous detections
    ctx.clearRect(0, 0, overlay.width, overlay.height);

    // Draw bounding boxes
    ctx.strokeStyle = '#00ff00';
    ctx.lineWidth = 2;
    ctx.font = '14px Arial';
    ctx.fillStyle = '#00ff00';

    data.detections.forEach((detection) => {
      const { x, y, width, height } = detection.bbox;
      ctx.strokeRect(x, y, width, height);
      ctx.fillText(
        `Person ${Math.round(detection.confidence * 100)}%`,
        x,
        y - 5
      );
    });
  };

  return (
    <div className="min-h-screen bg-gray-900 text-white p-8">
      <div className="max-w-4xl mx-auto">
        <h1 className="text-3xl font-bold mb-8">Person Detection</h1>
        
        <div className="relative mb-6">
          <canvas
            ref={videoRef}
            width={640}
            height={480}
            className="bg-black rounded-lg"
          />
          <canvas
            ref={overlayRef}
            width={640}
            height={480}
            className="absolute top-0 left-0 pointer-events-none"
          />
        </div>

        <div className="flex items-center justify-between mb-6">
          <button
            onClick={() => setIsDetecting(!isDetecting)}
            className={`px-4 py-2 rounded-lg font-medium ${
              isDetecting
                ? 'bg-red-500 hover:bg-red-600'
                : 'bg-green-500 hover:bg-green-600'
            }`}
          >
            {isDetecting ? 'Pause Detection' : 'Resume Detection'}
          </button>

          <div className="text-lg">
            {detectionData && (
              <span>
                People detected: {detectionData.total_people} (
                {detectionData.processing_time.toFixed(2)}ms)
              </span>
            )}
          </div>
        </div>

        {error && (
          <div className="bg-red-500 text-white p-4 rounded-lg mb-6">
            {error}
          </div>
        )}
      </div>
    </div>
  );
}

export default App;
