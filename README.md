# Real-time Person Detection with ESP32-CAM

This project implements a real-time person detection system using an ESP32-CAM, FastAPI backend, and React frontend. The system processes video frames from the ESP32-CAM using YOLOv5 to detect people and displays the results in a web interface.

## Project Structure

```
.
├── backend/
│   ├── app/
│   │   ├── main.py          # FastAPI application
│   │   ├── model.py         # YOLOv5 model handler
│   │   └── schemas.py       # Pydantic models
│   └── requirements.txt     # Python dependencies
└── frontend/
    ├── src/
    │   ├── App.tsx         # Main React component
    │   ├── types/          # TypeScript types
    │   └── ...
    ├── package.json        # Node.js dependencies
    └── ...
```

## Setup

### Backend

1. Create a Python virtual environment:
   ```bash
   python -m venv venv
   source venv/bin/activate  # On Windows: venv\Scripts\activate
   ```

2. Install dependencies:
   ```bash
   cd backend
   pip install -r requirements.txt
   ```

3. Start the FastAPI server:
   ```bash
   uvicorn app.main:app --reload --port 8000
   ```

The API will be available at http://localhost:8000 with Swagger documentation at http://localhost:8000/docs

### Frontend

1. Install dependencies:
   ```bash
   cd frontend
   npm install
   ```

2. Start the development server:
   ```bash
   npm run dev
   ```

The web interface will be available at http://localhost:5173

## Features

- Real-time person detection using YOLOv5
- Live video feed display with detection overlays
- Detection status and statistics
- Pause/Resume detection functionality
- Error handling and status messages
- Modern UI with Tailwind CSS

## ESP32-CAM Integration

The ESP32-CAM should be programmed to capture frames and send them to the backend endpoint:
`POST http://localhost:8000/detect/`

The image should be sent as multipart/form-data with the field name 'file'.

## API Endpoints

- `GET /` - Health check endpoint
- `POST /detect/` - Submit an image for person detection
  - Request: multipart/form-data with 'file' field
  - Response: JSON with detection results

## Technologies Used

- Backend:
  - FastAPI
  - PyTorch
  - YOLOv5
  - OpenCV
  - Python 3.8+

- Frontend:
  - React
  - TypeScript
  - Tailwind CSS
  - Vite
  - Axios

## Notes

- The frontend currently simulates a camera feed for testing. Replace with actual ESP32-CAM frames in production.
- Adjust the detection confidence threshold in `model.py` if needed.
- The system is configured for local development by default. 